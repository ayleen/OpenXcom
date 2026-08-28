#pragma once
/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * Geoscape HD v2 visual contract s.10.1 (2026-08-28 revision): canonical
 * styling for the primary command actions (command-icon-action pills and the
 * Session chip primary tone). Replaces the height-derived stadium radius and
 * the flat quiet fill with the fixed-radius gradient surfaces defined by the
 * hd-ui-theme.json commandAction token block.
 *
 * Pure, dependency-free data (only CalypsoHdFamilyAdapter.h for the style
 * struct, CalypsoHdInteractionState.h for the state enum, and the generated
 * theme header) -- NOT wrapped in #ifdef __EMSCRIPTEN__, matching the
 * Calypso pure-helper convention. Unit-tested natively by
 * CalypsoHdCommandActionStyleTest.
 */
#include <cstdint>

#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoHdInteractionState.h"
#include "Generated/CalypsoHdTheme.generated.h"

namespace OpenXcom
{
namespace Calypso
{

/// Session chip and other anchor actions render in the primary tone so the
/// persistent route reads above occasional command pills.
enum class CalypsoCommandActionTone
{
	Normal,
	Primary,
};

/// SDF panel style for one command action (tone, state). The radius is the
/// canonical fixed token: this function takes no geometry, so a height-derived
/// radius is unrepresentable. Keyboard focus keeps the shared focus-ring
/// machinery (2 px ring) layered over the state fill.
inline CalypsoHdPanelStyle calypsoCommandActionStyle(CalypsoInteractionState state,
	CalypsoCommandActionTone tone = CalypsoCommandActionTone::Normal)
{
	using namespace CalypsoHdThemeGen;
	const bool primary = tone == CalypsoCommandActionTone::Primary;
	CalypsoHdPanelStyle s;
	s.styled = true;
	s.radiusPx = kCommandActionRadiusPx;
	s.gradDirX = 0.0f;
	s.gradDirY = 1.0f;
	s.borderWidthPx = 1.0f;
	switch (state)
	{
	case CalypsoInteractionState::Hover:
		s.fillTopRgba = kCommandActionHoverFillTop;
		s.fillBottomRgba = kCommandActionHoverFillBottom;
		s.borderColorRgba = kCommandActionHoverBorder;
		s.glowRgba = kCommandActionHoverGlow;
		s.glowRadiusPx = kCommandActionHoverGlowRadiusPx;
		break;
	case CalypsoInteractionState::Pressed:
		s.fillTopRgba = kCommandActionPressedFillTop;
		s.fillBottomRgba = kCommandActionPressedFillBottom;
		s.borderColorRgba = kCommandActionPressedBorder;
		s.glowRgba = kCommandActionPressedGlow;
		s.glowRadiusPx = kCommandActionPressedGlowRadiusPx;
		break;
	case CalypsoInteractionState::Focus:
		if (primary)
		{
			s.fillTopRgba = kCommandActionPrimaryRestFillTop;
			s.fillBottomRgba = kCommandActionPrimaryRestFillBottom;
		}
		else
		{
			s.fillTopRgba = kCommandActionRestFillTop;
			s.fillBottomRgba = kCommandActionRestFillBottom;
		}
		s.borderColorRgba = kFocusRingSafe;
		s.borderWidthPx = 2.0f;
		s.glowRgba = kCommandActionRestGlow;
		s.glowRadiusPx = kCommandActionRestGlowRadiusPx;
		break;
	case CalypsoInteractionState::Disabled:
		s.fillTopRgba = kCommandActionDisabledFillTop;
		s.fillBottomRgba = kCommandActionDisabledFillBottom;
		s.borderColorRgba = kCommandActionDisabledBorder;
		s.glowRgba = kCommandActionRestGlow;
		s.glowRadiusPx = 0.0f;
		break;
	case CalypsoInteractionState::Rest:
	default:
		if (primary)
		{
			s.fillTopRgba = kCommandActionPrimaryRestFillTop;
			s.fillBottomRgba = kCommandActionPrimaryRestFillBottom;
			s.borderColorRgba = kCommandActionPrimaryRestBorder;
		}
		else
		{
			s.fillTopRgba = kCommandActionRestFillTop;
			s.fillBottomRgba = kCommandActionRestFillBottom;
			s.borderColorRgba = kCommandActionRestBorder;
		}
		s.glowRgba = kCommandActionRestGlow;
		s.glowRadiusPx = kCommandActionRestGlowRadiusPx;
		break;
	}
	return s;
}

} // namespace Calypso
} // namespace OpenXcom
