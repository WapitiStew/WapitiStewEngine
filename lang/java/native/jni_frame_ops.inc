// @file jni_frame_ops.inc
// @brief JNI entry points for the camera frame operations.
//
// Included by jni.cpp so that the frame work sits together instead of being scattered through the
// camera lifecycle. Moving or averaging pixels is image work, not device work.

// This file is included inside the `extern "C"` block that holds the JNI entry points, so the
// helpers below have to say that they are not entry points; a function with C linkage cannot
// return a C++ type.
extern "C++"
{
namespace
{

//! \~japanese Java側のFrame引数をCameraのFrameへ読み取る.
//! \~english  Reads a frame given as separate Java arguments into a camera frame.
bool readJavaFrame(
      wse::tmr::sCameraFrame* const p_frame_out
    , JNIEnv*     env_in
    , const jint  width_in
    , const jint  height_in
    , const jint  pixel_format_in
    , const jlong row_stride_in
    , const jlong sequence_in
    , const jlong timestamp_in
    , jobject     data_in )
{
    wse::tmr::sCameraFrame& frame_out = *p_frame_out;

    if ( data_in == nullptr ) return false;
    const void* bytes = env_in->GetDirectBufferAddress( data_in );
    const jlong size = env_in->GetDirectBufferCapacity( data_in );
    if ( bytes == nullptr || size < 0 ) return false;
    frame_out.description.width        = static_cast<std::uint32_t>( width_in );
    frame_out.description.height       = static_cast<std::uint32_t>( height_in );
    frame_out.description.pixel_format =
        static_cast<wse::tmr::eCameraPixelFormat>( pixel_format_in );
    // A caller who does not say what the row stride is means the rows are packed.
    frame_out.description.row_stride = row_stride_in > 0
        ? static_cast<std::size_t>( row_stride_in )
        : frame_out.description.minimumRowStride();
    frame_out.sequence               = static_cast<std::uint64_t>( sequence_in );
    frame_out.monotonic_timestamp_ns = timestamp_in;
    frame_out.data.assign(
        static_cast<const std::uint8_t*>( bytes ),
        static_cast<const std::uint8_t*>( bytes ) + size );
    return true;
}

wse::binding::Error directBufferError()
{
    return wse::binding::Error(
          wse::binding::eErrorCategory::InvalidArgument, 1
        , "The data of a frame must be a direct byte buffer." );
}

struct JavaFrameAccumulator final
{
    wse::tmr::CameraFrameAccumulator accumulator;
};

} // namespace
} // extern "C++"

JNIEXPORT jobject JNICALL Java_io_wapitistew_wse_Native_webCameraReadAveragedFrame(
      JNIEnv* env_in, jclass, const jlong handle_in, const jlong count_in
    , const jint timeout_ms_in )
{
    JavaCamera* camera = fromHandle<JavaCamera>( handle_in );
    if ( camera == nullptr ) { throwWseError( env_in, invalidStateError() ); return nullptr; }
    if ( count_in < 0 )
    {
        throwWseError( env_in, wse::binding::Error(
            wse::binding::eErrorCategory::InvalidArgument, 1, "count must not be negative." ) );
        return nullptr;
    }
    auto result = wse::tmr::readAveragedFrame( camera->camera.get(),
        static_cast<std::size_t>( count_in ), static_cast<std::uint32_t>( timeout_ms_in ) );
    if ( !result.succeeded() )
    {
        throwWseError( env_in, cameraError( result.error() ) );
        return nullptr;
    }
    return makeCameraFrame( env_in, std::move( result.value() ) );
}

