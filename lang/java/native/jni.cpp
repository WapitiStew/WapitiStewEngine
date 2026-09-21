//*****************************************************************************************************************
//!
//! @file    jni.cpp
//! @brief   \~japanese WSE Binding facadeのJNI Adapter.
//! @brief   \~english  JNI adapter for the WSE binding native facade.
//!
//! @date
//!   Aug-29, 2026   Create New.
//*****************************************************************************************************************

#include <utility>
#include <jni.h>

#include <wse/binding/stew.h>
#ifdef WSE_HAS_TMR
#include <tmr/camera/CameraFrameOps.h>
#include <tmr/stew.h>
#endif
#ifdef WSE_HAS_OUI
#include <oui/stew.h>
#endif
#ifdef WSE_HAS_IUI
#include <iui/stew.h>
#include <wse/binding/IuiErrorAdapter.h>
#endif
#ifdef WSE_HAS_XPT
#include <xpt/stew.h>
#include <wse/binding/XptErrorAdapter.h>
#include "../../common/transfer_failure.h"
#endif
#ifdef WSE_EXTENSION_JNI_BINDING
#include "wse_extension_jni_includes.inc"
#endif
#ifdef WSE_EXTENSION_TMR_BINDING
#include "wse_extension_tmr_includes.inc"
#endif

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <thread>
#include <vector>

namespace
{

// ----------------------------------------------------------------------------------------------
// Native holders behind Java handles
//
// Java sees every native object as an opaque `long`. Each holder below is the single RAII owner
// of what that handle refers to, and the matching `...Close` entry point is the only thing that
// deletes it. A holder that keeps a `jobject` callback holds a global reference, released on the
// same close path.
// ----------------------------------------------------------------------------------------------

JavaVM* g_java_vm = nullptr;

struct JavaRuntime final
{
    std::mutex mutex;
    bool closed;
    wse::binding::CancellationSource cancellation;

    //! @brief Construct all members with explicit defaults.
    JavaRuntime()
        : mutex        ()
        , closed       ( false )
        , cancellation ()
    {
    }
};

struct JavaBuffer final
{
    wse::binding::FrameBuffer frame;
    std::uint8_t empty_storage;

    //! @brief Construct all members with explicit defaults.
    JavaBuffer(
          const wse::binding::FrameBuffer& frame_in = {}
        , std::uint8_t empty_storage_in = 0U
    )
        : frame         ( frame_in )
        , empty_storage ( empty_storage_in )
    {
    }
};

struct JavaTimer final
{
    JavaVM* vm;
    jobject callback;
    wse::binding::CancellationSource cancellation;
    std::thread worker;
    std::atomic_bool delete_on_exit;

    //! @brief Construct all members with explicit defaults.
    JavaTimer()
        : vm             ( nullptr )
        , callback       ( nullptr )
        , cancellation   ()
        , worker         ()
        , delete_on_exit ( false )
    {
    }
};

#ifdef WSE_HAS_TMR
struct JavaCamera final
{
    std::unique_ptr<wse::tmr::WebCamera> camera;
    jobject callback;

