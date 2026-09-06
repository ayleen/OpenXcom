#pragma once
// F01 desktop-fit geometry (T04): pure derivation of the base-command-shell
// authored layout from explicit parameters. No engine, SDL, or generated
// dependencies; the T08 template supplies the numbers, this header only
// computes fit scale, the clamped command column, and the square deck.
// Not #ifdef-guarded, matching the Calypso pure-helper convention.

#include <algorithm>
#include <cmath>

namespace OpenXcom
{
namespace Calypso
{

/// Authored desktop numbers (T02 worksheet; T08 binds generated constants).
struct CalypsoBasescapeHdFitParams
{
	int minViewportW = 1280;
	int minViewportH = 720;
	int railWidth = 88;
	int headerHeight = 72;
	int inset = 24;
	int columnGap = 20;
	int commandMinW = 320;
	int commandMaxW = 440;
	float commandFraction = 0.30f;
	int titleBandH = 52;
	int titleGapH = 16;
	int minHit = 44;
	int minSelectorSlots = 8;
};

/// Derived authored geometry for one CSS viewport.
struct CalypsoBasescapeHdFitGeometry
{
	float fitScale = 1.0f;
	int authoredW = 0;
	int authoredH = 0;
	int commandX = 0;
	int commandW = 0;
	int deckX0 = 0;
	int deckY0 = 0;
	int deckSide = 0;
};

inline CalypsoBasescapeHdFitGeometry calypsoBasescapeHdFitGeometry(
	int cssW, int cssH, const CalypsoBasescapeHdFitParams &params)
{
	CalypsoBasescapeHdFitGeometry out;
	float scale = 1.0f;
	if (cssW < params.minViewportW)
	{
		scale = std::min(scale, static_cast<float>(cssW) / static_cast<float>(params.minViewportW));
	}
	if (cssH < params.minViewportH)
	{
		scale = std::min(scale, static_cast<float>(cssH) / static_cast<float>(params.minViewportH));
	}
	out.fitScale = scale;
	out.authoredW = static_cast<int>(std::round(static_cast<float>(cssW) / scale));
	out.authoredH = static_cast<int>(std::round(static_cast<float>(cssH) / scale));
	// Usable width between the rail and the right margin (T02 worksheet:
	// 1280 - 88 - 24 - 24 = 1144, so the command column rounds from 343.2).
	const int innerW = out.authoredW - params.railWidth - 2 * params.inset;
	out.commandW = std::min(params.commandMaxW,
		std::max(params.commandMinW,
			static_cast<int>(std::round(static_cast<float>(innerW) * params.commandFraction))));
	out.commandX = out.authoredW - params.inset - out.commandW;
	out.deckX0 = params.railWidth + params.inset;
	out.deckY0 = params.headerHeight + params.inset + params.titleBandH + params.titleGapH;
	const int deckX1 = out.commandX - params.columnGap;
	const int deckY1 = out.authoredH - params.inset;
	out.deckSide = std::min(deckX1 - out.deckX0, deckY1 - out.deckY0);
	return out;
}

} // namespace Calypso
} // namespace OpenXcom
