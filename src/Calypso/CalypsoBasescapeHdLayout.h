#pragma once
// F01 desktop-fit geometry (T04): pure derivation of the base-command-shell
// authored layout from explicit parameters. No engine or SDL dependencies;
// the generated template supplies the numbers, this header only computes
// fit scale, the clamped command column, and the square deck.
// Not #ifdef-guarded, matching the Calypso pure-helper convention.
//
// Cinematic correction (owner remediation): the canonical template owns one
// desktop-fit composition at min 1280x720 and this header derives every
// per-viewport rect from it, so widget placement (CalypsoBasescapeHdUi) and
// frame paint (CalypsoHdScreenRenderer::collectBasescape) share one math
// source. Design space is authored CSS pixels: at/above the minimum the CSS
// viewport itself, below it the uniformly upscaled viewport (same rule as
// CommandCenter::computeLayout, so the shared chrome and the base content
// agree on the fit scale).

#include <algorithm>
#include <cmath>
#include <string_view>

#include "CalypsoBaseGridInput.h"
#include "CalypsoHdInteractionState.h"
#include "Generated/CalypsoBasescapeCommandShell.generated.h"

namespace OpenXcom
{
namespace Calypso
{

// Template-owned parameters are emitted for native and browser consumers.
using CalypsoBasescapeHdFitParams = CalypsoBasescapeCommandShellGen::DesktopFitParams;


/// Authored CSS size for one real viewport: at/above the minimum the CSS
/// viewport itself, below it the uniformly upscaled viewport. Mirrors the
/// CommandCenter::computeLayout fit rule so shared chrome and base content
/// share one design space.
struct CalypsoBasescapeHdAuthoredSize
{
	float fitScale = 1.0f;
	int w = 1280;
	int h = 720;
};

inline CalypsoBasescapeHdAuthoredSize calypsoBasescapeHdAuthoredSize(
	int cssW, int cssH, const CalypsoBasescapeHdFitParams &params)
{
	CalypsoBasescapeHdAuthoredSize out;
	float scale = 1.0f;
	if (cssW < params.minViewportW)
	{
		scale = std::min(scale, static_cast<float>(cssW) / static_cast<float>(params.minViewportW));
	}
	if (cssH < params.minViewportH)
	{
		scale = std::min(scale, static_cast<float>(cssH) / static_cast<float>(params.minViewportH));
	}
	if (!(scale > 0.0f))
	{
		scale = 1.0f;
	}
	out.fitScale = scale;
	out.w = static_cast<int>(std::round(static_cast<float>(cssW) / scale));
	out.h = static_cast<int>(std::round(static_cast<float>(cssH) / scale));
	return out;
}

/// Plain authored-space rectangle (CSS pixels, shared design space).
struct CalypsoBasescapeHdRect
{
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
};

constexpr CalypsoBasescapeHdRect calypsoBasescapeHdTemplateRect(std::string_view id)
{
	for (const auto& region : CalypsoBasescapeCommandShellGen::kWideRegions)
		if (id == region.id) return {region.rect.x, region.rect.y, region.rect.w, region.rect.h};
	for (const auto& action : CalypsoBasescapeCommandShellGen::kWideActions)
		if (id == action.id) return {action.visible.x, action.visible.y, action.visible.w, action.visible.h};
	return {};
}
static_assert(calypsoBasescapeHdTemplateRect("titleName").w > 0
	&& calypsoBasescapeHdTemplateRect("titleRegion").w > 0
	&& calypsoBasescapeHdTemplateRect("titleSelector").w > 0
	&& calypsoBasescapeHdTemplateRect("hoverLine").w > 0
	&& calypsoBasescapeHdTemplateRect("navigation.world").w > 0,
	"Base template is missing a required fixed region");

// A connector crosses the west/north edge of the neighboring cell, not its
// interior. Artwork stays narrow even when the source PNG is fully opaque.
inline CalypsoBasescapeHdRect calypsoBasescapeHdConnectorRect(
	const CalypsoBasescapeHdRect& neighbor, bool horizontal,
	const CalypsoBasescapeHdFitParams& params)
{
	const int length = std::max(1, static_cast<int>(std::lround(
		(horizontal ? neighbor.w : neighbor.h) * params.connectorLengthRatio)));
	const int thickness = std::max(1, static_cast<int>(std::lround(
		(horizontal ? neighbor.h : neighbor.w) * params.connectorThicknessRatio)));
	return horizontal
		? CalypsoBasescapeHdRect{neighbor.x - length / 2,
			neighbor.y + (neighbor.h - thickness) / 2, length, thickness}
		: CalypsoBasescapeHdRect{neighbor.x + (neighbor.w - thickness) / 2,
			neighbor.y - length / 2, thickness, length};
}

/// Variable-width base-selector slot (F01 cinematic): the agreed shared API.
/// The selected position is selectorActiveWidth wide, every other position
/// is minHit wide, and the eight slots tile the authored selector rect
/// exactly (140 + 7 * 44 at the canonical size). A resized container scales
/// every slot by the same factor. Out-of-range indices return an empty rect;
/// an out-of-range selectedIndex leaves every slot at the inactive width.
/// Hidden MiniBaseView hit regions and both painters consume this helper.
inline CalypsoBasescapeHdRect calypsoBasescapeHdSelectorSlot(
	const CalypsoBasescapeHdRect &rect, int index, int selectedIndex,
	const CalypsoBasescapeHdFitParams &params)
{
	const CalypsoSelectorSlot slot = calypsoSelectorSlotRect(
		rect.x, rect.w, index, selectedIndex,
		params.selectorActiveWidth, params.minHit, 8);
	if (slot.w <= 0)
	{
		return {};
	}
	return {slot.x, rect.y, slot.w, rect.h};
}

/// One command-column row in canonical template order. slotRole/rowIndex
/// follow the component contract (logistics rows share "logistics-rows"
/// with rowIndex 0..2); actionId is the archetype semantic binding.
struct CalypsoBasescapeHdCommandRow
{
	const char *actionId;
	const char *slotRole;
	int rowIndex;
	CalypsoBasescapeHdRect rect;
};

/// Full per-viewport derivation of the base-command-shell composition.
/// At 1280x720 this reproduces the canonical template exactly; taller
/// viewports grow the five illustrated cards equally and shift later rows.
struct CalypsoBasescapeHdDerivedLayout
{
	int authoredW = 0;
	int authoredH = 0;
	int commandX = 0;
	int commandW = 0;
	CalypsoBasescapeHdRect titleName = calypsoBasescapeHdTemplateRect("titleName");
	CalypsoBasescapeHdRect titleRegion = calypsoBasescapeHdTemplateRect("titleRegion");
	CalypsoBasescapeHdRect titleSelector = calypsoBasescapeHdTemplateRect("titleSelector");
	CalypsoBasescapeHdRect hoverLine = calypsoBasescapeHdTemplateRect("hoverLine");
	CalypsoBasescapeHdRect deckPanel;
	CalypsoBasescapeHdRect deckHeading;
	CalypsoBasescapeHdRect deckGrid;
	CalypsoBasescapeHdRect deckFooter;
	CalypsoBasescapeHdRect fundsLine;
	CalypsoBasescapeHdRect columnHeading;
	CalypsoBasescapeHdRect railWorld = calypsoBasescapeHdTemplateRect("navigation.world");
	int deckCell = 0;
	bool showRowGutters = false;
	bool showColGutters = false;
	CalypsoBasescapeHdCommandRow rows[11];
};

inline CalypsoBasescapeHdDerivedLayout calypsoBasescapeHdDerivedLayout(
	int authoredW, int authoredH, const CalypsoBasescapeHdFitParams &params)
{
	CalypsoBasescapeHdDerivedLayout out;
	out.authoredW = authoredW;
	out.authoredH = authoredH;
	const int innerW = authoredW - params.railWidth - 2 * params.inset;
	out.commandW = std::min(params.commandMaxW,
		std::max(params.commandMinW,
			static_cast<int>(std::round(static_cast<float>(innerW) * params.commandFraction))));
	out.commandX = authoredW - params.inset - out.commandW;

	// Left deck panel: rail + inset to the command column minus the gap,
	// header band bottom (y164) to viewport minus inset.
	const int panelX0 = params.railWidth + params.inset;
	const int panelX1 = out.commandX - params.columnGap;
	const int panelY0 = params.headerHeight + params.inset + params.titleBandH + params.titleGapH;
	const int panelY1 = authoredH - params.inset;
	out.deckPanel = {panelX0, panelY0, panelX1 - panelX0, panelY1 - panelY0};
	out.deckHeading = {panelX0, panelY0, panelX1 - panelX0, params.deckHeadingH};
	out.deckFooter = {panelX0, panelY1 - params.deckFooterH, panelX1 - panelX0, params.deckFooterH};
	out.fundsLine = {panelX0 + params.deckInset, panelY1 - params.deckFooterH + 8,
		panelX1 - panelX0 - 2 * params.deckInset, params.deckFooterH - 16};

	// Centered square grid inside the heading/footer/inset furniture.
	const int contentY0 = panelY0 + params.deckHeadingH;
	const int contentY1 = panelY1 - params.deckFooterH;
	const int zoneX0 = panelX0 + params.deckInset;
	const int zoneX1 = panelX1 - params.deckInset;
	const int zoneY0 = contentY0 + params.deckInset;
	const int zoneY1 = contentY1 - params.deckInset;
	const int side = std::max(6 * params.minHit,
		std::min(zoneX1 - zoneX0, zoneY1 - zoneY0));
	const int gridX = panelX0 + (panelX1 - panelX0 - side) / 2;
	const int gridY = contentY0 + (contentY1 - contentY0 - side) / 2;
	out.deckGrid = {gridX, gridY, side, side};
	out.deckCell = side / 6;
	out.showRowGutters = (panelX1 - panelX0 - 2 * params.deckInset - side) / 2 >= params.gutterMinSlack;
	out.showColGutters = params.deckHeadingH >= params.headingFontSize + params.cardPad;
	out.titleSelector.x = panelX1 - out.titleSelector.w;
	out.titleName.w = out.titleSelector.x - panelX0 - params.titleGapH;
	out.titleRegion.w = out.titleName.w;

	// Command column rows. Extra height past the minimum grows the five
	// illustrated cards equally (remainder top-down); logistics, services,
	// and every gap keep canonical size.
	const int extra = std::max(0, authoredH - params.minViewportH);
	const int perCard = extra / 5;
	const int remainder = extra % 5;
	int cardH[5];
	for (int i = 0; i < 5; ++i)
	{
		cardH[i] = params.cardH + perCard + (i < remainder ? 1 : 0);
	}
	static const char *const cardIds[5] = {
		"base.divers", "base.research", "base.manufacture", "base.crafts", "base.build"};
	static const char *const cardSlots[5] = {
		"card-divers", "card-research", "card-manufacture", "card-craft", "card-build"};
	int cursor = params.columnTop;
	int row = 0;
	for (int i = 0; i < 4; ++i, ++row)
	{
		out.rows[row] = {cardIds[i], cardSlots[i], -1, {out.commandX, cursor, out.commandW, cardH[i]}};
		cursor += cardH[i] + params.cardGap;
	}
	static const char *const logisticsIds[3] = {"base.transfer", "base.purchase", "base.sell"};
	for (int i = 0; i < 3; ++i, ++row)
	{
		out.rows[row] = {logisticsIds[i], "logistics-rows", i,
			{out.commandX + params.logisticsArtW + params.logisticsGap, cursor,
				out.commandW - params.logisticsArtW - params.logisticsGap, params.logisticsH}};
		cursor += (i < 2 ? params.logisticsStride : params.logisticsH + params.buildGap);
	}
	out.rows[row++] = {cardIds[4], cardSlots[4], -1, {out.commandX, cursor, out.commandW, cardH[4]}};
	cursor += cardH[4] + params.serviceGap;
	const int serviceW = (out.commandW - params.serviceSplitGap) / 2;
	out.rows[row++] = {"base.info", "service-1", -1, {out.commandX, cursor, serviceW, params.serviceH}};
	out.rows[row++] = {"base.new", "service-2", -1,
		{out.commandX + serviceW + params.serviceSplitGap, cursor,
			out.commandW - serviceW - params.serviceSplitGap, params.serviceH}};
	out.rows[row++] = {"navigation.world", "rail-world", -1, out.railWorld};

	out.columnHeading = {out.commandX, panelY0 + params.deckInset,
		out.commandW, params.deckHeadingH - 2 * params.cardPad};
	return out;
}

/// Rectangular card thumbnail inside 8px card padding at ~1.5:1 aspect.
/// The label keeps at least cardLabelMinW; the aspect is preserved under
/// the width cap. Pure; the painter and the contract share it.
inline CalypsoBasescapeHdRect calypsoBasescapeHdCardThumb(
	const CalypsoBasescapeHdRect &card, const CalypsoBasescapeHdFitParams &params)
{
	int thumbH = std::max(1, card.h - 2 * params.cardPad);
	int thumbW = static_cast<int>(std::round(static_cast<float>(thumbH) * 1.5f));
	const int maxThumbW = std::max(1, card.w - 5 * params.cardPad - params.cardLabelMinW);
	if (thumbW > maxThumbW)
	{
		thumbW = maxThumbW;
		thumbH = std::max(1, static_cast<int>(std::round(static_cast<float>(thumbW) / 1.5f)));
	}
	return {card.x + params.cardPad, card.y + (card.h - thumbH) / 2, thumbW, thumbH};
}

// --- Shared CSS-viewport projection (contract item 1) -----------------------
// One math source for widget placement (CalypsoBasescapeHdUi) and frame
// paint (CalypsoHdScreenRenderer::collectBasescape). Inputs are plain
// scalars: the real CSS viewport, the frozen presentation metrics, and the
// CommandCenter fit scale at that viewport. The formulas mirror the
// Geoscape path (density per axis from frozen metrics, offset inversion)
// and CalypsoF21Painter::project exactly (llround on the same products),
// so both consumers land on identical logical pixels on each axis,
// including resize and fractional DPR.

struct CalypsoBasescapeHdProjection
{
	int winX = 0;
	int winY = 0;
	int winW = 1280;
	int winH = 720;
	int designW = 1280;
	int designH = 720;
	double uiScale = 1.0;
	double uiAspectY = 1.0;
};

inline CalypsoBasescapeHdProjection calypsoBasescapeHdProjection(
	int cssW, int cssH, int physicalW, int physicalH,
	double presScaleX, double presScaleY, double offsetX, double offsetY,
	double layoutScale)
{
	CalypsoBasescapeHdProjection out;
	const double densityX = static_cast<double>(physicalW) / static_cast<double>(cssW);
	const double densityY = static_cast<double>(physicalH) / static_cast<double>(cssH);
	const double logicalPerCssX = densityX / presScaleX;
	const double logicalPerCssY = densityY / presScaleY;
	out.winX = static_cast<int>(std::llround(-(offsetX / presScaleX)));
	out.winY = static_cast<int>(std::llround(-(offsetY / presScaleY)));
	out.winW = static_cast<int>(std::llround(static_cast<double>(cssW) * logicalPerCssX));
	out.winH = static_cast<int>(std::llround(static_cast<double>(cssH) * logicalPerCssY));
	out.designW = cssW;
	out.designH = cssH;
	out.uiScale = logicalPerCssX * layoutScale;
	out.uiAspectY = logicalPerCssY / logicalPerCssX;
	return out;
}

struct CalypsoBasescapeHdLogicalRect
{
	int x = 0;
	int y = 0;
	int w = 1;
	int h = 1;
};

/// Project one authored design rect into logical pixels. Identical rounding
/// to CalypsoF21Painter::project with windowDesign {0,0,designW,designH}.
inline CalypsoBasescapeHdLogicalRect calypsoBasescapeHdProjectRect(
	const CalypsoBasescapeHdProjection &projection, const CalypsoBasescapeHdRect &design)
{
	CalypsoBasescapeHdLogicalRect out;
	out.x = projection.winX + static_cast<int>(std::llround(
		static_cast<double>(design.x) * projection.uiScale));
	out.y = projection.winY + static_cast<int>(std::llround(
		static_cast<double>(design.y) * projection.uiScale * projection.uiAspectY));
	out.w = std::max(1, static_cast<int>(std::llround(
		static_cast<double>(design.w) * projection.uiScale)));
	out.h = std::max(1, static_cast<int>(std::llround(
		static_cast<double>(design.h) * projection.uiScale * projection.uiAspectY)));
	return out;
}

// --- Placement-mode command column (F01 construction) -----------------------
// One math source for the placement Cancel widget (CalypsoBasescapeHdUi) and
// the placement details paint (CalypsoHdScreenRenderer::collectBasescape).
// Header/rail/background/title/grid come from the shared derived layout
// above; only the command column is repurposed: facility name reuses the
// canonical columnHeading rect, details/guidance stack from columnTop, and
// Cancel pins to the service-row band full width.

struct CalypsoBasescapeHdPlacementColumn
{
	CalypsoBasescapeHdRect name;
	CalypsoBasescapeHdRect details;
	CalypsoBasescapeHdRect guidance;
	CalypsoBasescapeHdRect cancel;
};

inline int calypsoBasescapeHdPlacementDetailFont(const CalypsoBasescapeHdFitParams &params)
{
	return params.smallActionFontSize - 2;
}
inline int calypsoBasescapeHdPlacementDetailLine(const CalypsoBasescapeHdFitParams &params)
{
	return params.smallActionFontSize + 2;
}
inline int calypsoBasescapeHdPlacementSelectH(const CalypsoBasescapeHdFitParams &params)
{
	return params.smallActionFontSize + 6;
}
inline int calypsoBasescapeHdPlacementStatusH(const CalypsoBasescapeHdFitParams &params)
{
	return params.smallActionFontSize + 8;
}
/// Two-line guidance block (select prompt, validity status) between the
/// details and the anchored Cancel, sharing the card gap.
inline int calypsoBasescapeHdPlacementGuidanceH(const CalypsoBasescapeHdFitParams &params)
{
	return calypsoBasescapeHdPlacementSelectH(params) + params.cardGap
		+ calypsoBasescapeHdPlacementStatusH(params);
}

/// Cancel pins to the derived service-row band at full command width: the
/// service band top is the shared derivation's service-1 (base.info) row
/// (rows 0-3 cards, 4-6 logistics, 7 build, 8 info, 9 new, 10 rail-world),
/// so the holder and the renderer consume one math source, no literals.
inline CalypsoBasescapeHdRect calypsoBasescapeHdPlacementCancelRect(
	const CalypsoBasescapeHdDerivedLayout &derived,
	const CalypsoBasescapeHdFitParams &params)
{
	return {derived.commandX, derived.rows[8].rect.y, derived.commandW, params.serviceH};
}


inline CalypsoBasescapeHdPlacementColumn calypsoBasescapeHdPlacementColumn(
	const CalypsoBasescapeHdDerivedLayout &derived,
	const CalypsoBasescapeHdFitParams &params)
{
	CalypsoBasescapeHdPlacementColumn out;
	out.name = derived.columnHeading;
	out.cancel = calypsoBasescapeHdPlacementCancelRect(derived, params);
	// Details/guidance split the available right column between the column
	// top and the anchored Cancel: guidance keeps its derived two-line block
	// at the bottom, details take the rest, so every native detail line fits.
	const int detailsY = params.columnTop;
	const int guidanceH = calypsoBasescapeHdPlacementGuidanceH(params);
	const int detailsH = out.cancel.y - params.cardPad - guidanceH - detailsY;
	const int lineH = calypsoBasescapeHdPlacementDetailLine(params);
	out.details = {derived.commandX, detailsY, derived.commandW, std::max(lineH, detailsH)};
	out.guidance = {derived.commandX, detailsY + out.details.h + params.cardPad,
		derived.commandW, guidanceH};
	return out;
}

// Semantic focus is supplied by the State focus owner, not the native
// broadcast-key flag, which defaults to true on every InteractiveSurface.

struct CalypsoBasescapeHdWidgetState
{
	bool pressed = false;
	bool hovered = false;
	bool focused = false;
};

inline CalypsoInteractionState calypsoBasescapeHdCardVisualState(
	const CalypsoBasescapeHdWidgetState *states, int count, int index)
{
	if (states == nullptr || count <= 0 || index < 0 || index >= count)
	{
		return CalypsoInteractionState::Rest;
	}
	if (states[index].pressed)
	{
		return CalypsoInteractionState::Pressed;
	}
	if (states[index].hovered)
	{
		return CalypsoInteractionState::Hover;
	}
	return states[index].focused ? CalypsoInteractionState::Focus
		: CalypsoInteractionState::Rest;
}

} // namespace Calypso
} // namespace OpenXcom
