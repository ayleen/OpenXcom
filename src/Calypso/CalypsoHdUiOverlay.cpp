/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * Phase 46.2-HD (Calypso) -- HD UI overlay queue. Whole-file Emscripten guard.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoHdUiOverlay.h"
#include "CalypsoViewportMailbox.h"
#include "CalypsoGlStateGuard.h"

#include <algorithm>
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

void CalypsoHdUiOverlay::registerAdapter(const CalypsoHdFamilyAdapter* adapter)
{
	_adapter = adapter;
}

void CalypsoHdUiOverlay::clearAdapter(const CalypsoHdFamilyAdapter* adapter)
{
	if (_adapter == adapter) _adapter = nullptr;
}

bool CalypsoHdUiOverlay::widgetClaimed(const void* widget, std::uint64_t frameId) const
{
	if (!widget) return false;
	if (frameId != _controller.frameId()) return false; // stale-frame fail-safe
	auto it = _ptrClaim.find(widget);
	if (it == _ptrClaim.end()) return false;
	return _controller.claims().claimsLogical(it->second, frameId);
}

void CalypsoHdUiOverlay::beginFrame(int logicalWidth, int logicalHeight)
{
	// Backing-store poll: canvas width/height are physical device pixels. Only
	// polled when an adapter is registered -- while dormant the metrics are never
	// used and the JS observation already delivers physical dims on every resize.
	if (_adapter || _harnessEnabled)
	{
		const int physW = (int)EM_ASM_INT({ return document.getElementById('canvas').width; });
		const int physH = (int)EM_ASM_INT({ return document.getElementById('canvas').height; });
		if (physW > 0 && physH > 0)
		{
			calypsoHdViewportBackingStorePoll(physW, physH);
		}
	}

	_frozenMetrics = calypsoHdBuildPresentationMetrics(logicalWidth, logicalHeight);

	const CalypsoHdFrameController::BeginResult r =
		_controller.beginFrame(_frozenMetrics.valid());
	_mayGoPhysical = r.mayGoPhysical;

	// Reset per-frame state (A7: never sticky).
	_activeThisFrame = false;
	_ptrClaim.clear();
	_drawItems.clear();
}

void CalypsoHdUiOverlay::prepareFrame(int logicalWidth, int logicalHeight, const void* topState)
{
	beginFrame(logicalWidth, logicalHeight);

	if (!_mayGoPhysical || !_frozenMetrics.valid()) return;
	if (!_adapter || _adapter->topState() != topState) return;

	// GL must be ready to raster/upload; if not, no subgroup can be Ready and the
	// whole popup renders logically this frame (caches warm for next frame).
	ensureGpu();
	if (!_glReady) return;

	CalypsoHdFrameBuilder builder;
	_adapter->collect(builder);
	if (builder.empty()) return;

	// Resolve + commit each atomic subgroup independently.
	for (const CalypsoHdSubgroup& subgroup : builder.subgroups())
	{
		std::vector<ResolvedDraw> resolved;
		if (!resolveSubgroup(subgroup, resolved)) continue; // Not Ready -> logical

		// Commit: take claims for every item, enqueue the resolved draws.
		for (const CalypsoHdItem& item : subgroup.items)
		{
			_controller.claims().add(item.claim);
			if (item.widget) _ptrClaim[item.widget] = item.claim;
		}
		for (ResolvedDraw& d : resolved) _drawItems.push_back(d);
	}

	if (_drawItems.empty()) return;

	// Deterministic order; a complete-tuple collision is a submission bug.
	std::stable_sort(_drawItems.begin(), _drawItems.end(),
		[](const ResolvedDraw& a, const ResolvedDraw& b) {
			return calypsoOrderKeyLess(a.order, b.order);
		});
	for (std::size_t i = 1; i < _drawItems.size(); ++i)
	{
		if (_drawItems[i - 1].order == _drawItems[i].order)
		{
			Log(LOG_WARNING) << "CalypsoHdUiOverlay: duplicate HD order key (submission bug)";
			break;
		}
	}

	_activeThisFrame = true;
}

