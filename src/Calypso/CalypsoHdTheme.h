#pragma once
/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * Phase 46.2-HD styling (Calypso) -- shared HD theme tokens.
 *
 * Phase 46.4-F33: every value below is an ALIAS of the canonical contract
 * (src/Calypso/Contracts/hd-ui-theme.json -> Generated/CalypsoHdTheme.generated.h).
 * The engine and the DOM consume the SAME generated source (F33-PARITY-006);
 * editing a value means editing the JSON and regenerating, never this header.
 *
 * Pure, dependency-free data (only CalypsoHdUiModel.h for calypsoRgba and
 * CalypsoHdFamilyAdapter.h for the style struct) -- NOT wrapped in
 * #ifdef __EMSCRIPTEN__, matching the Calypso pure-helper convention.
 */
#include <cstdint>

#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoHdUiModel.h"
#include "Generated/CalypsoHdTheme.generated.h"

namespace OpenXcom
{
namespace Calypso
{

/// Accent / palette tokens (0xRRGGBBAA via calypsoRgba). Canonical values.
namespace CalypsoHdTheme
{

constexpr std::uint32_t kAccent          = CalypsoHdThemeGen::kAccent;
constexpr std::uint32_t kAccentSoft      = CalypsoHdThemeGen::kAccentSoft;
constexpr std::uint32_t kDanger          = CalypsoHdThemeGen::kDanger;
constexpr std::uint32_t kGold            = CalypsoHdThemeGen::kGold;
constexpr std::uint32_t kNearWhite       = CalypsoHdThemeGen::kNearWhite;
constexpr std::uint32_t kBackdropDim     = CalypsoHdThemeGen::kBackdropDim;

// Dialog chrome (canonical contract values).
constexpr float kDialogRadiusPx   = CalypsoHdThemeGen::kDialogRadiusPx;
constexpr float kButtonRadiusPx   = CalypsoHdThemeGen::kButtonRadiusPx;
constexpr float kBorderWidthPx    = CalypsoHdThemeGen::kBorderWidthPx;
constexpr float kTitleTrackingEm  = CalypsoHdThemeGen::kTitleTrackingEm;
constexpr float kLabelTrackingEm  = CalypsoHdThemeGen::kLabelTrackingEm;

// Window fill gradient (canonical contract values).
constexpr std::uint32_t kWindowFillTop    = CalypsoHdThemeGen::kDialogFillTop;
constexpr std::uint32_t kWindowFillBottom = CalypsoHdThemeGen::kDialogFillBottom;

// Buttons: YES destructive, NO safe (flat fills in the reference).
constexpr std::uint32_t kYesFill   = CalypsoHdThemeGen::kDestructiveRestFill;
constexpr std::uint32_t kNoFill    = CalypsoHdThemeGen::kSafeRestFill;

// Soft drop shadow + accent halo (canonical contract values).
constexpr float kShadowGlowRadiusPx = CalypsoHdThemeGen::kShadowGlowRadiusPx;
constexpr std::uint32_t kShadowGlow = CalypsoHdThemeGen::kShadowGlow;
constexpr float kHaloGlowRadiusPx   = CalypsoHdThemeGen::kHaloGlowRadiusPx;
constexpr std::uint32_t kHaloGlow   = CalypsoHdThemeGen::kHaloGlow;
constexpr float kTitleFontSizeScale = CalypsoHdThemeGen::kTitleFontSizeScale;
constexpr float kBodyFontSizeScale  = CalypsoHdThemeGen::kBodyFontSizeScale;
constexpr int kLabelFontSizePx      = CalypsoHdThemeGen::kLabelFontSizePx;
constexpr float kLabelFontSizeScaleWide = CalypsoHdThemeGen::kLabelFontSizeScaleWide;
constexpr float kLabelFontSizeScaleCompact = CalypsoHdThemeGen::kLabelFontSizeScaleCompact;
constexpr int kBodyFontSizePx       = CalypsoHdThemeGen::kBodyFontSizePx;
constexpr float kBodyFontSizeScaleWide = CalypsoHdThemeGen::kBodyFontSizeScaleWide;
constexpr float kBodyFontSizeScaleCompact = CalypsoHdThemeGen::kBodyFontSizeScaleCompact;
constexpr float kBodyFontWidthScaleWide = CalypsoHdThemeGen::kBodyFontWidthScaleWide;
constexpr float kBodyFontWidthScaleCompact = CalypsoHdThemeGen::kBodyFontWidthScaleCompact;
constexpr float kBodyWrapMeasureScaleWide = CalypsoHdThemeGen::kBodyWrapMeasureScaleWide;
constexpr float kBodyWrapMeasureScaleCompact = CalypsoHdThemeGen::kBodyWrapMeasureScaleCompact;
constexpr float kBodyProjectionLineHeightScaleWide = CalypsoHdThemeGen::kBodyProjectionLineHeightScaleWide;
constexpr float kBodyProjectionLineHeightScaleCompact = CalypsoHdThemeGen::kBodyProjectionLineHeightScaleCompact;
// DOM baseline token; the engine raster uses the layout-specific physical
// line-height scales below to account for SDL_ttf/native glyph metrics.
constexpr float kBodyLineHeight     = CalypsoHdThemeGen::kBodyLineHeight;
constexpr int kTitleFontWeight      = CalypsoHdThemeGen::kTitleFontWeight;
constexpr int kLabelFontWeight      = CalypsoHdThemeGen::kLabelFontWeight;
constexpr int kBodyFontWeight       = CalypsoHdThemeGen::kBodyFontWeight;

/// Ready-made styles ------------------------------------------------------------

/// Base dialog window: gradient fill, accent border, rounded.
inline CalypsoHdPanelStyle calypsoHdDialogStyle()
{
	CalypsoHdPanelStyle s;
	s.styled = true;
	s.radiusPx = kDialogRadiusPx;
	s.borderWidthPx = kBorderWidthPx;
	s.borderColorRgba = kAccent;
	s.fillTopRgba = kWindowFillTop;
	s.fillBottomRgba = kWindowFillBottom;
	s.gradDirX = 0.26f;
	s.gradDirY = 1.0f;
	return s;
}

/// A button slab over the dialog: flat fill (top==bottom), border, rounded.
inline CalypsoHdPanelStyle calypsoHdButtonStyle(std::uint32_t fill, std::uint32_t border)
{
	CalypsoHdPanelStyle s;
	s.styled = true;
	s.radiusPx = kButtonRadiusPx;
	s.borderWidthPx = kBorderWidthPx;
	s.borderColorRgba = border;
	s.fillTopRgba = fill;
	s.fillBottomRgba = fill;
	s.gradDirX = 0.26f;
	s.gradDirY = 1.0f;
	return s;
}

/// Glow-only quad (no shape): soft shadow below or accent halo. Submit with
/// an offset rect (shadows sit below their owner) and BEFORE the solid panel.
inline CalypsoHdPanelStyle calypsoHdGlowStyle(std::uint32_t glow, float radiusPx)
{
	CalypsoHdPanelStyle s;
	s.styled = true;
	s.glowRgba = glow;
	s.glowRadiusPx = radiusPx;
	return s;
}

} // namespace CalypsoHdTheme
} // namespace Calypso
} // namespace OpenXcom
