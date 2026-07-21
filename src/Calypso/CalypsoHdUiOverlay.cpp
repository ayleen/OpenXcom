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
#include <unordered_set>
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
	if (!adapter) return;
	if (std::find(_adapters.begin(), _adapters.end(), adapter) == _adapters.end())
		_adapters.push_back(adapter);
}

void CalypsoHdUiOverlay::clearAdapter(const CalypsoHdFamilyAdapter* adapter)
{
	_adapters.erase(std::remove(_adapters.begin(), _adapters.end(), adapter), _adapters.end());
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
	if (!_adapters.empty() || _harnessEnabled)
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
	_frameLiveHandles.clear();
}

void CalypsoHdUiOverlay::prepareFrame(int logicalWidth, int logicalHeight, const void* topState)
{
	beginFrame(logicalWidth, logicalHeight);

	if (!_mayGoPhysical || !_frozenMetrics.valid()) return;

	// Drive the registered adapter (if any) whose state is the current top state,
	// so a lower popup regains HD when an upper one is dismissed (GLM #3).
	const CalypsoHdFamilyAdapter* active = nullptr;
	for (const CalypsoHdFamilyAdapter* a : _adapters)
	{
		if (a->topState() == topState) { active = a; break; }
	}
	if (!active) return;

	// The pre-blit GPU preparation (shader/VAO creation + texture uploads) touches
	// GL state that SDL's renderer caches; bracket it in one state guard so the
	// later SDL_RenderCopy in Screen::flip() is not desynced (Codex #3). The guard
	// restores on every exit path from this scope.
	{
		CalypsoGlStateGuard guard;

		// GL must be ready to raster/upload; if not, no subgroup can be Ready and
		// the whole popup renders logically this frame (caches warm next frame).
		ensureGpu();
		if (!_glReady) return;

		CalypsoHdFrameBuilder builder;
		active->collect(builder);
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
	} // GL state guard restored

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
	// Delete (not just null) the shared textures so they are not leaked per
	// context restore (Fable #8); they are recreated lazily.
	delete _whiteTex;   _whiteTex = nullptr;
	delete _harnessTex; _harnessTex = nullptr;
	_controller.noteContextRestored();
}

GpuTexture* CalypsoHdUiOverlay::whiteTexture()
{
	if (_whiteTex && _whiteTex->isValid()) return _whiteTex;
	// Non-null but invalid (a failed first upload / lost context): rebuild rather
	// than return a dead texture that leaves panels permanently Not Ready (Fable #8).
	if (_whiteTex) { delete _whiteTex; _whiteTex = nullptr; }
	const std::uint8_t px[4] = { 255, 255, 255, 255 };
	_whiteTex = new GpuTexture(/*srgb=*/false,
		GpuTexture::Wrap::ClampToEdge, GpuTexture::Filter::Nearest);
	_whiteTex->uploadRGBA(px, 1, 1);
	return _whiteTex;
}

bool CalypsoHdUiOverlay::drawPhysQuad(GpuTexture* tex, const CalypsoPhysRect& r,
	std::uint32_t colorRgba, float u0, float v0, float u1, float v1)
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
		x0, y0, u0, v0,
		x1, y0, u1, v0,
		x0, y1, u0, v1,
		x0, y1, u0, v1,
		x1, y0, u1, v0,
		x1, y1, u1, v1,
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

	const int gw = d.naturalW, gh = d.naturalH;
	if (gw <= 0 || gh <= 0) return true;

	// Place the FULL natural-size glyph rect per alignment (it may extend outside
	// the box), then intersect with the box; the visible sub-rect's offset within
	// the natural rect gives the UV crop. So a centered oversized glyph shows its
	// CENTER, right/bottom show the correct edge (Codex #7), and a glyph that fits
	// is drawn 1:1 -- the box is a clip target, never a stretch target (B2).
	int nx = box.x;
	switch (d.hAlign)
	{
	case CalypsoHdHAlign::Center: nx = box.x + (box.w - gw) / 2; break;
	case CalypsoHdHAlign::Right:  nx = box.x + (box.w - gw); break;
	case CalypsoHdHAlign::Left:   default: nx = box.x; break;
	}
	int ny = box.y;
	switch (d.vAlign)
	{
	case CalypsoHdVAlign::Middle: ny = box.y + (box.h - gh) / 2; break;
	case CalypsoHdVAlign::Bottom: ny = box.y + (box.h - gh); break;
	case CalypsoHdVAlign::Top:    default: ny = box.y; break;
	}

	const CalypsoPhysRect natural{ nx, ny, gw, gh };
	CalypsoPhysRect vis;
	if (!calypsoClipPhysRect(natural, box, vis)) return true; // fully off-box
	const float u0 = (float)(vis.x - nx) / (float)gw;
	const float v0 = (float)(vis.y - ny) / (float)gh;
	const float u1 = (float)(vis.x - nx + vis.w) / (float)gw;
	const float v1 = (float)(vis.y - ny + vis.h) / (float)gh;
	return drawPhysQuad(d.tex, vis, 0 /*text tex is pre-coloured*/, u0, v0, u1, v1);
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
			_frameLiveHandles.insert(hit->second);            // pin this frame (Fable #3)
			evictTextTextures(_textTexLru.touch(hit->second, bytes)); // process evictions (Fable #9)
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
	_frameLiveHandles.insert(handle); // pin this frame (Fable #3)

	evictTextTextures(_textTexLru.touch(handle, byteCost));
	return tex;
}