JNIEXPORT jboolean JNICALL Java_io_wapitistew_wse_Native_cameraIsFrameOperationSupported(
      JNIEnv*, jclass, const jint pixel_format_in )
{
    return wse::tmr::isFrameOperationSupported(
        static_cast<wse::tmr::eCameraPixelFormat>( pixel_format_in ) ) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_io_wapitistew_wse_Native_cameraIsFrameAveragingSupported(
      JNIEnv*, jclass, const jint pixel_format_in )
{
    return wse::tmr::isFrameAveragingSupported(
        static_cast<wse::tmr::eCameraPixelFormat>( pixel_format_in ) ) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_io_wapitistew_wse_Native_cameraIsBayerFormat(
      JNIEnv*, jclass, const jint pixel_format_in )
{
    return wse::tmr::isBayerFormat(
        static_cast<wse::tmr::eCameraPixelFormat>( pixel_format_in ) ) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL Java_io_wapitistew_wse_Native_cameraBayerPatternOf(
      JNIEnv* env_in, jclass, const jint pixel_format_in )
{
    wse::eBayerPattern pattern = wse::eBayerPattern::Rggb;
    if ( !wse::tmr::bayerPatternOf(
             &pattern, static_cast<wse::tmr::eCameraPixelFormat>( pixel_format_in ) ) )
    {
        throwWseError( env_in, wse::binding::Error(
              wse::binding::eErrorCategory::InvalidArgument, 1
            , "This pixel format carries no Bayer layout." ) );
        return 0;
    }
    return static_cast<jint>( pattern );
}

JNIEXPORT jobject JNICALL Java_io_wapitistew_wse_Native_cameraApplyOrientation(
      JNIEnv* env_in, jclass, const jint width_in, const jint height_in
    , const jint pixel_format_in, const jlong row_stride_in, const jlong sequence_in
    , const jlong timestamp_in, jobject data_in, const jint orientation_in )
{
    wse::tmr::sCameraFrame frame;
    if ( !readJavaFrame( &frame, env_in, width_in, height_in, pixel_format_in, row_stride_in,
             sequence_in, timestamp_in, data_in ) )
    {
        throwWseError( env_in, directBufferError() );
        return nullptr;
    }
    auto result = wse::tmr::applyOrientation( frame,
        static_cast<wse::eImageOrientation>( orientation_in ) );
    if ( !result.succeeded() )
    {
        throwWseError( env_in, cameraError( result.error() ) );
        return nullptr;
    }
    return makeCameraFrame( env_in, std::move( result.value() ) );
}

JNIEXPORT jobject JNICALL Java_io_wapitistew_wse_Native_cameraDemosaicFrame(
      JNIEnv* env_in, jclass, const jint width_in, const jint height_in
    , const jint pixel_format_in, const jlong row_stride_in, const jlong sequence_in
    , const jlong timestamp_in, jobject data_in, const jint output_format_in
    , const jint method_in )
{
    wse::tmr::sCameraFrame frame;
    if ( !readJavaFrame( &frame, env_in, width_in, height_in, pixel_format_in, row_stride_in,
             sequence_in, timestamp_in, data_in ) )
    {
        throwWseError( env_in, directBufferError() );
        return nullptr;
    }
    auto result = wse::tmr::demosaicFrame( frame,
        static_cast<wse::tmr::eCameraPixelFormat>( output_format_in ),
        static_cast<wse::eDemosaicMethod>( method_in ) );
    if ( !result.succeeded() )
    {
        throwWseError( env_in, cameraError( result.error() ) );
        return nullptr;
    }
    return makeCameraFrame( env_in, std::move( result.value() ) );
}

JNIEXPORT jlong JNICALL Java_io_wapitistew_wse_Native_cameraAccumulatorCreate( JNIEnv*, jclass )
{
    return toHandle( new JavaFrameAccumulator() );
}

JNIEXPORT void JNICALL Java_io_wapitistew_wse_Native_cameraAccumulatorClose(
      JNIEnv*, jclass, const jlong handle_in )
{
    delete fromHandle<JavaFrameAccumulator>( handle_in );
}

JNIEXPORT void JNICALL Java_io_wapitistew_wse_Native_cameraAccumulatorAdd(
      JNIEnv* env_in, jclass, const jlong handle_in, const jint width_in, const jint height_in
    , const jint pixel_format_in, const jlong row_stride_in, const jlong sequence_in
    , const jlong timestamp_in, jobject data_in )
{
    JavaFrameAccumulator* accumulator = fromHandle<JavaFrameAccumulator>( handle_in );
    if ( accumulator == nullptr ) { throwWseError( env_in, invalidStateError() ); return; }
    wse::tmr::sCameraFrame frame;
    if ( !readJavaFrame( &frame, env_in, width_in, height_in, pixel_format_in, row_stride_in,
             sequence_in, timestamp_in, data_in ) )
    {
        throwWseError( env_in, directBufferError() );
        return;
    }
    const auto status = accumulator->accumulator.add( frame );
    if ( !status.succeeded() ) throwWseError( env_in, cameraError( status.error() ) );
}

JNIEXPORT jlong JNICALL Java_io_wapitistew_wse_Native_cameraAccumulatorCount(
      JNIEnv* env_in, jclass, const jlong handle_in )
{
    JavaFrameAccumulator* accumulator = fromHandle<JavaFrameAccumulator>( handle_in );
    if ( accumulator == nullptr ) { throwWseError( env_in, invalidStateError() ); return 0; }
    return static_cast<jlong>( accumulator->accumulator.count() );
}

JNIEXPORT jobject JNICALL Java_io_wapitistew_wse_Native_cameraAccumulatorAverage(
      JNIEnv* env_in, jclass, const jlong handle_in )
{
    JavaFrameAccumulator* accumulator = fromHandle<JavaFrameAccumulator>( handle_in );
    if ( accumulator == nullptr ) { throwWseError( env_in, invalidStateError() ); return nullptr; }
    auto result = accumulator->accumulator.average();
    if ( !result.succeeded() )
    {
        throwWseError( env_in, cameraError( result.error() ) );
        return nullptr;
    }
    return makeCameraFrame( env_in, std::move( result.value() ) );
}

JNIEXPORT void JNICALL Java_io_wapitistew_wse_Native_cameraAccumulatorReset(
      JNIEnv* env_in, jclass, const jlong handle_in )
{
    JavaFrameAccumulator* accumulator = fromHandle<JavaFrameAccumulator>( handle_in );
    if ( accumulator == nullptr ) { throwWseError( env_in, invalidStateError() ); return; }
    accumulator->accumulator.reset();
}
