/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * Phase 46.2-HD (Calypso) -- HD UI overlay queue. Whole-file Emscripten guard.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoHdUiOverlay.h"
#include "CalypsoViewportMailbox.h"

#include <emscripten.h>
#include <GLES3/gl3.h>
#include <SDL.h>
#include <SDL_render.h>
#include <vector>

#include "../Engine/GpuInit.h"
#include "../Engine/GpuTexture.h"
#include "../Engine/Shader.h"
#include "../Engine/ShaderManager.h"
#include "../Engine/Logger.h"

namespace OpenXcom
{
namespace Calypso
{

CalypsoHdUiOverlay& CalypsoHdUiOverlay::instance()
{
	static CalypsoHdUiOverlay s_overlay;
	return s_overlay;
}

void CalypsoHdUiOverlay::beginFrame(int logicalWidth, int logicalHeight)
{
	// Backing-store poll: the canvas width/height are physical device pixels
	// (mirrors the existing Screen::flip() poll). Authoritative for physical
	// canvas dimensions only; the JS observation owns every other field. Only
	// polled when the overlay is active -- while dormant the metrics are never
	// used for drawing and the JS observation already delivers physical dims on
	// every resize, so the two extra per-frame EM_ASM round-trips are wasted.
	if (hasEnabledGroups())
	{
		const int physW = (int)EM_ASM_INT({ return document.getElementById('canvas').width; });
		const int physH = (int)EM_ASM_INT({ return document.getElementById('canvas').height; });
		if (physW > 0 && physH > 0)
		{
			calypsoHdViewportBackingStorePoll(physW, physH);
		}
	}

	// Freeze ONE metrics snapshot for the whole frame.
	_frozenMetrics = calypsoHdBuildPresentationMetrics(logicalWidth, logicalHeight);

	// Advance the frame; decide whether HD physical output is permitted.
	const CalypsoHdFrameController::BeginResult r =
		_controller.beginFrame(_frozenMetrics.valid());
	_mayGoPhysical = r.mayGoPhysical;
}

void CalypsoHdUiOverlay::ensureGpu()
{
	if (_glReady) return;
	if (!GpuInit::ready()) return;

	if (!_hdShader)
	{
		_hdShader = new Shader();
		if (!_hdShader->loadFromEmbedded("hd_ui"))
		{
			Log(LOG_ERROR) << "CalypsoHdUiOverlay: failed to load 'hd_ui' shader";
			delete _hdShader;
			_hdShader = nullptr;
			return;
		}
	}

	if (!_vao)
	{
		// 6-vertex quad (2 triangles): each vertex is pos.xy + uv.xy (4 floats).
		glGenVertexArrays(1, &_vao);
		glGenBuffers(1, &_vbo);
		glBindVertexArray(_vao);
		glBindBuffer(GL_ARRAY_BUFFER, _vbo);
		glBufferData(GL_ARRAY_BUFFER, 6 * 4 * (GLsizeiptr)sizeof(float), nullptr, GL_DYNAMIC_DRAW);
		glEnableVertexAttribArray(0); // a_pos
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * (GLsizei)sizeof(float), (void*)0);
		glEnableVertexAttribArray(1); // a_uv
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * (GLsizei)sizeof(float), (void*)(2 * sizeof(float)));
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	if (!_gpuAliveFlag)
	{
		// Context-loss ladder (Phase 11.13 / Phase M): zero stale VAO/VBO
		// handles and force a rebuild on the next frame. The shader (registered
		// with ShaderManager) and any GpuTexture recover themselves.
		_gpuAliveFlag = std::make_shared<bool>(true);
		ShaderManager::instance().registerResetCallback(_gpuAliveFlag, [this]() {
			_vao = 0;
			_vbo = 0;
			_glReady = false;
			_controller.noteContextRestored();
		});
	}

