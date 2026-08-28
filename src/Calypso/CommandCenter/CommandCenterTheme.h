#pragma once
/*
 * Command Center -- canonical theme tokens (normative spec 2026-08-28, s.7).
 *
 * Every colour, size, radius, spacing step, and transition duration the
 * Command Center UI uses lives here. Renderers must reference these
 * constants; literal colours inside draw code are forbidden by the spec.
 *
 * Pure, dependency-free data (only CommandCenterTypes.h for Color8).
 */
#include <cstdint>

#include "CommandCenterTypes.h"

namespace OpenXcom
{
namespace Calypso
{
namespace CommandCenterTheme
{

using CommandCenter::Color8;

// Backgrounds
inline constexpr Color8 BgRoot        { 0x02, 0x06, 0x0E, 0xFF };
inline constexpr Color8 BgHeader      { 0x04, 0x10, 0x1C, 0xFF };
inline constexpr Color8 BgRail        { 0x04, 0x11, 0x1E, 0xFF };
inline constexpr Color8 BgStage       { 0x02, 0x0B, 0x18, 0xFF };
inline constexpr Color8 BgPanel       { 0x07, 0x16, 0x24, 0xFF };
inline constexpr Color8 BgPanelRaised { 0x0A, 0x1C, 0x2D, 0xFF };
inline constexpr Color8 BgHover       { 0x0C, 0x22, 0x32, 0xFF };
inline constexpr Color8 BgActive      { 0x0D, 0x29, 0x31, 0xFF };

// Borders
inline constexpr Color8 Border        { 0x17, 0x32, 0x46, 0xFF };
inline constexpr Color8 BorderStrong  { 0x28, 0x52, 0x67, 0xFF };
inline constexpr Color8 BorderSoft    { 0x6F, 0xA5, 0xB8, 0x2E };
inline constexpr Color8 BorderAccent  { 0x81, 0xE0, 0xB5, 0x8C };

// Mint accent
inline constexpr Color8 Accent        { 0x81, 0xE0, 0xB5, 0xFF };
inline constexpr Color8 AccentStrong  { 0x9A, 0xF3, 0xC9, 0xFF };
inline constexpr Color8 AccentDim     { 0x5D, 0xBB, 0x92, 0xFF };
inline constexpr Color8 AccentSoft    { 0x81, 0xE0, 0xB5, 0x1F };
inline constexpr Color8 AccentFaint   { 0x81, 0xE0, 0xB5, 0x0F };

// Text
inline constexpr Color8 TextPrimary   { 0xE7, 0xF0, 0xF3, 0xFF };
inline constexpr Color8 TextSecondary { 0xA8, 0xBB, 0xC4, 0xFF };
inline constexpr Color8 TextMuted     { 0x70, 0x85, 0x90, 0xFF };
inline constexpr Color8 TextDisabled  { 0x45, 0x59, 0x66, 0xFF };
inline constexpr Color8 TextOnAccent  { 0x03, 0x13, 0x0D, 0xFF };

// Semantic colours
inline constexpr Color8 Info          { 0x69, 0xB9, 0xFF, 0xFF };
inline constexpr Color8 Warning       { 0xF2, 0xC5, 0x6D, 0xFF };
inline constexpr Color8 Danger        { 0xFF, 0x6E, 0x72, 0xFF };
inline constexpr Color8 Success       { 0x81, 0xE0, 0xB5, 0xFF };

// Desktop sizes
inline constexpr float HeaderHeight   = 72.0f;
inline constexpr float RailWidth      = 88.0f;
inline constexpr float InspectorWidth = 320.0f;
inline constexpr float TimelineHeight = 104.0f;
inline constexpr float WorkspacePad   = 24.0f;
inline constexpr float WorkspaceGap   = 24.0f;

// Corner radii
inline constexpr float RadiusXS = 4.0f;
inline constexpr float RadiusSM = 6.0f;
inline constexpr float RadiusMD = 10.0f;
inline constexpr float RadiusLG = 14.0f;
inline constexpr float RadiusXL = 18.0f;

// Spacing scale
inline constexpr float Space1 = 4.0f;
inline constexpr float Space2 = 8.0f;
inline constexpr float Space3 = 12.0f;
inline constexpr float Space4 = 16.0f;
inline constexpr float Space5 = 20.0f;
inline constexpr float Space6 = 24.0f;
inline constexpr float Space8 = 32.0f;

// Transitions
inline constexpr float FastTransitionSeconds   = 0.12f;
inline constexpr float NormalTransitionSeconds = 0.18f;

// Pack a theme colour into the engine's 0xRRGGBBAA word (CalypsoHdUiModel
// convention) so overlay painters can consume theme tokens directly.
inline constexpr std::uint32_t packed(const Color8& c)
{
	return (static_cast<std::uint32_t>(c.r) << 24)
		| (static_cast<std::uint32_t>(c.g) << 16)
		| (static_cast<std::uint32_t>(c.b) << 8)
		| static_cast<std::uint32_t>(c.a);
}

} // namespace CommandCenterTheme
} // namespace Calypso
} // namespace OpenXcom
