#pragma once
/*
 * Calypso native HD small-confirmation renderer.
 *
 * Generated form contracts own copy, geometry, and rest-state tokens. State
 * adapters provide only localized text, live widgets, interaction state, and
 * projected rectangles. This renderer is the single physical implementation
 * of the archetype.
 */
#ifdef __EMSCRIPTEN__

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoHdInteractionState.h"
#include "CalypsoUiMetrics.h"

namespace OpenXcom
{
class Mod;
class Surface;
class TextButton;

namespace Calypso
{

struct CalypsoSmallConfirmationButton
{
	TextButton* widget = nullptr;
	TextButton* peer = nullptr;
	std::string text;
	CalypsoLogicalRect rect;
	CalypsoActionTone tone = CalypsoActionTone::Safe;
	std::uint32_t restFill = 0;
	std::uint32_t restBorder = 0;
	std::uint32_t textColor = 0;
};

struct CalypsoSmallConfirmationMotion
{
	bool presented = false;
	std::uint64_t presentedAtFrame = 0;
};

/**
 * Keep the generated rectangle as the visible action surface while expanding
 * only the logical input widget to the shared mobile touch floor. The right
 * edge stays on the generated action rail and vertical expansion is centered.
 */
template <typename Rect>
Rect calypsoSmallConfirmationTouchRect(Rect visual)
{
	const int width = std::max(visual.width, CALYPSO_MIN_TOUCH_TARGET);
	const int height = std::max(visual.height, CALYPSO_MIN_TOUCH_TARGET);
	visual.x -= width - visual.width;
	visual.y -= (height - visual.height) / 2;
	visual.width = width;
	visual.height = height;
	return visual;
}

struct CalypsoSmallConfirmationModel
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
	CalypsoLogicalRect warning;
	CalypsoLogicalRect title;
	CalypsoLogicalRect message;
	CalypsoLogicalRect footer;

	Surface* windowWidget = nullptr;
	Surface* warningWidget = nullptr;
	Surface* protocolWidget = nullptr;
	Surface* titleWidget = nullptr;
	Surface* messageWidget = nullptr;

	std::string protocolText;
	std::string warningGlyph = "!";
	std::string titleText;
	std::string messageText;
	std::vector<CalypsoSmallConfirmationButton> buttons;

	float cutCornerPx = 0.0f;
	float protocolTextInsetPx = 0.0f;
	std::uint32_t panelFillTop = 0;
	std::uint32_t panelFillBottom = 0;
	std::uint32_t frameColor = 0;
	std::uint32_t protocolColor = 0;
	std::uint32_t dividerColor = 0;
	std::uint32_t footerDotColor = 0;
	std::uint32_t warningColor = 0;

	double uiScale = 1.0;
	double visualScale = 1.0;
	double projectionScaleX = 1.0;
	double projectionScaleY = 1.0;
	int messageDesignWidth = 1;
	int titleDesignHeight = 1;
	int motionDurationMs = 0;
	double motionScaleFrom = 1.0;
};

void calypsoCollectSmallConfirmation(
	CalypsoHdFrameBuilder& builder,
	const CalypsoSmallConfirmationModel& model,
	CalypsoSmallConfirmationMotion& motion);

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