void CalypsoHdUiOverlay::evictTextTextures(const std::vector<std::uint64_t>& evicted)
{
	// Worklist so a re-touch of a frame-pinned handle can chase the SECONDARY
	// evictions it causes, freeing every non-pinned texture rather than leaving
	// it resident-but-untracked (Codex #8). Each handle is processed once, so a
	// frame where every texture is pinned still terminates (over budget for that
	// one frame; it self-heals next frame when nothing is pinned).
	std::vector<std::uint64_t> work(evicted.begin(), evicted.end());
	std::unordered_set<std::uint64_t> seen;
	while (!work.empty())
	{
		const std::uint64_t ev = work.back();
		work.pop_back();
		if (!seen.insert(ev).second) continue; // already handled
		auto kit = _texHandleToKey.find(ev);
		if (kit == _texHandleToKey.end()) continue;

		if (_frameLiveHandles.count(ev))
		{
			// Referenced by _drawItems this frame: must not be freed mid-frame.
			// Re-touch to keep it resident + LRU-tracked, and chase the secondary
			// evictions that re-insert causes.
			auto tit = _textTextures.find(kit->second);
			if (tit != _textTextures.end())
			{
				const std::size_t b = (std::size_t)tit->second->width()
					* (std::size_t)tit->second->height() * 4u;
				for (std::uint64_t sec : _textTexLru.touch(ev, b)) work.push_back(sec);
			}
			continue;
		}

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
	const bool committed = _activeThisFrame; // committed HD draws exist this frame

	// Dormant unless a subgroup (or the harness) committed this frame.
	if (!committed && !_harnessEnabled) return true;

	// A committed frame that cannot present its HD draws must skip present and
	// latch a wholly-logical next frame (Codex #2): the widgets it claimed were
	// already suppressed at blit, so presenting now would show a blank/partial
	// popup. A harness-only frame (nothing committed) just skips silently.
	auto failCommitted = [this]() {
		_controller.notePostClaimFailure(); // clears _controller.claims()
		_ptrClaim.clear();
		_drawItems.clear();
		_activeThisFrame = false;
	};

	if (!_mayGoPhysical || !_frozenMetrics.valid())
	{
		if (committed) { failCommitted(); return false; }
		return true;
	}

	// Boundary zero: flush SDL's batched composite before any raw GL. A failed
	// flush means the boundary is unsafe -- do not draw.
	if (renderer && SDL_RenderFlush(renderer) != 0)
	{
		if (committed) { failCommitted(); return false; }
		return true;
	}

	ensureGpu();
	if (!_glReady) // GL lost between prepare and present, or harness cold-start
	{
		if (committed) { failCommitted(); return false; }
		return true;
	}

	// One scoped guard around the whole boundary-zero section (A4).
	CalypsoGlStateGuard guard;
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	while (glGetError() != GL_NO_ERROR) {} // clear any pre-existing GL error

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

	// Any GL error raised across the whole stage counts as a draw failure -- the
	// individual drawPhysQuad calls don't report GL errors (Codex #2).
	if (glGetError() != GL_NO_ERROR) ok = false;

	if (!ok)
	{
		if (committed)
		{
			// Post-commit draw failure: discard claims, latch one wholly-logical
			// frame, and tell Screen to skip present so no half-popup is shown.
			failCommitted();
			return false;
		}
		// Harness-only failure: nothing was committed/claimed, just present clean.
	}

	// Consume this frame's committed draws AND claims symmetrically (Fable #4):
	// a second, unpaired Screen::flip() (e.g. a loading screen not routed through
	// Game::run's prepareFrame) must neither re-draw a stale popup NOR suppress a
	// widget's logical blit. beginFrame() re-populates both next frame.
	_drawItems.clear();
	_ptrClaim.clear();
	_controller.claims().clear();
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
