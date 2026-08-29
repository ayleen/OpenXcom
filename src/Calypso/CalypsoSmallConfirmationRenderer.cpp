/*
 * Shared physical renderer for generated small-confirmation forms.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoSmallConfirmationRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <SDL.h>

#include "../Interface/TextButton.h"
#include "CalypsoHdFontSource.h"
#include "CalypsoHdHarnessHostState.h"
#include "CalypsoHdTheme.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoUiMetrics.h"

namespace OpenXcom
{
namespace Calypso
{

namespace
{

enum Role : std::uint32_t
{
	ROLE_BACKDROP = 1,
	ROLE_WINDOW = 2,
	ROLE_DECORATION = 3,
	ROLE_WARNING = 4,
	ROLE_PROTOCOL = 5,
	ROLE_TITLE = 6,
	ROLE_MESSAGE = 7,
	ROLE_BUTTON_BASE = 20,
	ROLE_BUTTON_LABEL_BASE = 40
};

CalypsoInteractionState buttonVisualState(const TextButton* button, const TextButton* peer)
{
	if (!button) return CalypsoInteractionState::Rest;
	if (button->isPressed()) return CalypsoInteractionState::Pressed;
	if (button->isHovered()) return CalypsoInteractionState::Hover;
	// A lone TextButton is constructed focused even before the user navigates.
	// Treat focus as a visible interaction state only when a peer lets us prove
	// that focus actually moved within an action group; otherwise the generated
	// rest-state contract and browser reference would never be observable.
	if (peer && button->isFocused() && !peer->isFocused())
		return CalypsoInteractionState::Focus;
	return CalypsoInteractionState::Rest;
}

CalypsoHdPanelStyle buttonStyle(
	const CalypsoSmallConfirmationButton& button,
	CalypsoInteractionState state)
{
	const CalypsoInteractionTokenPair tokens = calypsoInteractionTokenPair(button.tone, state);
	const std::uint32_t border = state == CalypsoInteractionState::Focus
		? CalypsoHdThemeGen::calypsoHdThemeColorForToken(calypsoFocusRingToken(button.tone))
		: CalypsoHdThemeGen::calypsoHdThemeColorForToken(tokens.borderToken);
	const std::uint32_t fill = CalypsoHdThemeGen::calypsoHdThemeColorForToken(tokens.fillToken);
	const bool primary = button.tone == CalypsoActionTone::Primary;
	const std::uint32_t resolvedBorder =
		(state == CalypsoInteractionState::Rest
			|| (primary && state != CalypsoInteractionState::Focus))
		? button.restBorder : border;
	const std::uint32_t resolvedFill =
		(state == CalypsoInteractionState::Rest || primary)
		? button.restFill : fill;

	CalypsoHdPanelStyle style = CalypsoHdTheme::calypsoHdButtonStyle(
		resolvedFill, resolvedBorder);
	style.borderWidthPx = CalypsoHdTheme::kBorderWidthPx
		+ (state == CalypsoInteractionState::Focus ? 1.0f : 0.0f);
	return style;
}

template <typename Model>
CalypsoHdPanelStyle windowStyle(const Model& model)
{
	CalypsoHdPanelStyle style;
	style.styled = true;
	style.shape = CalypsoHdPanelShape::OpposingCutRect;
	style.cutCornerPx = model.cutCornerPx * model.visualScale;
	style.borderWidthPx = 1.0f;
	style.borderColorRgba = model.frameColor;
	style.fillTopRgba = model.panelFillTop;
	style.fillBottomRgba = model.panelFillBottom;
	style.gradDirX = 0.18f;
	style.gradDirY = 1.0f;
	return style;
}

template <typename Model>
CalypsoHdPanelStyle glowStyle(
	const Model& model,
	std::uint32_t color,
	float radius)
{
	CalypsoHdPanelStyle style = CalypsoHdTheme::calypsoHdGlowStyle(color, radius);
	style.shape = CalypsoHdPanelShape::OpposingCutRect;
	style.cutCornerPx = model.cutCornerPx * model.visualScale;
	return style;
}

template <typename ScaleFn, typename AddFn>
void addCanonicalFooterDots(
	const CalypsoLogicalRect& footer,
	int rightEdge,
	std::uint32_t color,
	const ScaleFn& scaledPx,
	const AddFn& addDecoration)
{
	const int dotInsetX = scaledPx(12.0);
	const int dotInsetTop = scaledPx(10.0);
	const int dotInsetBottom = scaledPx(8.0);
	const int dotPitch = scaledPx(8.0);
	for (int y = footer.y + dotInsetTop;
		y < footer.y + footer.h - dotInsetBottom; y += dotPitch)
		for (int x = footer.x + dotInsetX; x < rightEdge - dotInsetX; x += dotPitch)
			addDecoration({x, y, 1, 1}, color);
}

} // namespace

void calypsoCollectSmallConfirmation(
	CalypsoHdFrameBuilder& builder,
	const CalypsoSmallConfirmationModel& model,
	CalypsoSmallConfirmationMotion& motion)
{
	if (!model.mod || !model.instance || model.window.w <= 0 || model.window.h <= 0) return;

	CalypsoTtfSourceDescriptor heading;
	CalypsoTtfSourceDescriptor body;
	CalypsoTtfSourceDescriptor mono;
	if (!calypsoHdResolveFontDescriptor(model.mod, "FONT_F34_SAIRA_700", heading)) return;
	if (!calypsoHdResolveFontDescriptor(model.mod, "FONT_F33_BODY", body)) return;
	if (!calypsoHdResolveFontDescriptor(model.mod, "FONT_F34_MONO", mono)) return;

	if (!motion.presented)
	{
		motion.presented = true;
		motion.presentedAtFrame = CalypsoHdUiOverlay::instance().frameId();
	}
	double progress = 1.0;
	const int holdPct = calypsoHarnessSession().motionHoldPct;
	if (holdPct >= 0)
	{
		progress = std::min(1.0, (double)holdPct / 100.0);
	}
	else if (!calypsoHarnessSession().motionDisabled && model.motionDurationMs > 0)
	{
		const std::uint64_t totalFrames = std::max<std::uint64_t>(1,
			(std::uint64_t)std::llround(model.motionDurationMs * 60.0 / 1000.0));
		const std::uint64_t frame = CalypsoHdUiOverlay::instance().frameId();
		const std::uint64_t elapsed = frame >= motion.presentedAtFrame
			? frame - motion.presentedAtFrame : 0;
		progress = std::min(1.0, (double)elapsed / (double)totalFrames);
	}
	const double ease = 1.0 - (1.0 - progress) * (1.0 - progress);
	const double scale = model.motionScaleFrom + (1.0 - model.motionScaleFrom) * ease;
	const float opacity = (float)ease;

	auto motionRect = [&](const CalypsoLogicalRect& rect) -> CalypsoLogicalRect
	{
		if (scale >= 1.0) return rect;
		const double cx = model.window.x + model.window.w * 0.5;
		const double cy = model.window.y + model.window.h * 0.5;
		const int x = (int)std::llround(cx + (rect.x - cx) * scale);
		const int y = (int)std::llround(cy + (rect.y - cy) * scale);
		return {x, y,
			std::max(1, (int)std::llround(rect.w * scale)),
			std::max(1, (int)std::llround(rect.h * scale))};
	};
	const CalypsoHdPresentationMetrics& presentationMetrics =
		CalypsoHdUiOverlay::instance().frozenMetrics();
	auto motionTextScale = [&](double restingScale, const CalypsoLogicalRect& restingRect,
		const CalypsoLogicalRect& animatedRect, bool vertical) -> double
	{
		const CalypsoPhysRect restingPhysical =
			calypsoMapLogicalRect(restingRect, presentationMetrics);
		const CalypsoPhysRect animatedPhysical =
			calypsoMapLogicalRect(animatedRect, presentationMetrics);
		return calypsoHdMotionProjectionScale(restingScale,
			vertical ? restingPhysical.h : restingPhysical.w,
			vertical ? animatedPhysical.h : animatedPhysical.w);
	};
	auto scaledPx = [&](double value, int minimum = 1) -> int
	{
		return std::max(minimum,
			(int)calypsoHdRoundToInt(value * model.visualScale));
	};

	builder.beginSubgroup();
	int order = 0;
	auto stamp = [&](CalypsoHdItem& item, std::uint32_t role)
	{
		const std::uint64_t instance = reinterpret_cast<std::uintptr_t>(model.instance);
		item.claim = {model.familyId, role, instance, 1u, (std::uint32_t)order};
		item.order = {0, 0, model.familyId, instance, 0, 1u, order, role};
		++order;
	};
	auto addPanel = [&](const CalypsoLogicalRect& rect, std::uint32_t color,
		const void* widget, std::uint32_t role, bool animate)
	{
		if (rect.w <= 0 || rect.h <= 0) return;
		CalypsoHdItem item;
		item.kind = CalypsoHdItemKind::Panel;
		item.rect = animate ? motionRect(rect) : rect;
		item.colorRgba = color;
		item.opacity = animate ? opacity : 1.0f;
		item.widget = widget;
		stamp(item, role);
		builder.add(item);
	};
	auto addStyled = [&](const CalypsoLogicalRect& rect, const CalypsoHdPanelStyle& style,
		const void* widget, std::uint32_t role)
	{
		if (rect.w <= 0 || rect.h <= 0) return;
		CalypsoHdItem item;
		item.kind = CalypsoHdItemKind::Panel;
		item.rect = motionRect(rect);
		item.colorRgba = style.fillTopRgba;
		item.panelStyle = style;
		item.opacity = opacity;
		item.widget = widget;
		stamp(item, role);
		builder.add(item);
	};
	auto addDecoration = [&](const CalypsoLogicalRect& rect, std::uint32_t color)
	{
		CalypsoHdPanelStyle style;
		style.styled = true;
		style.fillTopRgba = color;
		style.fillBottomRgba = color;
		addStyled(rect, style, nullptr, ROLE_DECORATION);
	};
	auto addText = [&](const CalypsoLogicalRect& sourceRect, const void* widget,
		const CalypsoTtfSourceDescriptor& font, const std::string& text,
		std::uint32_t color, CalypsoHdHAlign hAlign, CalypsoHdVAlign vAlign,
		int fontSize, int wrapWidth, double trackingEm, std::uint32_t role)
	{
		if (text.empty() || sourceRect.w <= 0 || sourceRect.h <= 0) return;
		CalypsoHdTextRasterKey key;
		key.source = font;
		key.physicalPixelHeight = std::max(1, fontSize);
		key.text = text;
		key.wrapWidth = wrapWidth;
		key.colorRgba = color;
		key.direction = CalypsoTextDirection::LTR;
		if (trackingEm > 0.0 && wrapWidth == 0)
			key.letterSpacingPx = std::max(1, (int)calypsoHdRoundToInt(fontSize * trackingEm));

		CalypsoHdItem item;
		item.kind = CalypsoHdItemKind::Text;
		item.rect = motionRect(sourceRect);
		item.colorRgba = color;
		item.rasterKey = key;
		item.textScaleX = (float)motionTextScale(
			model.projectionScaleX, sourceRect, item.rect, false);
		item.textScaleY = (float)motionTextScale(
			model.projectionScaleY, sourceRect, item.rect, true);
		item.hAlign = hAlign;
		item.vAlign = vAlign;
		item.opacity = opacity;
		item.widget = widget;
		stamp(item, role);
		builder.add(item);
	};

	addPanel({0, 0, model.designWidth, model.designHeight},
		model.opaqueHarnessBackdrop ? calypsoRgba(0, 0, 0, 0xff) : CalypsoHdTheme::kBackdropDim,
		nullptr, ROLE_BACKDROP, false);
	const int shadowX = scaledPx(2.0);
	const int shadowY = scaledPx(8.0);
	addStyled({model.window.x - shadowX, model.window.y + shadowY,
		model.window.w + shadowX * 2, model.window.h},
		glowStyle(model, CalypsoHdTheme::kShadowGlow,
			CalypsoHdTheme::kShadowGlowRadiusPx * model.visualScale),
		nullptr, ROLE_WINDOW);
	addStyled(model.window,
		glowStyle(model, CalypsoHdTheme::kHaloGlow,
			CalypsoHdTheme::kHaloGlowRadiusPx * model.visualScale),
		nullptr, ROLE_WINDOW);
	addStyled(model.window, windowStyle(model), model.windowWidget, ROLE_WINDOW);

	addDecoration({model.status.x, model.status.y + model.status.h - 1, model.status.w, 1},
		model.dividerColor);
	addDecoration({model.footer.x, model.footer.y, model.footer.w, 1}, model.dividerColor);
	int firstButtonX = model.footer.x + model.footer.w;
	for (const auto& button : model.buttons)
		firstButtonX = std::min(firstButtonX, button.rect.x);
	addCanonicalFooterDots(model.footer, firstButtonX, model.footerDotColor,
		scaledPx, addDecoration);

	CalypsoHdPanelStyle warning;
	warning.styled = true;
	warning.shape = CalypsoHdPanelShape::WarningTriangle;
	warning.borderWidthPx = (float)scaledPx(2.0);
	warning.borderColorRgba = model.warningColor;
	warning.fillTopRgba = calypsoRgba(0, 0, 0, 0);
	warning.fillBottomRgba = calypsoRgba(0, 0, 0, 0);
	addStyled(model.warning, warning, model.warningWidget, ROLE_WARNING);

	for (std::size_t i = 0; i < model.buttons.size(); ++i)
	{
		const auto& button = model.buttons[i];
		const CalypsoInteractionState state = buttonVisualState(button.widget, button.peer);
		CalypsoHdPanelStyle style = buttonStyle(button, state);
		addStyled(button.rect, style, button.widget, ROLE_BUTTON_BASE + (std::uint32_t)i);
	}

	const int protocolInset = (int)std::llround(
		model.protocolTextInsetPx * model.visualScale * model.uiScale);
	const CalypsoLogicalRect protocolRect{
		model.status.x + protocolInset, model.status.y,
		std::max(1, model.status.w - 2 * protocolInset), model.status.h};
	addText(protocolRect, model.protocolWidget, mono, model.protocolText,
		model.protocolColor, CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle,
		scaledPx(model.wide ? 10.0 : 9.0, 8), 0, 0.10, ROLE_PROTOCOL);
	addText(model.warning, model.warningWidget, heading, model.warningGlyph,
		model.warningColor, CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle,
		scaledPx(model.wide ? 18.0 : 16.0, 11), 0, 0.0, ROLE_WARNING);
	addText(model.title, model.titleWidget, heading, model.titleText,
		CalypsoHdTheme::kNearWhite, CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle,
		std::max(1, (int)calypsoHdRoundToInt(
			model.titleDesignHeight * CalypsoHdTheme::kTitleFontSizeScale)),
		0, CalypsoHdTheme::kTitleTrackingEm, ROLE_TITLE);

	if (!model.messageText.empty())
	{
		const double bodySizeScale = model.wide
			? CalypsoHdTheme::kBodyFontSizeScaleWide : CalypsoHdTheme::kBodyFontSizeScaleCompact;
		const double bodyWidthScale = model.wide
			? CalypsoHdTheme::kBodyFontWidthScaleWide : CalypsoHdTheme::kBodyFontWidthScaleCompact;
		const double wrapMeasureScale = model.wide
			? CalypsoHdTheme::kBodyWrapMeasureScaleWide : CalypsoHdTheme::kBodyWrapMeasureScaleCompact;
		const double lineHeightScale = model.wide
			? CalypsoHdTheme::kBodyProjectionLineHeightScaleWide
			: CalypsoHdTheme::kBodyProjectionLineHeightScaleCompact;
		const int bodyPx = scaledPx(
			CalypsoHdTheme::kBodyFontSizePx * bodySizeScale, 12);

		CalypsoHdTextRasterKey key;
		key.source = body;
		key.physicalPixelHeight = bodyPx;
		key.text = model.messageText;
		key.wrapWidth = std::max(1, model.messageDesignWidth);
		const double lineHeight = CalypsoHdTheme::kBodyFontSizePx * bodySizeScale
			* CalypsoHdTheme::kBodyLineHeight * lineHeightScale * model.visualScale;
		key.lineHeightPx = std::max(1, (int)calypsoHdRoundToInt(lineHeight));
		key.lineHeightMilliPx = std::max(1, (int)calypsoHdRoundToInt(lineHeight * 1000.0));
		key.horizontalScalePermille = std::max(1,
			(int)calypsoHdRoundToInt(bodyWidthScale * 1000.0));
		key.verticalScalePermille = 1000;
		key.wrapMeasureScalePermille = std::max(1,
			(int)calypsoHdRoundToInt(wrapMeasureScale * 1000.0));
		key.colorRgba = CalypsoHdTheme::kNearWhite;
		key.direction = CalypsoTextDirection::LTR;

		CalypsoHdItem item;
		item.kind = CalypsoHdItemKind::Text;
		item.rect = motionRect(model.message);
		item.colorRgba = CalypsoHdTheme::kNearWhite;
		item.rasterKey = key;
		item.textScaleX = (float)motionTextScale(
			model.projectionScaleX, model.message, item.rect, false);
		item.textScaleY = (float)motionTextScale(
			model.projectionScaleY * (model.wide ? 1.0 : 0.98),
			model.message, item.rect, true);
		item.hAlign = CalypsoHdHAlign::Left;
		item.vAlign = CalypsoHdVAlign::Top;
		item.opacity = opacity;
		item.widget = model.messageWidget;
		stamp(item, ROLE_MESSAGE);
		builder.add(item);
	}

	const int labelPx = scaledPx(
		CalypsoHdTheme::kLabelFontSizePx * (model.wide
			? CalypsoHdTheme::kLabelFontSizeScaleWide
			: CalypsoHdTheme::kLabelFontSizeScaleCompact), 11);
	for (std::size_t i = 0; i < model.buttons.size(); ++i)
	{
		const auto& button = model.buttons[i];
		addText(button.rect, button.widget, heading, button.text, button.textColor,
			CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, labelPx, 0,
			CalypsoHdTheme::kLabelTrackingEm, ROLE_BUTTON_LABEL_BASE + (std::uint32_t)i);
	}
}

namespace {

enum BoardRole : std::uint32_t
{
	BOARD_ROLE_BACKDROP = 1,
	BOARD_ROLE_WINDOW = 2,
	BOARD_ROLE_DECORATION = 3,
	BOARD_ROLE_WARNING = 4,
	BOARD_ROLE_PROTOCOL = 5,
	BOARD_ROLE_TITLE = 6,
	BOARD_ROLE_MESSAGE = 7,
	BOARD_ROLE_PLOT_PANEL = 60,
	BOARD_ROLE_PLOT_GRID = 61,
	BOARD_ROLE_PLOT_BASE = 62,
	BOARD_ROLE_PLOT_CONTACT = 63,
	BOARD_ROLE_PLOT_COURSE = 64,
	BOARD_ROLE_PLOT_LABEL = 65,
	BOARD_ROLE_FACT = 66,
	BOARD_ROLE_NOTE = 67,
	BOARD_ROLE_BUTTON_BASE = 80,
	BOARD_ROLE_BUTTON_LABEL_BASE = 100
};

/// Design-px course unit vectors for the eight compass words.
bool courseVector(const std::string& word, double& dx, double& dy)
{
	static const struct { const char* word; double dx; double dy; } table[] = {
		{"N", 0.0, -1.0}, {"NE", 0.70710678, -0.70710678},
		{"E", 1.0, 0.0},  {"SE", 0.70710678, 0.70710678},
		{"S", 0.0, 1.0},  {"SW", -0.70710678, 0.70710678},
		{"W", -1.0, 0.0}, {"NW", -0.70710678, -0.70710678},
	};
	for (const auto& entry : table)
	{
		if (word == entry.word) { dx = entry.dx; dy = entry.dy; return true; }
	}
	return false;
}


} // namespace

void calypsoCollectContactIntelBoard(
	CalypsoHdFrameBuilder& builder,
	const CalypsoContactIntelBoardModel& model,
	CalypsoSmallConfirmationMotion& motion)
{
	if (!model.mod || !model.instance || model.window.w <= 0 || model.window.h <= 0) return;

	CalypsoTtfSourceDescriptor heading;
	CalypsoTtfSourceDescriptor body;
	CalypsoTtfSourceDescriptor mono;
	if (!calypsoHdResolveFontDescriptor(model.mod, "FONT_F34_SAIRA_700", heading)) return;
	if (!calypsoHdResolveFontDescriptor(model.mod, "FONT_F33_BODY", body)) return;
	if (!calypsoHdResolveFontDescriptor(model.mod, "FONT_F34_MONO", mono)) return;

	const bool wide = model.layout == CalypsoContactIntelLayout::Wide;

	if (!motion.presented)
	{
		motion.presented = true;
		motion.presentedAtFrame = CalypsoHdUiOverlay::instance().frameId();
	}
	double progress = 1.0;
	const int holdPct = calypsoHarnessSession().motionHoldPct;
	if (holdPct >= 0)
	{
		progress = std::min(1.0, (double)holdPct / 100.0);
	}
	else if (!calypsoHarnessSession().motionDisabled && model.motionDurationMs > 0)
	{
		const std::uint64_t totalFrames = std::max<std::uint64_t>(1,
			(std::uint64_t)std::llround(model.motionDurationMs * 60.0 / 1000.0));
		const std::uint64_t frame = CalypsoHdUiOverlay::instance().frameId();
		const std::uint64_t elapsed = frame >= motion.presentedAtFrame
			? frame - motion.presentedAtFrame : 0;
		progress = std::min(1.0, (double)elapsed / (double)totalFrames);
	}
	const double ease = 1.0 - (1.0 - progress) * (1.0 - progress);
	const double scale = model.motionScaleFrom + (1.0 - model.motionScaleFrom) * ease;
	const float opacity = (float)ease;

	auto motionRect = [&](const CalypsoLogicalRect& rect) -> CalypsoLogicalRect
	{
		if (scale >= 1.0) return rect;
		const double cx = model.window.x + model.window.w * 0.5;
		const double cy = model.window.y + model.window.h * 0.5;
		const int x = (int)std::llround(cx + (rect.x - cx) * scale);
		const int y = (int)std::llround(cy + (rect.y - cy) * scale);
		return {x, y,
			std::max(1, (int)std::llround(rect.w * scale)),
			std::max(1, (int)std::llround(rect.h * scale))};
	};
	const CalypsoHdPresentationMetrics& presentationMetrics =
		CalypsoHdUiOverlay::instance().frozenMetrics();
	auto motionTextScale = [&](double restingScale, const CalypsoLogicalRect& restingRect,
		const CalypsoLogicalRect& animatedRect, bool vertical) -> double
	{
		const CalypsoPhysRect restingPhysical =
			calypsoMapLogicalRect(restingRect, presentationMetrics);
		const CalypsoPhysRect animatedPhysical =
			calypsoMapLogicalRect(animatedRect, presentationMetrics);
		return calypsoHdMotionProjectionScale(restingScale,
			vertical ? restingPhysical.h : restingPhysical.w,
			vertical ? animatedPhysical.h : animatedPhysical.w);
	};
	auto scaledPx = [&](double value, int minimum = 1) -> int
	{
		return std::max(minimum,
			(int)calypsoHdRoundToInt(value * model.visualScale));
	};

	builder.beginSubgroup();
	int order = 0;
	// Canonical form roles: the board changes composition, never typography.
	const int protocolPx = scaledPx(wide ? 10.0 : 9.0, 8);
	const int titlePx = std::max(1, (int)calypsoHdRoundToInt(
		model.titleDesignHeight * CalypsoHdTheme::kTitleFontSizeScale));
	const int messagePx = scaledPx(CalypsoHdTheme::kBodyFontSizePx, 12);
	const int factLabelPx = scaledPx(CalypsoHdTheme::kBodyFontSizePx, 11);
	const int factValuePx = scaledPx(CalypsoHdTheme::kBodyFontSizePx, 12);
	const int actionPx = scaledPx(CalypsoHdTheme::kLabelFontSizePx, 12);
	const int cardinalPx = scaledPx(wide ? 12.0 : 11.0, 8);
	const int notePx = scaledPx(9.0, 8);
	auto stamp = [&](CalypsoHdItem& item, std::uint32_t role)
	{
		const std::uint64_t instance = reinterpret_cast<std::uintptr_t>(model.instance);
		item.claim = {model.familyId, role, instance, 1u, (std::uint32_t)order};
		item.order = {0, 0, model.familyId, instance, 0, 1u, order, role};
		++order;
	};
	auto addStyled = [&](const CalypsoLogicalRect& rect, const CalypsoHdPanelStyle& style,
		const void* widget, std::uint32_t role)
	{
		if (rect.w <= 0 || rect.h <= 0) return;
		CalypsoHdItem item;
		item.kind = CalypsoHdItemKind::Panel;
		item.rect = motionRect(rect);
		item.colorRgba = style.fillTopRgba;
		item.panelStyle = style;
		item.opacity = opacity;
		item.widget = widget;
		stamp(item, role);
		builder.add(item);
	};
	auto addQuad = [&](const CalypsoLogicalRect& rect, std::uint32_t color, std::uint32_t role)
	{
		if (rect.w <= 0 || rect.h <= 0) return;
		CalypsoHdPanelStyle style;
		style.styled = true;
		style.fillTopRgba = color;
		style.fillBottomRgba = color;
		addStyled(rect, style, nullptr, role);
	};
	// Circle outline through the existing rounded-rect SDF (radius = half the
	// side). No new shader primitive is required for the v2 radar.
	auto addRing = [&](int centerX, int centerY, int diameter, std::uint32_t color,
		float borderWidthPx, std::uint32_t role)
	{
		if (diameter <= 0) return;
		CalypsoHdPanelStyle style;
		style.styled = true;
		style.shape = CalypsoHdPanelShape::RoundedRect;
		style.radiusPx = diameter / 2.0f;
		style.borderWidthPx = borderWidthPx;
		style.borderColorRgba = color;
		style.fillTopRgba = calypsoRgba(0, 0, 0, 0);
		style.fillBottomRgba = calypsoRgba(0, 0, 0, 0);
		addStyled({centerX - diameter / 2, centerY - diameter / 2, diameter, diameter},
			style, nullptr, role);
	};
	auto addDisc = [&](int centerX, int centerY, int diameter, std::uint32_t color,
		std::uint32_t role)
	{
		if (diameter <= 0) return;
		CalypsoHdPanelStyle style;
		style.styled = true;
		style.shape = CalypsoHdPanelShape::RoundedRect;
		style.radiusPx = diameter / 2.0f;
		style.fillTopRgba = color;
		style.fillBottomRgba = color;
		addStyled({centerX - diameter / 2, centerY - diameter / 2, diameter, diameter},
			style, nullptr, role);
	};
	auto addText = [&](const CalypsoLogicalRect& sourceRect, const void* widget,
		const CalypsoTtfSourceDescriptor& font, const std::string& text,
		std::uint32_t color, CalypsoHdHAlign hAlign, CalypsoHdVAlign vAlign,
		int fontSize, int wrapWidth, double trackingEm, std::uint32_t role)
	{
		if (text.empty() || sourceRect.w <= 0 || sourceRect.h <= 0) return;
		CalypsoHdTextRasterKey key;
		key.source = font;
		key.physicalPixelHeight = std::max(1, fontSize);
		key.text = text;
		key.wrapWidth = wrapWidth;
		key.colorRgba = color;
		key.direction = CalypsoTextDirection::LTR;
		if (trackingEm > 0.0 && wrapWidth == 0)
			key.letterSpacingPx = std::max(1, (int)calypsoHdRoundToInt(fontSize * trackingEm));

		CalypsoHdItem item;
		item.kind = CalypsoHdItemKind::Text;
		item.rect = motionRect(sourceRect);
		item.colorRgba = color;
		item.rasterKey = key;
		item.textScaleX = (float)motionTextScale(
			model.projectionScaleX, sourceRect, item.rect, false);
		item.textScaleY = (float)motionTextScale(
			model.projectionScaleY, sourceRect, item.rect, true);
		item.hAlign = hAlign;
		item.vAlign = vAlign;
		item.opacity = opacity;
		item.widget = widget;
		stamp(item, role);
		builder.add(item);
	};

	addQuad({0, 0, model.designWidth, model.designHeight},
		model.opaqueHarnessBackdrop ? calypsoRgba(0, 0, 0, 0xff) : model.backdropColor,
		BOARD_ROLE_BACKDROP);

	const int shadowX = scaledPx(2.0);
	const int shadowY = scaledPx(8.0);
	addStyled({model.window.x - shadowX, model.window.y + shadowY,
		model.window.w + shadowX * 2, model.window.h},
		glowStyle(model, CalypsoHdTheme::kShadowGlow,
			CalypsoHdTheme::kShadowGlowRadiusPx * model.visualScale),
		nullptr, BOARD_ROLE_WINDOW);
	addStyled(model.window,
		glowStyle(model, CalypsoHdTheme::kHaloGlow,
			CalypsoHdTheme::kHaloGlowRadiusPx * model.visualScale),
		nullptr, BOARD_ROLE_WINDOW);
	addStyled(model.window, windowStyle(model), model.windowWidget,
		BOARD_ROLE_WINDOW);

	// Status strip: protocol rail + closing divider.
	addQuad({model.status.x, model.status.y + model.status.h - 1, model.status.w, 1},
		model.dividerColor, BOARD_ROLE_DECORATION);
	const int protocolInset = (int)std::llround(
		model.protocolTextInsetPx * model.visualScale * model.uiScale);
	addText({model.status.x + protocolInset, model.status.y,
			std::max(1, model.status.w - 2 * protocolInset), model.status.h},
		model.protocolWidget, mono, model.protocolText, model.protocolColor,
		CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle,
		protocolPx, 0, 0.10, BOARD_ROLE_PROTOCOL);


	const int centerX = model.plotArea.x + model.plotArea.w / 2;
	const int centerY = model.plotArea.y + model.plotArea.h / 2;
	const int radarMin = std::min(model.plotArea.w, model.plotArea.h);
	// Concentric range rings; the outer ring is the strong accent.
	const float ringPx = (float)std::max(1, scaledPx(1.0));
	for (const double fraction : {0.285, 0.51, 0.73})
	{
		addRing(centerX, centerY, (int)std::llround(radarMin * fraction),
			model.radarRingColor, ringPx, BOARD_ROLE_PLOT_GRID);
	}
	addRing(centerX, centerY, (int)std::llround(radarMin * 0.95),
		model.radarStrongRingColor, ringPx, BOARD_ROLE_PLOT_GRID);
	// Fine bearing ticks make the circular instrument read as a sonar rather
	// than a generic chart. Stronger twelve-point ticks anchor the clock face.
	const double fullTurn = 6.28318530717958647692;
	const double tickRadius = radarMin * 0.475;
	for (int i = 0; i < 72; ++i)
	{
		const double angle = fullTurn * i / 72.0;
		const int tick = scaledPx(i % 6 == 0 ? 2.0 : 1.0);
		addDisc(
			(int)std::llround(centerX + std::cos(angle) * tickRadius),
			(int)std::llround(centerY + std::sin(angle) * tickRadius),
			tick, i % 6 == 0 ? model.radarStrongRingColor : model.radarRingColor,
			BOARD_ROLE_PLOT_GRID);
	}
	// Cross axes.
	addQuad({model.plotArea.x, centerY, model.plotArea.w, 1},
		model.radarAxisColor, BOARD_ROLE_PLOT_GRID);
	addQuad({centerX, model.plotArea.y, 1, model.plotArea.h},
		model.radarAxisColor, BOARD_ROLE_PLOT_GRID);

	// Clockwise live sonar sweep. Only an explicit deterministic harness
	// capture may freeze it; stale harness flags never stop production motion.
	if (model.radarSweepPeriodMs > 0)
	{
		const CalypsoHarnessSession& session = calypsoHarnessSession();
		const bool frozenCapture = session.hostUp && session.motionDisabled;
		double turn = 0.0;
		if (!frozenCapture)
		{
			const std::uint32_t period = (std::uint32_t)model.radarSweepPeriodMs;
			turn = (double)(SDL_GetTicks() % period) / (double)period;
		}
		const double sweepAngle = turn * fullTurn - 1.57079632679489661923;
		const double beamRadius = radarMin * 0.475;
		for (int ray = 3; ray >= 0; --ray)
		{
			const double angle = sweepAngle - ray * 0.11;
			const double beamX = std::cos(angle);
			const double beamY = std::sin(angle);
			const int beamDot = std::max(1, scaledPx(ray == 0 ? 3.0 : 2.0));
			const std::uint32_t color =
				ray == 0 ? model.plotBaseColor : model.radarSweepColor;
			for (double distance = scaledPx(5.0); distance <= beamRadius;
				distance += beamDot)
			{
				const int x = (int)std::llround(centerX + beamX * distance);
				const int y = (int)std::llround(centerY + beamY * distance);
				addQuad({x - beamDot / 2, y - beamDot / 2, beamDot, beamDot},
					color, BOARD_ROLE_PLOT_GRID);
			}
		}
	}

	// Base marker: the radar center is the nearest base.
	if (model.base.x || model.base.y)
	{
		addDisc(centerX, centerY, scaledPx(8.0), model.plotBaseColor, BOARD_ROLE_PLOT_BASE);
	}

	// Contact bearing sweep: soft filled discs behind the contact tint the
	// direction without a rotated wedge primitive (deterministic, rest state).
	double cdx = 0.0;
	double cdy = 0.0;
	const bool hasCourse = courseVector(model.courseWord, cdx, cdy);
	if (hasCourse)
	{
		const double sweepDirX = (double)(model.contact.x - centerX);
		const double sweepDirY = (double)(model.contact.y - centerY);
		const double sweepLen = std::sqrt(sweepDirX * sweepDirX + sweepDirY * sweepDirY);
		if (sweepLen > 1.0)
		{
			addDisc(model.contact.x, model.contact.y,
				(int)std::llround(radarMin * 0.24), model.radarSweepColor,
				BOARD_ROLE_PLOT_GRID);
			addDisc(model.contact.x, model.contact.y,
				(int)std::llround(radarMin * 0.14), model.radarSweepColor,
				BOARD_ROLE_PLOT_GRID);
		}
	}

	// Dotted base->contact bearing line; the final quarter picks up the amber
	// course tint toward the target.
	{
		const double dxFull = (double)(model.contact.x - centerX);
		const double dyFull = (double)(model.contact.y - centerY);
		const double dist = std::sqrt(dxFull * dxFull + dyFull * dyFull);
		if (dist > 2.0)
		{
			const double step = std::max(3.0, (double)scaledPx(5.0));
			const int dot = std::max(2, scaledPx(2.0));
			for (double t = 0.0; t < 1.0; t += step / dist)
			{
				const int x = (int)std::llround(centerX + dxFull * t);
				const int y = (int)std::llround(centerY + dyFull * t);
				addQuad({x - dot / 2, y - dot / 2, dot, dot},
					t > 0.75 ? model.plotCourseColor : model.plotBaseColor,
					BOARD_ROLE_PLOT_COURSE);
			}
		}
	}

	// Contact marker: soft halo disc + hard square core (no pulse; captures).
	const int core = scaledPx(14.0);
	const int halo = scaledPx(28.0);
	addDisc(model.contact.x, model.contact.y, halo, model.plotContactHaloColor,
		BOARD_ROLE_PLOT_CONTACT);
	addQuad({model.contact.x - core / 2, model.contact.y - core / 2, core, core},
		model.plotContactColor, BOARD_ROLE_PLOT_CONTACT);

	// Course stub: short dotted heading ray from the contact marker.
	if (hasCourse)
	{
		const double stubLen = scaledPx(wide ? 28.0 : (model.layout
			== CalypsoContactIntelLayout::Portrait ? 22.0 : 24.0));
		const double step = std::max(3.0, (double)scaledPx(4.0));
		const int dot = std::max(2, scaledPx(2.0));
		const int start = core / 2 + dot;
		for (double d = start; d < stubLen; d += step)
		{
			addQuad({(int)std::llround(model.contact.x + cdx * d) - dot / 2,
					(int)std::llround(model.contact.y + cdy * d) - dot / 2, dot, dot},
				model.plotCourseColor, BOARD_ROLE_PLOT_COURSE);
		}
	}

	// Cardinal letters stay inside the plot panel even when the glyph boxes
	// leave the circular plot area.
	{
		const int card = cardinalPx;
		const int box = scaledPx(24.0);
		const int boxH = scaledPx(18.0);
		const int left = model.plotArea.x;
		const int top = model.plotArea.y;
		const int rightEdge = model.plotArea.x + model.plotArea.w;
		const int bottomEdge = model.plotArea.y + model.plotArea.h;
		struct Cardinal { const char* word; CalypsoLogicalRect rect; };
		const Cardinal cards[] = {
			{"N", {centerX - box / 2, top - scaledPx(4.0), box, boxH}},
			{"S", {centerX - box / 2, bottomEdge - scaledPx(14.0), box, boxH}},
			{"W", {left - scaledPx(4.0), centerY - boxH / 2, box, boxH}},
			{"E", {rightEdge - scaledPx(20.0), centerY - boxH / 2, box, boxH}},
		};
		for (const auto& card_ : cards)
		{
			addText(card_.rect, nullptr, mono, card_.word, CalypsoHdTheme::kNearWhite,
				CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, card, 0, 0.0,
				BOARD_ROLE_PLOT_LABEL);
		}
	}

	// Report panel: warning glyph, title, detection status.
	CalypsoHdPanelStyle warning;
	warning.styled = true;
	warning.shape = CalypsoHdPanelShape::WarningTriangle;
	warning.borderWidthPx = (float)scaledPx(2.0);
	warning.borderColorRgba = model.warningColor;
	warning.fillTopRgba = calypsoRgba(0, 0, 0, 0);
	warning.fillBottomRgba = calypsoRgba(0, 0, 0, 0);
	addStyled(model.warning, warning, model.warningWidget, BOARD_ROLE_WARNING);
	addText(model.warning, model.warningWidget, heading, model.warningGlyph,
		model.warningColor, CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle,
		scaledPx(wide ? 18.0 : 16.0, 11), 0, 0.0, BOARD_ROLE_WARNING);
	addText(model.title, model.titleWidget, heading, model.titleText,
		CalypsoHdTheme::kNearWhite, CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle,
		titlePx, 0, CalypsoHdTheme::kTitleTrackingEm, BOARD_ROLE_TITLE);
	addText(model.message, model.messageWidget, body, model.messageText,
		CalypsoHdTheme::kNearWhite, CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle,
		messagePx, 0, 0.08, BOARD_ROLE_MESSAGE);

	// Fact rows: canonical body-scale mono label left, bright heading value
	// right, and a hairline divider per row.
	for (std::size_t i = 0; i < model.facts.size(); ++i)
	{
		const std::size_t labelIndex = i * 2;
		const std::size_t valueIndex = i * 2 + 1;
		if (valueIndex >= model.factRects.size()) break;
		const auto& fact = model.facts[i];
		if (fact.label.empty() && fact.value.empty()) continue;
		const CalypsoLogicalRect& labelRect = model.factRects[labelIndex];
		const CalypsoLogicalRect& valueRect = model.factRects[valueIndex];
		addQuad({labelRect.x, labelRect.y + labelRect.h - 1,
				valueRect.x + valueRect.w - labelRect.x, 1},
			model.factDividerColor, BOARD_ROLE_FACT);
		addText(labelRect, nullptr, mono, fact.label, model.factLabelColor,
			CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, factLabelPx, 0, 0.0,
			BOARD_ROLE_FACT);
		addText(valueRect, nullptr, heading, fact.value, model.factValueColor,
			CalypsoHdHAlign::Right, CalypsoHdVAlign::Middle, factValuePx, 0, 0.02,
			BOARD_ROLE_FACT);
	}

	if (!model.noteText.empty() && model.note.w > 0 && model.note.h > 0)
	{
		addText(model.note, nullptr, mono, model.noteText, model.protocolColor,
			CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle,
			notePx, 0, 0.0, BOARD_ROLE_NOTE);
	}

	// Action area: canonical footer material with generated action geometry.
	addQuad({model.footer.x, model.footer.y, model.footer.w, 1},
		model.dividerColor, BOARD_ROLE_DECORATION);
	addCanonicalFooterDots(model.footer, model.footer.x + model.footer.w,
		model.footerDotColor, scaledPx,
		[&](const CalypsoLogicalRect& rect, std::uint32_t color)
		{
			addQuad(rect, color, BOARD_ROLE_DECORATION);
		});
	for (std::size_t i = 0; i < model.buttons.size(); ++i)
	{
		const auto& button = model.buttons[i];
		const TextButton* peer = model.buttons.size() > 1
			? model.buttons[(i + 1) % model.buttons.size()].widget
			: nullptr;
		const CalypsoInteractionState state = buttonVisualState(button.widget, peer);
		addStyled(button.rect, buttonStyle(button, state), button.widget,
			BOARD_ROLE_BUTTON_BASE + (std::uint32_t)i);
		const int buttonWrapWidth = scaledPx(std::max(1, button.rect.w - 12));
		addText(button.rect, button.widget, heading, button.text, button.textColor,
			CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, actionPx,
			buttonWrapWidth, 0.0,
			BOARD_ROLE_BUTTON_LABEL_BASE + (std::uint32_t)i);
	}
}

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
