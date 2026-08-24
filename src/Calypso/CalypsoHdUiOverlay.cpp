/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * Phase 46.2-HD (Calypso) -- HD UI overlay queue. Whole-file Emscripten guard.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoHdUiOverlay.h"
#include "CalypsoHdHarnessHostState.h"
#include "CalypsoViewportMailbox.h"
#include "CalypsoGlStateGuard.h"
#include "CalypsoPauseMenu.h"

#include <algorithm>
#include <cmath>
#include <emscripten.h>
#include <GLES3/gl3.h>
#include <SDL.h>
#include <SDL_render.h>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "../Engine/Exception.h"
#include "../Engine/GpuInit.h"
#include "../Engine/GpuTexture.h"
#include "../Engine/Shader.h"
#include "../Engine/ShaderManager.h"
#include "../Engine/Logger.h"

namespace OpenXcom
{
namespace Calypso
{

namespace
{
constexpr std::uint32_t kRetryableReadinessFrameBudget = 1800;
}

[[noreturn]] void CalypsoHdUiOverlay::failHdRoute(const std::string& detail)
{
	// The exception cancels the Emscripten loop before State::blit. Keep logical
	// suppression claims active while unwinding: a registered HD route must never
	// reveal native pixels as a recovery frame. Claims clear only at frame start
	// or when the adapter leaves the overlay lifecycle.
	_drawItems.clear();
	_activeThisFrame = false;
	calypsoReportHdRouteError("hd-overlay", detail);
	throw Exception("Calypso HD route failed: " + detail);
}

CalypsoHdUiOverlay& CalypsoHdUiOverlay::instance()
{
	static CalypsoHdUiOverlay s_overlay;
	return s_overlay;
}

bool CalypsoHdUiOverlay::resourcesReadyForFrame() const
{
	return _glReady && _vao != 0 && _hdShader != nullptr && _hdShader->isValid()
		&& _panelShader != nullptr && _panelShader->isValid();
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
	if (adapter == _activeAdapter)
	{
		_activeAdapter = nullptr;
		_retryableReadinessFrames = 0;
		_drawItems.clear();
		_ptrClaim.clear();
		_logicalSuppressedWidgets.clear();
		_frameLiveHandles.clear();
		_physicalStateThisFrame = nullptr;
		_activeThisFrame = false;
		_controller.claims().clear();
	}
}

bool CalypsoHdUiOverlay::widgetClaimed(const void* widget, std::uint64_t frameId) const
{
	if (!widget) return false;
	if (frameId != _controller.frameId()) return false; // stale-frame fail-safe
	auto it = _ptrClaim.find(widget);
	if (it == _ptrClaim.end()) return false;
	return _controller.claims().claimsLogical(it->second, frameId);
}

bool CalypsoHdUiOverlay::logicalWidgetSuppressed(const void* widget, std::uint64_t frameId) const
{
	if (!widget || frameId != _controller.frameId()) return false;
	return std::find(_logicalSuppressedWidgets.begin(), _logicalSuppressedWidgets.end(), widget)
		!= _logicalSuppressedWidgets.end();
}

bool CalypsoHdUiOverlay::logicalStateSuppressed(const void* state, std::uint64_t frameId) const
{
	return state && frameId == _controller.frameId() && state == _physicalStateThisFrame;
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
	_physicalStateThisFrame = nullptr;
	_logicalSuppressedWidgets.clear();
	_ptrClaim.clear();
	_drawItems.clear();
	_frameLiveHandles.clear();
}

void CalypsoHdUiOverlay::prepareFrame(int logicalWidth, int logicalHeight, const void* topState)
{
	beginFrame(logicalWidth, logicalHeight);

	// Drive the registered adapter (if any) whose state is the current top state,
	// so a lower popup regains HD when an upper one is dismissed (GLM #3).
	const CalypsoHdFamilyAdapter* active = nullptr;
	for (const CalypsoHdFamilyAdapter* a : _adapters)
	{
		if (a->topState() == topState) { active = a; break; }
	}
	if (!active)
	{
		_activeAdapter = nullptr;
		_retryableReadinessFrames = 0;
		return;
	}
	if (active != _activeAdapter)
	{
		_activeAdapter = active;
		_retryableReadinessFrames = 0;
	}
	CalypsoHdLogicalSuppression suppression;
	active->collectLogicalSuppression(suppression);
	_logicalSuppressedWidgets = suppression.widgets();
	if (!active->physicalReady())
		failHdRoute("HD route is not physically ready");
	if (!_frozenMetrics.valid())
		failHdRoute("invalid presentation metrics");
	if (!_mayGoPhysical)
		failHdRoute("physical presentation is blocked after context loss or restore");

	// The pre-blit GPU preparation (shader/VAO creation + texture uploads) touches
	// GL state that SDL's renderer caches; bracket it in one state guard so the
	// later SDL_RenderCopy in Screen::flip() is not desynced (Codex #3). The guard
	// restores on every exit path from this scope.
	{
		CalypsoGlStateGuard guard;

		// An enabled HD route is fail-fast. Resource preparation is synchronous;
		// failure cannot be hidden behind a logical/vanilla frame.
		ensureGpu();
		if (!_glReady)
			failHdRoute("GPU resources are unavailable");
		if (!active->completeFrameReady())
		{
			if (active->retryableReadiness())
			{
				if (++_retryableReadinessFrames <= kRetryableReadinessFrameBudget)
					return;
				failHdRoute("HD route readiness budget exhausted");
			}
			failHdRoute("complete HD frame is not ready");
		}

		CalypsoHdFrameBuilder builder;
		active->collect(builder);
		if (builder.empty())
			failHdRoute("adapter collected no HD subgroups");

		// A committed subgroup's order keys must be globally unique this frame: the
		// model treats a full-tuple collision as a submission bug whose paint order
		// would otherwise use non-deterministic insertion order. Detect it before
		// committing and fail the route (external review #11).
		std::set<CalypsoHdOrderKey, bool (*)(const CalypsoHdOrderKey&, const CalypsoHdOrderKey&)>
			committedKeys(&calypsoOrderKeyLess);

		// Resolve + commit each atomic subgroup independently.
		for (const CalypsoHdSubgroup& subgroup : builder.subgroups())
		{
			if (subgroup.items.empty())
				failHdRoute("adapter collected an empty HD subgroup");
			std::vector<ResolvedDraw> resolved;
			resolveSubgroup(subgroup, resolved);

			// Order-key collision check (intra-subgroup or vs an already-committed
			// subgroup). A collision is an adapter contract violation.
			std::vector<CalypsoHdOrderKey> justInserted;
			bool collision = false;
			for (const ResolvedDraw& d : resolved)
			{
				if (!committedKeys.insert(d.order).second) { collision = true; break; }
				justInserted.push_back(d.order);
			}
			if (collision)
			{
				for (const CalypsoHdOrderKey& k : justInserted) committedKeys.erase(k);
				failHdRoute("duplicate HD order key");
			}

			// Commit: take claims for every item, enqueue the resolved draws.
			for (const CalypsoHdItem& item : subgroup.items)
			{
				_controller.claims().add(item.claim);
				if (item.widget) _ptrClaim[item.widget] = item.claim;
			}
			for (ResolvedDraw& d : resolved) _drawItems.push_back(d);
		}
	} // GL state guard restored

	if (_drawItems.empty())
		failHdRoute("active adapter produced no drawable HD items");

	// Deterministic paint order. Uniqueness of the full tuple is already enforced
	// per-subgroup above, so equal keys cannot reach this sort.
	std::stable_sort(_drawItems.begin(), _drawItems.end(),
		[](const ResolvedDraw& a, const ResolvedDraw& b) {
			return calypsoOrderKeyLess(a.order, b.order);
		});

	_activeThisFrame = true;
	_retryableReadinessFrames = 0;
	if (active->suppressLogicalState())
		_physicalStateThisFrame = topState;
}

void CalypsoHdUiOverlay::resolveSubgroup(const CalypsoHdSubgroup& subgroup,
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
		d.panelStyle = item.panelStyle;
		d.hAlign = item.hAlign;
		d.vAlign = item.vAlign;
		d.textScaleX = item.textScaleX;
		d.textScaleY = item.textScaleY;
		d.opacity = item.opacity;

		if (item.kind == CalypsoHdItemKind::Panel)
		{
			GpuTexture* white = whiteTexture();
			if (!white || !white->isValid())
				failHdRoute("panel texture upload failed for item "
					+ std::to_string(item.order.stableId) + ":"
					+ std::to_string(item.order.itemId));
			d.tex = white;
		}
		else
		{
			GpuTexture* tex = textureForText(item.rasterKey);
			if (!tex || !tex->isValid())
				failHdRoute("text texture raster or upload failed for item "
					+ std::to_string(item.order.stableId) + ":"
					+ std::to_string(item.order.itemId));
			d.tex = tex;
			d.naturalW = (int)tex->width();
			d.naturalH = (int)tex->height();

			// A guarded multi-line texture is an atomic-fit item. Generic glyphs
			// may deliberately use their box as a UV clip target, but doing that to
			// a contract-owned wrapped surface discards the very top/bottom guard
			// rows that guarantee intact caps, baselines and descenders.
			const bool guardedMultiLine = item.rasterKey.wrapWidth > 0
				&& (item.rasterKey.lineHeightPx > 0 || item.rasterKey.lineHeightMilliPx > 0);
			if (guardedMultiLine)
			{
				const CalypsoPhysRect box = calypsoMapLogicalRect(d.rect, _frozenMetrics);
				const int projectedTextureHeight = std::max(1,
					(int)std::lround(d.naturalH * d.textScaleY));
				const bool fits = !box.empty() && projectedTextureHeight <= box.h;
				if (calypsoHarnessHostUp(calypsoHarnessSession()))
				{
					EM_ASM_({
						const evidence = globalThis.__calypsoHdTextFitEvidence
							|| (globalThis.__calypsoHdTextFitEvidence = Object.create(null));
						const key = String($0) + ':' + String($1);
						const row = Object.create(null);
						row.stableId = $0;
						row.itemId = $1;
						row.naturalTextureHeight = $2;
						row.projectedTextureHeight = $3;
						row.boxHeight = $4;
						row.fits = !!$5;
						evidence[key] = row;
						let output = document.getElementById('calypso-hd-text-fit-evidence');
						if (!output)
						{
							output = document.createElement('output');
							output.id = 'calypso-hd-text-fit-evidence';
							output.hidden = true;
							document.body.appendChild(output);
						}
						output.textContent = JSON.stringify(evidence);
					}, item.order.stableId, item.order.itemId, d.naturalH,
						projectedTextureHeight, box.h, fits ? 1 : 0);
				}
				if (!fits)
				{
					failHdRoute("guarded multi-line texture "
						+ std::to_string(projectedTextureHeight) + "px exceeds "
						+ std::to_string(box.h) + "px box for item "
						+ std::to_string(item.order.stableId) + ":"
						+ std::to_string(item.order.itemId));
				}
			}
		}
		tmp.push_back(d);
	}
	for (ResolvedDraw& d : tmp) out.push_back(d);
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

