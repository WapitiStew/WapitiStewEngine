#ifndef WSE_INTERNAL_CAMERA_OWNER_THREAD_H
#define WSE_INTERNAL_CAMERA_OWNER_THREAD_H

#include "CameraBackend.h"
#include <condition_variable>
#include <exception>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <type_traits>

namespace wse::tmr::detail
{
// All native object construction, operations and destruction use one thread. A synchronous
// stack task borrows its caller's arguments until completion; dispatch itself never allocates.
template<class Object> class CameraOwnerThread final
{
    struct Task { virtual ~Task() = default; virtual void run(Object&) noexcept = 0; };
    std::mutex m_mutex;
    std::condition_variable m_condition;
    Task* m_task = nullptr;
    bool m_ready = false, m_stop = false;
    std::exception_ptr m_start_error;
    std::thread m_thread;
public:
    explicit CameraOwnerThread(std::unique_ptr<Object> (*factory_in)())
        : m_thread([this, factory_in]()
        {
            std::unique_ptr<Object> object;
            try
            {
                object = factory_in();
                if(!object) throw std::runtime_error("Camera backend factory returned no owner.");
            }
            catch(...) { m_start_error = std::current_exception(); }
            std::unique_lock<std::mutex> lock(m_mutex);
            m_ready = true;
            m_condition.notify_all();
            if(m_start_error) return;
            while(true)
            {
                m_condition.wait(lock, [this]() { return m_stop || m_task; });
                if(m_stop) break;
                auto* const task = m_task;
                lock.unlock();
                task->run(*object);
                lock.lock();
                m_task = nullptr;
                m_condition.notify_all();
            }
            lock.unlock();
            object.reset(); // The runtime and its COM references die on their creation thread.
        })
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_condition.wait(lock, [this]() { return m_ready; });
        if(m_start_error)
        {
            lock.unlock(); m_thread.join();
            std::rethrow_exception(m_start_error);
        }
    }
    CameraOwnerThread(const CameraOwnerThread&) = delete;
    ~CameraOwnerThread() noexcept
    {
        { std::lock_guard<std::mutex> lock(m_mutex); m_stop = true; }
        m_condition.notify_all();
        m_thread.join();
    }
    template<class Function> auto invoke(Function&& function_in)
        -> std::invoke_result_t<Function, Object&>
    {
        using Result = std::invoke_result_t<Function, Object&>;
        using Stored = std::conditional_t<std::is_void_v<Result>, bool, Result>;
        struct Invocation final : Task
        {
            Function& function;
            std::optional<Stored> result;
            std::exception_ptr error;
            explicit Invocation(Function& function_in) : function(function_in) {}
            void run(Object& object_inout) noexcept override
            {
                try
                {
                    if constexpr(std::is_void_v<Result>) { function(object_inout); result = true; }
                    else result.emplace(function(object_inout));
                }
                catch(...) { error = std::current_exception(); }
            }
        } task(function_in);
        std::unique_lock<std::mutex> lock(m_mutex);
        m_condition.wait(lock, [this]() { return !m_task; });
        m_task = &task;
        m_condition.notify_all();
        m_condition.wait(lock, [this, &task]() { return m_task != &task; });
        if(task.error) std::rethrow_exception(task.error);
        if constexpr(!std::is_void_v<Result>) return std::move(*task.result);
    }
};

// The public callback worker remains separate: it can synchronously stop/close this backend
// without destroying COM state on that callback thread. Concurrent owner destruction is forbidden.
// Native operations must not synchronously re-enter this dispatcher from its owner thread.
class ThreadOwnedCameraBackend final : public ICameraBackend
{
    mutable CameraOwnerThread<ICameraBackend> m_owner;
public:
    explicit ThreadOwnedCameraBackend(std::unique_ptr<ICameraBackend> (*factory_in)()) : m_owner(factory_in) {}
    CameraStatus open(const sCameraOpenDescription& description_in) override
    { return m_owner.invoke([&](auto& backend_inout) { return backend_inout.open(description_in); }); }
    void close() noexcept override
    { m_owner.invoke([](auto& backend_inout) { backend_inout.close(); }); }
    CameraStatus start() override
    { return m_owner.invoke([](auto& backend_inout) { return backend_inout.start(); }); }
    CameraStatus stop() override
    { return m_owner.invoke([](auto& backend_inout) { return backend_inout.stop(); }); }
    CameraResult<sCameraFrame> readFrame(const std::uint32_t timeout_ms_in) override
    { return m_owner.invoke([&](auto& backend_inout) { return backend_inout.readFrame(timeout_ms_in); }); }
    CameraResult<sCameraControlValue> getControl(const eCameraControl control_in) override
    { return m_owner.invoke([&](auto& backend_inout) { return backend_inout.getControl(control_in); }); }
    CameraStatus setControl(const sCameraControlValue& value_in) override
    { return m_owner.invoke([&](auto& backend_inout) { return backend_inout.setControl(value_in); }); }
    CameraResult<sCameraExtensionUnitValue> getExtensionUnit(const sCameraExtensionUnitSelector& selector_in) override
    { return m_owner.invoke([&](auto& backend_inout) { return backend_inout.getExtensionUnit(selector_in); }); }
    CameraStatus setExtensionUnit(const sCameraExtensionUnitValue& value_in) override
    { return m_owner.invoke([&](auto& backend_inout) { return backend_inout.setExtensionUnit(value_in); }); }
    bool isOpen() const noexcept override
    { return m_owner.invoke([](auto& backend_inout) { return backend_inout.isOpen(); }); }
    bool isStreaming() const noexcept override
    { return m_owner.invoke([](auto& backend_inout) { return backend_inout.isStreaming(); }); }
};
}
#endif
