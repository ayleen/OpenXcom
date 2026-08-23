/*
 * Regression-test harness entry-points exported to JavaScript.
 * Phase 6a.1 — screenshot capture for snapshot diffing.
 * Phase 8b   — GPU framebuffer screenshot; ShaderManager hadGPUPass auto-route.
 *
 * calypso_screenshot(path) — writes a PNG of the current frame to `path`
 *   inside the Emscripten virtual filesystem; JS reads it back via
 *   Module.FS.readFile(path).  Auto-routes to GPU readback when the last
 *   frame had any registered GPU shader pass.
 * calypso_screenshot_gpu(path) — always uses GPU framebuffer readback.
 *
 * The global `game` pointer is declared in main.cpp (global namespace).
 * Game and Screen are included directly so the call chain resolves at
 * compile time without forward-declaration tricks.
 */
#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include <emscripten/heap.h>
#include <malloc.h>
#include <SDL.h>
#include <SDL_mixer.h>
#include <cstring>
#include <cmath>
#include <fstream>
#include <sstream>
#include <array>
#include <vector>
#include <algorithm>
#include "../Engine/Action.h"
#include "../Engine/Game.h"
#include "../Engine/Screen.h"
#include "../Engine/Surface.h"
#include "../Engine/SurfaceSet.h"
#include "../Engine/Options.h"
#include "../Engine/ShaderManager.h"
#include "../Engine/GpuTexture.h"
#include "../Engine/GpuInit.h"
#include "../Engine/Shader.h"
#include "../Engine/GpuSmokeState.h"
#ifdef CALYPSO_HD_UNIT_SPIKE
#include "HdUnitSpikeState.h"
#include "HdUnitBattleSpike.h"
#endif
#include "../Engine/Logger.h"
#include "../Engine/FileMap.h"
#include "../Mod/Mod.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/SavedBattleGame.h"
#include "../Battlescape/UnitSprite.h"
#include "HdUnitEmit.h"
#include "HdUnitRenderPlan.h"
#include "../Interface/Cursor.h"
// Phase 33 (mobile): pinch-zoom bridge + virtual-keyboard bridge for TextEdit.
#include "../Interface/TextEdit.h"
#include "../Battlescape/BattlescapeState.h"
// HTML main-menu bridge (Phase 2): the JS overlay drives these to push the same
// OXCE states the vanilla MainMenuState buttons would.
#include "../Menu/NewGameState.h"
#include "../Menu/NewBattleState.h"
#include "../Menu/ListLoadState.h"
#include "../Menu/ModListState.h"
#include "../Menu/OptionsVideoState.h"
#include "../Menu/OptionsBaseState.h"   // OptionsOrigin / OPT_MENU
#include "CalypsoPrologueCampaign.h" // Phase 41 (commit 4.5): launchScriptedBattle
#include <GLES3/gl3.h>

using namespace OpenXcom;

/* Phase 33 (mobile): the currently-focused TextEdit, set by TextEdit::setFocus
 * (see Interface/TextEdit.cpp). Declared OUTSIDE the extern "C" block below so
 * it keeps C++ language linkage and resolves to the namespaced definition
 * (_ZN8OpenXcom24g_calypsoFocusedTextEditE) — declaring it inside extern "C"
 * would give it C linkage and fail to link. The JS text-set bridge writes
 * through it. */
namespace OpenXcom { extern TextEdit *g_calypsoFocusedTextEdit; }
extern "C" int calypso_viewport_input_blocked(void);

/* ---- M5: heap-stats primitives -----------------------------------------------
 * mallinfo() fields are signed int — cast through unsigned to avoid negative
 * wrap when the dlmalloc arena grows past 2 GB (ALLOW_MEMORY_GROWTH). */
static size_t s_heapPrevUsed = 0;

static size_t heapUsedBytes()
{
    struct mallinfo mi = mallinfo();
    return (size_t)(unsigned int)mi.uordblks;
}

struct E1GpuEdgeSample
{
    int alpha = 0;
    std::array<unsigned char, 4> naturalCenter{};
    std::array<unsigned char, 4> reversedCenter{};
    std::array<unsigned char, 4> naturalEdge{};
    std::array<unsigned char, 4> naturalOutside{};
};

struct E1GpuEdgeProof
{
    bool available = false;
    bool passed = false;
    unsigned glError = 0;
    std::vector<E1GpuEdgeSample> samples;
    std::array<unsigned char, 4> clippedInside{};
    std::array<unsigned char, 4> clippedAdjacent{};
    std::array<unsigned char, 4> clippedOutside{};
    std::array<unsigned char, 4> foregroundOccluded{};
    bool tileCrossingEmit = false;
};

