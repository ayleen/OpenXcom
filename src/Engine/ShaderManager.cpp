/*
 * ShaderManager.cpp (Phase 8b).
 */
#include "ShaderManager.h"
#include "Shader.h"
#include "GpuTexture.h"
#include "RenderTarget.h"
#include "Logger.h"
#include <algorithm>
#include <functional>

namespace OpenXcom
{

ShaderManager& ShaderManager::instance()
{
    static ShaderManager inst;
    return inst;
}

/* ── registration ───────────────────────────────────────────────────────── */

void ShaderManager::registerShader(Shader* s)
{
    _shaders.push_back(s);
}

void ShaderManager::unregisterShader(Shader* s)
{
    _shaders.erase(std::remove(_shaders.begin(), _shaders.end(), s), _shaders.end());
}

void ShaderManager::registerTexture(GpuTexture* t)
{
    _textures.push_back(t);
}

void ShaderManager::unregisterTexture(GpuTexture* t)
{
    _textures.erase(std::remove(_textures.begin(), _textures.end(), t), _textures.end());
}

void ShaderManager::registerTarget(RenderTarget* r)
{
    _targets.push_back(r);
}

void ShaderManager::unregisterTarget(RenderTarget* r)
{
    _targets.erase(std::remove(_targets.begin(), _targets.end(), r), _targets.end());
}

/* ── lost-context recovery ──────────────────────────────────────────────── */

void ShaderManager::registerResetCallback(std::shared_ptr<bool> alive, std::function<void()> cb)
{
    _resetCallbacks.push_back({alive, std::move(cb)});
}

void ShaderManager::reuploadAll()
{
    Log(LOG_INFO) << "ShaderManager: WebGL context restored — reuploading "
                  << _shaders.size()  << " shaders, "
                  << _textures.size() << " textures, "
                  << _targets.size()  << " render targets";

    /* Serialised order: shaders first, then textures, then FBOs. */
    for (Shader*       s : _shaders)  if (s) s->reupload();
    for (GpuTexture*   t : _textures) if (t) t->reupload();
    for (RenderTarget* r : _targets)  if (r) r->reupload();

    /* VAO/VBO reset: sweep expired entries, then call live callbacks.
     * Copy snapshot first — callbacks may re-register (e.g. Cursor::initGPU). */
    _resetCallbacks.erase(
        std::remove_if(_resetCallbacks.begin(), _resetCallbacks.end(),
                       [](const ResetEntry& e) { return e.alive.expired(); }),
        _resetCallbacks.end());
    auto snapshot = _resetCallbacks;
    for (const auto& e : snapshot)
        if (!e.alive.expired()) e.cb();
}

} // namespace OpenXcom