bool CalypsoHdUiOverlay::resolveSubgroup(const CalypsoHdSubgroup& subgroup,
	std::vector<ResolvedDraw>& out)
{
	std::vector<ResolvedDraw> tmp;
	tmp.reserve(subgroup.items.size());
	for (const CalypsoHdItem& item : subgroup.items)
	{
		ResolvedDraw d;
		d.order = item.order;
		d.kind = item.kind;
		d.rect = item.rect;
		d.colorRgba = item.colorRgba;
		d.hAlign = item.hAlign;
		d.vAlign = item.vAlign;

		if (item.kind == CalypsoHdItemKind::Panel)
		{
			GpuTexture* white = whiteTexture();
			if (!white || !white->isValid()) return false;
			d.tex = white;
		}
		else
		{
			GpuTexture* tex = textureForText(item.rasterKey);
			if (!tex || !tex->isValid()) return false;
			d.tex = tex;
			d.naturalW = (int)tex->width();
			d.naturalH = (int)tex->height();
		}
		tmp.push_back(d);
	}
	for (ResolvedDraw& d : tmp) out.push_back(d);
	return true;
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
		// Context-loss ladder (Phase 11.13 / Phase M): on restore, zero stale
		// VAO/VBO handles, bump the context generation, and free every stale-gen
		// text texture so the context-generation-keyed cache actually segregates.
		_gpuAliveFlag = std::make_shared<bool>(true);
		ShaderManager::instance().registerResetCallback(_gpuAliveFlag, [this]() {
			onContextRestored();
		});
	}

	_glReady = true;
}

void CalypsoHdUiOverlay::onContextRestored()
{
	_vao = 0;
	_vbo = 0;
	_glReady = false;
	++_contextGen;      // A6: segregate the texture cache by generation
	dropTextTextures(); // free stale-generation GpuTextures
	_whiteTex = nullptr; // recreated lazily (GpuTexture self-recovers, but re-make)
	_harnessTex = nullptr;
	_controller.noteContextRestored();
}

GpuTexture* CalypsoHdUiOverlay::whiteTexture()
{
	if (_whiteTex && _whiteTex->isValid()) return _whiteTex;
	if (!_whiteTex)
	{
		const std::uint8_t px[4] = { 255, 255, 255, 255 };
		_whiteTex = new GpuTexture(/*srgb=*/false,
			GpuTexture::Wrap::ClampToEdge, GpuTexture::Filter::Nearest);
		_whiteTex->uploadRGBA(px, 1, 1);
	}
	return _whiteTex;
}

bool CalypsoHdUiOverlay::drawPhysQuad(GpuTexture* tex, const CalypsoPhysRect& r,
	std::uint32_t colorRgba)
{
	if (!tex || !tex->isValid() || !_hdShader || !_hdShader->isValid() || !_vao) return false;
	const float physW = (float)_frozenMetrics.physicalWidth;
	const float physH = (float)_frozenMetrics.physicalHeight;
	if (r.empty() || physW <= 0.0f || physH <= 0.0f) return false;

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

	_hdShader->use();
	if (colorRgba == 0)
	{
		_hdShader->setUniform4f("u_color", 0.0f, 0.0f, 0.0f, 0.0f); // unset => opaque white
	}
	else
	{
		_hdShader->setUniform4f("u_color",
			((colorRgba >> 24) & 0xFF) / 255.0f,
			((colorRgba >> 16) & 0xFF) / 255.0f,
			((colorRgba >> 8) & 0xFF) / 255.0f,
			(colorRgba & 0xFF) / 255.0f);
	}
	tex->bind(0);
	_hdShader->setUniform1i("u_tex", 0);

	glBindVertexArray(_vao);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
	return true;
}