	_glReady = true;
}

void CalypsoHdUiOverlay::drawTexturedQuad(GpuTexture* tex, const CalypsoLogicalRect& logical)
{
	if (!tex || !tex->isValid() || !_hdShader || !_hdShader->isValid() || !_vao) return;

	const CalypsoPhysRect r = calypsoMapLogicalRect(logical, _frozenMetrics);
	const float physW = (float)_frozenMetrics.physicalWidth;
	const float physH = (float)_frozenMetrics.physicalHeight;
	if (r.empty() || physW <= 0.0f || physH <= 0.0f) return;

	// Physical device-pixel rect -> NDC. Flip Y (SDL top-left -> GL bottom-left).
	const float x0 =  2.0f * (float)r.x / physW - 1.0f;
	const float x1 =  2.0f * (float)(r.x + r.w) / physW - 1.0f;
	const float y0 = -(2.0f * (float)r.y / physH - 1.0f);
	const float y1 = -(2.0f * (float)(r.y + r.h) / physH - 1.0f);

	const float verts[6 * 4] = {
		x0, y0, 0.0f, 0.0f,
		x1, y0, 1.0f, 0.0f,
		x0, y1, 0.0f, 1.0f,
		x0, y1, 0.0f, 1.0f,
		x1, y0, 1.0f, 0.0f,
		x1, y1, 1.0f, 1.0f,
	};

	glBindBuffer(GL_ARRAY_BUFFER, _vbo);
	glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)sizeof(verts), verts);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	GLint prevProgram = 0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	_hdShader->use();
	_hdShader->setUniform4f("u_color", 0.0f, 0.0f, 0.0f, 0.0f); // unset => opaque white (no tint)
	tex->bind(0);
	_hdShader->setUniform1i("u_tex", 0);

	glBindVertexArray(_vao);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);

	glDisable(GL_BLEND);
	glUseProgram((GLuint)prevProgram);
}

void CalypsoHdUiOverlay::submitText(const TextSubmit& item)
{
	_pendingText.push_back(item);
	_enabledGroupCount = 1; // an adapter opted in this frame -> not dormant
}

GpuTexture* CalypsoHdUiOverlay::textureForText(const CalypsoHdTextRasterKey& rasterKey)
{
	CalypsoHdTextTextureKey tk;
	tk.raster = rasterKey;
	tk.contextGeneration = _contextGen;

	auto it = _textTextures.find(tk);
	if (it != _textTextures.end())
	{
		auto hit = _texKeyToHandle.find(tk);
		if (hit != _texKeyToHandle.end())
		{
			GpuTexture* t = it->second;
			const std::size_t bytes = (std::size_t)t->width() * (std::size_t)t->height() * 4u;
			_textTexLru.touch(hit->second, bytes); // refresh recency; not evicted
		}
		return it->second;
	}

	SDL_Surface* raster = _textRaster.rasterFor(rasterKey);
	if (!raster) return nullptr;
	// Convert to R,G,B,A memory byte order for uploadRGBA (Phase 20 ModHd pattern).
	SDL_Surface* rgba = SDL_ConvertSurfaceFormat(raster, SDL_PIXELFORMAT_ABGR8888, 0);
	if (!rgba) return nullptr;
	GpuTexture* tex = new GpuTexture(/*srgb=*/false,
		GpuTexture::Wrap::ClampToEdge, GpuTexture::Filter::Linear);
	const bool ok = tex->uploadRGBA(static_cast<const std::uint8_t*>(rgba->pixels), rgba->w, rgba->h);
	const std::size_t byteCost = (std::size_t)rgba->w * (std::size_t)rgba->h * 4u;
	SDL_FreeSurface(rgba);
	if (!ok || !tex->isValid())
	{
		delete tex;
		return nullptr;
	}

	const std::uint64_t handle = _texNextHandle++;
	_textTextures.emplace(tk, tex);
	_texKeyToHandle.emplace(tk, handle);
	_texHandleToKey.emplace(handle, tk);

	for (std::uint64_t ev : _textTexLru.touch(handle, byteCost))
	{
		auto kit = _texHandleToKey.find(ev);
		if (kit == _texHandleToKey.end()) continue;
		const CalypsoHdTextTextureKey evKey = kit->second;
		auto tit = _textTextures.find(evKey);
		if (tit != _textTextures.end())
		{
			delete tit->second;
			_textTextures.erase(tit);
		}
		_texKeyToHandle.erase(evKey);
		_texHandleToKey.erase(kit);
	}
	return tex;
}

