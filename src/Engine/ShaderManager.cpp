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
#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#endif

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
    if (!r) return;
    if (std::find(_targets.begin(), _targets.end(), r) == _targets.end())
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

bool ShaderManager::reuploadAll()
{
    Log(LOG_INFO) << "ShaderManager: WebGL context restored — reuploading "
                  << _shaders.size()  << " shaders, "
                  << _textures.size() << " textures, "
                  << _targets.size()  << " render targets";

    /* Serialised order: shaders first, then textures, then FBOs. */
    bool ok = true;
    for (Shader*       s : _shaders)  if (s) ok = s->reupload() && ok;
    for (GpuTexture*   t : _textures) if (t) ok = t->reupload() && ok;
    for (RenderTarget* r : _targets)  if (r) ok = r->reupload() && ok;

    /* VAO/VBO reset: sweep expired entries, then call live callbacks.
     * Copy snapshot first — callbacks may re-register (e.g. Cursor::initGPU). */
    _resetCallbacks.erase(
        std::remove_if(_resetCallbacks.begin(), _resetCallbacks.end(),
                       [](const ResetEntry& e) { return e.alive.expired(); }),
        _resetCallbacks.end());
    auto snapshot = _resetCallbacks;
    for (const auto& e : snapshot)
        if (!e.alive.expired()) e.cb();
#ifdef __EMSCRIPTEN__
    if (glGetError() != GL_NO_ERROR) ok = false;
#endif
    if (!ok) Log(LOG_ERROR) << "ShaderManager: context restore resource verification failed";
    return ok;
}

} // namespace OpenXcom