    //! @brief Construct all members with explicit defaults.
    JavaCamera(
          std::unique_ptr<wse::tmr::WebCamera> camera_in = std::make_unique<wse::tmr::WebCamera>()
        , const jobject& callback_in = nullptr
    )
        : camera   ( std::move( camera_in ) )
        , callback ( callback_in )
    {
    }
};
#endif

template <typename T>
T* fromHandle( const jlong handle_in ) noexcept
{
    return reinterpret_cast<T*>( static_cast<std::uintptr_t>( handle_in ) );
}

template <typename T>
jlong toHandle( T* pointer_in ) noexcept
{
    return static_cast<jlong>( reinterpret_cast<std::uintptr_t>( pointer_in ) );
}

// ----------------------------------------------------------------------------------------------
// Error translation and Java object construction helpers. C++ exceptions never cross the JNI
// boundary; a failure becomes a thrown WseException carrying the portable error fields.
// ----------------------------------------------------------------------------------------------

void throwWseError( JNIEnv* env_in, const wse::binding::Error& error_in )
{
    jclass error_class = env_in->FindClass( "io/wapitistew/wse/WseException" );
    if ( error_class == nullptr )
    {
        return;
    }
    jmethodID constructor = env_in->GetMethodID(
        error_class, "<init>", "(IIJLjava/lang/String;)V" );
    if ( constructor == nullptr )
    {
        return;
    }
    jstring message = env_in->NewStringUTF( error_in.message().c_str() );
    jobject exception = env_in->NewObject(
          error_class
        , constructor
        , static_cast<jint>( error_in.category() )
        , static_cast<jint>( error_in.code() )
        , static_cast<jlong>( error_in.nativeCode() )
        , message
    );
    if ( exception != nullptr )
    {
        env_in->Throw( static_cast<jthrowable>( exception ) );
    }
}

jobject makeWseException( JNIEnv* env_in, const wse::binding::Error& error_in )
{
    jclass error_class = env_in->FindClass( "io/wapitistew/wse/WseException" );
    jmethodID constructor = error_class == nullptr ? nullptr : env_in->GetMethodID(
        error_class, "<init>", "(IIJLjava/lang/String;)V" );
    if ( constructor == nullptr )
    {
        return nullptr;
    }
    jstring message = env_in->NewStringUTF( error_in.message().c_str() );
    return env_in->NewObject(
          error_class
        , constructor
        , static_cast<jint>( error_in.category() )
        , static_cast<jint>( error_in.code() )
        , static_cast<jlong>( error_in.nativeCode() )
        , message
    );
}

wse::binding::Error invalidStateError()
{
    return wse::binding::Error(
          wse::binding::eErrorCategory::InvalidState
        , 1
        , "The WSE Java runtime is closed or invalid."
    );
}

wse::binding::Error invalidBufferError()
{
    return wse::binding::Error(
          wse::binding::eErrorCategory::InvalidArgument
        , 2
        , "copyFrame requires a direct ByteBuffer."
    );
}

wse::binding::CancellationToken operationToken(
      JNIEnv* env_in
    , JavaRuntime* runtime_in
)
{
    if ( runtime_in == nullptr )
    {
        throwWseError( env_in, invalidStateError() );
        return {};
    }
    std::lock_guard<std::mutex> lock( runtime_in->mutex );
    if ( runtime_in->closed )
    {
        throwWseError( env_in, invalidStateError() );
        return {};
    }
    return runtime_in->cancellation.token();
}

void destroyTimer( JNIEnv* env_in, JavaTimer* timer_in )
{
    if ( timer_in == nullptr )
    {
        return;
    }
    if ( timer_in->callback != nullptr )
    {
        env_in->DeleteGlobalRef( timer_in->callback );
        timer_in->callback = nullptr;
    }
    delete timer_in;
}

jobject makeNativeBuffer(
      JNIEnv* env_in
    , wse::binding::FrameBuffer frame_in
)
{
    auto* owner = new ( std::nothrow ) JavaBuffer{ std::move( frame_in ) };
    if ( owner == nullptr )
    {
        throwWseError( env_in, wse::binding::Error(
              wse::binding::eErrorCategory::ResourceExhausted
            , 1
            , "Unable to allocate a Java native buffer."
        ) );
        return nullptr;
    }
    const auto& bytes = owner->frame.bytes();
    jobject direct_buffer = env_in->NewDirectByteBuffer(
          bytes.empty() ? &owner->empty_storage : const_cast<std::uint8_t*>( bytes.data() )
        , static_cast<jlong>( bytes.size() )
    );
    jclass buffer_class = env_in->FindClass( "io/wapitistew/wse/NativeBuffer" );
    jmethodID constructor = buffer_class == nullptr ? nullptr : env_in->GetMethodID(
        buffer_class, "<init>", "(JLjava/nio/ByteBuffer;)V" );
    if ( direct_buffer == nullptr || constructor == nullptr )
    {
        delete owner;
        return nullptr;
    }
    return env_in->NewObject( buffer_class, constructor, toHandle( owner ), direct_buffer );
}

std::string javaString( JNIEnv* env_in, jstring value_in )
{
    if ( value_in == nullptr )
    {
        return {};
    }
    const char* text = env_in->GetStringUTFChars( value_in, nullptr );
    if ( text == nullptr )
    {
        return {};
    }
    std::string result( text );
    env_in->ReleaseStringUTFChars( value_in, text );
    return result;
}

jobject makeEnum(
      JNIEnv* env_in
    , const char* class_name_in
    , const jint code_in
)
{
    jclass enum_class = env_in->FindClass( class_name_in );
    if ( enum_class == nullptr )
    {
        return nullptr;
    }
    std::string signature = "(I)L";
    signature += class_name_in;
    signature += ";";
    jmethodID from_code = env_in->GetStaticMethodID(
        enum_class, "fromCode", signature.c_str() );
    return from_code == nullptr ? nullptr :
        env_in->CallStaticObjectMethod( enum_class, from_code, code_in );
}

#ifdef WSE_HAS_TMR
wse::binding::Error cameraError( const wse::tmr::CameraError& error_in )
{
    return wse::binding::fromTmrError( error_in );
}

jobject makeCameraDevice(
      JNIEnv* env_in
    , const wse::tmr::sCameraDeviceInfo& device_in
)
{
    jclass value_class = env_in->FindClass( "io/wapitistew/wse/CameraDevice" );
    jmethodID constructor = value_class == nullptr ? nullptr : env_in->GetMethodID(
        value_class, "<init>",
        "(Lio/wapitistew/wse/CameraBackend;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Lio/wapitistew/wse/CameraTransport;Lio/wapitistew/wse/CameraUsbIdentity;)V" );
    if ( constructor == nullptr ) return nullptr;
    jclass usb_class = env_in->FindClass( "io/wapitistew/wse/CameraUsbIdentity" );
    jmethodID usb_constructor = usb_class == nullptr ? nullptr : env_in->GetMethodID(
        usb_class, "<init>", "(IILjava/lang/String;I)V" );
    if ( usb_constructor == nullptr ) return nullptr;
    jobject usb = env_in->NewObject( usb_class, usb_constructor,
        static_cast<jint>( device_in.usb.vendor_id ),
        static_cast<jint>( device_in.usb.product_id ),
        env_in->NewStringUTF( device_in.usb.serial_number.c_str() ),
        static_cast<jint>( device_in.usb.uvc_version_bcd ) );
    return env_in->NewObject(
          value_class
        , constructor
        , makeEnum( env_in, "io/wapitistew/wse/CameraBackend",
              static_cast<jint>( device_in.backend ) )
        , env_in->NewStringUTF( device_in.id.c_str() )
        , env_in->NewStringUTF( device_in.display_name.c_str() )
        , env_in->NewStringUTF( device_in.transport.c_str() )
        , makeEnum( env_in, "io/wapitistew/wse/CameraTransport",
              static_cast<jint>( device_in.transport_type ) )
        , usb
    );
}

jobject makeCameraFormat(
      JNIEnv* env_in
    , const wse::tmr::sCameraFormat& format_in
)
{
    jclass value_class = env_in->FindClass( "io/wapitistew/wse/CameraFormat" );
    jmethodID constructor = value_class == nullptr ? nullptr : env_in->GetMethodID(
        value_class, "<init>", "(IIIILio/wapitistew/wse/CameraPixelFormat;)V" );
    return constructor == nullptr ? nullptr : env_in->NewObject(
          value_class
        , constructor
        , static_cast<jint>( format_in.width )
        , static_cast<jint>( format_in.height )
        , static_cast<jint>( format_in.frame_rate_numerator )
        , static_cast<jint>( format_in.frame_rate_denominator )
        , makeEnum( env_in, "io/wapitistew/wse/CameraPixelFormat",
              static_cast<jint>( format_in.pixel_format ) )
    );
}

jobject makeCameraControlCapability(
      JNIEnv* env_in
    , const wse::tmr::sCameraControlCapability& capability_in
)
{
    jclass value_class = env_in->FindClass(
        "io/wapitistew/wse/CameraControlCapability" );
    jmethodID constructor = value_class == nullptr ? nullptr : env_in->GetMethodID(
        value_class, "<init>",
        "(Lio/wapitistew/wse/CameraControl;JJJJZZLio/wapitistew/wse/CameraControlUnit;DZZLjava/lang/String;)V" );
    return constructor == nullptr ? nullptr : env_in->NewObject(
          value_class
        , constructor
        , makeEnum( env_in, "io/wapitistew/wse/CameraControl",
              static_cast<jint>( capability_in.control ) )
        , static_cast<jlong>( capability_in.minimum )
        , static_cast<jlong>( capability_in.maximum )
        , static_cast<jlong>( capability_in.step )
        , static_cast<jlong>( capability_in.default_value )
        , capability_in.supports_manual ? JNI_TRUE : JNI_FALSE
        , capability_in.supports_automatic ? JNI_TRUE : JNI_FALSE
        , makeEnum( env_in, "io/wapitistew/wse/CameraControlUnit",
              static_cast<jint>( capability_in.unit ) )
        , static_cast<jdouble>( capability_in.physical_scale )
        , capability_in.readable ? JNI_TRUE : JNI_FALSE
        , capability_in.writable ? JNI_TRUE : JNI_FALSE
        , env_in->NewStringUTF( capability_in.display_name.c_str() )
    );
}

jobject objectProperty( JNIEnv* env_in, jobject value_in, const char* name_in,
    const char* signature_in )
{
    if ( value_in == nullptr ) return nullptr;
    jclass value_class = env_in->GetObjectClass( value_in );
    jmethodID method = value_class == nullptr ? nullptr :
        env_in->GetMethodID( value_class, name_in, signature_in );
    return method == nullptr ? nullptr : env_in->CallObjectMethod( value_in, method );
}

jint enumCode( JNIEnv* env_in, jobject enum_in )
{
    if ( enum_in == nullptr ) return 0;
    jclass enum_class = env_in->GetObjectClass( enum_in );
    jmethodID code = enum_class == nullptr ? nullptr : env_in->GetMethodID( enum_class, "code", "()I" );
    return code == nullptr ? 0 : env_in->CallIntMethod( enum_in, code );
}

jint intProperty( JNIEnv* env_in, jobject value_in, const char* name_in )
{
    jclass value_class = env_in->GetObjectClass( value_in );
    jmethodID method = value_class == nullptr ? nullptr : env_in->GetMethodID( value_class, name_in, "()I" );
    return method == nullptr ? 0 : env_in->CallIntMethod( value_in, method );
}

jlong longProperty( JNIEnv* env_in, jobject value_in, const char* name_in )
{
    jclass value_class = env_in->GetObjectClass( value_in );
    jmethodID method = value_class == nullptr ? nullptr : env_in->GetMethodID( value_class, name_in, "()J" );
    return method == nullptr ? 0 : env_in->CallLongMethod( value_in, method );
}

jboolean boolProperty( JNIEnv* env_in, jobject value_in, const char* name_in )
{
    jclass value_class = env_in->GetObjectClass( value_in );
    jmethodID method = value_class == nullptr ? nullptr : env_in->GetMethodID( value_class, name_in, "()Z" );
    return method == nullptr ? JNI_FALSE : env_in->CallBooleanMethod( value_in, method );
}

wse::tmr::sCameraFormat cameraFormat( JNIEnv* env_in, jobject value_in )
{
    wse::tmr::sCameraFormat result;
    result.width = static_cast<std::uint32_t>( intProperty( env_in, value_in, "width" ) );
    result.height = static_cast<std::uint32_t>( intProperty( env_in, value_in, "height" ) );
    result.frame_rate_numerator = static_cast<std::uint32_t>( intProperty( env_in, value_in, "frameRateNumerator" ) );
    result.frame_rate_denominator = static_cast<std::uint32_t>( intProperty( env_in, value_in, "frameRateDenominator" ) );
    result.pixel_format = static_cast<wse::tmr::eCameraPixelFormat>( enumCode( env_in,
        objectProperty( env_in, value_in, "pixelFormat", "()Lio/wapitistew/wse/CameraPixelFormat;" ) ) );
    return result;
}

wse::tmr::sCameraDeviceInfo cameraDevice( JNIEnv* env_in, jobject value_in )
{
    wse::tmr::sCameraDeviceInfo result;
    result.backend = static_cast<wse::tmr::eCameraBackend>( enumCode( env_in,
        objectProperty( env_in, value_in, "backend", "()Lio/wapitistew/wse/CameraBackend;" ) ) );
    result.id = javaString( env_in, static_cast<jstring>(
        objectProperty( env_in, value_in, "id", "()Ljava/lang/String;" ) ) );
    result.display_name = javaString( env_in, static_cast<jstring>(
        objectProperty( env_in, value_in, "displayName", "()Ljava/lang/String;" ) ) );
    result.transport = javaString( env_in, static_cast<jstring>(
        objectProperty( env_in, value_in, "transport", "()Ljava/lang/String;" ) ) );
    result.transport_type = static_cast<wse::tmr::eCameraTransport>( enumCode( env_in,
        objectProperty( env_in, value_in, "transportType", "()Lio/wapitistew/wse/CameraTransport;" ) ) );
    jobject usb = objectProperty( env_in, value_in, "usb", "()Lio/wapitistew/wse/CameraUsbIdentity;" );
    result.usb.vendor_id = static_cast<std::uint16_t>( intProperty( env_in, usb, "vendorId" ) );
    result.usb.product_id = static_cast<std::uint16_t>( intProperty( env_in, usb, "productId" ) );
    result.usb.serial_number = javaString( env_in, static_cast<jstring>(
        objectProperty( env_in, usb, "serialNumber", "()Ljava/lang/String;" ) ) );
    result.usb.uvc_version_bcd = static_cast<std::uint16_t>( intProperty( env_in, usb, "uvcVersionBcd" ) );
    return result;
}

wse::tmr::sCameraExtensionUnitSelector cameraExtensionSelector(
    JNIEnv* env_in, jobject value_in )
{
    wse::tmr::sCameraExtensionUnitSelector result;
    auto guid = static_cast<jbyteArray>( objectProperty(
        env_in, value_in, "unitGuid", "()[B" ) );
    if ( guid != nullptr && env_in->GetArrayLength( guid ) == 16 )
        env_in->GetByteArrayRegion( guid, 0, 16,
            reinterpret_cast<jbyte*>( result.unit_guid.data() ) );
    result.unit_id = static_cast<std::uint8_t>( intProperty( env_in, value_in, "unitId" ) );
    result.selector = static_cast<std::uint8_t>( intProperty( env_in, value_in, "selector" ) );
    result.minimum_size = static_cast<std::size_t>( longProperty( env_in, value_in, "minimumSize" ) );
    result.maximum_size = static_cast<std::size_t>( longProperty( env_in, value_in, "maximumSize" ) );
    result.readable = boolProperty( env_in, value_in, "readable" ) == JNI_TRUE;
    result.writable = boolProperty( env_in, value_in, "writable" ) == JNI_TRUE;
    result.display_name = javaString( env_in, static_cast<jstring>(
        objectProperty( env_in, value_in, "displayName", "()Ljava/lang/String;" ) ) );
    return result;
}

jobject makeCameraStreamProfile( JNIEnv* env_in,
    const wse::tmr::sCameraStreamProfile& profile_in )
{
    jclass pixel_class = env_in->FindClass( "io/wapitistew/wse/CameraPixelFormat" );
    jobjectArray outputs = env_in->NewObjectArray(
        static_cast<jsize>( profile_in.output_formats.size() ), pixel_class, nullptr );
    for ( std::size_t index = 0U; index < profile_in.output_formats.size(); ++index )
        env_in->SetObjectArrayElement( outputs, static_cast<jsize>( index ),
            makeEnum( env_in, "io/wapitistew/wse/CameraPixelFormat",
                static_cast<jint>( profile_in.output_formats[index] ) ) );
    jclass value_class = env_in->FindClass( "io/wapitistew/wse/CameraStreamProfile" );
    jmethodID constructor = value_class == nullptr ? nullptr : env_in->GetMethodID(
        value_class, "<init>", "(Lio/wapitistew/wse/CameraFormat;[Lio/wapitistew/wse/CameraPixelFormat;)V" );
    return constructor == nullptr ? nullptr : env_in->NewObject(
        value_class, constructor, makeCameraFormat( env_in, profile_in.native_format ), outputs );
}

jobject makeCameraExtensionSelector( JNIEnv* env_in,
    const wse::tmr::sCameraExtensionUnitSelector& selector_in )
{
    jbyteArray guid = env_in->NewByteArray( 16 );
    env_in->SetByteArrayRegion( guid, 0, 16,
        reinterpret_cast<const jbyte*>( selector_in.unit_guid.data() ) );
    jclass value_class = env_in->FindClass( "io/wapitistew/wse/CameraExtensionUnitSelector" );
    jmethodID constructor = value_class == nullptr ? nullptr : env_in->GetMethodID(
        value_class, "<init>", "([BIIJJZZLjava/lang/String;)V" );
    return constructor == nullptr ? nullptr : env_in->NewObject( value_class, constructor,
        guid, static_cast<jint>( selector_in.unit_id ), static_cast<jint>( selector_in.selector ),
        static_cast<jlong>( selector_in.minimum_size ), static_cast<jlong>( selector_in.maximum_size ),
        selector_in.readable ? JNI_TRUE : JNI_FALSE, selector_in.writable ? JNI_TRUE : JNI_FALSE,
        env_in->NewStringUTF( selector_in.display_name.c_str() ) );
}

jobject makeCameraCapability( JNIEnv* env_in,
    const wse::tmr::sCameraCapability& capability_in )
{
    jclass format_class = env_in->FindClass( "io/wapitistew/wse/CameraFormat" );
    jobjectArray formats = env_in->NewObjectArray( static_cast<jsize>( capability_in.formats.size() ), format_class, nullptr );
    for ( std::size_t index = 0U; index < capability_in.formats.size(); ++index )
        env_in->SetObjectArrayElement( formats, static_cast<jsize>( index ), makeCameraFormat( env_in, capability_in.formats[index] ) );
    jclass control_class = env_in->FindClass( "io/wapitistew/wse/CameraControlCapability" );
    jobjectArray controls = env_in->NewObjectArray( static_cast<jsize>( capability_in.controls.size() ), control_class, nullptr );
    for ( std::size_t index = 0U; index < capability_in.controls.size(); ++index )
        env_in->SetObjectArrayElement( controls, static_cast<jsize>( index ), makeCameraControlCapability( env_in, capability_in.controls[index] ) );
    jclass profile_class = env_in->FindClass( "io/wapitistew/wse/CameraStreamProfile" );
    jobjectArray profiles = env_in->NewObjectArray( static_cast<jsize>( capability_in.stream_profiles.size() ), profile_class, nullptr );
    for ( std::size_t index = 0U; index < capability_in.stream_profiles.size(); ++index )
        env_in->SetObjectArrayElement( profiles, static_cast<jsize>( index ), makeCameraStreamProfile( env_in, capability_in.stream_profiles[index] ) );
    jclass extension_class = env_in->FindClass( "io/wapitistew/wse/CameraExtensionUnitSelector" );
    jobjectArray extensions = env_in->NewObjectArray( static_cast<jsize>( capability_in.extension_units.size() ), extension_class, nullptr );
    for ( std::size_t index = 0U; index < capability_in.extension_units.size(); ++index )
        env_in->SetObjectArrayElement( extensions, static_cast<jsize>( index ), makeCameraExtensionSelector( env_in, capability_in.extension_units[index] ) );
    jclass value_class = env_in->FindClass( "io/wapitistew/wse/CameraCapability" );
    jmethodID constructor = value_class == nullptr ? nullptr : env_in->GetMethodID( value_class, "<init>",
        "(Lio/wapitistew/wse/CameraDevice;[Lio/wapitistew/wse/CameraFormat;[Lio/wapitistew/wse/CameraControlCapability;[Lio/wapitistew/wse/CameraStreamProfile;[Lio/wapitistew/wse/CameraExtensionUnitSelector;)V" );
    return constructor == nullptr ? nullptr : env_in->NewObject( value_class, constructor,
        makeCameraDevice( env_in, capability_in.device ), formats, controls, profiles, extensions );
}

jobject makeCameraFrame(
      JNIEnv* env_in
    , wse::tmr::sCameraFrame frame_in
)
{
    jobject buffer = makeNativeBuffer(
        env_in, wse::binding::FrameBuffer( std::move( frame_in.data ) ) );
    if ( buffer == nullptr ) return nullptr;
    jclass value_class = env_in->FindClass( "io/wapitistew/wse/CameraFrame" );
    jmethodID constructor = value_class == nullptr ? nullptr : env_in->GetMethodID(
        value_class, "<init>",
        "(IILio/wapitistew/wse/CameraPixelFormat;JJJLio/wapitistew/wse/NativeBuffer;)V" );
    return constructor == nullptr ? nullptr : env_in->NewObject(
          value_class
        , constructor
        , static_cast<jint>( frame_in.description.width )
        , static_cast<jint>( frame_in.description.height )
        , makeEnum( env_in, "io/wapitistew/wse/CameraPixelFormat",
              static_cast<jint>( frame_in.description.pixel_format ) )
        , static_cast<jlong>( frame_in.description.row_stride )
        , static_cast<jlong>( frame_in.sequence )
        , static_cast<jlong>( frame_in.monotonic_timestamp_ns )
        , buffer
    );
}

void deliverCameraCallback(
      JavaCamera* camera_in
    , const wse::tmr::CameraResult<wse::tmr::sCameraFrame>& result_in
)
{
    if ( camera_in == nullptr || camera_in->callback == nullptr || g_java_vm == nullptr )
    {
        return;
    }
    JNIEnv* env = nullptr;
    bool attached = false;
    if ( g_java_vm->GetEnv( reinterpret_cast<void**>( &env ), JNI_VERSION_1_8 ) == JNI_EDETACHED )
    {
        attached = g_java_vm->AttachCurrentThreadAsDaemon(
            reinterpret_cast<void**>( &env ), nullptr ) == JNI_OK;
    }
    if ( env == nullptr ) return;
    jclass callback_class = env->GetObjectClass( camera_in->callback );
    if ( result_in.succeeded() )
    {
        jmethodID method = callback_class == nullptr ? nullptr : env->GetMethodID(
            callback_class, "onFrame", "(Lio/wapitistew/wse/CameraFrame;)V" );
        if ( method != nullptr )
        {
            env->CallVoidMethod(
                camera_in->callback, method, makeCameraFrame( env, result_in.value() ) );
        }
    }
    else
    {
        jmethodID method = callback_class == nullptr ? nullptr : env->GetMethodID(
            callback_class, "onError", "(Lio/wapitistew/wse/WseException;)V" );
        if ( method != nullptr )
        {
            env->CallVoidMethod(
                camera_in->callback, method, makeWseException( env, cameraError( result_in.error() ) ) );
        }
    }
    if ( env->ExceptionCheck() )
    {
        env->ExceptionDescribe();
        env->ExceptionClear();
    }
    if ( attached ) g_java_vm->DetachCurrentThread();
}
#endif // WSE_HAS_TMR

#ifdef WSE_HAS_OUI
wse::binding::Error rendererError( const wse::oui::RendererError& error_in )
{
    return wse::binding::fromOuiError( error_in );
}

wse::binding::Error projectionArgumentError( const std::string& message_in )
{
    return wse::binding::Error( wse::binding::eErrorCategory::InvalidArgument,
        1, message_in );
}

bool copyDirectBuffer(
      std::vector<std::uint8_t>* const p_bytes_out
    , JNIEnv* env_in, jobject buffer_in, const bool optional_in )
{
    std::vector<std::uint8_t>& bytes_out = *p_bytes_out;

    if ( buffer_in == nullptr ) return optional_in;
    void* address = env_in->GetDirectBufferAddress( buffer_in );
    const jlong capacity = env_in->GetDirectBufferCapacity( buffer_in );
    if ( address == nullptr || capacity < 0 ) return false;
    const auto* begin = static_cast<const std::uint8_t*>( address );
    bytes_out.assign( begin, begin + static_cast<std::size_t>( capacity ) );
    return true;
}
#endif // WSE_HAS_OUI

} // namespace

