/*
 * Shared physical renderer for generated tabbed-management shells.
 *
 * One archetype-owned implementation for every tabbed Forces/submarine
 * screen: header, summary rail, section tabs, toolbar, collection viewport,
 * detail panel, and footer actions are painted from generated geometry and
 * theme tokens only. Adapters bind live localized text and existing native
 * widgets; the renderer owns no selection, scroll, tab, or transaction
 * model. No screen-specific painting may live here.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoTabbedManagementRenderer.h"

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

enum TabbedRole : std::uint32_t
{
	TABBED_ROLE_WINDOW = 200,
	TABBED_ROLE_TITLE = 201,
	TABBED_ROLE_SUMMARY = 202,
	TABBED_ROLE_TAB_BASE = 210,
	TABBED_ROLE_TAB_LABEL_BASE = 230,
	TABBED_ROLE_TOOLBAR = 250,
	TABBED_ROLE_CONTROL_BASE = 260,
	TABBED_ROLE_CONTROL_LABEL_BASE = 280,
	TABBED_ROLE_COLLECTION = 300,
	TABBED_ROLE_COLUMN_BASE = 310,
	TABBED_ROLE_ROW_BASE = 320,
	TABBED_ROLE_ROW_LABEL_BASE = 350,
	TABBED_ROLE_TILE_BASE = 360,
	TABBED_ROLE_TILE_LABEL_BASE = 375,
	TABBED_ROLE_SCROLL_TRACK = 390,
	TABBED_ROLE_SCROLL_THUMB = 391,
	TABBED_ROLE_DETAIL = 400,
	TABBED_ROLE_DETAIL_TITLE = 401,
	TABBED_ROLE_DETAIL_METRIC_BASE = 410,
	TABBED_ROLE_ACTION_BASE = 440,
	TABBED_ROLE_ACTION_LABEL_BASE = 470,
	TABBED_ROLE_DECORATION = 500
};

CalypsoInteractionState tabbedButtonVisualState(const TextButton* button, const TextButton* peer)
{
	if (!button) return CalypsoInteractionState::Rest;
	if (button->isPressed()) return CalypsoInteractionState::Pressed;
	if (button->isHovered()) return CalypsoInteractionState::Hover;
	if (peer && button->isFocused() && !peer->isFocused())
		return CalypsoInteractionState::Focus;
	return CalypsoInteractionState::Rest;
}

CalypsoHdPanelStyle tabbedButtonStyle(
	const CalypsoTabbedAction& button,
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
CalypsoHdPanelStyle tabbedWindowStyle(const Model& model)
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
CalypsoHdPanelStyle tabbedGlowStyle(
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
void tabbedFooterDots(
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

void calypsoCollectTabbedManagement(
	CalypsoHdFrameBuilder& builder,
	const CalypsoTabbedModel& model,
	CalypsoSmallConfirmationMotion& motion)
{
	if (!model.mod || !model.instance || !model.listWidget
		|| model.window.w <= 0 || model.window.h <= 0) return;

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
		addStyled(rect, style, nullptr, TABBED_ROLE_DECORATION);
	};
	// Restrained 1px frame outline around a generated region, shared by
	// every tabbed screen (collection viewport, detail-adjacent bands).
	auto addFrame = [&](const CalypsoLogicalRect& rect, std::uint32_t color)
	{
		if (rect.w <= 2 || rect.h <= 2) return;
		addDecoration({rect.x, rect.y, rect.w, 1}, color);
		addDecoration({rect.x, rect.y + rect.h - 1, rect.w, 1}, color);
		addDecoration({rect.x, rect.y, 1, rect.h}, color);
		addDecoration({rect.x + rect.w - 1, rect.y, 1, rect.h}, color);
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
	auto addAction = [&](const CalypsoTabbedAction& button, std::uint32_t role, std::uint32_t labelRole)
	{
		const CalypsoInteractionState state = tabbedButtonVisualState(button.widget, button.peer);
		CalypsoHdPanelStyle style = tabbedButtonStyle(button, state);
		addStyled(button.rect, style, button.widget, role);
		const int labelPx = scaledPx(
			CalypsoHdTheme::kLabelFontSizePx * (model.wide
				? CalypsoHdTheme::kLabelFontSizeScaleWide
				: CalypsoHdTheme::kLabelFontSizeScaleCompact), 11);
		addText(button.rect, button.widget, heading, button.text,
			button.textColor,
			CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, labelPx, 0,
			CalypsoHdTheme::kLabelTrackingEm, labelRole);
	};

	// No scrim: the shell floats above the live undimmed base; the isolated
	// harness host owns the opaque backing.
	const int shadowX = scaledPx(2.0);
	const int shadowY = scaledPx(8.0);
	addStyled({model.window.x - shadowX, model.window.y + shadowY,
		model.window.w + shadowX * 2, model.window.h},
		tabbedGlowStyle(model, CalypsoHdTheme::kShadowGlow,
			CalypsoHdTheme::kShadowGlowRadiusPx * model.visualScale),
		nullptr, TABBED_ROLE_WINDOW);
	addStyled(model.window,
		tabbedGlowStyle(model, CalypsoHdTheme::kHaloGlow,
			CalypsoHdTheme::kHaloGlowRadiusPx * model.visualScale),
		nullptr, TABBED_ROLE_WINDOW);
	addStyled(model.window, tabbedWindowStyle(model), model.windowWidget, TABBED_ROLE_WINDOW);

	addDecoration({model.footer.x, model.footer.y, model.footer.w, 1}, model.dividerColor);
	int footerActionEdge = model.footer.x + model.footer.w;
	for (const auto& action : model.actions)
		footerActionEdge = std::min(footerActionEdge, action.rect.x);
	tabbedFooterDots(model.footer, footerActionEdge, model.footerDotColor,
		scaledPx, addDecoration);

	addText(model.title, model.titleWidget, heading, model.titleText,
		CalypsoHdTheme::kNearWhite, CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle,
		std::max(1, (int)calypsoHdRoundToInt(
			model.titleDesignHeight * CalypsoHdTheme::kTitleFontSizeScale)),
		0, CalypsoHdTheme::kTitleTrackingEm, TABBED_ROLE_TITLE);

	const int summaryPx = scaledPx(10.0, 8);
	addDecoration({model.summaryBar.x, model.summaryBar.y + model.summaryBar.h - 1,
		model.summaryBar.w, 1}, model.dividerColor);
	for (std::size_t i = 0; i < model.summary.size(); ++i)
	{
		addText(model.summary[i].rect, nullptr, mono, model.summary[i].text,
			model.mutedTextColor, CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle,
			summaryPx, 0, 0.10, TABBED_ROLE_SUMMARY);
	}

	for (std::size_t i = 0; i < model.tabs.size(); ++i)
	{
		const CalypsoTabbedTab& tab = model.tabs[i];
		if (tab.selected)
			addPanel(tab.rect, model.selectedTabColor, tab.widget,
				TABBED_ROLE_TAB_BASE + (std::uint32_t)i, true);
		else
			addFrame(tab.rect, model.dividerColor);
		addText(tab.rect, tab.widget, body, tab.label,
			!tab.enabled ? model.mutedTextColor
				: (tab.selected ? CalypsoHdTheme::kNearWhite : model.mutedTextColor),
			CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle,
			scaledPx(CalypsoHdTheme::kBodyFontSizePx * (model.wide
				? CalypsoHdTheme::kBodyFontSizeScaleWide
				: CalypsoHdTheme::kBodyFontSizeScaleCompact), 12),
			0, 0.0, TABBED_ROLE_TAB_LABEL_BASE + (std::uint32_t)i);
	}
	addDecoration({model.tabBar.x, model.tabBar.y + model.tabBar.h - 1,
		model.tabBar.w, 1}, model.dividerColor);

	const int controlPx = scaledPx(CalypsoHdTheme::kBodyFontSizePx * (model.wide
		? CalypsoHdTheme::kBodyFontSizeScaleWide
		: CalypsoHdTheme::kBodyFontSizeScaleCompact), 12);
	for (std::size_t i = 0; i < model.controls.size(); ++i)
	{
		addText(model.controls[i].rect, model.controls[i].widget, body,
			model.controls[i].text, model.textColor,
			CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle,
			controlPx, 0, 0.0, TABBED_ROLE_CONTROL_LABEL_BASE + (std::uint32_t)i);
	}
	for (std::size_t i = 0; i < model.toolbar.size(); ++i)
		addAction(model.toolbar[i],
			TABBED_ROLE_ACTION_BASE + 10 + (std::uint32_t)i,
			TABBED_ROLE_ACTION_LABEL_BASE + 10 + (std::uint32_t)i);
	addDecoration({model.toolbarBar.x, model.toolbarBar.y + model.toolbarBar.h - 1,
		model.toolbarBar.w, 1}, model.dividerColor);

	addFrame(model.collectionViewport, model.dividerColor);

	addPanel(model.collectionViewport, model.scrollTrackColor, model.listWidget,
		TABBED_ROLE_COLLECTION, true);
	const int headerPx = scaledPx(10.0, 8);
	const double bodySizeScale = model.wide
		? CalypsoHdTheme::kBodyFontSizeScaleWide : CalypsoHdTheme::kBodyFontSizeScaleCompact;
	const int rowPx = scaledPx(CalypsoHdTheme::kBodyFontSizePx * bodySizeScale, 12);
	if (!model.tiles.empty())
	{
		// Grid/card collection (mount cards): one framed card per tile,
		// no columns, rows, or scrollbar. The bound native widget owns
		// input; the label mirrors its live text, muted when disabled.
		CalypsoHdPanelStyle cardStyle;
		cardStyle.styled = true;
		cardStyle.shape = CalypsoHdPanelShape::OpposingCutRect;
		cardStyle.cutCornerPx = model.cutCornerPx * model.visualScale;
		cardStyle.borderWidthPx = 1.0f;
		cardStyle.borderColorRgba = model.frameColor;
		cardStyle.fillTopRgba = model.panelFillTop;
		cardStyle.fillBottomRgba = model.panelFillBottom;
		cardStyle.gradDirX = 0.18f;
		cardStyle.gradDirY = 1.0f;
		for (std::size_t i = 0; i < model.tiles.size(); ++i)
		{
			const CalypsoTabbedTile& tile = model.tiles[i];
			addStyled(tile.rect, cardStyle, tile.widget,
				TABBED_ROLE_TILE_BASE + (std::uint32_t)i);
			addText(tile.labelRect, tile.widget, body, tile.label,
				tile.enabled ? model.textColor : model.mutedTextColor,
				CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle,
				rowPx, 0, 0.0, TABBED_ROLE_TILE_LABEL_BASE + (std::uint32_t)i);
		}
	}
	else
	{
	for (std::size_t i = 0; i < model.columns.size(); ++i)
	{
		addText(model.columns[i].rect, model.listWidget, mono, model.columns[i].label,
			model.mutedTextColor, CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle,
			headerPx, 0, 0.10, TABBED_ROLE_COLUMN_BASE + (std::uint32_t)i);
	}
	if (!model.columns.empty())
	{
		const CalypsoLogicalRect& headerRect = model.columns[0].rect;
		addDecoration({model.collectionViewport.x, headerRect.y + headerRect.h - 1,
			model.collectionViewport.w, 1}, model.dividerColor);
	}

	const std::size_t total = model.rows.size();
	const std::size_t visible = (std::size_t)std::max(1, model.visibleRows);
	const std::size_t first = std::min(model.scrollOffset, total);
	const int rowInsetX = scaledPx(12.0, 8);
	for (std::size_t slot = 0; slot < model.rowSlots.size() && slot < visible; ++slot)
	{
		const std::size_t row = first + slot;
		if (row >= total) break;
		const CalypsoLogicalRect& slotRect = model.rowSlots[slot];
		if (model.hasSelection && row == model.selectedRow)
			addPanel(slotRect, model.selectionColor, model.listWidget,
				TABBED_ROLE_ROW_BASE + (std::uint32_t)slot, true);
		const CalypsoTabbedRow& data = model.rows[row];
		// Column-addressable rows: when the adapter supplies one cell per
		// header, each cell paints inside its header column so values stay
		// aligned across rows. The "open" column paints the row-activation
		// affordance marker from the native availability verdict instead of
		// text (› enabled, — disabled). Otherwise the composed single-line
		// text is painted as before.
		if (!data.cells.empty() && data.cells.size() == model.columns.size()
			&& !model.columns.empty())
		{
			const int baseX = model.columns[0].rect.x;
			for (std::size_t column = 0; column < model.columns.size(); ++column)
			{
				const CalypsoLogicalRect cellRect{
					slotRect.x + (model.columns[column].rect.x - baseX),
					slotRect.y,
					model.columns[column].rect.w, slotRect.h};
				if (model.columns[column].id == "open")
				{
					// Row-activation affordance from the native availability
					// verdict. Plain ASCII markers keep the source and every
					// raster path safe; the fixtures carry the styled
					// chevron copy for the reference renderer.
					addText(cellRect, model.listWidget,
						body, data.enabled ? ">" : "-",
						data.enabled ? model.textColor : model.mutedTextColor,
						CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle,
						rowPx, 0, 0.0, TABBED_ROLE_ROW_LABEL_BASE + (std::uint32_t)slot);
				}
				else
				{
					addText(cellRect, model.listWidget, body, data.cells[column],
						data.enabled ? model.textColor : model.mutedTextColor,
						CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle,
						rowPx, 0, 0.0, TABBED_ROLE_ROW_LABEL_BASE + (std::uint32_t)slot);
				}
			}
		}
		else
		{
		const CalypsoLogicalRect textRect{
			slotRect.x + rowInsetX, slotRect.y,
			std::max(1, slotRect.w - 2 * rowInsetX), slotRect.h};
		addText(textRect, model.listWidget, body, data.text,
			data.enabled ? model.textColor : model.mutedTextColor,
			CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle,
			rowPx, 0, 0.0, TABBED_ROLE_ROW_LABEL_BASE + (std::uint32_t)slot);
		}
		addDecoration({slotRect.x, slotRect.y + slotRect.h - 1, slotRect.w, 1},
			model.dividerColor);
	}

	if (total > visible && model.scrollBarWidth > 0)
	{
		if (model.hasNativeScrollGeometry && model.nativeTrack.w > 0 && model.nativeTrack.h > 0)
		{
			addPanel(model.nativeTrack, model.scrollTrackColor, model.listWidget,
				TABBED_ROLE_SCROLL_TRACK, true);
			if (model.nativeThumbVisible && model.nativeThumb.w > 0 && model.nativeThumb.h > 0)
			{
				addPanel(model.nativeThumb, model.scrollThumbColor,
					model.listWidget, TABBED_ROLE_SCROLL_THUMB, true);
			}
		}
		else
		{
			const CalypsoLogicalRect track{
				model.collectionViewport.x + model.collectionViewport.w - model.scrollBarWidth,
				model.collectionViewport.y,
				model.scrollBarWidth, model.collectionViewport.h};
			addPanel(track, model.scrollTrackColor, model.listWidget,
				TABBED_ROLE_SCROLL_TRACK, true);
			const std::size_t steps = total - visible;
			const int minThumb = model.minThumbHeight > 0 ? model.minThumbHeight : scaledPx(44.0);
			int thumbH = (int)((long long)track.h * (long long)visible / (long long)total);
			thumbH = std::min(track.h, std::max(minThumb, thumbH));
			const int thumbY = track.y + (steps > 0 && track.h > thumbH
				? (int)((long long)(track.h - thumbH) * (long long)std::min(first, steps) / (long long)steps)
				: 0);
			addPanel({track.x, thumbY, track.w, thumbH}, model.scrollThumbColor,
				model.listWidget, TABBED_ROLE_SCROLL_THUMB, true);
		}
	}
	}

	if (model.detail.present)
	{
		CalypsoHdPanelStyle detailStyle;
		detailStyle.styled = true;
		detailStyle.shape = CalypsoHdPanelShape::OpposingCutRect;
		detailStyle.cutCornerPx = model.cutCornerPx * model.visualScale;
		detailStyle.borderWidthPx = 1.0f;
		detailStyle.borderColorRgba = model.frameColor;
		detailStyle.fillTopRgba = model.panelFillTop;
		detailStyle.fillBottomRgba = model.panelFillBottom;
		detailStyle.gradDirX = 0.18f;
		detailStyle.gradDirY = 1.0f;
		addStyled(model.detail.panel, detailStyle, nullptr, TABBED_ROLE_DETAIL);
		addText(model.detail.titleRect, nullptr, heading, model.detail.titleText,
			CalypsoHdTheme::kNearWhite, CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle,
			scaledPx(CalypsoHdTheme::kBodyFontSizePx * bodySizeScale, 12),
			0, 0.0, TABBED_ROLE_DETAIL_TITLE);
		addText(model.detail.subtitleRect, nullptr, body, model.detail.subtitleText,
			model.mutedTextColor, CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle,
			controlPx, 0, 0.0, TABBED_ROLE_DETAIL_TITLE);
		for (std::size_t i = 0; i < model.detail.metrics.size(); ++i)
		{
			addText(model.detail.metrics[i].rect, nullptr, mono,
				model.detail.metrics[i].text, model.textColor,
				CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle,
				headerPx, 0, 0.10, TABBED_ROLE_DETAIL_METRIC_BASE + (std::uint32_t)i);
		}
		for (std::size_t i = 0; i < model.detail.actions.size(); ++i)
			addAction(model.detail.actions[i],
				TABBED_ROLE_ACTION_BASE + 20 + (std::uint32_t)i,
				TABBED_ROLE_ACTION_LABEL_BASE + 20 + (std::uint32_t)i);
	}

	for (std::size_t i = 0; i < model.actions.size(); ++i)
		addAction(model.actions[i],
			TABBED_ROLE_ACTION_BASE + (std::uint32_t)i,
			TABBED_ROLE_ACTION_LABEL_BASE + (std::uint32_t)i);
}

} // namespace Calypso
} // namespace OpenXcom
#endif
