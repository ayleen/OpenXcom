#pragma once
/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * Phase 46.2-HD styling (Calypso) -- shared HD theme tokens.
 *
 * One header of design constants mirroring the DOM reference design
 * (design/v1.1 "Calypso UI Components"): palette, radii, borders, tracking.
 * Family adapters take their colours from here instead of declaring local
 * kWindowFillRgba-style constants, so the engine HD look stays in lockstep
 * with the DOM cards (and a future retune is one edit).
 *
 * Pure, dependency-free data (only CalypsoHdUiModel.h for calypsoRgba and
 * CalypsoHdFamilyAdapter.h for the style struct) -- NOT wrapped in
 * #ifdef __EMSCRIPTEN__, matching the Calypso pure-helper convention.
 */
#include <cstdint>

#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoHdUiModel.h"

namespace OpenXcom
{
namespace Calypso
{

/// Accent / palette tokens (0xRRGGBBAA via calypsoRgba).
namespace CalypsoHdTheme
{

constexpr std::uint32_t kAccent          = calypsoRgba(0x74, 0xff, 0xb0);       // #74ffb0
constexpr std::uint32_t kAccentSoft      = calypsoRgba(0x74, 0xff, 0xb0, 0x59); // ~35% border
constexpr std::uint32_t kDanger          = calypsoRgba(0xff, 0x78, 0x78);       // #ff7878
constexpr std::uint32_t kGold            = calypsoRgba(0xff, 0xc1, 0x4d);       // #ffc14d
constexpr std::uint32_t kNearWhite       = calypsoRgba(0xe8, 0xff, 0xf5);       // #e8fff5
constexpr std::uint32_t kBackdropDim     = calypsoRgba(0x00, 0x00, 0x00, 0x73); // rgba(0,0,0,.45)

// Dialog chrome (matches the DOM reference cards).
constexpr float kDialogRadiusPx   = 6.0f;
constexpr float kButtonRadiusPx   = 4.0f;
constexpr float kBorderWidthPx    = 2.0f;
constexpr float kTitleTrackingEm  = 0.12f;  // letter-spacing on headings/labels
constexpr float kLabelTrackingEm  = 0.12f;

// Window fill gradient: linear-gradient(165deg, rgba(9,25,29,.98), rgba(5,15,20,.98)).
constexpr std::uint32_t kWindowFillTop    = calypsoRgba(0x09, 0x19, 0x1d, 0xfa);
constexpr std::uint32_t kWindowFillBottom = calypsoRgba(0x05, 0x0f, 0x14, 0xfa);

// Buttons: YES destructive, NO safe (flat fills in the reference).
constexpr std::uint32_t kYesFill   = calypsoRgba(0x3d, 0x16, 0x16);
constexpr std::uint32_t kNoFill    = calypsoRgba(0x16, 0x4c, 0x3d);

// Soft drop shadow + accent halo (box-shadow 0 26px 60px 0,0,0,.6 and
// 0 0 44px accent .14, approximated by glow quads).
constexpr float kShadowGlowRadiusPx = 26.0f;
constexpr std::uint32_t kShadowGlow = calypsoRgba(0x00, 0x00, 0x00, 0x8c);
constexpr float kHaloGlowRadiusPx   = 22.0f;
constexpr std::uint32_t kHaloGlow   = calypsoRgba(0x74, 0xff, 0xb0, 0x24);

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