void CalypsoHdUiOverlay::dropTextTextures()
{
	for (auto& kv : _textTextures) delete kv.second;
	_textTextures.clear();
	_texKeyToHandle.clear();
	_texHandleToKey.clear();
	_textTexLru.clear();
	_textRaster.dropContextTextures();
}

bool CalypsoHdUiOverlay::renderStages(SDL_Renderer* renderer)
{
	// Consume this frame's text submissions (adapters submit during state blit,
	// before flip()). Swapping clears the member so the next frame starts empty.
	std::vector<TextSubmit> pending;
	pending.swap(_pendingText);

	// Dormant until an adapter (or the harness) enables a group.
	if (!hasEnabledGroups() || !_mayGoPhysical) return true;
	if (!_frozenMetrics.valid()) return true;

	// Boundary zero: submit SDL's batched composite before any raw GL so the
	// renderer and our GL calls do not desync (Phase 14 lesson).
	if (renderer) SDL_RenderFlush(renderer);

	ensureGpu();
	if (!_glReady) return true; // GL not ready yet: this frame stays logical

	if (_harnessEnabled)
	{
		if (!_harnessTex)
		{
			// 64x64 magenta/cyan checkerboard so a browser screenshot makes the
			// physical-resolution quad and its crisp edges obvious.
			const int N = 64, CELL = 8;
			std::vector<std::uint8_t> px((std::size_t)N * N * 4);
			for (int y = 0; y < N; ++y)
				for (int x = 0; x < N; ++x)
				{
					const bool on = (((x / CELL) + (y / CELL)) & 1) != 0;
					std::uint8_t* p = &px[((std::size_t)y * N + x) * 4];
					p[0] = on ? 255 : 0;   // R
					p[1] = on ? 0 : 255;   // G
					p[2] = 255;            // B
					p[3] = 255;            // A
				}
			_harnessTex = new GpuTexture(/*srgb=*/false,
				GpuTexture::Wrap::ClampToEdge, GpuTexture::Filter::Nearest);
			_harnessTex->uploadRGBA(px.data(), N, N);
		}
		// Logical rect in the engine base-resolution grid; mapped to physical.
		drawTexturedQuad(_harnessTex, CalypsoLogicalRect{ 8, 8, 96, 40 });
	}

	// Physical HD text submitted by family adapters this frame.
	for (const TextSubmit& item : pending)
	{
		GpuTexture* tex = textureForText(item.rasterKey);
		if (tex) drawTexturedQuad(tex, item.rect);
	}

	return true;
}

void CalypsoHdUiOverlay::setHarnessEnabled(bool on)
{
	_harnessEnabled = on;
	_enabledGroupCount = on ? 1 : 0;
}

void CalypsoHdUiOverlay::contextLost()
{
	_controller.noteContextLost();
	_mayGoPhysical = false;
	_vao = 0;
	_vbo = 0;
	_glReady = false;
}

void CalypsoHdUiOverlay::contextRestored()
{
	_controller.noteContextRestored();
}

} // namespace Calypso
} // namespace OpenXcom

// --- Developer harness C ABI -----------------------------------------------

extern "C"
{
EMSCRIPTEN_KEEPALIVE
void calypso_hd_ui_harness(int on)
{
	OpenXcom::Calypso::CalypsoHdUiOverlay::instance().setHarnessEnabled(on != 0);
}
} // extern "C"

#endif // __EMSCRIPTEN__
