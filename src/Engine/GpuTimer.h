#pragma once
/*
 * GpuTimer — lightweight GPU-pass wall-clock timer (Phase 8b.9).
 *
 * Uses std::chrono::steady_clock (CPU-side).  GL timer queries
 * (EXT_disjoint_timer_query_webgl2 / GL_TIME_ELAPSED) are asynchronous and
 * unavailable on Safari — CPU timing is the portable baseline.
 *
 * Usage:
 *   GpuTimer t;
 *   t.start();
 *   // GL calls
 *   t.stop();
 *   long long us = t.elapsedUs();
 */
#include <chrono>
#include <cstdint>

namespace OpenXcom
{

class GpuTimer
{
public:
    void start()
    {
        _t0 = std::chrono::steady_clock::now();
    }

    void stop()
    {
        _t1 = std::chrono::steady_clock::now();
    }

    /* Elapsed time between last start()/stop() pair, in microseconds. */
    long long elapsedUs() const
    {
        return std::chrono::duration_cast<std::chrono::microseconds>(_t1 - _t0).count();
    }

private:
    std::chrono::steady_clock::time_point _t0, _t1;
};

} // namespace OpenXcom