static E1GpuEdgeProof runE1GpuEdgeProof()
{
    E1GpuEdgeProof result;
    if (!GpuInit::ready()) return result;

    GLint prevFbo = 0, prevRenderbuffer = 0, prevProgram = 0, prevVao = 0, prevArrayBuffer = 0;
    GLint prevViewport[4] = {0, 0, 0, 0}, prevScissorBox[4] = {0, 0, 0, 0};
    GLint prevDepthFunc = GL_LESS, prevBlendSrcRgb = GL_ONE, prevBlendDstRgb = GL_ZERO;
    GLint prevBlendSrcAlpha = GL_ONE, prevBlendDstAlpha = GL_ZERO, prevActiveTexture = GL_TEXTURE0;
    GLboolean prevDepthMask = GL_TRUE, prevColorMask[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
    GLfloat prevClearColor[4] = {0, 0, 0, 0};
    const GLboolean prevBlend = glIsEnabled(GL_BLEND);
    const GLboolean prevDepth = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean prevScissor = glIsEnabled(GL_SCISSOR_TEST);
    const GLboolean prevCull = glIsEnabled(GL_CULL_FACE);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &prevRenderbuffer);
    glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prevArrayBuffer);
    glGetIntegerv(GL_VIEWPORT, prevViewport);
    glGetIntegerv(GL_SCISSOR_BOX, prevScissorBox);
    glGetIntegerv(GL_DEPTH_FUNC, &prevDepthFunc);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &prevDepthMask);
    glGetBooleanv(GL_COLOR_WRITEMASK, prevColorMask);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, prevClearColor);
    glGetIntegerv(GL_BLEND_SRC_RGB, &prevBlendSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &prevBlendDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &prevBlendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &prevBlendDstAlpha);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &prevActiveTexture);
    const int units[] = {0, 1, 3, 6};
    GLint prevTex[4] = {0, 0, 0, 0};
    for (int i = 0; i < 4; ++i)
    {
        glActiveTexture(GL_TEXTURE0 + units[i]);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex[i]);
    }
    auto restoreState = [&]() {
        for (int i = 0; i < 4; ++i)
        {
            glActiveTexture(GL_TEXTURE0 + units[i]);
            glBindTexture(GL_TEXTURE_2D, (GLuint)prevTex[i]);
        }
        glActiveTexture((GLenum)prevActiveTexture);
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prevFbo);
        glBindRenderbuffer(GL_RENDERBUFFER, (GLuint)prevRenderbuffer);
        glUseProgram((GLuint)prevProgram);
        glBindVertexArray((GLuint)prevVao);
        glBindBuffer(GL_ARRAY_BUFFER, (GLuint)prevArrayBuffer);
        glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
        if (prevBlend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
        if (prevDepth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        glScissor(prevScissorBox[0], prevScissorBox[1],
                  prevScissorBox[2], prevScissorBox[3]);
        if (prevScissor) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
        if (prevCull) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
        glBlendFuncSeparate((GLenum)prevBlendSrcRgb, (GLenum)prevBlendDstRgb,
                            (GLenum)prevBlendSrcAlpha, (GLenum)prevBlendDstAlpha);
        glDepthFunc((GLenum)prevDepthFunc);
        glDepthMask(prevDepthMask);
        glColorMask(prevColorMask[0], prevColorMask[1], prevColorMask[2], prevColorMask[3]);
        glClearColor(prevClearColor[0], prevClearColor[1], prevClearColor[2], prevClearColor[3]);
    };
    while (glGetError() != GL_NO_ERROR) {}

    Shader r8Shader, rgbaShader;
    if (!r8Shader.loadFromEmbedded("tile_atlas")
     || !rgbaShader.loadFromEmbedded("tile_atlas_rgba"))
    {
        restoreState();
        return result;
    }

    GpuTexture r8Atlas(false, GpuTexture::Wrap::ClampToEdge, GpuTexture::Filter::Nearest);
    GpuTexture rgbaAtlas(false, GpuTexture::Wrap::ClampToEdge, GpuTexture::Filter::Linear);
    GpuTexture shadeTable(false, GpuTexture::Wrap::ClampToEdge, GpuTexture::Filter::Nearest);
    GpuTexture shadeCurve(false, GpuTexture::Wrap::ClampToEdge, GpuTexture::Filter::Nearest);
    const unsigned char r8Pixel = 1;
    std::vector<unsigned char> shades(16u * 256u * 4u, 0u);
    for (int shade = 0; shade < 16; ++shade)
    {
        const size_t off = ((size_t)1 * 16u + (size_t)shade) * 4u;
        shades[off + 1] = 255u; shades[off + 3] = 255u; // index 1 -> opaque green
    }
    std::vector<unsigned char> curve(16u, 255u);
    if (!r8Atlas.uploadR8(&r8Pixel, 1, 1)
     || !shadeTable.uploadRGBA(shades.data(), 16, 256)
     || !shadeCurve.uploadR8(curve.data(), 16, 1))
    {
        restoreState();
        return result;
    }

    GLuint fbo = 0, color = 0, depth = 0, vao = 0, cornerVbo = 0, instanceVbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glGenTextures(1, &color);
    glBindTexture(GL_TEXTURE_2D, color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 16, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
    glGenRenderbuffers(1, &depth);
    glBindRenderbuffer(GL_RENDERBUFFER, depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 16, 8);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        glDeleteRenderbuffers(1, &depth);
        glDeleteTextures(1, &color);
        glDeleteFramebuffers(1, &fbo);
        restoreState();
        return result;
    }

    const float corners[12] = {0,0, 1,0, 0,1, 0,1, 1,0, 1,1};
    const float baselineInstance[12] = {4,2, 0,0, 0,1,1, 0.20f, 0,0,1,1};
    const float overlayInstance[12]  = {4,2, 0,0, 0,1,1, 0.25f, 0,0,1,1};
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &cornerVbo);
    glBindBuffer(GL_ARRAY_BUFFER, cornerVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(corners), corners, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glGenBuffers(1, &instanceVbo);
    glBindBuffer(GL_ARRAY_BUFFER, instanceVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(baselineInstance), baselineInstance, GL_DYNAMIC_DRAW);
    const GLsizei stride = 12 * (GLsizei)sizeof(float);
    for (int attr = 1; attr <= 6; ++attr)
    {
        const int components = attr <= 2 ? 2 : 1;
        const int floatOffset = attr == 1 ? 0 : attr == 2 ? 2 : attr + 1;
        glEnableVertexAttribArray((GLuint)attr);
        glVertexAttribPointer((GLuint)attr, components, GL_FLOAT, GL_FALSE, stride,
                              (const void*)((size_t)floatOffset * sizeof(float)));
        glVertexAttribDivisor((GLuint)attr, 1);
    }
    glEnableVertexAttribArray(7);
    glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, stride,
                          (const void*)(8u * sizeof(float)));
    glVertexAttribDivisor(7, 1);

    auto configureCommon = [](Shader& shader) {
        shader.setUniform2f("u_screenSize", 16.0f, 8.0f);
        shader.setUniform2f("u_tilePixelSize", 8.0f, 4.0f);
        shader.setUniform2f("u_tileUVSize", 1.0f, 1.0f);
        shader.setUniform1f("u_animFrame", 0.0f);
        shader.setUniform1i("u_atlas", 0);
    };
    auto drawBaseline = [&](const void* instance) {
        r8Shader.use(); configureCommon(r8Shader);
        r8Shader.setUniform1i("u_shadeTable", 1);
        r8Shader.setUniform1f("u_unitShade", 0.0f);
        r8Shader.setUniform1i("u_hasHdMask", 1);
        r8Shader.setUniform1i("u_hdMask", 6);
        r8Shader.setUniform4f("u_hdMaskUv", 0, 0, 1, 1);
        r8Atlas.bind(0); shadeTable.bind(1); rgbaAtlas.bind(6);
        glBindBuffer(GL_ARRAY_BUFFER, instanceVbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(baselineInstance), instance);
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, 1);
    };
    auto drawOverlay = [&](const void* instance) {
        rgbaShader.use(); configureCommon(rgbaShader);
        rgbaShader.setUniform1i("u_shadeCurve", 3);
        rgbaShader.setUniform1i("u_hasNormalMap", 0);
        rgbaShader.setUniform1i("u_hasEmissive", 0);
        rgbaShader.setUniform1i("u_unitGeometry", 1);
        rgbaAtlas.bind(0); shadeCurve.bind(3);
        glBindBuffer(GL_ARRAY_BUFFER, instanceVbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(overlayInstance), instance);
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, 1);
    };
    auto drawForeground = [&](const void* instance) {
        r8Shader.use(); configureCommon(r8Shader);
        r8Shader.setUniform1i("u_shadeTable", 1);
        r8Shader.setUniform1f("u_unitShade", 0.0f);
        r8Shader.setUniform1i("u_hasHdMask", 0);
        r8Atlas.bind(0); shadeTable.bind(1);
        glBindBuffer(GL_ARRAY_BUFFER, instanceVbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(baselineInstance), instance);
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, 1);
    };

    glViewport(0, 0, 16, 8);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    // Straight-alpha source-over: RGB is weighted by source alpha, while alpha
    // itself uses the Porter-Duff source-over equation (srcA + dstA*(1-srcA)).
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                        GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    const int alphas[] = {0, 2, 3, 64, 128, 191};
    for (int alpha : alphas)
    {
        E1GpuEdgeSample sample; sample.alpha = alpha;
        const unsigned char rgba[4] = {255u, 0u, 0u, (unsigned char)alpha};
        rgbaAtlas.uploadRGBA(rgba, 1, 1);
        for (int reversed = 0; reversed < 2; ++reversed)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glClearColor(0, 0, 1, 1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            struct Cmd { float iso; int kind; } cmds[2] = {{0.20f, 0}, {0.25f, 1}};
            if (reversed) std::swap(cmds[0], cmds[1]);
            std::stable_sort(cmds, cmds + 2, [](const Cmd& a, const Cmd& b) {
                return UnitSprite::e1PainterOrderLess(a.iso, b.iso);
            });
            for (const Cmd& cmd : cmds)
            {
                if (cmd.kind == 0) drawBaseline(baselineInstance);
                else drawOverlay(overlayInstance);
            }
            std::array<unsigned char, 4> center{}, edge{}, outside{};
            glReadPixels(8, 4, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, center.data());
            glReadPixels(3, 4, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, edge.data());
            glReadPixels(2, 4, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, outside.data());
            if (!reversed)
            {
                sample.naturalCenter = center;
                sample.naturalEdge = edge;
                sample.naturalOutside = outside;
            }
            else sample.reversedCenter = center;
        }
        result.samples.push_back(sample);
    }

    // Exercise the production UnitSprite -> Calypso emit seam, not a hand-authored
    // clip attribute. A walking unit is submitted once for each tile it crosses;
    // two complementary GraphSubsets must produce two clipped siblings while a
    // third, fully disjoint tile is consumed without emitting colour or depth.
    const unsigned char clipRgba[4] = {255u, 0u, 0u, 128u};
    const bool clipTextureUploaded = rgbaAtlas.uploadRGBA(clipRgba, 1, 1);
    HdUnitAtlasSpec emitSpec;
    emitSpec.atlas = &r8Atlas;
    emitSpec.atlasW = emitSpec.atlasH = 1;
    emitSpec.tileWidth = emitSpec.tileHeight = 1;
    emitSpec.columns = 1;
    emitSpec.rgbaFormat = HdUnitAtlasSpec::RgbaOverlayFormat::RgbaOverlay;
    emitSpec.frameWidth = emitSpec.frameHeight = 1;
    emitSpec.rgbaColumns = 1;
    emitSpec.rgbaOverlayPages = {&rgbaAtlas};
    emitSpec.rgbaHasHd = {1};
    emitSpec.rgbaPageOf = {0};
    emitSpec.rgbaFramesPerPage = emitSpec.rgbaRowsPerPage = 1;
    emitSpec.rgbaPageW = emitSpec.rgbaPageH = 1;
    std::vector<HdTileInstance> emittedBaselines;
    std::vector<std::vector<HdRgbaOverlayInstance>> emittedOverlays(1);
    HdUnitEmitTargets emitTargets;
    emitTargets.bodyInstances = &emittedBaselines;
    emitTargets.bodySpec = &emitSpec;
    emitTargets.rgbaOverlayBodyPages = &emittedOverlays;
    emitTargets.renderWidth = 8;
    emitTargets.renderHeight = 4;
    auto emitTilePart = [&](int maskBegX, int maskEndX) {
        HdUnitEmitState state;
        setHdUnitEmitTargets(state, emitTargets, 1);
        return emitHdUnitPart(state, HdUnitPartKind::Body, 0, 0, 0, true,
                              4, 2, 0, maskBegX, maskEndX, 2, 6, 7, 2);
    };
    const bool emitsConsumed = emitTilePart(4, 8)
                            && emitTilePart(8, 12)
                            && emitTilePart(12, 16);
    result.tileCrossingEmit = clipTextureUploaded && emitsConsumed
        && emittedBaselines.size() == 2 && emittedOverlays[0].size() == 2
        && emittedOverlays[0][0].baselineIndex == 0
        && emittedOverlays[0][1].baselineIndex == 1
        && std::abs(emittedBaselines[0].clipX) < 0.0001f
        && std::abs(emittedBaselines[0].clipW - 0.5f) < 0.0001f
        && std::abs(emittedBaselines[1].clipX - 0.5f) < 0.0001f
        && std::abs(emittedBaselines[1].clipW - 0.5f) < 0.0001f;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (result.tileCrossingEmit)
    for (size_t i = 0; i < emittedBaselines.size(); ++i)
    {
        drawBaseline(&emittedBaselines[i]);
        drawOverlay(&emittedOverlays[0][i].instance);
    }
    glReadPixels(6, 4, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, result.clippedInside.data());
    glReadPixels(10, 4, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, result.clippedAdjacent.data());
    glReadPixels(3, 4, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, result.clippedOutside.data());

    // Production-equivalent depth proof: a priority-6 foreground wall/object
    // first owns depth, then the priority-4.125 fractional RGBA unit colour
    // replay runs under GL_LESS with depth writes off. The pixel must remain
    // the opaque green foreground, not turn into a red/green blend.
    const float foregroundIso = 6.0f / HdUnitRenderPlan::kIsoDivisor;
    const float unitIso = 4.125f / HdUnitRenderPlan::kIsoDivisor;
    const float foregroundInstance[12] = {
        4,2, 0,0, 0,1,1, foregroundIso, 0,0,1,1};
    HdTileInstance occludedUnitInstance = emittedOverlays[0].empty()
        ? HdTileInstance{4,2, 0,0, 0,1,1, unitIso, 0,0,1,1}
        : emittedOverlays[0][0].instance;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glDepthMask(GL_TRUE);
    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    drawForeground(foregroundInstance);
    glDepthMask(GL_FALSE);
    drawOverlay(&occludedUnitInstance);
    glReadPixels(6, 4, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                 result.foregroundOccluded.data());
    glDisable(GL_DEPTH_TEST);
    result.glError = (unsigned)glGetError();
    result.available = true;
    result.passed = result.glError == GL_NO_ERROR;
    const std::array<unsigned char, 4> blue = {0,0,255,255};
    const std::array<unsigned char, 4> green = {0,255,0,255};
    auto close = [](const std::array<unsigned char,4>& a,
                    const std::array<unsigned char,4>& b) {
        for (int i = 0; i < 4; ++i)
            if (((int)a[i] > (int)b[i] ? (int)a[i] - (int)b[i]
                                        : (int)b[i] - (int)a[i]) > 1) return false;
        return true;
    };
    for (const E1GpuEdgeSample& sample : result.samples)
    {
        // An authored RGBA frame replaces its complete R8 part. Transparent
        // texels reveal the already-painted scene rather than the old suit.
        std::array<unsigned char, 4> expected = sample.alpha < 3
            ? blue : std::array<unsigned char, 4>{(unsigned char)sample.alpha, 0,
                (unsigned char)(255 - sample.alpha), 255};
        result.passed = result.passed
            && close(sample.naturalCenter, expected)
            && close(sample.reversedCenter, expected)
            && close(sample.naturalEdge, expected)
            && close(sample.naturalOutside, blue);
    }
    result.passed = result.passed
        && result.tileCrossingEmit
        && close(result.clippedInside, {128,0,127,255})
        && close(result.clippedAdjacent, {128,0,127,255})
        && close(result.clippedOutside, blue)
        && close(result.foregroundOccluded, green);

    glDeleteBuffers(1, &instanceVbo);
    glDeleteBuffers(1, &cornerVbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteRenderbuffers(1, &depth);
    glDeleteTextures(1, &color);
    glDeleteFramebuffers(1, &fbo);
    restoreState();
    return result;
}

