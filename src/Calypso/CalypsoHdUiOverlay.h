#pragma once
/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * Phase 46.2-HD (Calypso) -- the screen-owned HD UI overlay queue.
 *
 * One instance for the renderer's lifetime. It owns the per-frame lifecycle
 * (CalypsoHdFrameController), the ONE frozen presentation-metrics snapshot for
 * the current frame, the ONE claim store (identity + an ephemeral widget-ptr
 * lookup), the shared GPU resources, the bounded raster/texture caches, and the
 * ordered committed draw list.
 *
 * Remediation lifecycle (A1/A3/A7):
 *   prepareFrame()  -- called at the PRE-BLIT boundary (Game::run, before any
 *                      visible State::blit): freeze metrics, advance the frame,
 *                      reset per-frame state, ask the active family adapter to
 *                      collect an immutable description, then raster+upload each
 *                      atomic subgroup and COMMIT claims + draws only when the
 *                      active HD route is fully ready. Any preparation failure
 *                      throws; an enabled route never exposes vanilla widgets.
 *   (State::blit)   -- claimed widgets skip their blit (widgetClaimed()).
 *   renderStages()  -- called after the legacy composite in Screen::flip():
 *                      draws the already-committed, already-uploaded items in
 *                      deterministic order behind one GL state guard. A draw
 *                      failure throws before SDL_RenderPresent.
 *
 * With no adapter committing this frame the queue is DORMANT: prepareFrame does
 * the cheap begin only, renderStages early-returns, and native behaviour is
 * byte-for-byte unchanged.
 *
 * Whole-file Emscripten guard (Phase 36). The heavy logic it composes
 * (CalypsoHdFrameController, presentation metrics, claim/readiness/order model)
 * is portable and natively tested.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoHdFrameController.h"
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoHdUiModel.h"
#include "CalypsoHdTextRasterKey.h"
#include "CalypsoHdTextRaster.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

struct SDL_Renderer;

namespace OpenXcom
{
class Shader;
class GpuTexture;

namespace Calypso
{

class CalypsoHdUiOverlay
{
public:
	static CalypsoHdUiOverlay& instance();

	/// Pre-blit boundary (Game::run, before visible State::blit). Poll the
	/// canvas backing store, freeze ONE presentation-metrics snapshot, advance
	/// the frame controller (clearing last frame's claims), then -- if physical
	/// output is permitted and the active adapter feeds `topState` -- collect,
	/// raster, upload, and commit every subgroup. An active adapter fails fast on
	/// any preparation error; it never falls back to vanilla. `logicalWidth/Height` are
	/// the engine's logical base resolution (Options::baseX/YResolution).
	void prepareFrame(int logicalWidth, int logicalHeight, const void* topState);

	/// After the legacy composite in Screen::flip(): draw this frame's committed
	/// items in order behind one GL state guard. Dormant (no-op, returns true)
	/// unless a subgroup committed this frame. An enabled HD route throws if its
	/// committed items cannot be drawn.
	bool renderStages(SDL_Renderer* renderer);

	/// Register/clear the active family adapter. A state registers itself while
	/// it is top and clears it on destruction (iff still active). Non-owning.
	void registerAdapter(const CalypsoHdFamilyAdapter* adapter);
	void clearAdapter(const CalypsoHdFamilyAdapter* adapter);

	/// True iff `widget` had a logical visual claimed this frame AND `frameId`
	/// is the current frame. Called from Text/TextButton/Window blit() to skip
	/// the logical draw exactly when the overlay took the visual over.
	bool widgetClaimed(const void* widget, std::uint64_t frameId) const;

	/// True when a covered-state widget must not enter the logical composite.
	/// Unlike a physical claim this remains active during an adapter's opening
	/// animation, so an underlying vanilla window cannot flash through.
	bool logicalWidgetSuppressed(const void* widget, std::uint64_t frameId) const;

	/// True after the active atomic HD subgroup commits for this exact state.
	/// State::blit uses this global gate to prevent an omitted top-level widget
	/// from leaking vanilla pixels beneath a physical HD form.
	bool logicalStateSuppressed(const void* state, std::uint64_t frameId) const;

	/// Developer harness (Emscripten export): a single physical-resolution test
	/// quad through the real GL path. Off by default.
	void setHarnessEnabled(bool on);

	/// WebGL context lost/restored -- forwarded to the frame controller and the
	/// GPU caches. Also driven by the ShaderManager reset-callback ladder.
	void contextLost();
	void contextRestored();

	const CalypsoHdPresentationMetrics& frozenMetrics() const { return _frozenMetrics; }
	bool mayGoPhysical() const { return _mayGoPhysical; }
	bool resourcesReadyForFrame() const;
	std::uint64_t frameId() const { return _controller.frameId(); }

	/// True once a subgroup (or the harness) committed physical output this
	/// frame. Derived per frame; never sticky (A7).
	bool activeThisFrame() const { return _activeThisFrame; }

	/// Fail closed for a registered HD world route; never expose partial or vanilla output.
	[[noreturn]] void failHdRoute(const std::string& detail);

private:
	CalypsoHdUiOverlay() = default;

