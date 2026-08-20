/*
 * Shared physical renderer for generated small-confirmation forms.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoSmallConfirmationRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

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

	CalypsoHdPanelStyle style;
	style.styled = true;
	style.radiusPx = 1.0f;
	style.borderWidthPx = CalypsoHdTheme::kBorderWidthPx
		+ (state == CalypsoInteractionState::Focus ? 1.0f : 0.0f);
	style.borderColorRgba = state == CalypsoInteractionState::Rest ? button.restBorder : border;
	style.fillTopRgba = state == CalypsoInteractionState::Rest ? button.restFill : fill;
	style.fillBottomRgba = style.fillTopRgba;
	style.gradDirX = 0.26f;
	style.gradDirY = 1.0f;
	return style;
}

CalypsoHdPanelStyle windowStyle(const CalypsoSmallConfirmationModel& model)
{
	CalypsoHdPanelStyle style;
	style.styled = true;
	style.shape = CalypsoHdPanelShape::OpposingCutRect;
	style.cutCornerPx = model.cutCornerPx;
	style.borderWidthPx = 1.0f;
	style.borderColorRgba = model.frameColor;
	style.fillTopRgba = model.panelFillTop;
	style.fillBottomRgba = model.panelFillBottom;
	style.gradDirX = 0.18f;
	style.gradDirY = 1.0f;
	return style;
}

CalypsoHdPanelStyle glowStyle(
	const CalypsoSmallConfirmationModel& model,
	std::uint32_t color,
	float radius)
{
	CalypsoHdPanelStyle style = CalypsoHdTheme::calypsoHdGlowStyle(color, radius);
	style.shape = CalypsoHdPanelShape::OpposingCutRect;
	style.cutCornerPx = model.cutCornerPx;
	return style;
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
		item.textScaleX = (float)model.projectionScaleX;
		item.textScaleY = (float)model.projectionScaleY;
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
	addStyled({model.window.x - 2, model.window.y + 8, model.window.w + 4, model.window.h},
		glowStyle(model, CalypsoHdTheme::kShadowGlow, CalypsoHdTheme::kShadowGlowRadiusPx),
		nullptr, ROLE_WINDOW);
	addStyled(model.window,
		glowStyle(model, CalypsoHdTheme::kHaloGlow, CalypsoHdTheme::kHaloGlowRadiusPx),
		nullptr, ROLE_WINDOW);
	addStyled(model.window, windowStyle(model), model.windowWidget, ROLE_WINDOW);

	addDecoration({model.status.x, model.status.y + model.status.h - 1, model.status.w, 1},
		model.dividerColor);
	addDecoration({model.footer.x, model.footer.y, model.footer.w, 1}, model.dividerColor);
	int firstButtonX = model.footer.x + model.footer.w;
	for (const auto& button : model.buttons) firstButtonX = std::min(firstButtonX, button.rect.x);
	for (int y = model.footer.y + 10; y < model.footer.y + model.footer.h - 8; y += 8)
		for (int x = model.footer.x + 12; x < firstButtonX - 12; x += 8)
			addDecoration({x, y, 1, 1}, model.footerDotColor);

	CalypsoHdPanelStyle warning;
	warning.styled = true;
	warning.shape = CalypsoHdPanelShape::WarningTriangle;
	warning.borderWidthPx = 2.0f;
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

	const int protocolInset = (int)std::llround(model.protocolTextInsetPx * model.uiScale);
	const CalypsoLogicalRect protocolRect{
		model.status.x + protocolInset, model.status.y,
		std::max(1, model.status.w - 2 * protocolInset), model.status.h};
	addText(protocolRect, model.protocolWidget, mono, model.protocolText,
		model.protocolColor, CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle,
		model.wide ? 10 : 9, 0, 0.10, ROLE_PROTOCOL);
	addText(model.warning, model.warningWidget, heading, model.warningGlyph,
		model.warningColor, CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle,
		model.wide ? 18 : 16, 0, 0.0, ROLE_WARNING);
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
		const int bodyPx = std::max(1, (int)calypsoHdRoundToInt(
			CalypsoHdTheme::kBodyFontSizePx * bodySizeScale));

		CalypsoHdTextRasterKey key;
		key.source = body;
		key.physicalPixelHeight = bodyPx;
		key.text = model.messageText;
		key.wrapWidth = std::max(1, model.messageDesignWidth);
		const double lineHeight = CalypsoHdTheme::kBodyFontSizePx * bodySizeScale
			* CalypsoHdTheme::kBodyLineHeight * lineHeightScale;
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
		item.textScaleX = (float)model.projectionScaleX;
		item.textScaleY = (float)(model.projectionScaleY * (model.wide ? 1.0 : 0.98));
		item.hAlign = CalypsoHdHAlign::Left;
		item.vAlign = CalypsoHdVAlign::Top;
		item.opacity = opacity;
		item.widget = model.messageWidget;
		stamp(item, ROLE_MESSAGE);
		builder.add(item);
	}

	const int labelPx = std::max(1, (int)calypsoHdRoundToInt(
		CalypsoHdTheme::kLabelFontSizePx * (model.wide
			? CalypsoHdTheme::kLabelFontSizeScaleWide
			: CalypsoHdTheme::kLabelFontSizeScaleCompact)));
	for (std::size_t i = 0; i < model.buttons.size(); ++i)
	{
		const auto& button = model.buttons[i];
		addText(button.rect, button.widget, heading, button.text, button.textColor,
			CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, labelPx, 0,
			CalypsoHdTheme::kLabelTrackingEm, ROLE_BUTTON_LABEL_BASE + (std::uint32_t)i);
	}
}

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
