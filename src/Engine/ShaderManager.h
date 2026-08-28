#pragma once
/*
 * ShaderManager — singleton registry for GPU resources (Phase 8b).
 *
 * Responsibilities:
 *  - Tracks all live Shader / GpuTexture / RenderTarget instances.
 *  - On SDL_RENDER_TARGETS_RESET (WebGL context restored after tab-suspend),
 *    calls reupload() on every registered resource.
 *  - Tracks the per-frame "had GPU pass" flag used by calypso_screenshot.
 *  - Phase 11.13: after resource reupload, fires reset callbacks for VAO/VBO
 *    recovery (VAOs are not auto-tracked and must be re-created after context loss).
 *
 * Thread safety: single-threaded (OXCE is not multi-threaded).
 */
#include <vector>
#include <functional>
#include <memory>

namespace OpenXcom
{
class Shader;
class GpuTexture;
class RenderTarget;

class ShaderManager
{
public:
    static ShaderManager& instance();

    void registerShader  (Shader*       s);
    void unregisterShader(Shader*       s);
    void registerTexture (GpuTexture*   t);
    void unregisterTexture(GpuTexture*  t);
    void registerTarget  (RenderTarget* r);
    void unregisterTarget(RenderTarget* r);

    /* Call on SDL_RENDER_TARGETS_RESET to rebuild all GPU objects. */
    bool reuploadAll();

    /* Per-frame GPU-pass tracking (used by calypso_screenshot). */
    bool hadGPUPass()  const { return _hadGPUPass; }
    void setHadGPUPass(bool v) { _hadGPUPass = v; }
    void resetFrameFlag() { _hadGPUPass = false; }

    /* Phase 11.13: VAO/VBO reset callbacks fired after resource reupload.
     * alive is the owner's alive-flag; expired entries are swept automatically. */
    struct ResetEntry { std::weak_ptr<bool> alive; std::function<void()> cb; };
    void registerResetCallback(std::shared_ptr<bool> alive, std::function<void()> cb);

private:
    ShaderManager() = default;
    ShaderManager(const ShaderManager&) = delete;
    ShaderManager& operator=(const ShaderManager&) = delete;

    std::vector<Shader*>       _shaders;
    std::vector<GpuTexture*>   _textures;
    std::vector<RenderTarget*> _targets;
    std::vector<ResetEntry>    _resetCallbacks;
    bool _hadGPUPass = false;
};
} // namespace OpenXcom
