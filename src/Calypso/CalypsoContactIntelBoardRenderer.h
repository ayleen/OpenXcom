#pragma once
/*
 * Calypso native HD contact-intel-board renderer.
 *
 * Shared physical renderer of the `contact-intel-board` archetype: a
 * board-dominant quadrant plot plus a contact report panel with an action
 * rail. Generated form contracts own copy, geometry, and rest-state tokens;
 * state adapters provide only localized text, live widgets, projected plot
 * coordinates, and interaction state. No screen identity may appear here.
 */
#ifdef __EMSCRIPTEN__

#include <cstdint>
#include <string>
#include <vector>

#include "CalypsoSmallConfirmationRenderer.h"

namespace OpenXcom
{
namespace Calypso
{

/// One semantic fact row of the contact report (label + runtime value).
struct CalypsoContactIntelFact
{
	std::string label;
	std::string value;
};

/// One plotted contact position: design-px point inside the plot area rect
/// plus an optional readable label anchored to the marker.
struct CalypsoContactIntelMarker
{
	int x = 0;
	int y = 0;
	std::string label;
};

struct CalypsoContactIntelBoardModel
{
	std::uint32_t familyId = 0;
	const void* instance = nullptr;
	Mod* mod = nullptr;
	bool wide = false;
	bool opaqueHarnessBackdrop = false;
	int designWidth = 0;
	int designHeight = 0;

	CalypsoLogicalRect window;
	CalypsoLogicalRect status;
	CalypsoLogicalRect plotPanel;
	CalypsoLogicalRect plotArea;
	CalypsoLogicalRect reportPanel;
	CalypsoLogicalRect warning;
	CalypsoLogicalRect title;
	CalypsoLogicalRect message;
	CalypsoLogicalRect footer;
	CalypsoLogicalRect note;
	/// Fact row rectangles in report order: label, value, label, value, ...
	std::vector<CalypsoLogicalRect> factRects;
	std::vector<CalypsoSmallConfirmationButton> buttons;
	std::vector<CalypsoContactIntelFact> facts;

	Surface* windowWidget = nullptr;
	Surface* warningWidget = nullptr;
	Surface* protocolWidget = nullptr;
	Surface* titleWidget = nullptr;
	Surface* messageWidget = nullptr;

	std::string protocolText;
	std::string warningGlyph = "!";
	std::string titleText;
	std::string messageText;
	std::string noteText;
	/// Course direction word (N/NE/.../NONE) driving the schematic vector.
	std::string courseWord;
	CalypsoContactIntelMarker contact;
	CalypsoContactIntelMarker base;

	float cutCornerPx = 0.0f;
	float protocolTextInsetPx = 0.0f;
	std::uint32_t panelFillTop = 0;
	std::uint32_t panelFillBottom = 0;
	std::uint32_t frameColor = 0;
	std::uint32_t protocolColor = 0;
	std::uint32_t dividerColor = 0;
	std::uint32_t footerDotColor = 0;
	std::uint32_t warningColor = 0;
	std::uint32_t plotFrameColor = 0;
	std::uint32_t plotGridColor = 0;
	std::uint32_t plotContactColor = 0;
	std::uint32_t plotContactHaloColor = 0;
	std::uint32_t plotBaseColor = 0;
	std::uint32_t plotCourseColor = 0;
	std::uint32_t factDividerColor = 0;

	double uiScale = 1.0;
	double visualScale = 1.0;
	double projectionScaleX = 1.0;
	double projectionScaleY = 1.0;
	int titleDesignHeight = 1;
	int motionDurationMs = 0;
	double motionScaleFrom = 1.0;
};

/// Collect the complete physical replacement of one contact-intel-board
/// frame. All-or-nothing: every item lands in one atomic subgroup.
void calypsoCollectContactIntelBoard(
	CalypsoHdFrameBuilder& builder,
	const CalypsoContactIntelBoardModel& model,
	CalypsoSmallConfirmationMotion& motion);

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