bool CalypsoHdUiOverlay::drawLogicalQuad(GpuTexture* tex, const CalypsoLogicalRect& logical,
	std::uint32_t colorRgba)
{
	const CalypsoPhysRect r = calypsoMapLogicalRect(logical, _frozenMetrics);
	return drawPhysQuad(tex, r, colorRgba);
}

bool CalypsoHdUiOverlay::drawGlyph(const ResolvedDraw& d)
{
	// Map the widget box to physical, then place the natural-size glyph bitmap
	// inside it per (hAlign,vAlign). The box is the layout + clip target; the
	// glyph is NOT stretched to fill it (remediation B2).
	const CalypsoPhysRect box = calypsoMapLogicalRect(d.rect, _frozenMetrics);
	if (box.empty()) return true; // nothing to draw is not a failure

	int gw = d.naturalW, gh = d.naturalH;
	if (gw <= 0 || gh <= 0) return true;
	// Clamp to the box (overflow should be prevented by metrics-based sizing +
	// wrapping; clamping keeps a stray overflow inside its box rather than
	// bleeding across the popup).
	if (gw > box.w) gw = box.w;
	if (gh > box.h) gh = box.h;

	int gx = box.x;
	switch (d.hAlign)
	{
	case CalypsoHdHAlign::Center: gx = box.x + (box.w - gw) / 2; break;
	case CalypsoHdHAlign::Right:  gx = box.x + (box.w - gw); break;
	case CalypsoHdHAlign::Left:   default: gx = box.x; break;
	}
	int gy = box.y;
	switch (d.vAlign)
	{
	case CalypsoHdVAlign::Middle: gy = box.y + (box.h - gh) / 2; break;
	case CalypsoHdVAlign::Bottom: gy = box.y + (box.h - gh); break;
	case CalypsoHdVAlign::Top:    default: gy = box.y; break;
	}

	CalypsoPhysRect dst{ gx, gy, gw, gh };
	return drawPhysQuad(d.tex, dst, 0 /*text tex is pre-coloured*/);
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
	// Dormant unless a subgroup (or the harness) committed this frame.
	if (!_activeThisFrame && !_harnessEnabled) return true;
	if (!_mayGoPhysical || !_frozenMetrics.valid()) return true;

	// Boundary zero: flush SDL's batched composite before any raw GL.
	if (renderer) SDL_RenderFlush(renderer);

	ensureGpu();
	if (!_glReady) return true; // GL not ready: this frame stays logical

	// One scoped guard around the whole boundary-zero section (A4).
	CalypsoGlStateGuard guard;
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	bool ok = true;

	if (_harnessEnabled)
	{
		if (!_harnessTex)
		{
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
		drawLogicalQuad(_harnessTex, CalypsoLogicalRect{ 8, 8, 96, 40 }, 0);
	}

	// Committed items in deterministic order (panels + text interleaved by key).
	for (const ResolvedDraw& d : _drawItems)
	{
		bool drawn = true;
		if (d.kind == CalypsoHdItemKind::Panel)
			drawn = drawLogicalQuad(d.tex, d.rect, d.colorRgba);
		else
			drawn = drawGlyph(d);
		if (!drawn) ok = false;
	}

	if (!ok)
	{
		// Post-commit draw failure: discard claims, latch one wholly-logical
		// frame, and tell Screen to skip present so no half-popup is shown.
		_controller.notePostClaimFailure();
		_drawItems.clear();
		_activeThisFrame = false;
		return false;
	}

	// Consume this frame's committed draws so a second, unpaired Screen::flip()
	// (e.g. a loading screen not routed through Game::run's prepareFrame) does
	// not re-draw a stale popup. beginFrame() re-populates them next frame.
	_drawItems.clear();
	_activeThisFrame = false;
	return true;
}

void CalypsoHdUiOverlay::setHarnessEnabled(bool on)
{
	_harnessEnabled = on;
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
	onContextRestored();
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