	if (!_panelShader)
	{
		_panelShader = new Shader();
		if (!_panelShader->loadFromEmbedded("hd_ui_panel"))
		{
			Log(LOG_ERROR) << "CalypsoHdUiOverlay: failed to load 'hd_ui_panel' shader";
			delete _panelShader;
			_panelShader = nullptr;
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

void CalypsoHdUiOverlay::uploadQuadVerts(const CalypsoPhysRect& r)
{
	const float physW = (float)_frozenMetrics.physicalWidth;
	const float physH = (float)_frozenMetrics.physicalHeight;

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
}

bool CalypsoHdUiOverlay::drawPhysQuad(GpuTexture* tex, const CalypsoPhysRect& r,
	std::uint32_t colorRgba, float u0, float v0, float u1, float v1, float opacity)
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
	_hdShader->setUniform1f("u_opacity", opacity); // Phase 46.4-F33 opening motion
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

bool CalypsoHdUiOverlay::drawStyledPanel(const ResolvedDraw& d)
{
	if (!_panelShader || !_panelShader->isValid() || !_vao) return false;
	if (!_frozenMetrics.valid()) return false;

	const CalypsoPhysRect shape = calypsoMapLogicalRect(d.rect, _frozenMetrics);
	if (shape.empty()) return true; // nothing to draw is not a failure

	// Design px -> physical px: same mapping family as the rect edges. The
	// average of the axis scales keeps radii/borders circular under the
	// stretched-canvas presentation.
	const float pxScale = (float)((_frozenMetrics.scaleX + _frozenMetrics.scaleY) * 0.5);
	const CalypsoHdPanelStyle& st = d.panelStyle;

	float radius = st.radiusPx * pxScale;
	const float maxRadius = 0.5f * (float)std::min(shape.w, shape.h);
	if (radius > maxRadius) radius = maxRadius;
	if (radius < 0.0f) radius = 0.0f;
	const float borderWidth = std::max(0.0f, st.borderWidthPx * pxScale);
	const float cutCorner = std::max(0.0f, st.cutCornerPx * pxScale);
	const float glowRadius = std::max(0.0f, st.glowRadiusPx * pxScale);
	const int pad = (int)std::ceil(glowRadius);

	// The quad pads the shape on every side so the glow falloff fits; the
	// shape's offset inside the quad feeds the SDF coordinate reconstruction.
	const CalypsoPhysRect quad{
		shape.x - pad, shape.y - pad,
		shape.w + pad * 2, shape.h + pad * 2 };
	if (quad.empty()) return true;
	uploadQuadVerts(quad);

	auto rgba = [](std::uint32_t c, float out[4]) {
		out[0] = ((c >> 24) & 0xFF) / 255.0f;
		out[1] = ((c >> 16) & 0xFF) / 255.0f;
		out[2] = ((c >> 8) & 0xFF) / 255.0f;
		out[3] = (c & 0xFF) / 255.0f;
	};
	float borderC[4], fillT[4], fillB[4], glowC[4];
	rgba(st.borderColorRgba, borderC);
	rgba(st.fillTopRgba, fillT);
	rgba(st.fillBottomRgba, fillB);
	rgba(st.glowRgba, glowC);

	_panelShader->use();
	_panelShader->setUniform2f("u_quadSize", (float)quad.w, (float)quad.h);
	_panelShader->setUniform2f("u_shapeOffset", (float)pad, (float)pad);
	_panelShader->setUniform2f("u_size", (float)shape.w, (float)shape.h);
	_panelShader->setUniform1f("u_radius", radius);
	_panelShader->setUniform1f("u_cutCorner", cutCorner);
	_panelShader->setUniform1i("u_shapeKind", static_cast<int>(st.shape));
	_panelShader->setUniform1f("u_borderWidth", borderWidth);
	_panelShader->setUniform4f("u_borderColor", borderC[0], borderC[1], borderC[2], borderC[3]);
	_panelShader->setUniform4f("u_fillTop", fillT[0], fillT[1], fillT[2], fillT[3]);
	_panelShader->setUniform4f("u_fillBottom", fillB[0], fillB[1], fillB[2], fillB[3]);
	_panelShader->setUniform2f("u_gradDir", st.gradDirX, st.gradDirY);
	_panelShader->setUniform4f("u_glowColor", glowC[0], glowC[1], glowC[2], glowC[3]);
	_panelShader->setUniform1f("u_glowRadius", glowRadius);
	_panelShader->setUniform1f("u_opacity", d.opacity); // Phase 46.4-F33 opening motion

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

	const int gw = std::max(1, (int)std::lround(d.naturalW * d.textScaleX));
	const int gh = std::max(1, (int)std::lround(d.naturalH * d.textScaleY));
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
	return drawPhysQuad(d.tex, vis, 0 /*text tex is pre-coloured*/, u0, v0, u1, v1, d.opacity);
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
			// Pin-aware eviction: the LRU never returns a frame-pinned handle, so
			// pinned textures stay resident AND accounted (external review #7).
			evictTextTextures(_textTexLru.touch(hit->second, bytes, &_frameLiveHandles));
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

	// Pin-aware eviction: the LRU never returns a frame-pinned handle, so pinned
	// textures stay resident AND accounted (external review #7).
	evictTextTextures(_textTexLru.touch(handle, byteCost, &_frameLiveHandles));
	return tex;
}

void CalypsoHdUiOverlay::evictTextTextures(const std::vector<std::uint64_t>& evicted)
{
	// Pin-aware LRU guarantees no frame-pinned handle is ever returned here, so
	// every evicted handle is safe to free -- no worklist / re-touch cascade, and
	// no risk of a resident-but-untracked texture (external review #7). The
	// _frameLiveHandles guard below is belt-and-suspenders: a pinned handle would
	// simply be left resident + accounted (still in the LRU), never leaked.
	for (std::uint64_t ev : evicted)
	{
		if (_frameLiveHandles.count(ev)) continue; // never free a pinned texture
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

	if (!_mayGoPhysical || !_frozenMetrics.valid())
	{
		if (committed) failHdRoute("invalid presentation metrics at draw stage");
		return true;
	}

	// Boundary zero: flush SDL's batched composite before any raw GL. A failed
	// flush means the boundary is unsafe -- do not draw.
	if (renderer && SDL_RenderFlush(renderer) != 0)
	{
		if (committed)
			failHdRoute("SDL_RenderFlush failed: " + std::string(SDL_GetError()));
		return true;
	}

	ensureGpu();
	if (!_glReady) // GL lost between prepare and present, or harness cold-start
	{
		if (committed) failHdRoute("GPU resources are unavailable at draw stage");
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
			drawn = d.panelStyle.styled ? drawStyledPanel(d)
			                            : drawLogicalQuad(d.tex, d.rect, d.colorRgba);
		else
			drawn = drawGlyph(d);
		if (!drawn) ok = false;
	}

	// Any GL error raised across the whole stage counts as a draw failure -- the
	// individual drawPhysQuad calls don't report GL errors (Codex #2).
	if (glGetError() != GL_NO_ERROR)
	{
		ok = false;
	}

	if (!ok)
	{
		if (committed) failHdRoute("HD draw stage failed");
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
