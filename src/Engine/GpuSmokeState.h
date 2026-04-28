#pragma once
/*
 * GpuSmokeState — developer-only GPU pipeline smoke-test (Phase 8b).
 *
 * Activated via ?harness=gpu-smoke.  Renders:
 *   1. A solid-colour quad (colorquad.frag, u_color = #3399e5).
 *   2. A textured quad using a 256x256 procedural checker+gradient pattern.
 * After 5 frames it calls screenshotGPU and saves gpu-smoke.png to MEMFS.
 *
 * Not a proper OXCE State — it registers a one-shot GPU pass with Screen
 * and schedules a screenshot via emscripten_set_timeout.
 */
#ifdef __EMSCRIPTEN__
#include <string>

namespace OpenXcom
{
class Screen;

struct GpuSmokeState
{
    static void activate(Screen* screen, const std::string& outPath = "/tmp/gpu-smoke.png");
};
} // namespace OpenXcom

#endif /* __EMSCRIPTEN__ */
