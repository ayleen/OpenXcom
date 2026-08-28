/*
 * Shared physical renderer for generated contact-intel-board forms.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoContactIntelBoardRenderer.h"

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

CalypsoInteractionState boardButtonVisualState(const TextButton* button, const TextButton* peer)
{
	if (!button) return CalypsoInteractionState::Rest;
	if (button->isPressed()) return CalypsoInteractionState::Pressed;
	if (button->isHovered()) return CalypsoInteractionState::Hover;
	if (peer && button->isFocused() && !peer->isFocused())
		return CalypsoInteractionState::Focus;
	return CalypsoInteractionState::Rest;
}

CalypsoHdPanelStyle boardButtonStyle(const CalypsoSmallConfirmationButton& button,
	CalypsoInteractionState state)
{
	const CalypsoInteractionTokenPair tokens = calypsoInteractionTokenPair(button.tone, state);
	const std::uint32_t border = state == CalypsoInteractionState::Focus
		? CalypsoHdThemeGen::calypsoHdThemeColorForToken(calypsoFocusRingToken(button.tone))
		: CalypsoHdThemeGen::calypsoHdThemeColorForToken(tokens.borderToken);
	const std::uint32_t fill = CalypsoHdThemeGen::calypsoHdThemeColorForToken(tokens.fillToken);
	const std::uint32_t resolvedBorder = state == CalypsoInteractionState::Rest
		? button.restBorder : border;
	const std::uint32_t resolvedFill = state == CalypsoInteractionState::Rest
		? button.restFill : fill;
	CalypsoHdPanelStyle style = CalypsoHdTheme::calypsoHdButtonStyle(resolvedFill, resolvedBorder);
	style.borderWidthPx = CalypsoHdTheme::kBorderWidthPx
		+ (state == CalypsoInteractionState::Focus ? 1.0f : 0.0f);
	return style;
}

CalypsoHdPanelStyle boardWindowStyle(const CalypsoContactIntelBoardModel& model)
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
		model.opaqueHarnessBackdrop ? calypsoRgba(0, 0, 0, 0xff) : CalypsoHdTheme::kBackdropDim,
		BOARD_ROLE_BACKDROP);

	addQuad(model.window, model.panelFillBottom, BOARD_ROLE_WINDOW);
	addStyled(model.window, boardWindowStyle(model), model.windowWidget, BOARD_ROLE_WINDOW);

	// Status strip: protocol rail + closing divider.
	addQuad({model.status.x, model.status.y + model.status.h - 1, model.status.w, 1},
		model.dividerColor, BOARD_ROLE_DECORATION);
	const int protocolInset = (int)std::llround(
		model.protocolTextInsetPx * model.visualScale * model.uiScale);
	addText({model.status.x + protocolInset, model.status.y,
			std::max(1, model.status.w - 2 * protocolInset), model.status.h},
		model.protocolWidget, mono, model.protocolText, model.protocolColor,
		CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle,
		scaledPx(model.wide ? 10.0 : 9.0, 8), 0, 0.10, BOARD_ROLE_PROTOCOL);

	// Plot panel: framed quadrant grid.
	addStyled(model.plotPanel,
		CalypsoHdTheme::calypsoHdButtonStyle(model.panelFillTop, model.plotFrameColor),
		nullptr, BOARD_ROLE_PLOT_PANEL);
	const int gridCellW = 60;
	const int gridCellH = 64;
	for (int gx = model.plotArea.x + gridCellW; gx < model.plotArea.x + model.plotArea.w; gx += gridCellW)
		addQuad({gx, model.plotArea.y, 1, model.plotArea.h},
			model.plotGridColor, BOARD_ROLE_PLOT_GRID);
	for (int gy = model.plotArea.y + gridCellH; gy < model.plotArea.y + model.plotArea.h; gy += gridCellH)
		addQuad({model.plotArea.x, gy, model.plotArea.w, 1},
			model.plotGridColor, BOARD_ROLE_PLOT_GRID);

	// Course dashes: from the contact marker along the compass word.
	double dx = 0.0;
	double dy = 0.0;
	const bool hasCourse = courseVector(model.courseWord, dx, dy);
	if (hasCourse)
	{
		const int dash = scaledPx(9.0);
		const int gap = scaledPx(7.0);
		double px = (double)model.contact.x;
		double py = (double)model.contact.y;
		for (int step = 0; step < 24; ++step)
		{
			const int sx = (int)std::llround(px);
			const int sy = (int)std::llround(py);
			const int ex = (int)std::llround(px + dx * dash);
			const int ey = (int)std::llround(py + dy * dash);
			const CalypsoLogicalRect seg{std::min(sx, ex), std::min(sy, ey),
				std::max(1, std::abs(ex - sx)), std::max(1, std::abs(ey - sy))};
			if (seg.x < model.plotArea.x || seg.y < model.plotArea.y
				|| seg.x + seg.w > model.plotArea.x + model.plotArea.w
				|| seg.y + seg.h > model.plotArea.y + model.plotArea.h)
				break;
			addQuad(seg, model.plotCourseColor, BOARD_ROLE_PLOT_COURSE);
			px += dx * (dash + gap);
			py += dy * (dash + gap);
		}
	}

	// Contact marker: soft halo + hard core (pulsing is capture-hostile; rest state only).
	const int core = scaledPx(model.wide ? 14.0 : 12.0);
	const int halo = scaledPx(model.wide ? 26.0 : 22.0);
	addQuad({model.contact.x - halo / 2, model.contact.y - halo / 2, halo, halo},
		model.plotContactHaloColor, BOARD_ROLE_PLOT_CONTACT);
	addQuad({model.contact.x - core / 2, model.contact.y - core / 2, core, core},
		model.plotContactColor, BOARD_ROLE_PLOT_CONTACT);
	if (!model.contact.label.empty())
	{
		addText({model.contact.x + core, model.contact.y - core,
				model.plotArea.x + model.plotArea.w - model.contact.x - core,
				core * 2},
			nullptr, mono, model.contact.label, model.plotContactColor,
			CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle,
			scaledPx(model.wide ? 13.0 : 11.0, 9), 0, 0.0, BOARD_ROLE_PLOT_LABEL);
	}

	// Base marker: bordered triangle + label.
	if (model.base.x && model.base.y)
	{
		const int base = scaledPx(model.wide ? 18.0 : 15.0);
		CalypsoHdPanelStyle triangle;
		triangle.styled = true;
		triangle.shape = CalypsoHdPanelShape::WarningTriangle;
		triangle.borderWidthPx = (float)scaledPx(2.0);
		triangle.borderColorRgba = model.plotBaseColor;
		triangle.fillTopRgba = calypsoRgba(0, 0, 0, 0);
		triangle.fillBottomRgba = calypsoRgba(0, 0, 0, 0);
		addStyled({model.base.x - base / 2, model.base.y - base / 2, base, base},
			triangle, nullptr, BOARD_ROLE_PLOT_BASE);
		if (!model.base.label.empty())
		{
			addText({model.base.x - base, model.base.y + base / 2,
					model.plotArea.x + model.plotArea.w - model.base.x + base,
					base},
				nullptr, mono, model.base.label, model.plotBaseColor,
				CalypsoHdHAlign::Center, CalypsoHdVAlign::Top,
				scaledPx(model.wide ? 12.0 : 10.0, 8), 0, 0.0, BOARD_ROLE_PLOT_LABEL);
		}
	}

	// Report panel: warning glyph, title, detection status, fact rows, note.
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
		std::min(scaledPx(model.wide ? 18.0 : 16.0, 11),
			std::max(11, (int)model.warning.h)), 0, 0.0, BOARD_ROLE_WARNING);
	addText(model.title, model.titleWidget, heading, model.titleText,
		CalypsoHdTheme::kNearWhite, CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle,
		std::max(1, (int)calypsoHdRoundToInt(
			model.titleDesignHeight * CalypsoHdTheme::kTitleFontSizeScale)),
		0, CalypsoHdTheme::kTitleTrackingEm, BOARD_ROLE_TITLE);
	addText(model.message, model.messageWidget, body, model.messageText,
		CalypsoHdTheme::kNearWhite, CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle,
		scaledPx(model.wide ? 12.0 : 10.0, 9), 0, 0.08, BOARD_ROLE_MESSAGE);

	const int factLabelPx = scaledPx(model.wide ? 11.0 : 9.0, 8);
	const int factValuePx = scaledPx(model.wide ? 13.0 : 11.0, 9);
	for (std::size_t i = 0; i < model.facts.size(); ++i)
	{
		const std::size_t labelIndex = i * 2;
		const std::size_t valueIndex = i * 2 + 1;
		if (valueIndex >= model.factRects.size()) break;
		const auto& fact = model.facts[i];
		if (fact.label.empty() && fact.value.empty()) continue;
		const CalypsoLogicalRect& labelRect = model.factRects[labelIndex];
		const CalypsoLogicalRect& valueRect = model.factRects[valueIndex];
		addQuad({labelRect.x, labelRect.y + labelRect.h - 1, valueRect.x + valueRect.w - labelRect.x, 1},
			model.factDividerColor, BOARD_ROLE_FACT);
		addText(labelRect, nullptr, mono, fact.label, model.protocolColor,
			CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, factLabelPx, 0, 0.06,
			BOARD_ROLE_FACT);
		addText(valueRect, nullptr, heading, fact.value, CalypsoHdTheme::kNearWhite,
			CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, factValuePx, 0, 0.02,
			BOARD_ROLE_FACT);
	}

	if (!model.noteText.empty())
	{
		addText(model.note, nullptr, body, model.noteText, model.protocolColor,
			CalypsoHdHAlign::Left, CalypsoHdVAlign::Top,
			scaledPx(model.wide ? 11.0 : 9.0, 8), std::max(1, model.note.w), 0.0,
			BOARD_ROLE_NOTE);
	}

	// Action rail.
	addQuad({model.footer.x, model.footer.y, model.footer.w, 1},
		model.dividerColor, BOARD_ROLE_DECORATION);
	for (std::size_t i = 0; i < model.buttons.size(); ++i)
	{
		const auto& button = model.buttons[i];
		const CalypsoInteractionState state =
			boardButtonVisualState(button.widget, i ? model.buttons[0].widget : nullptr);
		addStyled(button.rect, boardButtonStyle(button, state), button.widget,
			BOARD_ROLE_BUTTON_BASE + (std::uint32_t)i);
		const int labelPx = scaledPx(
			CalypsoHdTheme::kLabelFontSizePx * (model.wide
				? CalypsoHdTheme::kLabelFontSizeScaleWide
				: CalypsoHdTheme::kLabelFontSizeScaleCompact), 11);
		addText(button.rect, button.widget, heading, button.text, button.textColor,
			CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, labelPx, 0,
			CalypsoHdTheme::kLabelTrackingEm,
			BOARD_ROLE_BUTTON_LABEL_BASE + (std::uint32_t)i);
	}
}

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