extern "C" {

/* Log one [HEAP] line: total / used / free in MB (1 decimal) + delta vs the
 * previous calypso_log_heap() call (first call's baseline is 0). */
EMSCRIPTEN_KEEPALIVE
void calypso_log_heap(const char *tag)
{
    const size_t MiB = 1048576;
    size_t total  = (size_t)emscripten_get_heap_size();
    size_t used   = heapUsedBytes();
    size_t free_  = total > used ? total - used : 0;
    long long delta  = (long long)used - (long long)s_heapPrevUsed;
    s_heapPrevUsed   = used;
    size_t adelta = (size_t)(delta >= 0 ? delta : -delta);
    char   sign   = delta >= 0 ? '+' : '-';
    auto   mb     = [MiB](size_t b) { return (long long)(b / MiB); };
    auto   mb1    = [MiB](size_t b) { return (long long)((b % MiB) * 10 / MiB); };
    Log(LOG_INFO) << "[HEAP] " << tag
                  << ": total=" << mb(total)  << "." << mb1(total)  << "MB"
                  << " used="   << mb(used)   << "." << mb1(used)   << "MB"
                  << " free="   << mb(free_)  << "." << mb1(free_)  << "MB"
                  << " delta="  << sign       << mb(adelta) << "." << mb1(adelta) << "MB";
}

/* Return heap utilisation / total linear-memory size in bytes as double for
 * JS-side polling: Module.ccall('calypso_heap_used', 'number', [], []). */
EMSCRIPTEN_KEEPALIVE
double calypso_heap_used(void)
{
    return (double)heapUsedBytes();
}

EMSCRIPTEN_KEEPALIVE
double calypso_heap_total(void)
{
    return (double)(size_t)emscripten_get_heap_size();
}

EMSCRIPTEN_KEEPALIVE
void calypso_screenshot(const char *path)
{
	OpenXcom::Game *g = OpenXcom::getCurrentGame();
	if (!g || !g->getScreen()) return;

	/* Auto-route to GPU framebuffer readback when a GPU pass ran this frame.
	 * Block 11.12: log the route at [INFO] so the regression harness can assert
	 * that Battlescape scenarios always capture via GPU. */
	if (OpenXcom::ShaderManager::instance().hadGPUPass())
	{
		Log(LOG_INFO) << "screenshot via GPU readback";
		g->getScreen()->screenshotGPU(path);
	}
	else
	{
		Log(LOG_INFO) << "screenshot via CPU surface";
		g->getScreen()->screenshot(path);
	}
}

/* Always read back from the GPU framebuffer, regardless of GPU-pass flag. */
EMSCRIPTEN_KEEPALIVE
void calypso_screenshot_gpu(const char *path)
{
	OpenXcom::Game *g = OpenXcom::getCurrentGame();
	if (g && g->getScreen())
		g->getScreen()->screenshotGPU(path);
}

/* ---- Semantic capture readiness (phase-test-infra-mainmenu-gpl) -------------
 * Harness-only observation of real presentations for regression scenarios that
 * opt into readyWhen semantic readiness. Game::iterate() calls
 * calypso_harness_note_presented_frame() only when Screen::flip() returned
 * true — i.e. iff that central Game iteration actually reached
 * SDL_RenderPresent / SDL_GL_SwapWindow (see Engine/Game.cpp). Skipped presents
 * (WebGL context loss, HD-overlay gate, missing GL buffer surface) never
 * advance the serial; State::think(), State::blit(), browser rAF ticks and
 * wall-clock time never do either. The counter is inactive by default:
 * ordinary browser play stays on the no-observation path (the note call
 * returns immediately).
 * Page-local, single-threaded, test-only; never persisted or saved.
 * Calls before Game construction are valid — no Game* dependency. */
static double g_calypsoPresentedFrameSerial = 0.0;
static bool g_calypsoPresentedFramesArmed = false;

void calypso_harness_note_presented_frame()
{
	if (!g_calypsoPresentedFramesArmed) return;
	g_calypsoPresentedFrameSerial += 1.0;
}

/* Arm: reset the serial to zero and activate observation. Returns 1. */
EMSCRIPTEN_KEEPALIVE
int calypso_harness_arm_presented_frames()
{
	g_calypsoPresentedFrameSerial = 0.0;
	g_calypsoPresentedFramesArmed = true;
	return 1;
}

/* Disarm: deactivate observation. Idempotent cleanup; returns 1. The serial
 * value is left untouched so a late diagnostic query still sees the last run. */
EMSCRIPTEN_KEEPALIVE
int calypso_harness_disarm_presented_frames()
{
	g_calypsoPresentedFramesArmed = false;
	return 1;
}

/* Query the exact non-negative integer serial. A double keeps every integer
 * value exactly representable from JavaScript without an i64/BigInt ABI. */
EMSCRIPTEN_KEEPALIVE
double calypso_harness_presented_frame_serial()
{
	return g_calypsoPresentedFrameSerial;
}

/* Activate the GPU smoke-test scenario (Phase 8b — ?harness=gpu-smoke).
 * Registers a shader pass with Screen that renders for 5 frames then
 * saves a PNG to `path`.  Requires callMain to have been invoked first. */
EMSCRIPTEN_KEEPALIVE
void calypso_gpu_smoke_activate(const char *path)
{
	OpenXcom::Game *g = OpenXcom::getCurrentGame();
	if (!g || !g->getScreen())
	{
		/* Log to stderr so JS can detect the failure. */
		EM_ASM({ console.error('calypso_gpu_smoke_activate: game not running'); });
		return;
	}
	OpenXcom::GpuSmokeState::activate(g->getScreen(), path ? path : "/tmp/gpu-smoke.png");
}

/* Phase 42 E1 regression: exercise the production R8/RGBA unit shaders with
 * fractional alpha, a moving GraphSubset clip and a foreground depth owner.
 * The GPL gpu-smoke browser scenario calls this in normal (non-spike) builds,
 * so the __EMSCRIPTEN__ path is a CI regression rather than diagnostic-only
 * JSON hidden behind CALYPSO_HD_UNIT_SPIKE. */
EMSCRIPTEN_KEEPALIVE
int calypso_hd_unit_e1_gpu_probe()
{
	const E1GpuEdgeProof proof = runE1GpuEdgeProof();
	if (!proof.available || !proof.passed)
	{
		Log(LOG_ERROR) << "Phase 42 E1 GPU probe failed: available="
		               << (proof.available ? "true" : "false")
		               << " glError=" << proof.glError;
		return 0;
	}
	return 1;
}

#ifdef CALYPSO_HD_UNIT_SPIKE
/* Disposable Phase-42 G0 synthetic HD-unit diagnostic. The PNG must already
 * exist in MEMFS and contain the documented 4x3 body/RH/LH/sentinel atlas.
 * Returns 1 only after decode, shader compile, upload and forced reload have
 * succeeded and the three-frame capture pass has been registered. */
EMSCRIPTEN_KEEPALIVE
int calypso_hd_unit_spike_activate(const char *assetPath, const char *outPath,
	                              const char *metricsPath)
{
	OpenXcom::Game *g = OpenXcom::getCurrentGame();
	if (!g || !g->getScreen())
	{
		EM_ASM({ console.error('calypso_hd_unit_spike_activate: game not running'); });
		return 0;
	}
	return OpenXcom::HdUnitSpikeState::activate(
		g->getScreen(),
		assetPath ? assetPath : "/tmp/hd-unit-atlas.png",
		outPath ? outPath : "/tmp/hd-unit-spike.png",
		metricsPath ? metricsPath : "/tmp/hd-unit-spike.json") ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
int calypso_hd_unit_battle_g0_activate(const char *assetPath, const char *metricsPath)
{
	Game *g = getCurrentGame();
	return g && HdUnitBattleSpike::activate(g, assetPath, metricsPath) ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
int calypso_hd_unit_battle_g0_select(int unitId, int activeHand, int aiming, int center)
{
	Game *g = getCurrentGame();
	return g ? HdUnitBattleSpike::select(g, unitId, activeHand, aiming, center != 0) : 0;
}

EMSCRIPTEN_KEEPALIVE
int calypso_hd_unit_battle_g0_find_occluder_target(int unitId, const char *outJson)
{
	Game *g = getCurrentGame();
	return g && HdUnitBattleSpike::findOccluderTarget(g, unitId, outJson) ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
int calypso_hd_unit_battle_g0_find_walk_target(int unitId, const char *outJson)
{
	Game *g = getCurrentGame();
	return g && HdUnitBattleSpike::findWalkTarget(g, unitId, outJson) ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
int calypso_hd_unit_battle_g0_inspect_input(const char *outJson)
{
	Game *g = getCurrentGame();
	return g && HdUnitBattleSpike::inspectInput(g, outJson) ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
int calypso_hd_unit_battle_g0_checkpoint(const char *label)
{
	return HdUnitBattleSpike::checkpoint(label) ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
int calypso_hd_unit_battle_g0_finish(const char *outPng)
{
	Game *g = getCurrentGame();
	return g && HdUnitBattleSpike::finish(g, outPng) ? 1 : 0;
}

/* ---- Phase 42 E1: production unit-atlas overlay probe -----------------------
 * Writes a JSON snapshot of ONE unit sprite atlas (R8 baseline + sparse
 * per-PCK-frame RGBA overlay state) to `outJsonPath`. `sheet` is the SurfaceSet
 * name (e.g. "TDXCOM_0.PCK"). Lets the JS harness verify — without a screenshot
 * — that the `unitAtlas:` ruleset key built the expected pages, frame
 * dimensions and per-frame hasHd coverage, and that the GpuTexture skip-cache
 * invariant holds (debugCachedBytes()==0). Returns 1 when the file was written,
 * 0 on failure (no game / unknown sheet / bad path). */
EMSCRIPTEN_KEEPALIVE
int calypso_unit_atlas_probe(const char *sheet, const char *outJsonPath)
{
	Game *g = getCurrentGame();
	if (!g || !g->getMod() || !sheet || !outJsonPath) return 0;
	const Mod::UnitAtlasSpec *spec = g->getMod()->getUnitAtlas(sheet);
	std::ostringstream o;
	o << "{\"sheet\":\"" << sheet << "\"";
	o << ",\"hasAtlas\":" << (spec && spec->atlas ? "true" : "false");
	if (spec)
	{
		o << ",\"r8\":{\"w\":" << spec->atlasW << ",\"h\":" << spec->atlasH
		  << ",\"tileW\":" << spec->tileWidth << ",\"tileH\":" << spec->tileHeight
		  << ",\"columns\":" << spec->columns << "}";
		o << ",\"rgba\":{\"format\":\""
		  << (spec->rgbaFormat == Mod::UnitAtlasSpec::RgbaOverlayFormat::RgbaOverlay ? "rgba-overlay" : "none")
		  << "\",\"hasOverlay\":" << (spec->hasRgbaOverlay() ? "true" : "false")
		  << ",\"frameW\":" << spec->frameWidth << ",\"frameH\":" << spec->frameHeight
		  << ",\"columns\":" << spec->rgbaColumns << ",\"maxPageSize\":" << spec->maxPageSize
		  << ",\"pages\":" << spec->pages.size()
		  << ",\"loadedPages\":" << spec->rgbaOverlayPages.size()
		  << ",\"pageW\":" << spec->rgbaPageW << ",\"pageH\":" << spec->rgbaPageH
		  << ",\"framesPerPage\":" << spec->rgbaFramesPerPage
		  << ",\"rowsPerPage\":" << spec->rgbaRowsPerPage << "}";
		const int liveTileScale = g->getMod()->getBattlescapeTileScale();
		const SurfaceSet* blanks = g->getMod()->getSurfaceSet("BLANKS.PCK", false);
		const Surface* blankFrame = blanks ? blanks->getFrame(0) : nullptr;
		// Match Map construction exactly: the live unit quad is based on the
		// BLANKS frame box, not on the body sheet being probed.
		const int liveFrameW = blankFrame ? blankFrame->getWidth() * liveTileScale : 0;
		const int liveFrameH = blankFrame ? blankFrame->getHeight() * liveTileScale : 0;
		const int bodyRuntimeScale = spec->partScaleForFrame(liveFrameW, liveFrameH);
		const Mod::UnitAtlasSpec* handobSpec = g->getMod()->getUnitAtlas("HANDOB.PCK");
		const int handobRuntimeScale = handobSpec
		    ? handobSpec->partScaleForFrame(liveFrameW, liveFrameH)
		    : bodyRuntimeScale;
		const bool handobScaleCompatible = handobRuntimeScale == bodyRuntimeScale;
		const bool r8FallbackScaled = bodyRuntimeScale > 0;
		Mod::UnitAtlasSpec mismatchedHandob;
		mismatchedHandob.sourceFrameWidth = 16;
		mismatchedHandob.sourceFrameHeight = 20;
		const bool mismatchRejected = mismatchedHandob.partScaleForFrame(128, 160) != 4;
		// Authored texture resolution and live render geometry are independent:
		// a 4x RGBA cell may be downsampled into a 2x Battlescape quad. The
		// declaration only has to be internally valid; runtime body/HANDOB scales
		// must still agree with each other.
		const bool declaredScaleCompatible = !spec->partOffsetScaleConfigured
		    || spec->partOffsetScaleValid;
		const bool e2Passed = UnitSprite::debugE2OffsetProof()
		    && r8FallbackScaled && bodyRuntimeScale == liveTileScale
		    && declaredScaleCompatible && handobScaleCompatible && mismatchRejected;
		o << ",\"e2PartOffsetScale\":{\"sourceFrame\":["
		  << spec->sourceFrameWidth << "," << spec->sourceFrameHeight << "]"
		  << ",\"declaredFrame\":[" << spec->frameWidth << "," << spec->frameHeight << "]"
		  << ",\"liveRenderFrame\":[" << liveFrameW << "," << liveFrameH << "]"
		  << ",\"configured\":" << (spec->partOffsetScaleConfigured ? "true" : "false")
		  << ",\"valid\":" << (spec->partOffsetScaleValid ? "true" : "false")
		  << ",\"declaredScale\":" << spec->partOffsetScale
		  << ",\"runtimeScale\":" << bodyRuntimeScale
		  << ",\"handobRuntimeScale\":" << handobRuntimeScale
		  << ",\"representativeLogicalOffsets\":[-7,-2,-1,0,1,2,7,22]"
		  << ",\"representativeScale4Offsets\":["
		  << UnitSprite::debugE2ScaledOffset(-7, 4) << ","
		  << UnitSprite::debugE2ScaledOffset(-2, 4) << ","
		  << UnitSprite::debugE2ScaledOffset(-1, 4) << ","
		  << UnitSprite::debugE2ScaledOffset(0, 4) << ","
		  << UnitSprite::debugE2ScaledOffset(1, 4) << ","
		  << UnitSprite::debugE2ScaledOffset(2, 4) << ","
		  << UnitSprite::debugE2ScaledOffset(7, 4) << ","
		  << UnitSprite::debugE2ScaledOffset(22, 4) << "]"
		  << ",\"sharedBodyHandobScale\":" << (handobScaleCompatible ? "true" : "false")
		  << ",\"mismatchedHandobRejected\":" << (mismatchRejected ? "true" : "false")
		  << ",\"appliesToR8Fallback\":" << (r8FallbackScaled ? "true" : "false")
		  << ",\"centralEmitSeam\":\"blitBody+blitItem\""
		  << ",\"passed\":" << (e2Passed ? "true" : "false")
		  << "}";
		int hdCount = 0;
		for (uint8_t v : spec->rgbaHasHd) if (v) ++hdCount;
		const int fallbackCount = (int)spec->rgbaHasHd.size() - hdCount;
		o << ",\"hdFrames\":" << hdCount;
		o << ",\"totalSlots\":" << spec->rgbaHasHd.size();
		o << ",\"fallbackFrames\":" << fallbackCount;
		o << ",\"mixedFallback\":"
		  << (hdCount > 0 && fallbackCount > 0 ? "true" : "false");
		// skip-cache invariant: production RGBA pages must keep no CPU mirror.
		int cachedPages = 0;
		size_t cachedBytes = 0;
		for (GpuTexture *p : spec->rgbaOverlayPages)
			if (p) { cachedBytes += p->debugCachedBytes(); if (p->debugCachedBytes()) ++cachedPages; }
		o << ",\"rgbaCachedBytes\":" << cachedBytes;
		o << ",\"rgbaCachedPages\":" << cachedPages;
		// Exercise the exact context-loss lifecycle used by ShaderManager: every
		// page must become unavailable after eviction and reappear through its
		// MEMFS-backed reload callback without creating a CPU mirror.
		bool availableBefore = !spec->rgbaOverlayPages.empty();
		int recoveryFrame = -1;
		for (size_t i = 0; i < spec->rgbaHasHd.size(); ++i)
			if (spec->rgbaHasHd[i]) { recoveryFrame = (int)i; break; }
		for (GpuTexture *p : spec->rgbaOverlayPages)
			availableBefore = availableBefore && p && p->isValid();
		for (GpuTexture *p : spec->rgbaOverlayPages) if (p) p->evictGL();
		const bool r8FallbackAfterEvict = recoveryFrame >= 0
		    && !hdUnitRgbaPageUsable(spec, recoveryFrame);
		bool unavailableAfterEvict = !spec->rgbaOverlayPages.empty();
		for (GpuTexture *p : spec->rgbaOverlayPages)
			unavailableAfterEvict = unavailableAfterEvict && p && !p->isValid();
		for (GpuTexture *p : spec->rgbaOverlayPages) if (p) p->reupload();
		bool availableAfterRestore = !spec->rgbaOverlayPages.empty();
		size_t cachedAfterRestore = 0;
		for (GpuTexture *p : spec->rgbaOverlayPages)
		{
			availableAfterRestore = availableAfterRestore && p && p->isValid();
			if (p) cachedAfterRestore += p->debugCachedBytes();
		}
		const bool overlayAfterRestore = recoveryFrame >= 0
		    && hdUnitRgbaPageUsable(spec, recoveryFrame);
		o << ",\"contextRecovery\":{\"availableBefore\":"
		  << (availableBefore ? "true" : "false")
		  << ",\"unavailableAfterEvict\":"
		  << (unavailableAfterEvict ? "true" : "false")
		  << ",\"r8FallbackAfterEvict\":"
		  << (r8FallbackAfterEvict ? "true" : "false")
		  << ",\"availableAfterRestore\":"
		  << (availableAfterRestore ? "true" : "false")
		  << ",\"overlayAfterRestore\":"
		  << (overlayAfterRestore ? "true" : "false")
		  << ",\"cachedBytesAfterRestore\":" << cachedAfterRestore
		  << ",\"passed\":"
		  << (availableBefore && unavailableAfterEvict && r8FallbackAfterEvict
		      && availableAfterRestore && overlayAfterRestore
		      && cachedAfterRestore == 0 ? "true" : "false") << "}";

		// Shared renderer helpers provide body/HANDOB call-order evidence and an
		// exhaustive proof over the real z/y/x base-priority lattice.
		const int proofBase = 3 * 65536 + 47 * 1024 + 47 * 8;
		o << ",\"subpriorityProof\":{\"depth24Distinct\":"
		  << (UnitSprite::debugE1DepthProof() ? "true" : "false")
		  << ",\"basePriority\":" << proofBase
		  << ",\"surroundingSlots\":[3,6],\"emissions\":[";
		for (int sequence = 0; sequence < 8; ++sequence)
		{
			if (sequence) o << ",";
			o << "{\"sequence\":" << sequence
			  << ",\"baselinePriority\":"
			  << UnitSprite::debugE1LocalPriority(sequence, false)
			  << ",\"overlayPriority\":"
			  << UnitSprite::debugE1LocalPriority(sequence, true)
			  << ",\"baselineDepth24\":"
			  << UnitSprite::debugE1DepthCode(proofBase, sequence, false)
			  << ",\"overlayDepth24\":"
			  << UnitSprite::debugE1DepthCode(proofBase, sequence, true) << "}";
		}
		o << "]}";
		const unsigned int expectedFractional = 0x80007FFFu; // RGBA [128,0,127,255]
		const unsigned int naturalBuckets = UnitSprite::debugE1FractionalPixel(false);
		const unsigned int reversedBuckets = UnitSprite::debugE1FractionalPixel(true);
		o << ",\"fractionalAlphaPixelCase\":{\"behind\":[0,0,255,255]"
		  << ",\"front\":[255,0,0,128],\"expected\":[128,0,127,255]"
		  << ",\"frontR8FallbackMaskedByRgbaAlpha\":true"
		  << ",\"naturalPackedRgba\":" << naturalBuckets
		  << ",\"reversedPackedRgba\":" << reversedBuckets
		  << ",\"passed\":"
		  << (naturalBuckets == expectedFractional
		      && reversedBuckets == expectedFractional ? "true" : "false") << "}";
		const HdUnitRenderPlan::QuadClip movingClip = HdUnitRenderPlan::clipQuad(
			100.0f, 50.0f, 128.0f, 160.0f, 132, 196, 70, 190);
		const HdUnitRenderPlan::Rgba8 clippedEdge = HdUnitRenderPlan::sourceOver(
			{220, 100, 40, 128}, {20, 40, 60, 255});
		const bool clipPassed = movingClip.visible && movingClip.clipped
		    && std::abs(movingClip.x - 0.25f) < 0.0001f
		    && std::abs(movingClip.y - 0.125f) < 0.0001f
		    && std::abs(movingClip.w - 0.5f) < 0.0001f
		    && std::abs(movingClip.h - 0.75f) < 0.0001f
		    && clippedEdge.r == 120 && clippedEdge.g == 70
		    && clippedEdge.b == 50 && clippedEdge.a == 255
		    && HdUnitRenderPlan::foregroundOccludes(6.0f, 4.125f);
		o << ",\"movingClipReplayProof\":{\"clipRect\":["
		  << movingClip.x << "," << movingClip.y << ","
		  << movingClip.w << "," << movingClip.h << "]"
		  << ",\"sharedBy\":[\"r8-color\",\"rgba-color\",\"depth\"]"
		  << ",\"fractionalEdgeAfterClip\":[" << (unsigned)clippedEdge.r << ","
		  << (unsigned)clippedEdge.g << "," << (unsigned)clippedEdge.b << ","
		  << (unsigned)clippedEdge.a << "]"
		  << ",\"foregroundPriority6Occludes\":"
		  << (HdUnitRenderPlan::foregroundOccludes(6.0f, 4.125f) ? "true" : "false")
		  << ",\"passed\":" << (clipPassed ? "true" : "false") << "}";
		const E1GpuEdgeProof gpuEdge = runE1GpuEdgeProof();
		auto emitRgba = [&o](const std::array<unsigned char, 4>& rgba) {
			o << "[" << (unsigned)rgba[0] << "," << (unsigned)rgba[1]
			  << "," << (unsigned)rgba[2] << "," << (unsigned)rgba[3] << "]";
		};
		o << ",\"gpuEdgePixelProof\":{\"available\":"
		  << (gpuEdge.available ? "true" : "false")
		  << ",\"passed\":" << (gpuEdge.passed ? "true" : "false")
		  << ",\"glError\":" << gpuEdge.glError
		  << ",\"unitRgbaOverdrawPerSide\":1"
		  << ",\"terrainRgbaOverdrawPerSide\":2,\"samples\":[";
		for (size_t i = 0; i < gpuEdge.samples.size(); ++i)
		{
			if (i) o << ",";
			const E1GpuEdgeSample& sample = gpuEdge.samples[i];
			o << "{\"alpha\":" << sample.alpha << ",\"naturalCenter\":";
			emitRgba(sample.naturalCenter);
			o << ",\"reversedCenter\":"; emitRgba(sample.reversedCenter);
			o << ",\"naturalEdge\":"; emitRgba(sample.naturalEdge);
			o << ",\"naturalOutside\":"; emitRgba(sample.naturalOutside);
			o << "}";
		}
		o << "],\"tileCrossingEmit\":"
		  << (gpuEdge.tileCrossingEmit ? "true" : "false");
		o << ",\"movingClipInside\":"; emitRgba(gpuEdge.clippedInside);
		o << ",\"movingClipAdjacent\":"; emitRgba(gpuEdge.clippedAdjacent);
		o << ",\"movingClipOutside\":"; emitRgba(gpuEdge.clippedOutside);
		o << ",\"foregroundOccluded\":"; emitRgba(gpuEdge.foregroundOccluded);
		o << "}";
		// g0 spike overlay (disposable) presence for parity diagnostics.
		o << ",\"g0Overlay\":" << (spec->g0OverlayAtlas ? "true" : "false");
	}
	o << "}";
	std::ofstream f(outJsonPath, std::ios::binary | std::ios::trunc);
	if (!f) return 0;
	f << o.str();
	return 1;
}
#endif

/* ---- HTML main-menu bridge (Phase 2) ----------------------------------------
 * The JS menu overlay (web/public/menu.js) calls these to push the same OXCE
 * states the vanilla MainMenuState buttons would. Called from JS between frames;
 * pushState is applied on the next Game::run iteration. `using namespace OpenXcom`
 * (above) resolves Game/getCurrentGame/the State classes/OPT_MENU.
 *
 * Each returns int: 1 when a live Game handled the call, 0 when the engine isn't
 * ready yet (called before callMain / audio init). The JS bridge treats only a 1
 * as success, so a click that lands before boot is a safe no-op instead of a
 * false "navigated" that would tear the overlay down over a blank canvas. */
EMSCRIPTEN_KEEPALIVE int calypso_menu_new_game()   { if (Game *g = getCurrentGame()) { g->pushState(new NewGameState);            return 1; } return 0; }
EMSCRIPTEN_KEEPALIVE int calypso_menu_new_battle() { if (Game *g = getCurrentGame()) { g->pushState(new NewBattleState);          return 1; } return 0; }
EMSCRIPTEN_KEEPALIVE int calypso_menu_load()       { if (Game *g = getCurrentGame()) { g->pushState(new ListLoadState(OPT_MENU)); return 1; } return 0; }
EMSCRIPTEN_KEEPALIVE int calypso_menu_mods()       { if (Game *g = getCurrentGame()) { g->pushState(new ModListState);            return 1; } return 0; }
EMSCRIPTEN_KEEPALIVE int calypso_menu_options()    { if (Game *g = getCurrentGame()) { g->pushState(new OptionsVideoState(OPT_MENU)); return 1; } return 0; }

/* Silence the engine's menu music while the HTML menu (with its own water-ambience
 * audio) is shown; restore the engine volume when a menu action hands control to a
 * game state. Gated on a live Game so a pre-boot mute (audio not opened yet) can't
 * poison the saved volume — returns 0 until the engine is up, matching the menu_*
 * exports. Saved-volume guard makes repeated mute calls idempotent. */
static int s_calypsoSavedMusicVol = -1;
EMSCRIPTEN_KEEPALIVE int calypso_music_mute()
{
	if (!getCurrentGame()) return 0;
	if (s_calypsoSavedMusicVol < 0) { s_calypsoSavedMusicVol = Mix_VolumeMusic(-1); }
	Mix_VolumeMusic(0);
	return 1;
}
EMSCRIPTEN_KEEPALIVE int calypso_music_unmute()
{
	if (!getCurrentGame()) return 0;
	if (s_calypsoSavedMusicVol >= 0) { Mix_VolumeMusic(s_calypsoSavedMusicVol); s_calypsoSavedMusicVol = -1; }
	return 1;
}

/* Full QA mute: music AND every SDL_mixer channel (sound effects, UI blips,
 * ambient). Used by the ?mute=1 / ?hdHarness= silent-boot path in menu.js —
 * unlike the music pair above there is deliberately no unmute counterpart:
 * a QA session dies with its page and must stay silent for its whole life.
 * Same live-Game guard and saved-volume idempotence as calypso_music_mute. */
EMSCRIPTEN_KEEPALIVE int calypso_audio_mute()
{
	if (!getCurrentGame()) return 0;
	if (s_calypsoSavedMusicVol < 0) { s_calypsoSavedMusicVol = Mix_VolumeMusic(-1); }
	Mix_VolumeMusic(0);
	Mix_Volume(-1, 0);
	return 1;
}

/* The SDL2 Emscripten port routes WebGL-canvas pointermove events as
 * SDL_MOUSEBUTTONDOWN (buttonless), not SDL_MOUSEMOTION, which leaves the
 * OXCE Cursor stuck.  Hosting code in main.js registers a JS mousemove
 * listener that calls this with backing-store coordinates; we update the
 * Cursor directly (the SDL queue path was unreliable). */
/* Phase 8c §C2: opt-in perf log gate for Globe::drawSphereGPU. */
int g_calypsoProfileGlobe = 0;

EMSCRIPTEN_KEEPALIVE
void calypso_set_profile_globe(int on)
{
	g_calypsoProfileGlobe = on ? 1 : 0;
}

/* Phase 46.4 Stage 10.2.1 (Calypso): opt-in request for the GPU-direct globe * composite. Production stays 0; until the marker-layer migration lands the * engine acknowledges the request in the log and keeps the canonical path. */
int g_calypsoGlobeGpuDirect = 0;

EMSCRIPTEN_KEEPALIVE
void calypso_set_globe_gpu_direct(int on)
{
    g_calypsoGlobeGpuDirect = on ? 1 : 0;
}

/* Loopback-only Geoscape physical-shell preview. Canonical F16 remains off. */
int g_calypsoGeoscapeHdPreview = 0;

EMSCRIPTEN_KEEPALIVE
void calypso_set_geoscape_hd_preview(int on)
{
	g_calypsoGeoscapeHdPreview = on ? 1 : 0;
}

/* Phase 11.0: opt-in CPU perf gate for Map::drawTerrain.
 * JS toggles via calypso_set_profile_battlescape(1); production stays 0. */
int g_calypsoProfileBattlescape = 0;

EMSCRIPTEN_KEEPALIVE
void calypso_set_profile_battlescape(int on)
{
	g_calypsoProfileBattlescape = on ? 1 : 0;
}

/* Phase 11.1: opt-in readback-cost probe gate for Map::drawTerrain.
 * Runs FBO solid-colour + glReadPixels at Battlescape surface size;
 * self-terminates after 30 samples. */
int g_calypsoProfileReadback = 0;

EMSCRIPTEN_KEEPALIVE
void calypso_set_profile_readback(int on)
{
	g_calypsoProfileReadback = on ? 1 : 0;
}

/* Asset-audit mode: logs every resolved asset relpath once, tagged VANILLA
 * (served from the streamed TFTD payload) or REPLACED (served from a Calypso
 * mod overlay). JS toggles via ?audit=1 -> calypso_set_audit_mode(1); the
 * printErr handler in main.js parses the "[CALYPSO] ASSET ..." marker into
 * window.__assetAudit. See FileMap::at() and scripts/gen-asset-coverage.py. */
EMSCRIPTEN_KEEPALIVE
void calypso_set_audit_mode(int on)
{
	FileMap::setAuditMode(on != 0);
}

/* Phase 28: underwater colour-grade strength (0 = neutral .. 1 = deepest).
 * Map::drawSceneGrade() reads this each frame as the u_strength uniform.
 * Live-tunable from the JS console: Module._calypso_set_underwater_strength(0.4).
 * Default matches the "L1" starting look chosen during authoring. */
float g_calypsoUnderwaterStrength = 0.20f;

EMSCRIPTEN_KEEPALIVE
void calypso_set_underwater_strength(float v)
{
	g_calypsoUnderwaterStrength = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

/* Phase 28 batch-1 beauty FX amplitudes (0 = off). All read by
 * Map::drawSceneGrade() each frame and live-tunable from the JS console. */
float g_calypsoUwCaustics = 0.55f;
float g_calypsoUwRefract  = 0.40f;   // weaker — subtle wobble, not seasick
float g_calypsoUwBubbles  = 0.0f;    // seabed vents OFF (screen-anchored for now)
float g_calypsoUwSnow     = 0.5f;
float g_calypsoUwUnitBub  = 1.0f;    // HD bubbles, driven by the vanilla breath anim
float g_calypsoUwGodray   = 0.1f;    // batch 2: light shafts — subtle
float g_calypsoUwBloom    = 0.5f;    // batch 2: glow on bright spots
float g_calypsoUwBreath   = 0.6f;    // batch 2: slow global light pulse
float g_calypsoUwChroma   = 0.0f;    // OFF — no visible effect (scene edges are void)
float g_calypsoUwShock    = 0.7f;    // E2: explosion shockwave-ring distortion (underwater)
// Phase 25 (R1): coloured emissive halo amount (fire). The uw_ prefix keeps it in
// the existing knob family, but the emissive pass is mission-agnostic — it fires
// on land maps too (fire tiles), unlike the underwater-only grade/beauty FX.
float g_calypsoUwEmissive = 1.0f;
// Phase 25 (R6): HD material-emissive atlas multiplier (lava / bioluminescence).
// Scales the per-dataset emissiveFile glow added in tile_atlas_rgba.frag, into
// the HDR SSAA buffer (R0). 0 = off; > ~1 pushes bright texels past 1.0 so the
// HDR tonemap blooms them. Default subtle; live-tune via _calypso_set_tile_emissive.
float g_calypsoTileEmissive = 1.5f;
// Phase 25 (R7): unit "fake lighting" amount — a sprite-local vertical AO/relief on
// unit bodies (in tile_atlas.frag) so they gain volume + a grounding shadow without
// an RGBA atlas or baked-AO art. 0 = off (legacy flat units); 1 = full. Tiles +
// floor items are never affected. Live-tune via _calypso_set_unit_shade.
float g_calypsoUnitShade = 1.0f;

// Phase 42: HD unit weapon registration. The 3D-rendered arm poses place the
// hand at a different screen pixel than the vanilla arm sprites the HANDOB
// offset tables were tuned for, so a held weapon lands beside/below the hand.
// These per-(slot, two-handed, direction) pixel nudges re-seat the HANDOB sprite
// in the rendered hand; slot 0 = right item, 1 = left item. All zero by default
// (vanilla placement). UnitSprite.cpp reads these under __EMSCRIPTEN__; live-tune
// via _calypso_set_weapon_hand_offset(slot, twoHanded, dir, dx, dy).
//
// The HD carry poses are solved so the rendered palm lands on the vanilla grip
// itself (tools/hd-unit-hand-pixels.py measures where the vanilla routine-13
// arm meets a HANDOB weapon; tools/hd-unit-hand-fit-blender.py fits the single
// armature-space point that explains all eight directions and the arm poses
// aim at it).  One rigid 3D grip cannot reproduce eight hand-drawn frames
// exactly, and this table is exactly that leftover: rendered hand minus vanilla
// hand, per direction, in 32x40 logical pixels.  It is generated, not tuned --
// regenerate with the two tools above and copy the emitted
// `weapon_hand_offsets` block from docs/measurements/phase-42-hand-targets.json.
// Slot 0 = right item (1H uses the one-hand-carry arm 248, 2H the trigger arm
// 240), slot 1 = left item (always drawn against the support arm 232).
// Directions where the vanilla arm is occluded by the torso and only grazes the
// weapon carry no correction: there the 3D fit is the more trustworthy of the
// two, so those entries stay 0.
int g_calypsoWeaponHandOffX[2][2][8] = {
	{ { 0, 0, 0, 0, -1, 1, 0, -2 }, { 1, -3, 2, 2, -2, 2, 0, -1 } },
	{ { 0, -3, 2, -2, -1, 3, -1, 0 }, { 0, -3, 2, -2, -1, 3, -1, 0 } },
};
int g_calypsoWeaponHandOffY[2][2][8] = {
	{ { 1, 0, 1, 0, 0, -1, -1, -1 }, { 0, 0, 0, 2, 0, -1, 0, -1 } },
	{ { 0, -1, 0, 2, 0, 0, 0, -1 }, { 0, -1, 0, 2, 0, 0, 0, -1 } },
};

static float clamp01p(float v) { return v < 0.0f ? 0.0f : (v > 2.0f ? 2.0f : v); }
static float clamp08 (float v) { return v < 0.0f ? 0.0f : (v > 8.0f ? 8.0f : v); }
static float clamp01 (float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

EMSCRIPTEN_KEEPALIVE void calypso_set_uw_caustics(float v) { g_calypsoUwCaustics = clamp01p(v); }
EMSCRIPTEN_KEEPALIVE void calypso_set_uw_refract (float v) { g_calypsoUwRefract  = clamp01p(v); }
EMSCRIPTEN_KEEPALIVE void calypso_set_uw_bubbles (float v) { g_calypsoUwBubbles  = clamp01p(v); }
EMSCRIPTEN_KEEPALIVE void calypso_set_uw_snow    (float v) { g_calypsoUwSnow     = clamp01p(v); }
EMSCRIPTEN_KEEPALIVE void calypso_set_uw_unitbub (float v) { g_calypsoUwUnitBub  = clamp01p(v); }
EMSCRIPTEN_KEEPALIVE void calypso_set_uw_godray  (float v) { g_calypsoUwGodray   = clamp01p(v); }
EMSCRIPTEN_KEEPALIVE void calypso_set_uw_bloom   (float v) { g_calypsoUwBloom    = clamp01p(v); }
EMSCRIPTEN_KEEPALIVE void calypso_set_uw_breath  (float v) { g_calypsoUwBreath   = clamp01p(v); }
EMSCRIPTEN_KEEPALIVE void calypso_set_uw_chroma  (float v) { g_calypsoUwChroma   = clamp01p(v); }
EMSCRIPTEN_KEEPALIVE void calypso_set_uw_shock   (float v) { g_calypsoUwShock    = clamp01p(v); }
EMSCRIPTEN_KEEPALIVE void calypso_set_uw_emissive(float v) { g_calypsoUwEmissive = clamp01p(v); }
EMSCRIPTEN_KEEPALIVE void calypso_set_tile_emissive(float v) { g_calypsoTileEmissive = clamp08(v); } // Phase 25 R6
EMSCRIPTEN_KEEPALIVE void calypso_set_unit_shade  (float v) { g_calypsoUnitShade   = clamp01(v); } // Phase 25 R7
EMSCRIPTEN_KEEPALIVE void calypso_set_weapon_hand_offset(int slot, int twoHanded, int dir, int dx, int dy) { // Phase 42
	if (slot < 0 || slot > 1 || twoHanded < 0 || twoHanded > 1 || dir < 0 || dir > 7) return;
	g_calypsoWeaponHandOffX[slot][twoHanded][dir] = dx;
	g_calypsoWeaponHandOffY[slot][twoHanded][dir] = dy;
}

/* L2 (memory-reduction): runtime SSAA supersample-factor override.
 * 0 = "unset" — Map::ensureSsaaTarget falls back to _ssaaScale (default 2×).
 * calypso_set_ssaa_scale(1) disables supersampling (HDR retained), freeing
 * ~105 MiB GPU VRAM at FHD.  Valid clamped range: 1–4. */
int g_calypsoSsaaScale = 0;

EMSCRIPTEN_KEEPALIVE
void calypso_set_ssaa_scale(int s)
{
	g_calypsoSsaaScale = s < 1 ? 0 : (s > 4 ? 4 : s);
}

/* Phase 25 (R3): tangent-space sun direction for normal-map relief. The shader
 * normalises it. In production the engine DRIVES this automatically (a per-turn
 * azimuth sweep — "time of day" — in the upper hemisphere, coherent with the
 * surface god-rays); see Map::drawTileGLPass. g_calypsoSunAuto gates that. The
 * relief STRENGTH is baked into the atlas at build time (ruleset normalStrength:).
 * Dev override: Module._calypso_set_sun_dir(x, y, z) freezes a manual direction;
 * Module._calypso_set_sun_auto(1) resumes the automatic sweep. */
float g_calypsoSunDir[3] = { -0.40f, -0.40f, 0.82f };
int   g_calypsoSunAuto   = 1;   // 1 = engine drives the sun; 0 = manual override

EMSCRIPTEN_KEEPALIVE
void calypso_set_sun_dir(float x, float y, float z)
{
	// Reject a degenerate zero vector: the shaders do normalize(u_sunDir), and
	// normalize(vec3(0)) is UB in GLSL ES (NaN on most drivers) — it would
	// corrupt the relief term for every normal-mapped tile. Keep the prior value.
	if (x * x + y * y + z * z < 1e-12f) return;
	g_calypsoSunDir[0] = x; g_calypsoSunDir[1] = y; g_calypsoSunDir[2] = z;
	g_calypsoSunAuto = 0;   // a manual set freezes the automatic sweep (dev override)
}

EMSCRIPTEN_KEEPALIVE
void calypso_set_sun_auto(int on) { g_calypsoSunAuto = on ? 1 : 0; }

/* Phase-14 railings debug: one-shot tile/painter dump.
 * JS toggles via Module._calypso_dump_emit_once() before forcing a redraw;
 * Map::emitTilePass() and Map::draw() (painter) each log every tile they
 * see and reset the flag, so production runs at zero cost. */
int g_calypsoDumpEmit = 0;

EMSCRIPTEN_KEEPALIVE
void calypso_dump_emit_once()
{
	g_calypsoDumpEmit = 1;
}

/* M6h: tab-hide pause.
 *
 * Called by the JS visibilitychange listener when document.hidden becomes
 * true.  Sets a flag that BattlescapeState::think() polls each tick: if the
 * battlescape is the top state and buttons are allowed, it opens the
 * PauseState menu.  The pause menu stops map redraws so the GL context sits
 * idle — Chrome classifies a quiet context-loss as "innocent" and
 * auto-restores it cleanly, converting the hard recovery path into the easy
 * one.  GeoscapeState::init() clears the flag so a tab-switch on the
 * geoscape cannot pop a menu when the player later enters a new battle. */
int g_calypsoTabHiddenPause = 0;

EMSCRIPTEN_KEEPALIVE
void calypso_on_tab_hidden(void)
{
	g_calypsoTabHiddenPause = 1;
}

/* M6c: WebGL context-loss / restore freeze.
 *
 * g_calypsoContextLost is tested at the top of Screen::flip() (and any other
 * per-frame GL entry) so the engine skips ALL GL calls while the context is
 * dead.  JS sets this flag synchronously on the 'webglcontextlost' event
 * (before the browser discards the GL objects) and clears it on restore.
 *
 * Emscripten's main loop is paused so the event / timer callbacks that drive
 * the game loop stop firing; only the SDL event queue (which is safe on a dead
 * context) and the two canvas event listeners continue to run.
 *
 * Edge cases handled:
 *   • Double-loss  — guard in calypso_gl_context_lost prevents double-pause.
 *   • Restore without prior loss — SDL_RENDER_TARGETS_RESET is still pushed;
 *     emscripten_resume_main_loop is a no-op when the loop is already running.
 *   • Loss before callMain — emscripten_pause_main_loop is a no-op when no
 *     loop exists yet; emscripten_resume_main_loop is likewise safe. */
int g_calypsoContextLost = 0;

EMSCRIPTEN_KEEPALIVE
void calypso_gl_context_lost(void)
{
	if (!g_calypsoContextLost)
	{
		g_calypsoContextLost = 1;
		emscripten_pause_main_loop();
	}
}

EMSCRIPTEN_KEEPALIVE
void calypso_gl_context_restored(void)
{
	const int wasLost = g_calypsoContextLost;
	g_calypsoContextLost = 0;

	SDL_Event e;
	SDL_zero(e);
	e.type = SDL_RENDER_TARGETS_RESET;
	SDL_PushEvent(&e);

	if (wasLost)
		emscripten_resume_main_loop();
}

EMSCRIPTEN_KEEPALIVE
void calypso_push_mouse_motion(int x, int y)
{
	OpenXcom::Game *g = OpenXcom::getCurrentGame();
	if (!g) return;
	OpenXcom::Cursor *c = g->getCursor();
	OpenXcom::Screen *s = g->getScreen();
	if (!c || !s) return;
	/* JS sends canvas-backing pixels; convert to game-coords via the
	 * Screen's current xScale/yScale (canvas / base). */
	double sx = s->getXScale();
	double sy = s->getYScale();
	if (sx <= 0.0) sx = 1.0;
	if (sy <= 0.0) sy = 1.0;
	c->setX((int)(x / sx));
	c->setY((int)(y / sy));
}

/* Phase 33: one-time touch-device defaults.  Called by JS after callMain
 * (options.cfg is loaded by then), and ONLY on the first visit from a touch
 * device (JS guards with a localStorage marker) so later user changes in
 * Options are never stomped.  profile: 1 = tablet, 2 = phone. */
EMSCRIPTEN_KEEPALIVE
int calypso_apply_touch_defaults(int profile)
{
	using namespace OpenXcom;
	if (!getCurrentGame()) return 0;
	Options::touchEnabled = true;                    /* drag-scroll: no selector chase */
	Options::oxceFatFingerLinks = true;              /* bigger extended-links buttons */
	Options::oxceBattleTouchButtonsEnabled = true;   /* on-screen RMB/CTRL/ALT/SHIFT   */
	Options::oxceBaseTouchButtons = true;
	if (Options::battleDragScrollButton == 0)
		Options::battleDragScrollButton = SDL_BUTTON_LEFT;
	/* Bigger UI on small screens; user can change it in Options → Video. */
	Options::battlescapeScale = (profile >= 2) ? SCALE_SCREEN_DIV_3 : SCALE_SCREEN_DIV_2;
	Options::geoscapeScale    = (profile >= 2) ? SCALE_SCREEN_DIV_3 : SCALE_SCREEN_DIV_2;
	Options::save();
	return 1;
}

/* Phase 33: pinch-zoom bridge.  Steps the Battlescape display-fraction ladder
 * (same path as the mouse wheel — BattlescapeState::zoom, Emscripten-only).
 * Returns 0 when the top state is not a battle (harmless no-op for JS). */
EMSCRIPTEN_KEEPALIVE
int calypso_battlescape_zoom(int direction)
{
	OpenXcom::Game *g = OpenXcom::getCurrentGame();
	if (!g) return 0;
	OpenXcom::BattlescapeState *bs =
		dynamic_cast<OpenXcom::BattlescapeState *>(g->getTopState());
	if (!bs) return 0;
	bs->zoom(direction > 0 ? 1 : -1);
	return 1;
}

/* Phase 33: virtual-keyboard bridge.  TextEdit::setFocus calls
 * calypso_notify_text_focus; it forwards to the JS hook
 * globalThis.__calypsoTextFocus (no-op when the hook is absent, i.e. desktop).
 * Coordinates are converted base-resolution → canvas pixels here, mirroring
 * calypso_push_mouse_motion in reverse. */
EMSCRIPTEN_KEEPALIVE
void calypso_notify_text_focus(int focused, int x, int y, int w, int h,
	const char *utf8, int multiline, int enterPolicy)
{
	OpenXcom::Game *g = OpenXcom::getCurrentGame();
	double sx = 1.0, sy = 1.0;
	if (g && g->getScreen())
	{
		sx = g->getScreen()->getXScale(); if (sx <= 0.0) sx = 1.0;
		sy = g->getScreen()->getYScale(); if (sy <= 0.0) sy = 1.0;
	}
	EM_ASM({
		if (globalThis.__calypsoTextFocus)
			globalThis.__calypsoTextFocus($0, $1, $2, $3, $4, UTF8ToString($5), $6, $7);
	}, focused, (int)(x * sx), (int)(y * sy), (int)(w * sx), (int)(h * sy), utf8,
		multiline, enterPolicy);
}

EMSCRIPTEN_KEEPALIVE
void calypso_text_set(const char *utf8)
{
	if (calypso_viewport_input_blocked()) return;
	if (OpenXcom::g_calypsoFocusedTextEdit)
		OpenXcom::g_calypsoFocusedTextEdit->setTextExternal(utf8 ? utf8 : "");
}

EMSCRIPTEN_KEEPALIVE
void calypso_text_commit(void)
{
	if (calypso_viewport_input_blocked()) return;
	SDL_Event e;
	SDL_zero(e);
	e.type = SDL_KEYDOWN;
	e.key.keysym.sym = SDLK_RETURN;
	SDL_PushEvent(&e);          /* routes through the normal keydown path →
	                               TextEdit ENTER handling (TextEdit.cpp:561) */
}

EMSCRIPTEN_KEEPALIVE
void calypso_text_commit_multiline(void)
{
	if (calypso_viewport_input_blocked()) return;
	OpenXcom::TextEdit *edit = OpenXcom::g_calypsoFocusedTextEdit;
	if (!edit || !edit->isMultiline()) return;
	SDL_Event e;
	SDL_zero(e);
	e.type = SDL_KEYDOWN;
	e.key.keysym.sym = SDLK_RETURN;
	OpenXcom::Action action(&e, 1.0, 1.0, 0, 0);
	// Terminal operation: commit may synchronously pop the owner and delete edit.
	edit->commit(&action);
}

EMSCRIPTEN_KEEPALIVE
void calypso_text_cancel(void)
{
	if (calypso_viewport_input_blocked()) return;
	SDL_Event e;
	SDL_zero(e);
	e.type = SDL_KEYDOWN;
	e.key.keysym.sym = SDLK_ESCAPE;
	SDL_PushEvent(&e);          /* Route through the hardware-Escape path.
	                               Legacy single-line edits clear and commit;
	                               multiline edits defer to their owner. */
}

/* Phase 41 (commit 4.5): dev-only scene-preview bridge (plan §41.1c). Boots
 * straight into the named deployment's battlescape with the CalypsoDirector
 * suppressed, the map fully revealed, and a tile-coordinate readout under the
 * cursor -- lets a mapScript/MAP/RMP edit be checked without playing through
 * the whole scripted mission. Called from web/src/main.js off
 * ?scenePreview=<deploymentId>; no menu entry. */
EMSCRIPTEN_KEEPALIVE
void calypso_scene_preview(const char *deploymentId)
{
	Game *g = getCurrentGame();
	if (!g || !g->getMod() || !deploymentId || !*deploymentId) return;
	if (!g->getMod()->getDeployment(deploymentId))
	{
		Log(LOG_ERROR) << "[scene-preview] unknown deployment '" << deploymentId << "'";
		return;
	}
	Calypso::launchScriptedBattle(g, deploymentId, /*preview=*/true);
}

/* PR #78 / P2 — test/diagnostic only: serialize the live HD HUD layout geometry.
 * Writes a JSON file to `outJsonPath` (Emscripten MEMFS) describing the HD panel
 * top / Map scissor / BattlescapeButton transform and representative portrait/
 * name/stat GL overlay rectangles vs their CPU HUD widgets. A WASM regression
 * (scripts/test-battlescape-hud-resize.js) resizes the viewport height and reads this
 * probe after each resize to assert the GL overlay stays aligned with the CPU
 * widgets — the exact P2 bug (width-preserving height resize left the HUD stale).
 * Returns 1 when the file was written, 0 when there is no live battlescape. */
EMSCRIPTEN_KEEPALIVE
int calypso_hud_layout_probe(const char *outJsonPath)
{
	if (!outJsonPath || !*outJsonPath) return 0;   // test/diagnostic: guard null/empty path
	Game *g = getCurrentGame();
	if (!g || !g->getSavedGame()) return 0;
	OpenXcom::SavedBattleGame *battle = g->getSavedGame()->getSavedBattle();
	if (!battle || !battle->getBattleState()) return 0;
	OpenXcom::BattlescapeState *bs = battle->getBattleState();
	std::string out;
	bs->debugHudLayoutProbe(out);
	std::ofstream f(outJsonPath, std::ios::binary | std::ios::trunc);
	if (!f) return 0;
	f << out;
	return 1;
}

} /* extern "C" */

#endif /* __EMSCRIPTEN__ */
