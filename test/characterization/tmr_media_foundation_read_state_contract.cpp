#include "../../platform/tmr/win/camera/MediaFoundationReadState.h"
#include <cstdlib>
#include <iostream>
#include <thread>

#define CHECK(x) do { if(!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::abort(); } } while(false)
using wse::tmr::detail::MediaFoundationReadState;
using namespace std::chrono_literals;
int main()
{
    CHECK(SUCCEEDED(MFStartup(MF_VERSION)));
    Microsoft::WRL::ComPtr<IMFSample> sample;
    CHECK(SUCCEEDED(MFCreateSample(&sample)));
    for(int scenario = 0; scenario < 3; ++scenario)
    {
        MediaFoundationReadState state;
        state.reset();
        const auto result = wse::tmr::detail::flushMediaFoundationReader(state, [&]() {
            if(scenario == 0) state.flushed();
            return scenario == 1 ? E_FAIL : S_OK;
        }, 1ms);
        CHECK(result.status == (scenario == 1 ? E_FAIL : S_OK));
        CHECK(result.timed_out == (scenario == 2));
        state.reset(); state.receive(S_OK, 0, 1, sample.Get());
        CHECK(state.ready == (scenario == 0));
        if(scenario != 0)
        {
            unsigned calls = 0;
            const auto closed = wse::tmr::detail::flushMediaFoundationReader(state,
                [&]() { ++calls; return S_OK; }, 0ms);
            CHECK(closed.status == MF_E_SHUTDOWN && calls == 0);
        }
    }
    {
        MediaFoundationReadState state;
        state.reset();
        state.receive(S_OK, 7, 99, sample.Get());
        CHECK(state.ready && state.sample.Get() == sample.Get() && state.timestamp == 99);
        CHECK(state.beginFlush());
        CHECK(!state.ready && !state.sample && !state.waitFlushed(0ms));
        state.flushed(); CHECK(state.waitFlushed(0ms));
        state.receive(S_OK, 0, 100, sample.Get());
        CHECK(!state.ready); // Publication is closed between streams.
        state.reset();
        state.request_in_flight = true;
        CHECK(state.beginFlush());
        state.receive(S_OK, 0, 101, sample.Get());
        CHECK(!state.ready && state.request_in_flight && !state.sample);
        CHECK(!state.waitFlushed(1ms));
        state.reset(); // Restart cannot cancel an outstanding flush.
        CHECK(!state.waitFlushed(0ms));
        std::thread callback([&]() { state.flushed(); });
        CHECK(state.waitFlushed(1000ms));
        callback.join();
        CHECK(!state.request_in_flight);
        state.reset();
        state.receive(S_OK, 0, 102, sample.Get());
        CHECK(state.ready && state.timestamp == 102);
        state.flushed(); // An unsolicited flush must not erase a new frame.
        CHECK(state.ready && state.timestamp == 102);
        state.close();
        state.reset(); state.receive(S_OK, 0, 103, sample.Get());
        CHECK(!state.ready && !state.sample && !state.waitFlushed(0ms));
    }
    for(bool close_before_notification : {false, true})
    {
        MediaFoundationReadState state;
        state.reset(); state.request_in_flight = true;
        CHECK(state.beginFlush());
        if(close_before_notification) state.close();
        state.flushed();
        CHECK(state.waitFlushed(0ms) == !close_before_notification);
        state.close(); state.flushed();
        CHECK(!state.waitFlushed(0ms));
    }
    // The callback's ComPtr, not the caller, retains a delivered sample.
    {
        MediaFoundationReadState state;
        state.reset(); state.receive(S_OK, 0, 1, sample.Get()); sample.Reset();
        CHECK(state.sample && SUCCEEDED(state.sample->SetSampleTime(123)));
        state.close(); CHECK(!state.sample);
    }
    CHECK(SUCCEEDED(MFShutdown()));
    std::cout << "MF read/flush publication, delayed/early/late completion, timeout, "
                 "restart and owned sample tests passed\n";
}