	/// One resolved, uploaded draw ready for renderStages. Panels use the shared
	/// white texture; text carries its natural glyph size for in-box placement.
	/// A Panel with panelStyle.styled paints via the hd_ui_panel SDF shader
	/// instead of the tinted white quad.
	struct ResolvedDraw
	{
		CalypsoHdOrderKey order;
		CalypsoHdItemKind kind = CalypsoHdItemKind::Panel;
		CalypsoLogicalRect rect;
		std::uint32_t colorRgba = 0;
		CalypsoHdPanelStyle panelStyle;
		GpuTexture* tex = nullptr;
		int naturalW = 0;
		int naturalH = 0;
		float textScaleX = 1.0f;
		float textScaleY = 1.0f;
		CalypsoHdHAlign hAlign = CalypsoHdHAlign::Left;
		CalypsoHdVAlign vAlign = CalypsoHdVAlign::Middle;
		float opacity = 1.0f; // presentation opacity (opening motion)
	};

	void beginFrame(int logicalWidth, int logicalHeight);
	void ensureGpu();
	void onContextRestored();

	/// Resolve every item of one subgroup to an uploaded texture. Appends all
	/// resolved draws to `out`; any failure throws instead of exposing vanilla.
	void resolveSubgroup(const CalypsoHdSubgroup& subgroup, std::vector<ResolvedDraw>& out);

	/// Core NDC draw of `tex` into a physical device-pixel rect, sampling the
	/// texture over the UV sub-rect [u0,v0]-[u1,v1] (default full 0..1). Returns
	/// false if the shader/VAO/texture are not drawable. Assumes the GL guard +
	/// blend are already set by renderStages.
	bool drawPhysQuad(GpuTexture* tex, const CalypsoPhysRect& r, std::uint32_t colorRgba,
		float u0 = 0.0f, float v0 = 0.0f, float u1 = 1.0f, float v1 = 1.0f,
		float opacity = 1.0f);
	/// Map a logical rect to physical and draw (panels).
	bool drawLogicalQuad(GpuTexture* tex, const CalypsoLogicalRect& logical, std::uint32_t colorRgba);
	/// SDF styled panel (rounded/border/gradient/glow) via hd_ui_panel; the
	/// quad is padded by the glow radius so the falloff fits inside it.
	bool drawStyledPanel(const ResolvedDraw& d);
	/// Place a natural-size glyph bitmap inside the mapped box per alignment and
	/// draw it (text) -- the box is the layout/clip target, not a stretch target.
	bool drawGlyph(const ResolvedDraw& d);
	/// Upload one quad's vertices (NDC positions + full UVs) into the shared
	/// VBO/VAO. Shared by drawPhysQuad and drawStyledPanel.
	void uploadQuadVerts(const CalypsoPhysRect& r);

	GpuTexture* whiteTexture();
	GpuTexture* textureForText(const CalypsoHdTextRasterKey& rasterKey);
	/// Process an LRU eviction list: free + forget each evicted text texture,
	/// EXCEPT handles pinned this frame (still referenced by _drawItems), which
	/// are re-touched to stay resident and tracked (Fable #3/#9).
	void evictTextTextures(const std::vector<std::uint64_t>& evicted);
	void dropTextTextures();

	CalypsoHdFrameController _controller;
	CalypsoHdPresentationMetrics _frozenMetrics;
	bool _mayGoPhysical = false;
	bool _activeThisFrame = false;
	const void* _physicalStateThisFrame = nullptr;
	std::vector<const void*> _logicalSuppressedWidgets;

	// All currently-registered family adapters (a State registers on create,
	// clears on destroy). prepareFrame() drives the one whose topState() is the
	// current top state, so stacked popups of the same family each work when they
	// become top again -- not just the last-registered one (GLM #3).
	std::vector<const CalypsoHdFamilyAdapter*> _adapters;
	const CalypsoHdFamilyAdapter* _activeAdapter = nullptr;
	std::uint32_t _retryableReadinessFrames = 0;

	// The ONE claim store: identity lives in _controller.claims(); this ephemeral
	// map is the per-frame widget-ptr -> committed claim lookup used by blit().
	// Cleared every beginFrame; populated only at commit.
	std::unordered_map<const void*, CalypsoHdClaimId> _ptrClaim;

	// This frame's committed, uploaded draws (sorted by order key).
	std::vector<ResolvedDraw> _drawItems;

	// Shared GL resources (created on first active frame; recovered via the
	// ShaderManager reset-callback ladder).
	Shader* _hdShader = nullptr;
	Shader* _panelShader = nullptr;
	unsigned _vao = 0;
	unsigned _vbo = 0;
	bool _glReady = false;
	std::shared_ptr<bool> _gpuAliveFlag;

	// Developer harness quad (off in production).
	bool _harnessEnabled = false;
	GpuTexture* _harnessTex = nullptr;
	GpuTexture* _whiteTex = nullptr; // 1x1 white, tinted for solid panels

	// HD text pipeline: CPU rasteriser + a bounded, context-generation-keyed GPU
	// texture cache.
	CalypsoHdTextRaster _textRaster;
	std::unordered_map<CalypsoHdTextTextureKey, GpuTexture*> _textTextures;
	std::unordered_map<CalypsoHdTextTextureKey, std::uint64_t> _texKeyToHandle;
	std::unordered_map<std::uint64_t, CalypsoHdTextTextureKey> _texHandleToKey;
	CalypsoLruByteBudget _textTexLru{ 16u * 1024u * 1024u };
	std::uint64_t _texNextHandle = 1;
	std::uint64_t _contextGen = 0;
	// Handles resolved THIS frame (referenced by _drawItems); never evicted/freed
	// mid-frame so a later resolve can't dangle an already-queued texture
	// (remediation Fable #3). Cleared each beginFrame.
	std::unordered_set<std::uint64_t> _frameLiveHandles;
};

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
