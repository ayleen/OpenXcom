#pragma once
/*
 * RenderTarget — off-screen FBO wrapper (Phase 8b).
 *
 * bind() / unbind() save and restore the previously-bound framebuffer so
 * nesting is safe. blitTo() provides a CPU readback path for the harness.
 *
 * Instances register themselves with ShaderManager for lost-context recovery.
 */

struct SDL_Texture;

namespace OpenXcom
{
class RenderTarget
{
public:
    RenderTarget() = default;
    ~RenderTarget();

    bool create(int w, int h);
    void bind();
    void unbind();
    /* CPU readback into an SDL_Texture (harness screenshot path). */
    void blitTo(SDL_Texture* dest);

    int  width()  const { return _w; }
    int  height() const { return _h; }
    bool isValid() const { return _fbo != 0u; }

    /* Raw GL texture handle — for use as sampler input in a follow-up pass. */
    unsigned colorTexture() const { return _colorTex; }

    /* Called by ShaderManager on SDL_RENDER_TARGETS_RESET. */
    bool reupload();

private:
    unsigned _fbo      = 0u;
    unsigned _colorTex = 0u;
    int      _w        = 0;
    int      _h        = 0;
    int      _prevFbo  = 0;

    bool createInternal(int w, int h, bool registerTarget);
    void release();
};
} // namespace OpenXcom
