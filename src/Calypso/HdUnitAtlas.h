#pragma once
/*
 * HdUnitAtlas.h — Calypso-owned unit-PCK GPU sprite atlas layout record.
 *
 * Relocated from Mod.h (Phase 36 / Phase 42 review #2): the UnitAtlasSpec
 * struct grew past the in-place #ifdef policy limit once the sparse per-PCK-frame
 * RGBA overlay pages were added. It is Calypso-only (Emscripten) data, so it
 * lives here under a whole-file guard. Mod.h pulls it in and exposes it as
 * `Mod::UnitAtlasSpec` via a using-alias, so every existing call site
 * (`Mod::getUnitAtlas()`, Map/UnitSprite/ModHd) keeps compiling unchanged.
 *
 * This is a pure relocation (policy R6): the fields and inline helpers are
 * byte-for-byte the struct that previously sat inside class Mod.
 */
#ifdef __EMSCRIPTEN__
#include <cstdint>
#include <string>
#include <vector>

namespace OpenXcom
{
class GpuTexture;  // pointer-only here; full def via Engine/GpuTexture.h at use sites

/// Layout record for a unit-PCK GPU sprite atlas (Phase 14.1).
/// atlas_tile_index == PCK_frame_index (frames packed in declaration order).
struct HdUnitAtlasSpec
{
	GpuTexture* atlas      = nullptr;  // R8 palette-index atlas; owned by Mod
	int         atlasW     = 0;        // atlas pixel width
	int         atlasH     = 0;        // atlas pixel height
	int         tileWidth  = 64;       // cell width (2x upscale of 32)
	int         tileHeight = 80;       // cell height (2x upscale of 40)
	int         columns    = 16;
	// Disposable Phase-42 real-battle G0 probe. Never populated by rulesets:
	// the Emscripten harness temporarily borrows a MEMFS-backed RGBA atlas.
	GpuTexture* g0OverlayAtlas = nullptr;
	std::vector<uint8_t> g0OverlayMask;

	// ---- Phase 42 E1: production sparse per-PCK-frame RGBA overlay pages ----
	// The R8 baseline atlas above is always built; zero or more RGBA overlay
	// PAGES carry authored HD frames. Each PCK frame may have an opaque RGBA
	// slot (hasHd=1); missing/transparent slots fall back to the R8 baseline.
	// Configured by the `unitAtlas:` ruleset key (parsed in ModHd.cpp), built
	// lazily by ensureUnitAtlas(). Pages are owned by Mod (deleted in
	// clearUnitAtlases, evicted/restored with the other battle atlases).
	enum class RgbaOverlayFormat : unsigned char { None, RgbaOverlay };
	RgbaOverlayFormat rgbaFormat = RgbaOverlayFormat::None;
	int               frameWidth      = 0;   // RGBA overlay cell W (e.g. 128)
	int               frameHeight     = 0;   // RGBA overlay cell H (e.g. 160)
	int               rgbaColumns     = 16;  // columns per page
	int               maxPageSize     = 4096;// portable page dimension cap
	std::vector<std::string> pages;          // mod-relative PNG paths (one per page)
	// Runtime (populated by ensureUnitAtlas once pages[] are configured):
	std::vector<GpuTexture*> rgbaOverlayPages; // one texture per page; sRGB, LINEAR
	std::vector<uint8_t>     rgbaHasHd;        // per-PCK-frame: 1 = opaque RGBA slot, 0 = R8 fallback
	std::vector<int>         rgbaPageOf;       // per-PCK-frame: page index, -1 = no overlay
	int                      rgbaFramesPerPage = 0;
	int                      rgbaRowsPerPage   = 0;
	int                      rgbaPageW         = 0; // page pixel width  (rgbaColumns * frameWidth)
	int                      rgbaPageH         = 0; // page pixel height (rgbaRowsPerPage * frameHeight)
	// Phase 42 E2: drawRoutine* offsets are source-PCK pixels. Derive one
	// uniform scale from the declared RGBA cell and the real source frame.
	int                      sourceFrameWidth  = 0;
	int                      sourceFrameHeight = 0;
	int                      partOffsetScale   = 1;
	bool                     partOffsetScaleConfigured = false;
	bool                     partOffsetScaleValid      = true;

	/// True when at least one RGBA overlay page is configured AND loaded.
	bool hasRgbaOverlay() const {
		return rgbaFormat == RgbaOverlayFormat::RgbaOverlay && !rgbaOverlayPages.empty();
	}
	/// Bounds-safe per-frame HD lookup (false for absent/out-of-range frames).
	bool frameHasHd(int frame) const {
		return frame >= 0 && (size_t)frame < rgbaHasHd.size()
		    && rgbaHasHd[(size_t)frame] != 0;
	}
	/// Bounds-safe per-frame page index (-1 when absent/out-of-range).
	int framePageOf(int frame) const {
		return (frame >= 0 && (size_t)frame < rgbaPageOf.size())
		       ? rgbaPageOf[(size_t)frame] : -1;
	}
	/// Resolve the source-PCK-pixel -> live-quad scale for every sheet,
	/// including R8-only fallback. A declared RGBA scale must match it.
	int partScaleForFrame(int renderW, int renderH) const {
		if (!partOffsetScaleValid || sourceFrameWidth <= 0 || sourceFrameHeight <= 0
		 || renderW <= 0 || renderH <= 0
		 || renderW % sourceFrameWidth != 0 || renderH % sourceFrameHeight != 0)
			return 0;
		const int scaleX = renderW / sourceFrameWidth;
		const int scaleY = renderH / sourceFrameHeight;
		if (scaleX <= 0 || scaleX != scaleY) return 0;
		if (partOffsetScaleConfigured && scaleX != partOffsetScale) return 0;
		return scaleX;
	}
	bool partScaleMatchesFrame(int renderW, int renderH) const {
		return partScaleForFrame(renderW, renderH) > 0;
	}
};
} // namespace OpenXcom
#endif
