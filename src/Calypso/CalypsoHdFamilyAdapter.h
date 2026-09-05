#pragma once
/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * Phase 46.2-HD (Calypso) -- HD UI family adapter interface + frame builder
 * (remediation A1/A3).
 *
 * A family adapter (F34 Error/Statistics/Notes, ...) is registered by the top
 * state and asked ONCE per frame, at the pre-blit boundary, to describe the
 * physical replacement it wants -- as an immutable list of subgroups of draw
 * items. It reads a const snapshot of its widgets; it never mutates live
 * position/size/text/visibility during collection.
 *
 * Each item carries its complete claim identity (CalypsoHdClaimId) and its
 * complete deterministic order key (CalypsoHdOrderKey). The overlay resolves
 * each subgroup atomically (raster + upload), then commits claims and draws.
 * Any failure on an enabled route throws; logical widgets remain interaction
 * owners but are never a visible fallback.
 *
 * Whole-file Emscripten guard (Phase 36). Depends on the portable model
 * (CalypsoHdUiModel.h) + raster key; no SDL/GL here.
 */
#ifdef __EMSCRIPTEN__

#include <cstdint>
#include <vector>

#include "CalypsoHdUiModel.h"
#include "CalypsoHdTextRasterKey.h"

namespace OpenXcom
{
namespace Calypso
{

enum class CalypsoHdItemKind { Panel, Text };

/// Reusable SDF silhouettes for styled panels. RoundedRect preserves the
/// existing family behavior; F33 uses OpposingCutRect for its command frame,
/// WarningTriangle for the small amber caution glyph, and Radar for the
/// procedural contact-intel instrument (one GPU quad).
enum class CalypsoHdPanelShape {
	RoundedRect = 0,
	OpposingCutRect = 1,
	WarningTriangle = 2,
	Radar = 3
};

/// Horizontal/vertical glyph alignment inside the item's logical box. Mirrors
/// the engine's ALIGN_* enums by value (0/1/2) but kept local so the builder
/// stays free of Interface/Text.h.
enum class CalypsoHdHAlign { Left = 0, Center = 1, Right = 2 };
enum class CalypsoHdVAlign { Top = 0, Middle = 1, Bottom = 2 };

/// Optional styling for a Panel item, rendered by the hd_ui_panel SDF shader.
/// All px values are DESIGN-space (the same space as `rect`) and scale with
/// the logical->physical mapping. A style with `styled == false` (default)
/// keeps the plain tinted-quad path. Colours are packed 0xRRGGBBAA.
struct CalypsoHdPanelStyle
{
	bool styled = false;
	CalypsoHdPanelShape shape = CalypsoHdPanelShape::RoundedRect;
	float radiusPx = 0.0f;          // rounded-corner radius
	float cutCornerPx = 0.0f;       // opposing cut size (top-left/bottom-right)
	float borderWidthPx = 0.0f;     // ring thickness at the shape edge
	std::uint32_t borderColorRgba = 0;
	std::uint32_t fillTopRgba = 0;  // gradient stop at the grad direction origin
	std::uint32_t fillBottomRgba = 0;
	float gradDirX = 0.26f;         // gradient direction (normalized-ish);
	float gradDirY = 1.0f;          // default ~165deg-like downward drift
	std::uint32_t glowRgba = 0;     // soft outer falloff colour (alpha = strength)
	float glowRadiusPx = 0.0f;      // 0 => no glow

	// Radar-only uniforms. They are deliberately carried in the existing
	// per-item style so animation values are uploaded every draw and cannot
	// become stale in the shared panel shader.
	std::uint32_t radarRingColorRgba = 0;
	std::uint32_t radarStrongRingColorRgba = 0;
	std::uint32_t radarAxisColorRgba = 0;
	std::uint32_t radarSweepColorRgba = 0;
	float radarSweepAngle = 0.0f;   // north=0, clockwise radians
	float radarTrailRadians = 0.55f;
	float radarRingWidthPx = 1.0f;
	float radarTickWidthPx = 1.0f;
	float radarGrainAmount = 0.02f;
	float radarSeed = 0.0f;
};

/// One physical draw the adapter requests. A Panel is a solid/tinted rect
/// (window fill, bevel, badge) -- or, with `panelStyle.styled`, an SDF-shaped
/// panel (rounded, bordered, gradient, glow). A Text item rasterises
/// `rasterKey` and places the natural-size glyph bitmap inside `rect` per
/// (hAlign,vAlign) -- `rect` is the layout + clip box, never a stretch target.
struct CalypsoHdItem
{
	CalypsoHdItemKind kind = CalypsoHdItemKind::Panel;
	CalypsoLogicalRect rect;
	std::uint32_t colorRgba = 0;     // panel fill / text colour (0xRRGGBBAA)
	CalypsoHdPanelStyle panelStyle;  // Panel only; styled=false => tinted quad

