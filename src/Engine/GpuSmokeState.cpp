/*
 * GpuSmokeState.cpp — GPU pipeline smoke test (Phase 8b).
 *
 * Activated by calypso_gpu_smoke_activate() from EmscriptenHarness (JS side
 * calls it via ?harness=gpu-smoke after callMain has started the game).
 *
 * Registers a persistent GPU pass with Screen.  The pass renders two quads:
 *   - Solid colour (colorquad shader, sky-blue #3399e5) in the left half.
 *   - Procedural checker+gradient texture (textured shader) in right half.
 * After 5 rendered frames it captures a GPU screenshot and becomes a no-op.
 *
 * Memory safety: the SmokePass is held by a shared_ptr captured in the lambda
 * so it remains alive as long as Screen holds the lambda (forever), but the
 * pass stops doing work after the screenshot is taken.
 */
#ifdef __EMSCRIPTEN__

#include "GpuSmokeState.h"
#include "GpuInit.h"
#include "Shader.h"
#include "GpuTexture.h"
#include "ShaderManager.h"
#include "Screen.h"
#include "Logger.h"
#include <GLES3/gl3.h>
#include <cmath>
#include <vector>
#include <memory>
#include <string>

namespace OpenXcom
{

/* ── procedural test pattern ────────────────────────────────────────────── */

static std::vector<uint8_t> makeCheckerGradient(int w, int h)
{
    std::vector<uint8_t> px((size_t)w * h * 4);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            float u = (float)x / w;
            float v = (float)y / h;
            bool checker = ((x / 32) + (y / 32)) & 1;
            uint8_t r = checker ? (uint8_t)(u * 255) : 0u;
            uint8_t g = checker ? (uint8_t)(v * 255) : (uint8_t)((u + v) * 127);
            uint8_t b = checker ? 0u : 200u;
            size_t i = ((size_t)y * w + x) * 4;
            px[i+0]=r; px[i+1]=g; px[i+2]=b; px[i+3]=255u;
        }
    return px;
}

/* ── quad VAO ───────────────────────────────────────────────────────────── */

static GLuint makeQuadVAO(float x0, float y0, float x1, float y1)
{
    float verts[] = {
        x0,y0, 0.f,0.f,  x1,y0, 1.f,0.f,  x0,y1, 0.f,1.f,
        x0,y1, 0.f,1.f,  x1,y0, 1.f,0.f,  x1,y1, 1.f,1.f,
    };
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    return vao;
}

/* ── GL state save/restore ──────────────────────────────────────────────── */

struct GlSave
{
    GLint prog, vao; GLboolean blend, depth;
    void save()
    {
        glGetIntegerv(GL_CURRENT_PROGRAM,      &prog);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
        blend = glIsEnabled(GL_BLEND);
        depth = glIsEnabled(GL_DEPTH_TEST);
    }
    void restore()
    {
        glUseProgram((GLuint)prog);
        glBindVertexArray((GLuint)vao);
        if (blend) glEnable(GL_BLEND);    else glDisable(GL_BLEND);
        if (depth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    }
};

/* ── SmokePass ──────────────────────────────────────────────────────────── */

struct SmokePass
{
    Shader     solidShader;
    Shader     texShader;
    GpuTexture testTex;
    GLuint     solidVAO = 0u;
    GLuint     texVAO   = 0u;
    bool       ready    = false;
    int        frames   = 0;
    int        maxFrames;
    Screen*    screen;
    std::string outPath;

    SmokePass(Screen* s, const std::string& p, int mf)
        : testTex(false), screen(s), outPath(p), maxFrames(mf) {}

    bool init()
    {
        if (!solidShader.loadFromEmbedded("colorquad") ||
            !texShader.loadFromEmbedded("textured"))
        {
            Log(LOG_ERROR) << "GpuSmoke: shader compile failed";
            return false;
        }

        auto pixels = makeCheckerGradient(256, 256);
        testTex.uploadRGBA(pixels.data(), 256, 256);

        solidVAO = makeQuadVAO(-1.f, 0.5f, 0.f, 1.f); /* left half, top */
        texVAO   = makeQuadVAO( 0.f, 0.5f, 1.f, 1.f); /* right half, top */

        ready = true;
        Log(LOG_INFO) << "GpuSmoke: initialised";
        return true;
    }

    /* Called every frame from Screen::flip() via the registered GPU pass. */
    void renderFrame()
    {
        if (!ready || frames >= maxFrames) return;

        GlSave st; st.save();
        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);

        solidShader.use();
        solidShader.setUniform4f("u_color", 0.2f, 0.6f, 0.9f, 1.0f);
        glBindVertexArray(solidVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        texShader.use();
        testTex.bind(0);
        texShader.setUniform1i("u_tex", 0);
        glBindVertexArray(texVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        ShaderManager::instance().setHadGPUPass(true);
        st.restore();

        ++frames;
        if (frames == maxFrames)
        {
            screen->screenshotGPU(outPath);
            Log(LOG_INFO) << "GpuSmoke: screenshot saved to " << outPath;
        }
    }
};

/* ── public API ─────────────────────────────────────────────────────────── */

void GpuSmokeState::activate(Screen* screen, const std::string& outPath)
{
    if (!GpuInit::ready())
    {
        Log(LOG_WARNING) << "GpuSmoke: GPU pipeline not ready";
        return;
    }

    /* Shared ownership: the lambda captures the pass; it stays alive
     * indefinitely (no Screen::removeGPUPass yet), but becomes a no-op
     * after maxFrames. */
    auto pass = std::make_shared<SmokePass>(screen, outPath, 5);
    if (!pass->init()) return;

    screen->registerGPUPass([pass]() { pass->renderFrame(); });
    Log(LOG_INFO) << "GpuSmoke: registered — will screenshot after 5 frames to " << outPath;
}

} // namespace OpenXcom

#endif /* __EMSCRIPTEN__ */