extern "C"
{

// ----------------------------------------------------------------------------------------------
// Runtime, buffer, and timer entry points
//
// The VM pointer captured at load time is what worker threads later attach through to deliver a
// callback; JNIEnv itself is thread-bound and never stored.
// ----------------------------------------------------------------------------------------------

JNIEXPORT jint JNICALL JNI_OnLoad( JavaVM* vm_in, void* )
{
    g_java_vm = vm_in;
    return JNI_VERSION_1_8;
}

JNIEXPORT void JNICALL JNI_OnUnload( JavaVM*, void* )
{
    g_java_vm = nullptr;
}

JNIEXPORT jstring JNICALL Java_io_wapitistew_wse_Native_runtimeVersion(
      JNIEnv* env_in
    , jclass
)
{
    return env_in->NewStringUTF( wse::binding::Runtime().info().version.c_str() );
}

JNIEXPORT jint JNICALL Java_io_wapitistew_wse_Native_bindingAbiVersion(
      JNIEnv*
    , jclass
)
{
    return static_cast<jint>( wse::binding::Runtime().info().binding_abi_version );
}

JNIEXPORT jint JNICALL Java_io_wapitistew_wse_Native_componentFlags(
      JNIEnv*
    , jclass
)
{
    const auto info = wse::binding::Runtime().info();
    return ( info.has_xpt ? 1 : 0 )
        | ( info.has_tmr ? 2 : 0 )
        | ( info.has_oui ? 4 : 0 )
        | ( info.has_gef ? 8 : 0 )
        | ( info.has_iui ? 16 : 0 )
        | ( info.has_vpj ? 32 : 0 );
}

JNIEXPORT jlong JNICALL Java_io_wapitistew_wse_Native_runtimeCreate(
      JNIEnv* env_in
    , jclass
)
{
    auto* runtime = new ( std::nothrow ) JavaRuntime();
    if ( runtime == nullptr )
    {
        throwWseError( env_in, wse::binding::Error(
              wse::binding::eErrorCategory::ResourceExhausted
            , 1
            , "Unable to allocate the WSE Java runtime."
        ) );
        return 0;
    }
    return toHandle( runtime );
}

JNIEXPORT void JNICALL Java_io_wapitistew_wse_Native_runtimeClose(
      JNIEnv*
    , jclass
    , const jlong handle_in
)
{
    JavaRuntime* runtime = fromHandle<JavaRuntime>( handle_in );
    if ( runtime == nullptr )
    {
        return;
    }
    {
        std::lock_guard<std::mutex> lock( runtime->mutex );
        runtime->closed = true;
        runtime->cancellation.cancel();
    }
    delete runtime;
}

JNIEXPORT jobject JNICALL Java_io_wapitistew_wse_Native_runtimeCopyFrame(
      JNIEnv* env_in
    , jclass
    , const jlong runtime_handle_in
    , jobject buffer_in
)
{
    JavaRuntime* runtime = fromHandle<JavaRuntime>( runtime_handle_in );
    const auto token = operationToken( env_in, runtime );
    if ( env_in->ExceptionCheck() )
    {
        return nullptr;
    }

    void* data = env_in->GetDirectBufferAddress( buffer_in );
    const jlong capacity = env_in->GetDirectBufferCapacity( buffer_in );
    if ( capacity < 0 || ( capacity != 0 && data == nullptr ) )
    {
        throwWseError( env_in, invalidBufferError() );
        return nullptr;
    }

    try
    {
        std::vector<std::uint8_t> input( static_cast<std::size_t>( capacity ) );
        if ( !input.empty() )
        {
            std::memcpy( input.data(), data, input.size() );
        }
        if ( token.isCancellationRequested() )
        {
            throwWseError( env_in, invalidStateError() );
            return nullptr;
        }
        auto result = wse::binding::Runtime().copyFrame( input );
        if ( !result.succeeded() )
        {
            throwWseError( env_in, result.error() );
            return nullptr;
        }
        return makeNativeBuffer( env_in, std::move( result.value() ) );
    }
    catch ( const std::bad_alloc& )
    {
        throwWseError( env_in, wse::binding::Error(
              wse::binding::eErrorCategory::ResourceExhausted
            , 1
            , "Unable to allocate the WSE Java frame."
        ) );
        return nullptr;
    }
    catch ( ... )
    {
        throwWseError( env_in, wse::binding::Error(
              wse::binding::eErrorCategory::Internal
            , 1
            , "The WSE Java copy operation failed unexpectedly."
        ) );
        return nullptr;
    }
}

JNIEXPORT void JNICALL Java_io_wapitistew_wse_Native_runtimeWait(
      JNIEnv* env_in
    , jclass
    , const jlong runtime_handle_in
    , const jlong duration_ms_in
)
{
    JavaRuntime* runtime = fromHandle<JavaRuntime>( runtime_handle_in );
    const auto token = operationToken( env_in, runtime );
    if ( env_in->ExceptionCheck() )
    {
        return;
    }
    if ( duration_ms_in < 0 || duration_ms_in > 86400000 )
    {
        throwWseError( env_in, wse::binding::Error(
              wse::binding::eErrorCategory::InvalidArgument
            , 1
            , "wait duration must be between 0 and 86400000 milliseconds."
        ) );
        return;
    }
    const auto status = wse::binding::Runtime().wait(
        std::chrono::milliseconds( duration_ms_in ), token );
    if ( !status.succeeded() )
    {
        throwWseError( env_in, status.error() );
    }
}

JNIEXPORT void JNICALL Java_io_wapitistew_wse_Native_bufferClose(
      JNIEnv*
    , jclass
    , const jlong handle_in
)
{
    delete fromHandle<JavaBuffer>( handle_in );
}

JNIEXPORT jlong JNICALL Java_io_wapitistew_wse_Native_timerCreate(
      JNIEnv* env_in
    , jclass
    , const jlong runtime_handle_in
    , const jlong duration_ms_in
    , jobject callback_in
)
{
    JavaRuntime* runtime = fromHandle<JavaRuntime>( runtime_handle_in );
    const auto token = operationToken( env_in, runtime );
    if ( env_in->ExceptionCheck() )
    {
        return 0;
    }
    if ( duration_ms_in < 0 || duration_ms_in > 86400000 || callback_in == nullptr )
    {
        throwWseError( env_in, wse::binding::Error(
              wse::binding::eErrorCategory::InvalidArgument
            , 1
            , "runAfter requires a callback and 0 to 86400000 milliseconds."
        ) );
        return 0;
    }

    auto* timer = new ( std::nothrow ) JavaTimer();
    if ( timer == nullptr )
    {
        throwWseError( env_in, wse::binding::Error(
              wse::binding::eErrorCategory::ResourceExhausted
            , 1
            , "Unable to allocate the WSE Java timer."
        ) );
        return 0;
    }
    timer->vm = g_java_vm;
    timer->callback = env_in->NewGlobalRef( callback_in );
    if ( timer->vm == nullptr || timer->callback == nullptr )
    {
        destroyTimer( env_in, timer );
        return 0;
    }

    try
    {
        timer->worker = std::thread( [timer, duration_ms_in, token]()
        {
            auto status = wse::binding::Status::success();
            std::int64_t remaining_ms = duration_ms_in;
            while ( remaining_ms > 0 && !token.isCancellationRequested()
                    && !timer->cancellation.token().isCancellationRequested() )
            {
                const std::int64_t slice_ms = remaining_ms < 10 ? remaining_ms : 10;
                status = wse::binding::Runtime().wait(
                    std::chrono::milliseconds( slice_ms ), timer->cancellation.token() );
                if ( !status.succeeded() )
                {
                    break;
                }
                remaining_ms -= slice_ms;
            }
            JNIEnv* env = nullptr;
            bool attached = false;
            if ( status.succeeded() && !token.isCancellationRequested()
                 && timer->vm->GetEnv(
                        reinterpret_cast<void**>( &env ), JNI_VERSION_1_8 ) == JNI_EDETACHED )
            {
                attached = timer->vm->AttachCurrentThreadAsDaemon(
                    reinterpret_cast<void**>( &env ), nullptr ) == JNI_OK;
            }
            if ( status.succeeded() && !token.isCancellationRequested()
                 && !timer->cancellation.token().isCancellationRequested()
                 && env != nullptr )
            {
                jclass callback_class = env->GetObjectClass( timer->callback );
                jmethodID run = callback_class == nullptr ? nullptr :
                    env->GetMethodID( callback_class, "run", "()V" );
                if ( run != nullptr )
                {
                    env->CallVoidMethod( timer->callback, run );
                    if ( env->ExceptionCheck() )
                    {
                        env->ExceptionDescribe();
                        env->ExceptionClear();
                    }
                }
            }
            if ( timer->delete_on_exit.load( std::memory_order_acquire ) )
            {
                if ( env != nullptr && timer->callback != nullptr )
                {
                    env->DeleteGlobalRef( timer->callback );
                    timer->callback = nullptr;
                }
                if ( attached )
                {
                    timer->vm->DetachCurrentThread();
                }
                delete timer;
                return;
            }
            if ( attached )
            {
                timer->vm->DetachCurrentThread();
            }
        } );
    }
    catch ( ... )
    {
        destroyTimer( env_in, timer );
        throwWseError( env_in, wse::binding::Error(
              wse::binding::eErrorCategory::ResourceExhausted
            , 2
            , "Unable to start the WSE Java timer thread."
        ) );
        return 0;
    }
    return toHandle( timer );
}

JNIEXPORT void JNICALL Java_io_wapitistew_wse_Native_timerCancel(
      JNIEnv*
    , jclass
    , const jlong handle_in
)
{
    JavaTimer* timer = fromHandle<JavaTimer>( handle_in );
    if ( timer != nullptr )
    {
        timer->cancellation.cancel();
    }
}

JNIEXPORT void JNICALL Java_io_wapitistew_wse_Native_timerClose(
      JNIEnv* env_in
    , jclass
    , const jlong handle_in
)
{
    JavaTimer* timer = fromHandle<JavaTimer>( handle_in );
    if ( timer == nullptr )
    {
        return;
    }
    timer->cancellation.cancel();
    if ( timer->worker.joinable()
         && timer->worker.get_id() == std::this_thread::get_id() )
    {
        timer->delete_on_exit.store( true, std::memory_order_release );
        timer->worker.detach();
        return;
    }
    if ( timer->worker.joinable() )
    {
        timer->worker.join();
    }
    destroyTimer( env_in, timer );
}

#ifdef WSE_HAS_OUI
JNIEXPORT jobject JNICALL Java_io_wapitistew_wse_Native_projectionRender(
      JNIEnv* env_in, jclass, const jint output_width_in, const jint output_height_in,
      const jfloat clear_red_in, const jfloat clear_green_in, const jfloat clear_blue_in,
      const jfloat clear_alpha_in, const jint supersample_scale_in, const jint backend_in,
      const jboolean software_in, const jboolean validation_in, jstring adapter_name_in,
      const jint timeout_in,
      jintArray widths_in, jintArray heights_in, jobjectArray rgba_in, jobjectArray alpha_in,
      jobjectArray vertices_in, jobjectArray indices_in, jintArray filters_in,
      jfloatArray opacities_in, jobjectArray edges_in, jintArray curves_in )
{
    if ( widths_in == nullptr || heights_in == nullptr || rgba_in == nullptr
         || alpha_in == nullptr || vertices_in == nullptr || indices_in == nullptr
         || filters_in == nullptr || opacities_in == nullptr || edges_in == nullptr
         || curves_in == nullptr )
    {
        throwWseError( env_in, projectionArgumentError( "Projection arrays must not be null." ) );
        return nullptr;
    }
    const jsize count = env_in->GetArrayLength( widths_in );
    if ( env_in->GetArrayLength( heights_in ) != count
         || env_in->GetArrayLength( rgba_in ) != count
         || env_in->GetArrayLength( alpha_in ) != count
         || env_in->GetArrayLength( vertices_in ) != count
         || env_in->GetArrayLength( indices_in ) != count
         || env_in->GetArrayLength( filters_in ) != count
         || env_in->GetArrayLength( opacities_in ) != count
         || env_in->GetArrayLength( edges_in ) != count
         || env_in->GetArrayLength( curves_in ) != count )
    {
        throwWseError( env_in, projectionArgumentError( "Projection layer array lengths differ." ) );
        return nullptr;
    }

    std::vector<jint> widths( static_cast<std::size_t>( count ) );
    std::vector<jint> heights( static_cast<std::size_t>( count ) );
    std::vector<jint> filters( static_cast<std::size_t>( count ) );
    std::vector<jfloat> opacities( static_cast<std::size_t>( count ) );
    std::vector<jint> curves( static_cast<std::size_t>( count ) );
    env_in->GetIntArrayRegion( widths_in, 0, count, widths.data() );
    env_in->GetIntArrayRegion( heights_in, 0, count, heights.data() );
    env_in->GetIntArrayRegion( filters_in, 0, count, filters.data() );
    env_in->GetFloatArrayRegion( opacities_in, 0, count, opacities.data() );
    env_in->GetIntArrayRegion( curves_in, 0, count, curves.data() );
    if ( env_in->ExceptionCheck() ) return nullptr;

    wse::oui::sProjectionRenderRequest request;
    request.output_width = static_cast<std::uint32_t>( output_width_in );
    request.output_height = static_cast<std::uint32_t>( output_height_in );
    request.clear_color = { clear_red_in, clear_green_in, clear_blue_in, clear_alpha_in };
    request.supersample_scale = static_cast<std::uint32_t>( supersample_scale_in );
    request.backend = static_cast<wse::oui::eRendererBackend>( backend_in );
    request.use_software_adapter = software_in == JNI_TRUE;
    request.enable_validation = validation_in == JNI_TRUE;
    if ( adapter_name_in != nullptr )
    {
        const char* adapter_name = env_in->GetStringUTFChars( adapter_name_in, nullptr );
        if ( adapter_name != nullptr )
        {
            request.adapter_name = adapter_name;
            env_in->ReleaseStringUTFChars( adapter_name_in, adapter_name );
        }
    }
    request.timeout_ms = static_cast<std::uint32_t>( timeout_in );
    request.layers.reserve( static_cast<std::size_t>( count ) );

    for ( jsize index = 0; index < count; ++index )
    {
        wse::oui::sProjectionImageLayer layer;
        layer.width = static_cast<std::uint32_t>( widths[ static_cast<std::size_t>( index ) ] );
        layer.height = static_cast<std::uint32_t>( heights[ static_cast<std::size_t>( index ) ] );
        jobject rgba = env_in->GetObjectArrayElement( rgba_in, index );
        jobject alpha = env_in->GetObjectArrayElement( alpha_in, index );
        if ( !copyDirectBuffer( &layer.rgba, env_in, rgba, false )
             || !copyDirectBuffer( &layer.alpha, env_in, alpha, true ) )
        {
            throwWseError( env_in, projectionArgumentError(
                "Projection images must be direct ByteBuffers." ) );
            return nullptr;
        }

        auto vertex_array = static_cast<jfloatArray>(
            env_in->GetObjectArrayElement( vertices_in, index ) );
        auto index_array = static_cast<jintArray>(
            env_in->GetObjectArrayElement( indices_in, index ) );
        auto edge_array = static_cast<jfloatArray>(
            env_in->GetObjectArrayElement( edges_in, index ) );
        if ( vertex_array == nullptr || index_array == nullptr || edge_array == nullptr
             || ( env_in->GetArrayLength( vertex_array ) % 4 ) != 0
             || env_in->GetArrayLength( edge_array ) != 4 )
        {
            throwWseError( env_in, projectionArgumentError(
                "Projection mesh or edge array is invalid." ) );
            return nullptr;
        }
        const jsize vertex_value_count = env_in->GetArrayLength( vertex_array );
        std::vector<jfloat> vertex_values( static_cast<std::size_t>( vertex_value_count ) );
        env_in->GetFloatArrayRegion( vertex_array, 0, vertex_value_count, vertex_values.data() );
        for ( jsize vertex = 0; vertex < vertex_value_count; vertex += 4 )
            layer.vertices.emplace_back( wse::oui::sRendererVertex2D{
                vertex_values[static_cast<std::size_t>( vertex )],
                vertex_values[static_cast<std::size_t>( vertex + 1 )],
                vertex_values[static_cast<std::size_t>( vertex + 2 )],
                vertex_values[static_cast<std::size_t>( vertex + 3 )] } );
        const jsize index_count = env_in->GetArrayLength( index_array );
        std::vector<jint> index_values( static_cast<std::size_t>( index_count ) );
        env_in->GetIntArrayRegion( index_array, 0, index_count, index_values.data() );
        for ( const jint value : index_values ) layer.indices.emplace_back(
            static_cast<std::uint32_t>( value ) );
        jfloat edge_values[4] = {};
        env_in->GetFloatArrayRegion( edge_array, 0, 4, edge_values );
        if ( env_in->ExceptionCheck() ) return nullptr;
        layer.sampling_filter = static_cast<wse::oui::eTextureSamplingFilter>(
            filters[ static_cast<std::size_t>( index ) ] );
        layer.opacity = opacities[ static_cast<std::size_t>( index ) ];
        layer.edge_blend = { edge_values[0], edge_values[1], edge_values[2], edge_values[3],
            static_cast<wse::oui::eEdgeBlendCurve>( curves[ static_cast<std::size_t>( index ) ] ) };
        request.layers.emplace_back( std::move( layer ) );
        env_in->DeleteLocalRef( rgba );
        if ( alpha != nullptr ) env_in->DeleteLocalRef( alpha );
        env_in->DeleteLocalRef( vertex_array );
        env_in->DeleteLocalRef( index_array );
        env_in->DeleteLocalRef( edge_array );
    }

    auto result = wse::oui::renderProjection( request );
    if ( !result.succeeded() )
    {
        throwWseError( env_in, rendererError( result.error() ) );
        return nullptr;
    }
    jobject buffer = makeNativeBuffer( env_in, std::move( result.value().data ) );
    if ( buffer == nullptr ) return nullptr;
    jclass frame_class = env_in->FindClass( "io/wapitistew/wse/ProjectionFrame" );
    jmethodID constructor = frame_class == nullptr ? nullptr : env_in->GetMethodID(
        frame_class, "<init>",
        "(IIJLjava/lang/String;Lio/wapitistew/wse/NativeBuffer;)V" );
    if ( constructor == nullptr ) return nullptr;
    jstring adapter_name = env_in->NewStringUTF( result.value().adapter_name.c_str() );
    return env_in->NewObject( frame_class, constructor,
        static_cast<jint>( result.value().width ), static_cast<jint>( result.value().height ),
        static_cast<jlong>( result.value().row_pitch ), adapter_name, buffer );
}
#endif // WSE_HAS_OUI

#ifdef WSE_HAS_TMR
JNIEXPORT jobjectArray JNICALL Java_io_wapitistew_wse_Native_webCameraEnumerate(
      JNIEnv* env_in
    , jclass
    , const jint backend_in
)
{
    const auto result = wse::tmr::WebCamera::enumerate(
        static_cast<wse::tmr::eCameraBackend>( backend_in ) );
    if ( !result.succeeded() )
    {
        throwWseError( env_in, cameraError( result.error() ) );
        return nullptr;
    }
    jclass device_class = env_in->FindClass( "io/wapitistew/wse/CameraDevice" );
    jobjectArray values = env_in->NewObjectArray(
        static_cast<jsize>( result.value().size() ), device_class, nullptr );
    for ( std::size_t index = 0U; index < result.value().size(); ++index )
    {
        env_in->SetObjectArrayElement(
            values, static_cast<jsize>( index ), makeCameraDevice( env_in, result.value()[ index ] ) );
    }
    return values;
}

JNIEXPORT jobject JNICALL Java_io_wapitistew_wse_Native_webCameraCapabilities(
      JNIEnv* env_in
    , jclass
    , jobject device_in
)
{
    const auto result = wse::tmr::WebCamera::capabilities( cameraDevice( env_in, device_in ) );
    if ( !result.succeeded() )
    {
        throwWseError( env_in, cameraError( result.error() ) );
        return nullptr;
    }
    return makeCameraCapability( env_in, result.value() );
}

JNIEXPORT jlong JNICALL Java_io_wapitistew_wse_Native_webCameraCreate(
      JNIEnv* env_in
    , jclass
)
{
    auto* camera = new ( std::nothrow ) JavaCamera();
    if ( camera == nullptr )
    {
        throwWseError( env_in, wse::binding::Error(
              wse::binding::eErrorCategory::ResourceExhausted, 1
            , "Unable to allocate WebCamera." ) );
        return 0;
    }
    return toHandle( camera );
}

JNIEXPORT void JNICALL Java_io_wapitistew_wse_Native_webCameraOpen(
      JNIEnv* env_in
    , jclass
    , const jlong handle_in
    , jobject device_in
    , jobject configuration_in
)
{
    JavaCamera* camera = fromHandle<JavaCamera>( handle_in );
    if ( camera == nullptr )
    {
        throwWseError( env_in, invalidStateError() );
        return;
    }
    auto status = wse::tmr::CameraStatus::success();
    const auto device = cameraDevice( env_in, device_in );
    if ( configuration_in == nullptr ) status = camera->camera->open( device );
    else
    {
        wse::tmr::sCameraStreamConfiguration configuration;
        configuration.native_format = cameraFormat( env_in, objectProperty( env_in, configuration_in,
            "nativeFormat", "()Lio/wapitistew/wse/CameraFormat;" ) );
        configuration.output_format = static_cast<wse::tmr::eCameraPixelFormat>( enumCode( env_in,
            objectProperty( env_in, configuration_in, "outputFormat",
                "()Lio/wapitistew/wse/CameraPixelFormat;" ) ) );
        configuration.allow_conversion = boolProperty( env_in, configuration_in, "allowConversion" ) == JNI_TRUE;
        status = camera->camera->open( device, configuration );
    }
    if ( !status.succeeded() ) throwWseError( env_in, cameraError( status.error() ) );
}

JNIEXPORT void JNICALL Java_io_wapitistew_wse_Native_webCameraStart(
      JNIEnv* env_in
    , jclass
    , const jlong handle_in
    , jobject callback_in
)
{
    JavaCamera* camera = fromHandle<JavaCamera>( handle_in );
    if ( camera == nullptr )
    {
        throwWseError( env_in, invalidStateError() );
        return;
    }
    auto status = wse::tmr::CameraStatus::success();
    if ( callback_in == nullptr )
    {
        status = camera->camera->start();
    }
    else
    {
        if ( camera->callback != nullptr ) env_in->DeleteGlobalRef( camera->callback );
        camera->callback = env_in->NewGlobalRef( callback_in );
        status = camera->camera->start(
            [camera]( const wse::tmr::CameraResult<wse::tmr::sCameraFrame>& result_in )
            {
                deliverCameraCallback( camera, result_in );
            } );
    }
    if ( !status.succeeded() )
    {
        if ( camera->callback != nullptr )
        {
            env_in->DeleteGlobalRef( camera->callback );
            camera->callback = nullptr;
        }
        throwWseError( env_in, cameraError( status.error() ) );
    }
}

JNIEXPORT void JNICALL Java_io_wapitistew_wse_Native_webCameraStop(
      JNIEnv* env_in, jclass, const jlong handle_in )
{
    JavaCamera* camera = fromHandle<JavaCamera>( handle_in );
    if ( camera == nullptr ) { throwWseError( env_in, invalidStateError() ); return; }
    const auto status = camera->camera->stop();
    if ( camera->callback != nullptr )
    {
        env_in->DeleteGlobalRef( camera->callback );
        camera->callback = nullptr;
    }
    if ( !status.succeeded() ) throwWseError( env_in, cameraError( status.error() ) );
}

JNIEXPORT jobject JNICALL Java_io_wapitistew_wse_Native_webCameraReadFrame(
      JNIEnv* env_in, jclass, const jlong handle_in, const jint timeout_ms_in )
{
    JavaCamera* camera = fromHandle<JavaCamera>( handle_in );
    if ( camera == nullptr ) { throwWseError( env_in, invalidStateError() ); return nullptr; }
    auto result = camera->camera->readFrame( static_cast<std::uint32_t>( timeout_ms_in ) );
    if ( !result.succeeded() )
    {
        throwWseError( env_in, cameraError( result.error() ) );
        return nullptr;
    }
    return makeCameraFrame( env_in, std::move( result.value() ) );
}

#include "jni_frame_ops.inc"

JNIEXPORT jobject JNICALL Java_io_wapitistew_wse_Native_webCameraCurrentCapabilities(
      JNIEnv* env_in, jclass, const jlong handle_in )
{
    JavaCamera* camera = fromHandle<JavaCamera>( handle_in );
    if ( camera == nullptr ) { throwWseError( env_in, invalidStateError() ); return nullptr; }
    const auto result = camera->camera->currentCapabilities();
    if ( !result.succeeded() )
    {
        throwWseError( env_in, cameraError( result.error() ) );
        return nullptr;
    }
    return makeCameraCapability( env_in, result.value() );
}

JNIEXPORT jobject JNICALL Java_io_wapitistew_wse_Native_webCameraControlCapability(
      JNIEnv* env_in, jclass, const jlong handle_in, const jint control_in )
{
    JavaCamera* camera = fromHandle<JavaCamera>( handle_in );
    if ( camera == nullptr ) { throwWseError( env_in, invalidStateError() ); return nullptr; }
    const auto result = camera->camera->controlCapability(
        static_cast<wse::tmr::eCameraControl>( control_in ) );
    if ( !result.succeeded() )
    {
        throwWseError( env_in, cameraError( result.error() ) );
        return nullptr;
    }
    return makeCameraControlCapability( env_in, result.value() );
}

JNIEXPORT jobject JNICALL Java_io_wapitistew_wse_Native_webCameraGetControl(
      JNIEnv* env_in, jclass, const jlong handle_in, const jint control_in )
{
    JavaCamera* camera = fromHandle<JavaCamera>( handle_in );
    if ( camera == nullptr ) { throwWseError( env_in, invalidStateError() ); return nullptr; }
    const auto result = camera->camera->getControl(
        static_cast<wse::tmr::eCameraControl>( control_in ) );
    if ( !result.succeeded() )
    {
        throwWseError( env_in, cameraError( result.error() ) );
        return nullptr;
    }
    jclass value_class = env_in->FindClass( "io/wapitistew/wse/CameraControlValue" );
    jmethodID constructor = value_class == nullptr ? nullptr : env_in->GetMethodID(
        value_class, "<init>",
        "(Lio/wapitistew/wse/CameraControl;Lio/wapitistew/wse/CameraControlMode;J)V" );
    return constructor == nullptr ? nullptr : env_in->NewObject(
          value_class, constructor
        , makeEnum( env_in, "io/wapitistew/wse/CameraControl",
              static_cast<jint>( result.value().control ) )
        , makeEnum( env_in, "io/wapitistew/wse/CameraControlMode",
              static_cast<jint>( result.value().mode ) )
        , static_cast<jlong>( result.value().value ) );
}

JNIEXPORT void JNICALL Java_io_wapitistew_wse_Native_webCameraSetControl(
      JNIEnv* env_in, jclass, const jlong handle_in, jobject value_in )
{
    JavaCamera* camera = fromHandle<JavaCamera>( handle_in );
    if ( camera == nullptr ) { throwWseError( env_in, invalidStateError() ); return; }
    wse::tmr::sCameraControlValue value;
    value.control = static_cast<wse::tmr::eCameraControl>( enumCode( env_in,
        objectProperty( env_in, value_in, "control", "()Lio/wapitistew/wse/CameraControl;" ) ) );
    value.mode = static_cast<wse::tmr::eCameraControlMode>( enumCode( env_in,
        objectProperty( env_in, value_in, "mode", "()Lio/wapitistew/wse/CameraControlMode;" ) ) );
    value.value = static_cast<std::int64_t>( longProperty( env_in, value_in, "value" ) );
    const auto status = camera->camera->setControl( value );
    if ( !status.succeeded() ) throwWseError( env_in, cameraError( status.error() ) );
}

JNIEXPORT jobject JNICALL Java_io_wapitistew_wse_Native_webCameraGetExtensionUnit(
      JNIEnv* env_in, jclass, const jlong handle_in, jobject selector_in )
{
    JavaCamera* camera = fromHandle<JavaCamera>( handle_in );
    if ( camera == nullptr ) { throwWseError( env_in, invalidStateError() ); return nullptr; }
    const auto result = camera->camera->getExtensionUnit(
        cameraExtensionSelector( env_in, selector_in ) );
    if ( !result.succeeded() )
    {
        throwWseError( env_in, cameraError( result.error() ) );
        return nullptr;
    }
    jbyteArray payload = env_in->NewByteArray( static_cast<jsize>( result.value().payload.size() ) );
    if ( !result.value().payload.empty() ) env_in->SetByteArrayRegion( payload, 0,
        static_cast<jsize>( result.value().payload.size() ),
        reinterpret_cast<const jbyte*>( result.value().payload.data() ) );
    jclass value_class = env_in->FindClass( "io/wapitistew/wse/CameraExtensionUnitValue" );
    jmethodID constructor = value_class == nullptr ? nullptr : env_in->GetMethodID(
        value_class, "<init>", "(Lio/wapitistew/wse/CameraExtensionUnitSelector;[B)V" );
    return constructor == nullptr ? nullptr : env_in->NewObject( value_class, constructor,
        makeCameraExtensionSelector( env_in, result.value().selector ), payload );
}

JNIEXPORT void JNICALL Java_io_wapitistew_wse_Native_webCameraSetExtensionUnit(
      JNIEnv* env_in, jclass, const jlong handle_in, jobject value_in )
{
    JavaCamera* camera = fromHandle<JavaCamera>( handle_in );
    if ( camera == nullptr ) { throwWseError( env_in, invalidStateError() ); return; }
    wse::tmr::sCameraExtensionUnitValue value;
    value.selector = cameraExtensionSelector( env_in, objectProperty( env_in, value_in,
        "selector", "()Lio/wapitistew/wse/CameraExtensionUnitSelector;" ) );
    auto payload = static_cast<jbyteArray>( objectProperty( env_in, value_in, "payload", "()[B" ) );
    if ( payload != nullptr )
    {
        const jsize size = env_in->GetArrayLength( payload );
        value.payload.resize( static_cast<std::size_t>( size ) );
        if ( size != 0 ) env_in->GetByteArrayRegion( payload, 0, size,
            reinterpret_cast<jbyte*>( value.payload.data() ) );
    }
    const auto status = camera->camera->setExtensionUnit( value );
    if ( !status.succeeded() ) throwWseError( env_in, cameraError( status.error() ) );
}

JNIEXPORT jboolean JNICALL Java_io_wapitistew_wse_Native_webCameraIsOpen(
      JNIEnv* env_in, jclass, const jlong handle_in )
{
    JavaCamera* camera = fromHandle<JavaCamera>( handle_in );
    if ( camera == nullptr ) { throwWseError( env_in, invalidStateError() ); return JNI_FALSE; }
    return camera->camera->isOpen() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_io_wapitistew_wse_Native_webCameraIsStreaming(
      JNIEnv* env_in, jclass, const jlong handle_in )
{
    JavaCamera* camera = fromHandle<JavaCamera>( handle_in );
    if ( camera == nullptr ) { throwWseError( env_in, invalidStateError() ); return JNI_FALSE; }
    return camera->camera->isStreaming() ? JNI_TRUE : JNI_FALSE;
}

//! \~japanese Streamを止めてDeviceを閉じる. Handleは残るため再度openできる.
//! \~english  Stops streaming and closes the device while the handle survives, so `open` may run
//!            again. This ends the session only; `webCameraClose` is the terminal release.
JNIEXPORT void JNICALL Java_io_wapitistew_wse_Native_webCameraCloseCamera(
      JNIEnv* env_in, jclass, const jlong handle_in )
{
    JavaCamera* camera = fromHandle<JavaCamera>( handle_in );
    if ( camera == nullptr ) { throwWseError( env_in, invalidStateError() ); return; }
    // Closing an already closed camera is a no-op, so neither call reports a session failure.
    camera->camera->stop();
    camera->camera->close();
    if ( camera->callback != nullptr )
    {
        env_in->DeleteGlobalRef( camera->callback );
        camera->callback = nullptr;
    }
}

JNIEXPORT void JNICALL Java_io_wapitistew_wse_Native_webCameraClose(
      JNIEnv* env_in, jclass, const jlong handle_in )
{
    JavaCamera* camera = fromHandle<JavaCamera>( handle_in );
    if ( camera == nullptr ) return;
    camera->camera->stop();
    camera->camera->close();
    if ( camera->callback != nullptr ) env_in->DeleteGlobalRef( camera->callback );
    delete camera;
}
#else
JNIEXPORT jobjectArray JNICALL Java_io_wapitistew_wse_Native_webCameraEnumerate(
      JNIEnv* env_in, jclass, jint )
{
    throwWseError( env_in, wse::binding::Error(
        wse::binding::eErrorCategory::Unsupported, 1,
        "WSE was built without Tmr." ) );
    return nullptr;
}
#endif // WSE_HAS_TMR

} // extern "C"

#ifdef WSE_HAS_IUI

namespace
{

//! \~japanese Java所有のKeyboard. Handleの寿命は`keyboardClose`で終端する.
//! \~english  Java-owned keyboard whose handle lifetime ends at `keyboardClose`.
struct JavaKeyboard final
{
    wse::iui::Keyboard keyboard;
};

//! \~japanese 読取可否を判定し、読取不能ならWseExceptionを送出する.
//! \~english  Checks readiness and throws WseException when the keyboard is not readable.
bool requireReadableKeyboard( JNIEnv* env_in, JavaKeyboard* keyboard_in )
{
    if ( keyboard_in == nullptr )
    {
        throwWseError( env_in, invalidStateError() );
        return false;
    }
    const wse::binding::Error readiness =
        wse::binding::fromIuiKeyboardState( keyboard_in->keyboard.accessState() );
    if ( !readiness.ok() )
    {
        throwWseError( env_in, readiness );
        return false;
    }
    return true;
}

//! \~japanese Key groupをJavaのboolean[]へ複製する.
//! \~english  Copies one key group into a Java boolean array.
template <std::size_t Count>
jbooleanArray toBooleanArray( JNIEnv* env_in, const std::array<bool, Count>& source_in )
{
    jbooleanArray result = env_in->NewBooleanArray( static_cast<jsize>( Count ) );
    if ( result == nullptr ) return nullptr;
    jboolean values[ Count ];
    for ( std::size_t index = 0U; index < Count; ++index )
    {
        values[ index ] = source_in[ index ] ? JNI_TRUE : JNI_FALSE;
    }
    env_in->SetBooleanArrayRegion( result, 0, static_cast<jsize>( Count ), values );
    return result;
}

} // namespace

extern "C" {

JNIEXPORT jlong JNICALL Java_io_wapitistew_wse_Native_keyboardCreate(
      JNIEnv* env_in, jclass )
{
    auto* keyboard = new ( std::nothrow ) JavaKeyboard();
    if ( keyboard == nullptr )
    {
        throwWseError( env_in, wse::binding::Error(
              wse::binding::eErrorCategory::ResourceExhausted, 1
            , "Unable to allocate Keyboard." ) );
        return 0;
    }
    return toHandle( keyboard );
}

JNIEXPORT void JNICALL Java_io_wapitistew_wse_Native_keyboardClose(
      JNIEnv*, jclass, const jlong handle_in )
{
    delete fromHandle<JavaKeyboard>( handle_in );
}

JNIEXPORT jint JNICALL Java_io_wapitistew_wse_Native_keyboardAccessState(
      JNIEnv* env_in, jclass, const jlong handle_in )
{
    JavaKeyboard* keyboard = fromHandle<JavaKeyboard>( handle_in );
    if ( keyboard == nullptr ) { throwWseError( env_in, invalidStateError() ); return 0; }
    return static_cast<jint>( keyboard->keyboard.accessState() );
}

JNIEXPORT jboolean JNICALL Java_io_wapitistew_wse_Native_keyboardIsAvailable(
      JNIEnv* env_in, jclass, const jlong handle_in )
{
    JavaKeyboard* keyboard = fromHandle<JavaKeyboard>( handle_in );
    if ( keyboard == nullptr ) { throwWseError( env_in, invalidStateError() ); return JNI_FALSE; }
    return keyboard->keyboard.isAvailable() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jobjectArray JNICALL Java_io_wapitistew_wse_Native_keyboardSnapshot(
      JNIEnv* env_in, jclass, const jlong handle_in )
{
    JavaKeyboard* keyboard = fromHandle<JavaKeyboard>( handle_in );
    if ( !requireReadableKeyboard( env_in, keyboard ) ) return nullptr;

    const wse::iui::KeyboardState state = keyboard->keyboard.snapshot();
    jclass array_class = env_in->FindClass( "[Z" );
    if ( array_class == nullptr ) return nullptr;
    jobjectArray result = env_in->NewObjectArray( 5, array_class, nullptr );
    if ( result == nullptr ) return nullptr;

    env_in->SetObjectArrayElement( result, 0, toBooleanArray( env_in, state.ascii ) );
    env_in->SetObjectArrayElement( result, 1, toBooleanArray( env_in, state.function ) );
    env_in->SetObjectArrayElement( result, 2, toBooleanArray( env_in, state.arrow ) );
    env_in->SetObjectArrayElement( result, 3, toBooleanArray( env_in, state.lock ) );
    env_in->SetObjectArrayElement( result, 4, toBooleanArray( env_in, state.command ) );
    return result;
}

JNIEXPORT jbyte JNICALL Java_io_wapitistew_wse_Native_keyboardPressedAscii(
      JNIEnv* env_in, jclass, const jlong handle_in )
{
    JavaKeyboard* keyboard = fromHandle<JavaKeyboard>( handle_in );
    if ( !requireReadableKeyboard( env_in, keyboard ) ) return 0;
    return static_cast<jbyte>( keyboard->keyboard.getASCII() );
}

} // extern "C"

#endif // WSE_HAS_IUI

// ----------------------------------------------------------------------------------------------
// XPT transports
//
// XPT has no default timeout: every operation requires an explicit deadline, so the Java side
// always passes one. A cancellation arrives as a `long` handle where `0` means none. Only the
// canonical XPT API is exposed.
// ----------------------------------------------------------------------------------------------

#ifdef WSE_HAS_XPT

namespace
{

struct JavaCancellation final
{
    wse::xpt::CancellationSource source;
};

struct JavaTcpClient final
{
    wse::xpt::TcpClient client;
};

struct JavaUdpClient final
{
    wse::xpt::UdpClient client;
};

struct JavaSerialPort final
{
    wse::xpt::SerialPort port;
};

void throwTransportError( JNIEnv* env_in, const wse::xpt::TransportError& error_in )
{
    throwWseError( env_in, wse::binding::fromXptError( error_in ) );
}


//! \~japanese Java側のTimeoutとCancellation HandleをXPTの必須制御へ変換する.
//! \~english  Converts the Java timeout and cancellation handle into the required XPT controls.
wse::xpt::OperationContext buildContext(
    const jlong timeout_ms_in, const jlong cancellation_handle_in )
{
    const wse::xpt::Timeout timeout = wse::xpt::Timeout::milliseconds( timeout_ms_in );
    JavaCancellation* cancellation = fromHandle<JavaCancellation>( cancellation_handle_in );
    if ( cancellation == nullptr )
    {
        return wse::xpt::OperationContext( timeout );
    }
    return wse::xpt::OperationContext( timeout, cancellation->source.token() );
}

//! \~japanese `std::string`をJava Stringへ変換する.
//! \~english  Converts a `std::string` into a Java string.
jstring toJavaString( JNIEnv* env_in, const std::string& text_in )
{
    return env_in->NewStringUTF( text_in.c_str() );
}

//! \~japanese Java StringをUTF-8の`std::string`へ変換する.
//! \~english  Converts a Java string into a UTF-8 `std::string`.
bool fromJavaString(
      std::string* const p_text_out
    , JNIEnv* env_in, jstring value_in )
{
    std::string& text_out = *p_text_out;

    if ( value_in == nullptr ) return false;
    const char* text = env_in->GetStringUTFChars( value_in, nullptr );
    if ( text == nullptr ) return false;
    text_out.assign( text );
    env_in->ReleaseStringUTFChars( value_in, text );
    return true;
}

//! \~japanese Java byte[]を`std::vector`へ複製する.
//! \~english  Copies a Java byte array into a `std::vector`.
bool fromJavaBytes(
      std::vector<std::uint8_t>* const p_bytes_out
    , JNIEnv* env_in, jbyteArray value_in )
{
    std::vector<std::uint8_t>& bytes_out = *p_bytes_out;

    if ( value_in == nullptr ) return false;
    const jsize size = env_in->GetArrayLength( value_in );
    bytes_out.resize( static_cast<std::size_t>( size ) );
    if ( size != 0 )
    {
        env_in->GetByteArrayRegion(
            value_in, 0, size, reinterpret_cast<jbyte*>( bytes_out.data() ) );
    }
    return true;
}

//! \~japanese `std::vector`をJava byte[]へ複製する.
//! \~english  Copies a `std::vector` into a Java byte array.
jbyteArray toJavaBytes( JNIEnv* env_in, const std::vector<std::uint8_t>& bytes_in )
{
    jbyteArray result = env_in->NewByteArray( static_cast<jsize>( bytes_in.size() ) );
    if ( result == nullptr ) return nullptr;
    if ( !bytes_in.empty() )
    {
        env_in->SetByteArrayRegion(
              result
            , 0
            , static_cast<jsize>( bytes_in.size() )
            , reinterpret_cast<const jbyte*>( bytes_in.data() ) );
    }
    return result;
}

//! \~japanese `Endpoint`をJavaの`{ host, port }` Object配列へ変換する.
//! \~english  Converts an endpoint into a Java `{ host, port }` object array.
jobjectArray toJavaEndpoint( JNIEnv* env_in, const wse::xpt::Endpoint& endpoint_in )
{
    jclass object_class = env_in->FindClass( "java/lang/Object" );
    if ( object_class == nullptr ) return nullptr;
    jobjectArray result = env_in->NewObjectArray( 2, object_class, nullptr );
    if ( result == nullptr ) return nullptr;

    jclass integer_class = env_in->FindClass( "java/lang/Integer" );
    if ( integer_class == nullptr ) return nullptr;
    jmethodID value_of = env_in->GetStaticMethodID(
        integer_class, "valueOf", "(I)Ljava/lang/Integer;" );
    if ( value_of == nullptr ) return nullptr;

    env_in->SetObjectArrayElement( result, 0, toJavaString( env_in, endpoint_in.host() ) );
    env_in->SetObjectArrayElement( result, 1, env_in->CallStaticObjectMethod(
        integer_class, value_of, static_cast<jint>( endpoint_in.port() ) ) );
    return result;
}

//! \~japanese HTTP ResponseをJavaの`{ status, attempts, headers, body }` Object配列へ変換する.
//! \~english  Converts an HTTP response into the Java `{ status, attempts, headers, body }` array.
//!
//! A success and an HTTP status failure both hand the caller a response, so both build it here
//! and a rejected request reports exactly what a successful one would.
jobjectArray toJavaHttpResponse( JNIEnv* env_in, const wse::xpt::HttpResponse& response_in )
{
    jclass object_class = env_in->FindClass( "java/lang/Object" );
    jclass string_class = env_in->FindClass( "java/lang/String" );
    jclass integer_class = env_in->FindClass( "java/lang/Integer" );
    if ( object_class == nullptr || string_class == nullptr || integer_class == nullptr )
    {
        return nullptr;
    }
    jmethodID value_of = env_in->GetStaticMethodID(
        integer_class, "valueOf", "(I)Ljava/lang/Integer;" );
    if ( value_of == nullptr ) return nullptr;

    jobjectArray headers = env_in->NewObjectArray(
        static_cast<jsize>( response_in.headers().size() * 2U ), string_class, nullptr );
    if ( headers == nullptr ) return nullptr;
    jsize header_index = 0;
    for ( const auto& header : response_in.headers() )
    {
        env_in->SetObjectArrayElement( headers, header_index, toJavaString( env_in, header.name ) );
        env_in->SetObjectArrayElement(
            headers, header_index + 1, toJavaString( env_in, header.value ) );
        header_index += 2;
    }

    jobjectArray result = env_in->NewObjectArray( 4, object_class, nullptr );
    if ( result == nullptr ) return nullptr;
    env_in->SetObjectArrayElement( result, 0, env_in->CallStaticObjectMethod(
        integer_class, value_of, static_cast<jint>( response_in.status_code() ) ) );
    env_in->SetObjectArrayElement( result, 1, env_in->CallStaticObjectMethod(
        integer_class, value_of, static_cast<jint>( response_in.attempt_count() ) ) );
    env_in->SetObjectArrayElement( result, 2, headers );
    env_in->SetObjectArrayElement( result, 3, toJavaBytes( env_in, response_in.body() ) );
    return result;
}

// JNI locals are scoped even if normalization or managed object construction fails.
void throwTransferError( JNIEnv* env_in,
    const wse::binding::detail::TransferFailureView& failure_in ) noexcept
{
    if( env_in->ExceptionCheck() || env_in->PushLocalFrame( 8 ) < 0 ) return;
    struct LocalFrame
    {
        JNIEnv* env;
        ~LocalFrame() noexcept { env->PopLocalFrame( nullptr ); }
    } frame{ env_in };
    try
    {
        const auto error = wse::binding::fromXptError( failure_in.error );
        jclass type = env_in->FindClass( "io/wapitistew/wse/TransferException" );
        if( !type ) return;
        jmethodID constructor = env_in->GetMethodID( type, "<init>",
            "(IIJLjava/lang/String;J[BLjava/lang/String;I)V" );
        if( !constructor ) return;
        jstring message = toJavaString( env_in, error.message() );
        if( !message ) return;
        jbyteArray bytes = nullptr;
        jstring host = nullptr;
        jint port = 0;
        if( failure_in.datagram )
        {
            bytes = toJavaBytes( env_in, failure_in.datagram->payload() );
            if( !bytes || env_in->ExceptionCheck() ) return;
            host = toJavaString( env_in, failure_in.datagram->source().host() );
            if( !host ) return;
            port = static_cast<jint>( failure_in.datagram->source().port() );
        }
        jobject exception = env_in->NewObject( type, constructor,
            static_cast<jint>(error.category()), static_cast<jint>(error.code()),
            static_cast<jlong>(error.nativeCode()), message,
            static_cast<jlong>(failure_in.bytes_transferred), bytes, host, port );
        if( exception ) env_in->Throw( static_cast<jthrowable>(exception) );
    }
    catch( ... )
    {
        if( !env_in->ExceptionCheck() )
        {
            jclass type = env_in->FindClass( "java/lang/OutOfMemoryError" );
            if( type ) env_in->ThrowNew( type, "Unable to construct WSE transfer error." );
        }
    }
}

//! \~japanese Responseを伴うHTTP Status ErrorをHttpStatusExceptionとして送出する.
//! \~english  Throws an HTTP status failure as an `HttpStatusException` carrying the response.
//!
//! The exception derives from `WseException`, so an existing catch of the common type still
//! sees it.
void throwHttpStatusError(
      JNIEnv* env_in
    , const wse::binding::Error& error_in
    , jobjectArray response_in )
{
    jclass error_class = env_in->FindClass( "io/wapitistew/wse/HttpStatusException" );
    jmethodID constructor = error_class == nullptr ? nullptr : env_in->GetMethodID(
        error_class, "<init>", "(IIJLjava/lang/String;[Ljava/lang/Object;)V" );
    if ( constructor == nullptr ) return;
    jstring message = env_in->NewStringUTF( error_in.message().c_str() );
    jobject exception = env_in->NewObject(
          error_class
        , constructor
        , static_cast<jint>( error_in.category() )
        , static_cast<jint>( error_in.code() )
        , static_cast<jlong>( error_in.nativeCode() )
        , message
        , response_in
    );
    if ( exception != nullptr )
    {
        env_in->Throw( static_cast<jthrowable>( exception ) );
    }
}

} // namespace

extern "C" {

// ----------------------------------------------------------------------------------------------
// Cancellation
// ----------------------------------------------------------------------------------------------

JNIEXPORT jlong JNICALL Java_io_wapitistew_wse_Native_transportCancellationCreate(
      JNIEnv* env_in, jclass )
{
    auto* cancellation = new ( std::nothrow ) JavaCancellation();
    if ( cancellation == nullptr )
    {
        throwWseError( env_in, wse::binding::Error(
              wse::binding::eErrorCategory::ResourceExhausted, 1
            , "Unable to allocate a cancellation source." ) );
        return 0;
    }
    return toHandle( cancellation );
}

JNIEXPORT void JNICALL Java_io_wapitistew_wse_Native_transportCancellationClose(
      JNIEnv*, jclass, const jlong handle_in )
{
    delete fromHandle<JavaCancellation>( handle_in );
}

JNIEXPORT void JNICALL Java_io_wapitistew_wse_Native_transportCancellationCancel(
      JNIEnv* env_in, jclass, const jlong handle_in )
{
    JavaCancellation* cancellation = fromHandle<JavaCancellation>( handle_in );
    if ( cancellation == nullptr ) { throwWseError( env_in, invalidStateError() ); return; }
    cancellation->source.cancel();
}

JNIEXPORT jboolean JNICALL Java_io_wapitistew_wse_Native_transportCancellationIsRequested(
      JNIEnv* env_in, jclass, const jlong handle_in )
{
    JavaCancellation* cancellation = fromHandle<JavaCancellation>( handle_in );
    if ( cancellation == nullptr ) { throwWseError( env_in, invalidStateError() ); return JNI_FALSE; }
    return cancellation->source.isCancellationRequested() ? JNI_TRUE : JNI_FALSE;
}

// ----------------------------------------------------------------------------------------------
// TCP
// ----------------------------------------------------------------------------------------------

JNIEXPORT jlong JNICALL Java_io_wapitistew_wse_Native_tcpCreate( JNIEnv* env_in, jclass )
{
    auto* client = new ( std::nothrow ) JavaTcpClient();
    if ( client == nullptr )
    {
        throwWseError( env_in, wse::binding::Error(
              wse::binding::eErrorCategory::ResourceExhausted, 1
            , "Unable to allocate a TCP client." ) );
        return 0;
    }
    return toHandle( client );
}

JNIEXPORT void JNICALL Java_io_wapitistew_wse_Native_tcpClose(
      JNIEnv*, jclass, const jlong handle_in )
{
    delete fromHandle<JavaTcpClient>( handle_in );
}

JNIEXPORT void JNICALL Java_io_wapitistew_wse_Native_tcpConnect(
      JNIEnv* env_in, jclass, const jlong handle_in, jstring host_in, const jint port_in
    , const jlong timeout_ms_in, const jlong cancellation_in )
{
    JavaTcpClient* client = fromHandle<JavaTcpClient>( handle_in );
    std::string host;
    if ( client == nullptr || !fromJavaString( &host, env_in, host_in ) )
    {
        throwWseError( env_in, invalidStateError() );
        return;
    }
    const auto status = client->client.connect(
          wse::xpt::Endpoint( host, static_cast<std::uint16_t>( port_in ) )
        , buildContext( timeout_ms_in, cancellation_in ) );
    if ( !status.succeeded() ) throwTransportError( env_in, status.error() );
}

JNIEXPORT void JNICALL Java_io_wapitistew_wse_Native_tcpDisconnect(
      JNIEnv* env_in, jclass, const jlong handle_in )
{
    JavaTcpClient* client = fromHandle<JavaTcpClient>( handle_in );
    if ( client == nullptr ) { throwWseError( env_in, invalidStateError() ); return; }
    client->client.disconnect();
}

JNIEXPORT jboolean JNICALL Java_io_wapitistew_wse_Native_tcpIsConnected(
      JNIEnv* env_in, jclass, const jlong handle_in )
{
    JavaTcpClient* client = fromHandle<JavaTcpClient>( handle_in );
    if ( client == nullptr ) { throwWseError( env_in, invalidStateError() ); return JNI_FALSE; }
    return client->client.isConnected() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL Java_io_wapitistew_wse_Native_tcpCheckPeerConnection(
      JNIEnv* env_in, jclass, const jlong handle_in )
{
    JavaTcpClient* client = fromHandle<JavaTcpClient>( handle_in );
    if ( client == nullptr ) { throwWseError( env_in, invalidStateError() ); return; }
    const auto status = client->client.checkPeerConnection();
    if ( !status.succeeded() ) throwTransportError( env_in, status.error() );
}

JNIEXPORT jobjectArray JNICALL Java_io_wapitistew_wse_Native_tcpRemoteEndpoint(
      JNIEnv* env_in, jclass, const jlong handle_in )
{
    JavaTcpClient* client = fromHandle<JavaTcpClient>( handle_in );
    if ( client == nullptr ) { throwWseError( env_in, invalidStateError() ); return nullptr; }
    return toJavaEndpoint( env_in, client->client.getRemoteEndpoint() );
}

JNIEXPORT jobjectArray JNICALL Java_io_wapitistew_wse_Native_tcpLocalEndpoint(
      JNIEnv* env_in, jclass, const jlong handle_in )
{
    JavaTcpClient* client = fromHandle<JavaTcpClient>( handle_in );
    if ( client == nullptr ) { throwWseError( env_in, invalidStateError() ); return nullptr; }
    return toJavaEndpoint( env_in, client->client.getLocalEndpoint() );
}

JNIEXPORT jlong JNICALL Java_io_wapitistew_wse_Native_tcpSend(
      JNIEnv* env_in, jclass, const jlong handle_in, jbyteArray data_in
    , const jlong timeout_ms_in, const jlong cancellation_in )
{
    JavaTcpClient* client = fromHandle<JavaTcpClient>( handle_in );
    std::vector<std::uint8_t> bytes;
    if ( client == nullptr || !fromJavaBytes( &bytes, env_in, data_in ) )
    {
        throwWseError( env_in, invalidStateError() );
        return 0;
    }
    const auto result = client->client.send(
        bytes, buildContext( timeout_ms_in, cancellation_in ) );
    if ( !result.succeeded() ) { throwTransferError( env_in, wse::binding::detail::transferFailure( result ) ); return 0; }
    return static_cast<jlong>( result.value() );
}

JNIEXPORT jbyteArray JNICALL Java_io_wapitistew_wse_Native_tcpReceive(
      JNIEnv* env_in, jclass, const jlong handle_in, const jlong maximum_size_in
    , const jlong timeout_ms_in, const jlong cancellation_in )
{
    JavaTcpClient* client = fromHandle<JavaTcpClient>( handle_in );
    if ( client == nullptr ) { throwWseError( env_in, invalidStateError() ); return nullptr; }
    auto result = client->client.receive(
          static_cast<std::size_t>( maximum_size_in )
        , buildContext( timeout_ms_in, cancellation_in ) );
    if ( !result.succeeded() ) { throwTransportError( env_in, result.error() ); return nullptr; }
    return toJavaBytes( env_in, result.value() );
}

// ----------------------------------------------------------------------------------------------
// UDP
// ----------------------------------------------------------------------------------------------

JNIEXPORT jlong JNICALL Java_io_wapitistew_wse_Native_udpMaximumDatagramSize( JNIEnv*, jclass )
{
    return static_cast<jlong>( wse::xpt::UdpClient::maximumDatagramSize() );
}

JNIEXPORT jlong JNICALL Java_io_wapitistew_wse_Native_udpCreate( JNIEnv* env_in, jclass )
{
    auto* client = new ( std::nothrow ) JavaUdpClient();
    if ( client == nullptr )
    {
        throwWseError( env_in, wse::binding::Error(
              wse::binding::eErrorCategory::ResourceExhausted, 1
            , "Unable to allocate a UDP client." ) );
        return 0;
    }
    return toHandle( client );
}

JNIEXPORT void JNICALL Java_io_wapitistew_wse_Native_udpClose(
      JNIEnv*, jclass, const jlong handle_in )
{
    delete fromHandle<JavaUdpClient>( handle_in );
}

JNIEXPORT void JNICALL Java_io_wapitistew_wse_Native_udpBind(
      JNIEnv* env_in, jclass, const jlong handle_in, jstring host_in, const jint port_in
    , const jlong timeout_ms_in, const jlong cancellation_in )
{
    JavaUdpClient* client = fromHandle<JavaUdpClient>( handle_in );
    std::string host;
    if ( client == nullptr || !fromJavaString( &host, env_in, host_in ) )
    {
        throwWseError( env_in, invalidStateError() );
        return;
    }
    const auto status = client->client.bind(
          wse::xpt::Endpoint( host, static_cast<std::uint16_t>( port_in ) )
        , buildContext( timeout_ms_in, cancellation_in ) );
    if ( !status.succeeded() ) throwTransportError( env_in, status.error() );
}

JNIEXPORT void JNICALL Java_io_wapitistew_wse_Native_udpCloseSocket(
      JNIEnv* env_in, jclass, const jlong handle_in )
{
    JavaUdpClient* client = fromHandle<JavaUdpClient>( handle_in );
    if ( client == nullptr ) { throwWseError( env_in, invalidStateError() ); return; }
    client->client.close();
}

JNIEXPORT jboolean JNICALL Java_io_wapitistew_wse_Native_udpIsOpen(
      JNIEnv* env_in, jclass, const jlong handle_in )
{
    JavaUdpClient* client = fromHandle<JavaUdpClient>( handle_in );
    if ( client == nullptr ) { throwWseError( env_in, invalidStateError() ); return JNI_FALSE; }
    return client->client.isOpen() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jobjectArray JNICALL Java_io_wapitistew_wse_Native_udpLocalEndpoint(
      JNIEnv* env_in, jclass, const jlong handle_in )
{
    JavaUdpClient* client = fromHandle<JavaUdpClient>( handle_in );
    if ( client == nullptr ) { throwWseError( env_in, invalidStateError() ); return nullptr; }
    return toJavaEndpoint( env_in, client->client.getLocalEndpoint() );
}

JNIEXPORT jlong JNICALL Java_io_wapitistew_wse_Native_udpSendTo(
      JNIEnv* env_in, jclass, const jlong handle_in, jstring host_in, const jint port_in
    , jbyteArray data_in, const jlong timeout_ms_in, const jlong cancellation_in )
{
    JavaUdpClient* client = fromHandle<JavaUdpClient>( handle_in );
    std::string host;
    std::vector<std::uint8_t> bytes;
    if ( client == nullptr
         || !fromJavaString( &host, env_in, host_in )
         || !fromJavaBytes( &bytes, env_in, data_in ) )
    {
        throwWseError( env_in, invalidStateError() );
        return 0;
    }
    const auto result = client->client.sendTo(
          wse::xpt::Endpoint( host, static_cast<std::uint16_t>( port_in ) )
        , bytes
        , buildContext( timeout_ms_in, cancellation_in ) );
    if ( !result.succeeded() ) { throwTransferError( env_in, wse::binding::detail::transferFailure( result ) ); return 0; }
    return static_cast<jlong>( result.value() );
}

//! @return \~japanese `{ host, Integer port, byte[] payload }`の3要素Object配列.
//! @return \~english  A three-element object array of `{ host, Integer port, byte[] payload }`.
JNIEXPORT jobjectArray JNICALL Java_io_wapitistew_wse_Native_udpReceiveFrom(
      JNIEnv* env_in, jclass, const jlong handle_in, const jlong maximum_size_in
    , const jlong timeout_ms_in, const jlong cancellation_in )
{
    JavaUdpClient* client = fromHandle<JavaUdpClient>( handle_in );
    if ( client == nullptr ) { throwWseError( env_in, invalidStateError() ); return nullptr; }

    auto result = client->client.receiveFrom(
          static_cast<std::size_t>( maximum_size_in )
        , buildContext( timeout_ms_in, cancellation_in ) );
    if ( !result.succeeded() )
    {
        const auto failure = wse::binding::detail::transferFailure( result );
        if( failure.datagram ) throwTransferError( env_in, failure );
        else throwTransportError( env_in, result.error() );
        return nullptr;
    }

    const wse::xpt::UdpDatagram& datagram = result.value();
    jclass object_class = env_in->FindClass( "java/lang/Object" );
    if ( object_class == nullptr ) return nullptr;
    jobjectArray output = env_in->NewObjectArray( 3, object_class, nullptr );
    if ( output == nullptr ) return nullptr;

    jclass integer_class = env_in->FindClass( "java/lang/Integer" );
    if ( integer_class == nullptr ) return nullptr;
    jmethodID value_of = env_in->GetStaticMethodID(
        integer_class, "valueOf", "(I)Ljava/lang/Integer;" );
    if ( value_of == nullptr ) return nullptr;

    env_in->SetObjectArrayElement(
        output, 0, toJavaString( env_in, datagram.source().host() ) );
    env_in->SetObjectArrayElement( output, 1, env_in->CallStaticObjectMethod(
        integer_class, value_of, static_cast<jint>( datagram.source().port() ) ) );
    env_in->SetObjectArrayElement( output, 2, toJavaBytes( env_in, datagram.payload() ) );
    return output;
}

// ----------------------------------------------------------------------------------------------
// Serial
// ----------------------------------------------------------------------------------------------

JNIEXPORT jlong JNICALL Java_io_wapitistew_wse_Native_serialCreate( JNIEnv* env_in, jclass )
{
    auto* port = new ( std::nothrow ) JavaSerialPort();
    if ( port == nullptr )
    {
        throwWseError( env_in, wse::binding::Error(
              wse::binding::eErrorCategory::ResourceExhausted, 1
            , "Unable to allocate a serial port." ) );
        return 0;
    }
    return toHandle( port );
}

JNIEXPORT void JNICALL Java_io_wapitistew_wse_Native_serialClose(
      JNIEnv*, jclass, const jlong handle_in )
{
    delete fromHandle<JavaSerialPort>( handle_in );
}

JNIEXPORT void JNICALL Java_io_wapitistew_wse_Native_serialOpen(
      JNIEnv* env_in, jclass, const jlong handle_in, jstring device_in, const jint baud_rate_in
    , const jlong timeout_ms_in, const jlong cancellation_in )
{
    JavaSerialPort* port = fromHandle<JavaSerialPort>( handle_in );
    std::string device;
    if ( port == nullptr || !fromJavaString( &device, env_in, device_in ) )
    {
        throwWseError( env_in, invalidStateError() );
        return;
    }
    const auto status = port->port.open(
          device
        , static_cast<std::int32_t>( baud_rate_in )
        , buildContext( timeout_ms_in, cancellation_in ) );
    if ( !status.succeeded() ) throwTransportError( env_in, status.error() );
}

JNIEXPORT void JNICALL Java_io_wapitistew_wse_Native_serialClosePort(
      JNIEnv* env_in, jclass, const jlong handle_in )
{
    JavaSerialPort* port = fromHandle<JavaSerialPort>( handle_in );
    if ( port == nullptr ) { throwWseError( env_in, invalidStateError() ); return; }
    port->port.close();
}

JNIEXPORT jboolean JNICALL Java_io_wapitistew_wse_Native_serialIsOpen(
      JNIEnv* env_in, jclass, const jlong handle_in )
{
    JavaSerialPort* port = fromHandle<JavaSerialPort>( handle_in );
    if ( port == nullptr ) { throwWseError( env_in, invalidStateError() ); return JNI_FALSE; }
    return port->port.isOpen() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL Java_io_wapitistew_wse_Native_serialDeviceName(
      JNIEnv* env_in, jclass, const jlong handle_in )
{
    JavaSerialPort* port = fromHandle<JavaSerialPort>( handle_in );
    if ( port == nullptr ) { throwWseError( env_in, invalidStateError() ); return nullptr; }
    return toJavaString( env_in, port->port.getDeviceName() );
}

JNIEXPORT jint JNICALL Java_io_wapitistew_wse_Native_serialBaudRate(
      JNIEnv* env_in, jclass, const jlong handle_in )
{
    JavaSerialPort* port = fromHandle<JavaSerialPort>( handle_in );
    if ( port == nullptr ) { throwWseError( env_in, invalidStateError() ); return 0; }
    return static_cast<jint>( port->port.getBaudRate() );
}

JNIEXPORT jlong JNICALL Java_io_wapitistew_wse_Native_serialSend(
      JNIEnv* env_in, jclass, const jlong handle_in, jbyteArray data_in
    , const jlong timeout_ms_in, const jlong cancellation_in )
{
    JavaSerialPort* port = fromHandle<JavaSerialPort>( handle_in );
    std::vector<std::uint8_t> bytes;
    if ( port == nullptr || !fromJavaBytes( &bytes, env_in, data_in ) )
    {
        throwWseError( env_in, invalidStateError() );
        return 0;
    }
    const auto result = port->port.send(
        bytes, buildContext( timeout_ms_in, cancellation_in ) );
    if ( !result.succeeded() ) { throwTransferError( env_in, wse::binding::detail::transferFailure( result ) ); return 0; }
    return static_cast<jlong>( result.value() );
}

JNIEXPORT jbyteArray JNICALL Java_io_wapitistew_wse_Native_serialReceive(
      JNIEnv* env_in, jclass, const jlong handle_in, const jlong maximum_size_in
    , const jlong timeout_ms_in, const jlong cancellation_in )
{
    JavaSerialPort* port = fromHandle<JavaSerialPort>( handle_in );
    if ( port == nullptr ) { throwWseError( env_in, invalidStateError() ); return nullptr; }
    auto result = port->port.receive(
          static_cast<std::size_t>( maximum_size_in )
        , buildContext( timeout_ms_in, cancellation_in ) );
    if ( !result.succeeded() ) { throwTransportError( env_in, result.error() ); return nullptr; }
    return toJavaBytes( env_in, result.value() );
}

// ----------------------------------------------------------------------------------------------
// HTTP
// ----------------------------------------------------------------------------------------------

//!
//! \~japanese
//! @brief   HTTP Requestを1回実行する.
//! @details Header名／値は交互に並べた1本のString配列で受け取る. `username`が`null`のときは
//!          認証なしで実行する. Bindingは呼出元に代わってRetryしない.
//! @return  `{ Integer statusCode, Integer attemptCount, String[] headers, byte[] body }`.
//! \~english
//! @brief   Executes one HTTP request.
//! @details Header names and values arrive interleaved in one string array. A `null` `username`
//!          executes without authentication. The binding never retries on the caller's behalf.
//!          A 4xx or 5xx answer throws `HttpStatusException`, which carries the same array; every
//!          other failure throws `WseException`.
//! @return  `{ Integer statusCode, Integer attemptCount, String[] headers, byte[] body }`.
//!
JNIEXPORT jobjectArray JNICALL Java_io_wapitistew_wse_Native_httpExecute(
      JNIEnv* env_in, jclass, const jint method_in, jstring url_in
    , jobjectArray headers_in, jbyteArray body_in, const jlong maximum_body_in
    , jstring username_in, jstring secret_in
    , const jlong timeout_ms_in, const jlong cancellation_in )
{
    std::string url;
    if ( !fromJavaString( &url, env_in, url_in ) )
    {
        throwWseError( env_in, invalidStateError() );
        return nullptr;
    }
    if ( method_in < 0
         || method_in > static_cast<jint>( wse::xpt::eHttpMethod::Delete ) )
    {
        throwWseError( env_in, wse::binding::Error(
              wse::binding::eErrorCategory::InvalidArgument, 1
            , "method is outside the supported range." ) );
        return nullptr;
    }

    wse::xpt::HttpRequest request(
        static_cast<wse::xpt::eHttpMethod>( method_in ), url );

    if ( headers_in != nullptr )
    {
        const jsize count = env_in->GetArrayLength( headers_in );
        for ( jsize index = 0; index + 1 < count; index += 2 )
        {
            auto name_value = static_cast<jstring>(
                env_in->GetObjectArrayElement( headers_in, index ) );
            auto header_value = static_cast<jstring>(
                env_in->GetObjectArrayElement( headers_in, index + 1 ) );
            std::string name;
            std::string text;
            if ( fromJavaString( &name, env_in, name_value )
                 && fromJavaString( &text, env_in, header_value ) )
            {
                request.addHeader( name, text );
            }
        }
    }

    if ( body_in != nullptr )
    {
        std::vector<std::uint8_t> body;
        if ( fromJavaBytes( &body, env_in, body_in ) )
        {
            request.setBody( body );
        }
    }

    // The binding never retries on the caller's behalf.
    const wse::xpt::HttpExecutionOptions options(
          static_cast<std::size_t>( maximum_body_in )
        , wse::xpt::RetryPolicy()
        , wse::xpt::eRetryOperationSafety::NonIdempotent
        , false );
    const wse::xpt::OperationContext context =
        buildContext( timeout_ms_in, cancellation_in );
    const wse::xpt::HttpClient client;

    std::string username;
    std::string secret;
    const bool authenticated =
        ( username_in != nullptr ) && fromJavaString( &username, env_in, username_in );
    if ( authenticated && secret_in != nullptr )
    {
        fromJavaString( &secret, env_in, secret_in );
    }

    auto result = authenticated
        ? client.executeAuthenticated( request, options,
              wse::xpt::HttpAuthentication(
                  wse::xpt::eHttpAuthenticationPolicy::ServerNegotiated, username, secret ),
              context )
        : client.execute( request, options, context );
    if ( !result.succeeded() )
    {
        // An HTTP status error is the one failure that still carries a response: the exchange
        // finished and the server answered, so Core returns the error and the value together.
        // Throwing stays the contract, but the response rides along on the exception rather than
        // being dropped, so a caller can still read the status the server sent.
        if ( result.error().code() == wse::xpt::eTransportErrorCode::HttpStatusError )
        {
            jobjectArray carried = toJavaHttpResponse( env_in, result.value() );
            if ( carried != nullptr )
            {
                throwHttpStatusError(
                    env_in, wse::binding::fromXptError( result.error() ), carried );
                return nullptr;
            }
        }
        throwTransportError( env_in, result.error() );
        return nullptr;
    }

    return toJavaHttpResponse( env_in, result.value() );
}

} // extern "C"

#endif // WSE_HAS_XPT
// ----------------------------------------------------------------------------------------------
// Projector control: model profiles and control sessions
//
// Only the new profile-driven surface is exposed. The bool-returning legacy control surface and
// the legacy profile adapter are deliberately excluded.
// ----------------------------------------------------------------------------------------------

#ifdef WSE_EXTENSION_JNI_BINDING
#include "wse_extension_jni_body.inc"
#endif
#ifdef WSE_EXTENSION_TMR_BINDING
#include "wse_extension_tmr_jni_body.inc"
#endif