	// Text only:
	CalypsoHdTextRasterKey rasterKey;
	CalypsoHdHAlign hAlign = CalypsoHdHAlign::Left;
	CalypsoHdVAlign vAlign = CalypsoHdVAlign::Middle;
	// Text-only presentation projection. Kept out of rasterKey so the same
	// design-resolution surface is reused across viewport/DPR changes and the
	// real GPU linear sampler performs the final CSS-like projection.
	float textScaleX = 1.0f;
	float textScaleY = 1.0f;

	// Presentation opacity (Phase 46.4-F33 opening motion): 1 = opaque.
	float opacity = 1.0f;

	// Identity + ordering + the live widget this visual replaces (ephemeral
	// blit-skip key; may be null for pure-decoration items with no widget).
	const void* widget = nullptr;
	CalypsoHdClaimId claim;
	CalypsoHdOrderKey order;
};

/// An atomic subgroup: all-or-fatal. Every item must rasterise/upload and the
/// whole subgroup commits claims + draws; failure throws before presentation.
/// Adapters group visuals that must appear together (e.g. one popup = one
/// subgroup) so neither a half-rasterised popup nor vanilla fallback can show.
struct CalypsoHdSubgroup
{
	std::vector<CalypsoHdItem> items;
};

/// Collected per-frame description. The adapter fills this; the overlay
/// consumes it. Nothing here touches GL or SDL.
class CalypsoHdFrameBuilder
{
public:
	void beginSubgroup() { _subgroups.emplace_back(); }

	void add(const CalypsoHdItem& item)
	{
		if (_subgroups.empty()) beginSubgroup();
		_subgroups.back().items.push_back(item);
	}

	const std::vector<CalypsoHdSubgroup>& subgroups() const { return _subgroups; }
	bool empty() const { return _subgroups.empty(); }

private:
	std::vector<CalypsoHdSubgroup> _subgroups;
};

/// Per-frame logical widgets belonging to a covered lower state. This is
/// separate from physical claims: covered chrome must stay hidden while the
/// top popup is still in its native opening animation.
class CalypsoHdLogicalSuppression
{
public:
	void add(const void* widget)
	{
		if (widget) _widgets.push_back(widget);
	}

	const std::vector<const void*>& widgets() const { return _widgets; }

private:
	std::vector<const void*> _widgets;
};

/// Implemented by each family's state-side adapter. Registered with the overlay
/// while its state is top; unregistered on state destruction.
class CalypsoHdFamilyAdapter
{
public:
	virtual ~CalypsoHdFamilyAdapter() = default;

	/// The State* this adapter feeds. The overlay only calls collect() when this
	/// equals the current top state, so a state pushed on top never lets a lower
	/// popup's physical replacement draw over it.
	virtual const void* topState() const = 0;

	/// Returns false for a transient frame in which the logical widget is still
	/// playing its native opening animation. The overlay leaves that logical
	/// frame visible instead of treating an intentionally empty collection as a
	/// fatal HD route error.
	virtual bool physicalReady() const { return true; }
	virtual bool completeFrameReady() const { return true; }
	/// True when completeFrameReady() is false only because the model is still
	/// warming. The overlay retries without publishing an error or exposing
	/// native presentation; concrete preparation failures remain terminal.
	virtual bool retryableReadiness() const { return false; }

	/// Atomic families own all of their top-state logical UI once the physical
	/// subgroup commits. Adapters that overlay another state can also list that
	/// state's chrome here; the overlay applies it before readiness is checked.
	virtual bool suppressLogicalState() const { return true; }
	virtual void collectLogicalSuppression(CalypsoHdLogicalSuppression&) const {}

	/// Fail-closed covered-state ownership (Phase 46.4 Stage 8/9 closure):
	/// when true, this adapter's collectLogicalSuppression list is applied
	/// every frame EVEN while it is not the active top adapter, so its
	/// reprojected logical chrome can never leak around a blocking modal or
	/// nested form. Default false keeps every existing family lifecycle
	/// byte-identical; only adapters that reproject persistent lower shells
	/// opt in.
	virtual bool suppressWhenCovered() const { return false; }

	/// Describe this frame's physical replacement into `builder`, reading a const
	/// snapshot of the widgets. MUST NOT mutate live widget state.
	virtual void collect(CalypsoHdFrameBuilder& builder) const = 0;
};

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
