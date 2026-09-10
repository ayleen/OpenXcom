#include "CalypsoHdOperationsRenderer.h"

#ifdef __EMSCRIPTEN__

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <string>
#include <utility>

#include "CalypsoHdUiOverlay.h"
#include "CalypsoHdOperationsChrome.h"
#include "CalypsoViewportRuntime.h"
#include "CommandCenter/CommandCenterLayout.h"
#include "CommandCenter/CommandCenterRenderer.h"
#include "CommandCenter/CommandCenterTheme.h"
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "Generated/CalypsoHdTheme.generated.h"

namespace OpenXcom
{
namespace Calypso
{
namespace
{

std::uint32_t stableHash(const std::string& value)
{
	std::uint32_t hash = 2166136261u;
	for (unsigned char byte : value)
	{
		hash ^= byte;
		hash *= 16777619u;
	}
	return hash == 0 ? 1u : hash;
}

CalypsoLogicalRect logical(const CalypsoHdOperationsModel& model,
	const CalypsoHdOperationsRect& rect)
{
	const CalypsoHdOperationsRect projected =
		calypsoHdOperationsProjectForCurrentPresentation(
			rect, model.geometry.designWidth, model.geometry.designHeight);
	return {projected.x, projected.y, projected.w, projected.h};
}
CalypsoHdOperationsRect insetHorizontal(CalypsoHdOperationsRect rect, int inset)
{
	if (!rect.valid()) return rect;
	const int applied = std::min(inset, std::max(0, (rect.w - 1) / 2));
	rect.x += applied;
	rect.w -= 2 * applied;
	return rect;
}

std::pair<CalypsoHdOperationsRect, CalypsoHdOperationsRect> toolbarTextRects(
	const CalypsoHdOperationsRect& toolbar)
{
	if (!toolbar.valid()) return {};
	const int inset = std::min(12, std::max(0, (toolbar.w - 2) / 2));
	const int innerWidth = toolbar.w - 2 * inset;
	const int gap = std::min(12, std::max(0, innerWidth - 2));
	const int headingWidth = std::max(1, (innerWidth - gap) / 2);
	return {
		{toolbar.x + inset, toolbar.y, headingWidth, toolbar.h},
		{toolbar.x + inset + headingWidth + gap, toolbar.y,
			std::max(1, innerWidth - headingWidth - gap), toolbar.h}};
}

std::pair<CalypsoHdOperationsRect, CalypsoHdOperationsRect> stackedTextRects(
	const CalypsoHdOperationsRect& rect)
{
	if (!rect.valid()) return {};
	const int labelHeight = std::max(1, rect.h / 2);
	return {
		{rect.x, rect.y, rect.w, labelHeight},
		{rect.x, rect.y + labelHeight, rect.w, rect.h - labelHeight}
	};
}

CalypsoHdOperationsRect regionFieldLabelRect(
	const CalypsoHdOperationsRegion& region,
	const CalypsoHdOperationsRect& value)
{
	const int x = region.rect.x + 12;
	return {x, value.y, std::max(0, value.x - x - 12), value.h};
}


struct OperationsTypography
{
	int titlePx;
	int detailTitlePx;
	int bodyPx;
	int labelPx;
	int dataPx;
	int inputPx;
	int actionPx;
};

OperationsTypography operationsTypography(const CalypsoHdOperationsModel& model)
{
	const double chromeScale = std::min({1.0,
		static_cast<double>(model.geometry.designWidth) / 1280.0,
		static_cast<double>(model.geometry.designHeight) / 720.0});
	const auto& metrics = CalypsoHdUiOverlay::instance().frozenMetrics();
	auto px = [&](int canonicalPx) {
		const int designPx = std::max(1,
			static_cast<int>(std::lround(canonicalPx * chromeScale)));
		return calypsoHdOperationsPhysicalFontPx(
			designPx, model.geometry.designHeight, metrics.physicalHeight);
	};
	return {
		px(28),
		px(18),
		px(14),
		px(11),
		px(15),
		px(14),
		px(14)
	};
}

CalypsoHdPanelStyle panelStyle(const CalypsoHdOperationsStyle& style,
	std::uint32_t fillTop, std::uint32_t fillBottom)
{
	CalypsoHdPanelStyle result;
	result.styled = true;
	result.shape = CalypsoHdPanelShape::RoundedRect;
	result.radiusPx = style.cornerRadiusPx;
	result.borderWidthPx = 1.0f;
	result.borderColorRgba = style.frame;
	result.fillTopRgba = fillTop;
	result.fillBottomRgba = fillBottom;
	result.glowRgba = style.selection;
	result.glowRadiusPx = 0.0f;
	return result;
}

CalypsoHdItem baseItem(const CalypsoHdOperationsModel& model,
	const std::string& stableName, int itemOrder, const CalypsoHdOperationsRect& rect,
	const void* widget)
{
	CalypsoHdItem item;
	item.rect = logical(model, rect);
	item.widget = widget;
	item.claim.familyId = model.familyId;
	item.claim.stableId = stableHash(stableName);
	item.claim.instanceKey = reinterpret_cast<std::uintptr_t>(model.ownerState);
	item.claim.stableSubgroupId = 1;
	item.claim.stableVisualId = static_cast<std::uint32_t>(itemOrder);
	item.order.stage = static_cast<int>(CalypsoHdStage::HdUi);
	item.order.groupOrder = 0;
	item.order.stableId = model.familyId;
	item.order.instanceKey = item.claim.instanceKey;
	item.order.subgroupOrder = 0;
	item.order.subgroupId = 1;
	item.order.itemOrder = itemOrder;
	item.order.itemId = item.claim.stableVisualId;
	return item;
}

void addPanel(CalypsoHdFrameBuilder& builder, const CalypsoHdOperationsModel& model,
	const std::string& name, int order, const CalypsoHdOperationsRect& rect,
	std::uint32_t fillTop, std::uint32_t fillBottom, const void* widget = nullptr,
	std::uint32_t border = 0)
{
	if (!rect.valid()) return;
	CalypsoHdItem item = baseItem(model, name, order, rect, widget);
	item.kind = CalypsoHdItemKind::Panel;
	item.colorRgba = fillTop;
	item.panelStyle = panelStyle(model.style, fillTop, fillBottom);
	if (border != 0) item.panelStyle.borderColorRgba = border;
	builder.add(item);
}

void addText(CalypsoHdFrameBuilder& builder, const CalypsoHdOperationsModel& model,
	const CalypsoTtfSourceDescriptor& source, int physicalPixelHeight,
	const std::string& name, int order, const CalypsoHdOperationsRect& rect,
	const std::string& text, std::uint32_t color,
	CalypsoHdHAlign align, const void* widget = nullptr)
{
	if (!rect.valid() || text.empty()) return;
	CalypsoHdItem item = baseItem(model, name, order, rect, widget);
	item.kind = CalypsoHdItemKind::Text;
	item.colorRgba = color;
	item.hAlign = align;
	item.vAlign = CalypsoHdVAlign::Middle;
	item.rasterKey.source = source;
	item.rasterKey.physicalPixelHeight = physicalPixelHeight;
	item.rasterKey.text = text;
	item.rasterKey.colorRgba = color;
	item.rasterKey.explicitBreaksOnly = true;
	const auto& metrics = CalypsoHdUiOverlay::instance().frozenMetrics();
	item.rasterKey.wrapWidth = metrics.valid()
		? std::max(1, calypsoHdRoundToInt(rect.w * metrics.scaleX))
		: rect.w;
	builder.add(item);
}
void addImage(CalypsoHdFrameBuilder& builder, const CalypsoHdOperationsModel& model,
	const std::string& name, int order, const CalypsoHdOperationsRect& rect,
	const std::string& source, float opacity = 1.0f)
{
	if (!rect.valid() || source.empty()) return;
	CalypsoHdItem item = baseItem(model, name, order, rect, nullptr);
	item.kind = CalypsoHdItemKind::RgbaImage;
	item.image.source = source;
	item.image.cover = true;
	item.opacity = opacity;
	builder.add(item);
}

std::string operationsArtSource(const std::string& artId)
{
	if (artId == "base-research" || artId == "base-manufacture")
		return "Resources/basescape/cards/" + artId + ".png";
	return {};
}

std::string operationsSection(const CalypsoHdOperationsModel& model)
{
	if (!model.sectionLabel.empty()) return model.sectionLabel;
	if (model.familyId == 9u) return "RESEARCH";
	if (model.familyId == 10u) return "PRODUCTION";
	return "OPERATIONS";
}

void collectOperationsShell(CalypsoHdFrameBuilder& builder,
	const CalypsoHdOperationsModel& model,
	const OperationsTypography& typography, int& order)
{
	const auto& g = model.geometry;
	const bool wide = g.designWidth >= 1000;
	const double chromeScale = std::min({1.0,
		static_cast<double>(g.designWidth) / 1280.0,
		static_cast<double>(g.designHeight) / 720.0});
	const int railWidth = static_cast<int>(std::lround(
		CommandCenterTheme::RailWidth * chromeScale));
	const int headerHeight = static_cast<int>(std::lround(
		CommandCenterTheme::HeaderHeight * chromeScale));
	addPanel(builder, model, "window", order++, g.window,
		CommandCenterTheme::packed(CommandCenterTheme::BgRoot),
		CommandCenterTheme::packed(CommandCenterTheme::BgRoot));
	const CalypsoHdOperationsRect workspace{
		railWidth, headerHeight, g.designWidth - railWidth,
		g.designHeight - headerHeight};
	addImage(builder, model, "workspace-background", order++, workspace,
		"Resources/basescape/background.png", 0.18f);
	addPanel(builder, model, "workspace-veil", order++, workspace,
		0x020B14DEu, 0x020B14DEu);

	addPanel(builder, model, "screen-header", order++, g.screenHeader,
		model.style.regionFill, model.style.regionFill);
	addImage(builder, model, "screen-header-art", order++, g.headerArt,
		operationsArtSource(model.headerArtId), 0.9f);
	const CalypsoHdOperationsRect breadcrumb{
		g.title.x, g.screenHeader.y + (wide ? 8 : 3), g.title.w, wide ? 14 : 9};
	addText(builder, model, model.monoFont, typography.labelPx, "breadcrumb", order++,
		breadcrumb,
		(model.baseName.empty() ? std::string("BASES") : model.baseName)
			+ " / " + operationsSection(model),
		model.style.accent, CalypsoHdHAlign::Left);
}

void collectCanonicalBasescapeChrome(CalypsoHdFrameBuilder& builder,
	const CalypsoHdOperationsModel& model, int& order)
{
	Game* game = getCurrentGame();
	const Mod* mod = game ? game->getMod() : nullptr;
	const CommandCenter::CommandCenterFonts fonts =
		CommandCenter::calypsoCcResolveFonts(mod);
	if (!fonts.ready) return;
	const CalypsoHdPresentationMetrics& metrics =
		CalypsoHdUiOverlay::instance().frozenMetrics();
	const CalypsoLayoutMetrics& viewport = calypsoViewportRuntime().current();
	const int cssWidth = std::max(1, viewport.logicalWidth);
	const int cssHeight = std::max(1, viewport.logicalHeight);
	const CommandCenter::CommandCenterLayout layout = CommandCenter::computeLayout(
		CommandCenter::Size2{static_cast<float>(cssWidth), static_cast<float>(cssHeight)},
		false, CommandCenter::InsetsF{
			static_cast<float>(viewport.safeX), static_cast<float>(viewport.safeY),
			static_cast<float>(cssWidth - viewport.safeX - viewport.safeWidth),
			static_cast<float>(cssHeight - viewport.safeY - viewport.safeHeight)});
	const double densityX = static_cast<double>(metrics.physicalWidth) / cssWidth;
	const double densityY = static_cast<double>(metrics.physicalHeight) / cssHeight;
	const double logicalPerCssX = densityX / metrics.scaleX;
	const double logicalPerCssY = densityY / metrics.scaleY;
	CalypsoF21Painter painter{builder, model.familyId,
		reinterpret_cast<std::uintptr_t>(model.ownerState), 0, 1.0f, 1.0,
		CalypsoF21Rect{
			static_cast<int>(std::llround(-(metrics.contentOffsetX / metrics.scaleX))),
			static_cast<int>(std::llround(-(metrics.contentOffsetY / metrics.scaleY))),
			static_cast<int>(std::llround(cssWidth * logicalPerCssX)),
			static_cast<int>(std::llround(cssHeight * logicalPerCssY))},
		metrics.scaleX, metrics.scaleY};
	painter.winLogical = {
		static_cast<int>(std::llround(-(metrics.contentOffsetX / metrics.scaleX))),
		static_cast<int>(std::llround(-(metrics.contentOffsetY / metrics.scaleY))),
		static_cast<int>(std::llround(cssWidth * logicalPerCssX)),
		static_cast<int>(std::llround(cssHeight * logicalPerCssY))};
	painter.windowDesign = {0, 0, cssWidth, cssHeight};
	painter.uiScale = logicalPerCssX * layout.scale;
	painter.uiAspectY = logicalPerCssY / logicalPerCssX;
	std::uint32_t role = static_cast<std::uint32_t>(order);
	CommandCenter::calypsoCcPaintHeaderBackground(painter, layout.header, role);
	CommandCenter::calypsoCcPaintRailBackground(painter, layout.navigationRail, role);
	const char* labels[5] = {
		CommandCenter::calypsoCcRailLabel(0), CommandCenter::calypsoCcRailLabel(1),
		CommandCenter::calypsoCcRailLabel(2), CommandCenter::calypsoCcRailLabel(3),
		CommandCenter::calypsoCcRailLabel(4)};
	CommandCenter::calypsoCcPaintRailItems(painter, layout.navigationRail,
		CommandCenter::RailAction::Bases, labels, fonts, role);
	CommandCenter::CommandCenterSnapshot snapshot;
	snapshot.baseCaption = model.baseCaption;
	snapshot.baseName = model.baseName;
	snapshot.displayTime = model.clockTime;
	snapshot.displayDate = model.clockDate;
	CommandCenter::calypsoCcPaintHeaderContent(
		painter, layout, snapshot, fonts, role, false);
	order = static_cast<int>(role);
}


void collectAction(CalypsoHdFrameBuilder& builder, const CalypsoHdOperationsModel& model,
	const CalypsoTtfSourceDescriptor& source, int actionPx,
	const CalypsoHdOperationsAction& action, int& order)
{
	if (!calypsoHdOperationsActionVisible(action)) return;
	const auto palette = calypsoHdOperationsActionPalette(
		model.style, action.tone, action.state);
	addPanel(builder, model, "action/" + action.id, order++, action.visible,
		palette.fill, palette.fill, action.widget, palette.border);
	addText(builder, model, source, actionPx, "action-label/" + action.id, order++,
		action.visible, action.label, palette.text, CalypsoHdHAlign::Center, action.widget);
}

void collectControl(CalypsoHdFrameBuilder& builder,
	const CalypsoHdOperationsModel& model, const CalypsoTtfSourceDescriptor& source,
	int inputPx, int actionPx, const CalypsoHdOperationsControl& control, int& order)
{
	if (!control.state.visible) return;
	const std::uint32_t fill = control.state.disabled
		? model.style.disabled : model.style.panelFillTop;
	addPanel(builder, model, "control/" + control.id, order++, control.rect,
		fill, fill, control.widget);
	if (control.kind == CalypsoHdOperationsControlKind::Stepper)
	{
		addPanel(builder, model, "control/" + control.id + "/value", order++,
			control.valueRect, fill, fill, control.widget);
		addText(builder, model, source, inputPx, "control-label/" + control.id, order++,
			control.valueRect, control.displayValue,
			control.state.disabled ? model.style.disabled : model.style.text,
			CalypsoHdHAlign::Center, control.widget);
		collectAction(builder, model, source, actionPx, control.decrement, order);
		collectAction(builder, model, source, actionPx, control.increment, order);
		return;
	}
	const std::string value = control.displayValue.empty()
		? control.label : control.displayValue;
	addText(builder, model, source, inputPx, "control-label/" + control.id, order++,
		control.rect, value,
		control.state.disabled ? model.style.disabledText : model.style.text,
		CalypsoHdHAlign::Center, control.widget);
}

void collectCollection(CalypsoHdFrameBuilder& builder,
	const CalypsoHdOperationsModel& model,
	const CalypsoTtfSourceDescriptor& source,
	const CalypsoTtfSourceDescriptor& headingSource,
	int titlePx, int bodyPx, int labelPx, int dataPx,
	const CalypsoHdOperationsCollection& collection,
	const CalypsoHdOperationsRect& fallbackViewport,
	const std::vector<CalypsoHdOperationsRect>& columnGeometry,
	const std::vector<CalypsoHdOperationsRect>& rowGeometry,
	const CalypsoHdOperationsRect& fallbackScrollTrack,
	const CalypsoHdOperationsRect& fallbackScrollThumb,
	const std::string& prefix, int& order)
{
	const CalypsoHdOperationsRect viewport =
		calypsoHdOperationsCollectionViewport(collection, fallbackViewport);
	addPanel(builder, model, prefix + "/collection", order++, viewport,
		model.style.regionFill, model.style.regionFill);
	for (std::size_t i = 0; i < collection.columns.size(); ++i)
	{
		const auto& column = collection.columns[i];
		const CalypsoHdOperationsRect rect = i < columnGeometry.size()
			&& columnGeometry[i].valid() ? columnGeometry[i] : column.rect;
		const auto role = calypsoHdOperationsContentRole(column.contentRole, column.id);
		const bool data = role == CalypsoHdOperationsContentRole::Count
			|| role == CalypsoHdOperationsContentRole::Quantity
			|| role == CalypsoHdOperationsContentRole::Money
			|| role == CalypsoHdOperationsContentRole::Time
			|| role == CalypsoHdOperationsContentRole::Date
			|| role == CalypsoHdOperationsContentRole::Type;
		addText(builder, model, data ? model.monoFont : source,
			data ? dataPx : labelPx, prefix + "/column/" + column.id, order++,
			insetHorizontal(rect, 6), column.label, model.style.mutedText,
			data ? CalypsoHdHAlign::Right : CalypsoHdHAlign::Left);
	}
	if (collection.rows.empty())
	{
		int contentTop = viewport.y;
		for (const auto& rect : columnGeometry)
			if (rect.valid()) contentTop = std::max(contentTop, rect.y + rect.h);
		const int requestedInset = model.geometry.designWidth >= 1000 ? 48 : 24;
		const int inset = std::min(requestedInset, std::max(0, (viewport.w - 2) / 2));
		const int width = std::max(1, viewport.w - 2 * inset);
		const int availableHeight = std::max(1, viewport.y + viewport.h - contentTop);
		const int titleHeight = std::max(32, collection.rowHeight);
		const int bodyHeight = std::max(24, collection.rowHeight / 2);
		const int contentHeight = titleHeight + bodyHeight + 8;
		const int titleY = contentTop + std::max(0, (availableHeight - contentHeight) / 2);
		const int cardY = std::max(contentTop, titleY - 16);
		const int cardBottom = std::min(viewport.y + viewport.h, titleY + contentHeight + 16);
		const CalypsoHdOperationsRect emptyCard{
			viewport.x + inset, cardY, width, std::max(1, cardBottom - cardY)};
		const bool noMatch = collection.emptyKind == "no-match";
		addPanel(builder, model, prefix + "/empty-card", order++, emptyCard,
			model.style.panelFillTop, model.style.panelFillTop, nullptr,
			noMatch ? model.style.selection : model.style.accent);
		addText(builder, model, headingSource, titlePx,
			prefix + "/empty-title", order++,
			{viewport.x + inset, titleY, width, titleHeight},
			collection.emptyTitle, model.style.text, CalypsoHdHAlign::Center);
		addText(builder, model, source, bodyPx,
			prefix + "/empty-body", order++,
			{viewport.x + inset, titleY + titleHeight + 8, width, bodyHeight},
			collection.emptyBody, model.style.mutedText, CalypsoHdHAlign::Center);
	}
	// Generated rowSlots are the only geometry authority. `rowGeometry` is
	// supplied for the main workspace's generated geometry; nested collections
	// carry the same contract directly on the collection.
	const auto& generatedRows = rowGeometry.empty() ? collection.rowSlots : rowGeometry;
	const std::size_t visible = calypsoHdOperationsVisibleRows(collection);
	const std::size_t slots = std::min(visible, generatedRows.size());
	for (std::size_t slot = 0; slot < slots; ++slot)
	{
		const auto rowSlot = slot < collection.rowSlots.size()
			? calypsoHdOperationsRowSlot(collection, slot)
			: (slot < generatedRows.size() ? generatedRows[slot]
				: CalypsoHdOperationsRect{});
		const auto rowIndexResult = calypsoHdOperationsRowIndexForSlot(collection, slot);
		if (!rowSlot.valid() || !rowIndexResult.has_value())
			continue;
		const std::size_t rowIndex = rowIndexResult.value();
		const auto& row = collection.rows[rowIndex];
		const bool selected = calypsoHdOperationsRowSelected(collection, rowIndex);
		if (row.kind == CalypsoHdOperationsRowKind::GroupHeader)
		{
			addPanel(builder, model, prefix + "/group/" + row.id, order++, rowSlot,
				model.style.panelFillTop, model.style.panelFillTop, row.widget);
			const std::string label = !row.values.empty() ? row.values.front() : std::string();
			if (!label.empty())
				addText(builder, model, headingSource, labelPx,
					prefix + "/group-label/" + row.id, order++,
					{rowSlot.x + 8, rowSlot.y, std::max(1, rowSlot.w - 16), rowSlot.h},
					label, model.style.text, CalypsoHdHAlign::Left, row.widget);
			continue;
		}
		if (selected)
		{
			addPanel(builder, model, prefix + "/row-selection/" + row.id, order++, rowSlot,
				model.style.selection, model.style.selection, row.widget);
			addPanel(builder, model, prefix + "/row-rule/" + row.id, order++,
				{rowSlot.x, rowSlot.y, 3, rowSlot.h},
				model.style.accent, model.style.accent, row.widget);
		}
		for (std::size_t col = 0; col < collection.columns.size(); ++col)
		{
			const auto& column = collection.columns[col];
			const CalypsoHdOperationsCell* typed = col < row.cells.size()
				? &row.cells[col] : nullptr;
			const std::string value = typed ? typed->value
				: (col < row.values.size() ? row.values[col] : std::string());
			if (value.empty()) continue;
			const auto role = calypsoHdOperationsContentRole(
				typed && !typed->contentRole.empty() ? typed->contentRole
					: column.contentRole, column.id);
			const bool data = role == CalypsoHdOperationsContentRole::Count
				|| role == CalypsoHdOperationsContentRole::Quantity
				|| role == CalypsoHdOperationsContentRole::Money
				|| role == CalypsoHdOperationsContentRole::Time
				|| role == CalypsoHdOperationsContentRole::Date
				|| role == CalypsoHdOperationsContentRole::Type;
			CalypsoHdOperationsRect cell = col < columnGeometry.size()
				&& columnGeometry[col].valid() ? columnGeometry[col] : column.rect;
			cell.y = rowSlot.y;
			cell.h = rowSlot.h;
			addText(builder, model, data ? model.monoFont : source,
				data ? dataPx : bodyPx,
				prefix + "/cell/" + row.id + "/" + column.id, order++, cell, value,
				(typed && typed->state.disabled) || row.state.disabled
					? model.style.disabledText : model.style.text,
				data ? CalypsoHdHAlign::Right : CalypsoHdHAlign::Left, row.widget);
		}
	}
	const auto scrollTrack = calypsoHdOperationsScrollTrack(collection, fallbackScrollTrack);
	const auto visibleScrollThumb = calypsoHdOperationsVisibleScrollThumb(
		collection, fallbackScrollTrack, fallbackScrollThumb);
	if (visibleScrollThumb.valid() && scrollTrack.valid())
		addPanel(builder, model, prefix + "/scroll-track", order++, scrollTrack,
			model.style.regionFill, model.style.regionFill);
	if (visibleScrollThumb.valid())
		addPanel(builder, model, prefix + "/scroll-thumb", order++, visibleScrollThumb,
			model.style.selection, model.style.selection);
}
void collectOperationsWorkspace(CalypsoHdFrameBuilder& builder,
	const CalypsoHdOperationsModel& model, const CalypsoTtfSourceDescriptor& source,
	const CalypsoTtfSourceDescriptor& heading, const OperationsTypography& typography,
	int& order)
{
	const auto& g = model.geometry;
	collectOperationsShell(builder, model, typography, order);
	addText(builder, model, heading, typography.titlePx, "title", order++, g.title, model.title,
		model.style.text, CalypsoHdHAlign::Left);
	if (g.summaryBar.valid())
	{
		addPanel(builder, model, "summary", order++, g.summaryBar,
			model.style.regionFill, model.style.regionFill);
		for (const auto& field : model.summaryFields)
		{
			if (!field.state.visible) continue;
			const auto textRects = stackedTextRects(field.rect);
			addText(builder, model, source, typography.labelPx,
				"summary-label/" + field.id, order++, textRects.first, field.label,
				model.style.mutedText, CalypsoHdHAlign::Left, field.widget);
			addText(builder, model, model.monoFont, typography.dataPx,
				"summary-value/" + field.id, order++, textRects.second, field.value,
				model.style.text, CalypsoHdHAlign::Left, field.widget);
		}
	}
	if (g.toolbarBar.valid())
	{
		addPanel(builder, model, "toolbar", order++, g.toolbarBar,
			model.style.regionFill, model.style.regionFill);
		const auto textRects = toolbarTextRects(g.toolbarBar);
		if (!model.collection.heading.empty())
			addText(builder, model, source, typography.bodyPx, "collection-heading", order++,
				textRects.first, model.collection.heading, model.style.text,
				CalypsoHdHAlign::Left);
		if (!model.collection.meta.empty())
			addText(builder, model, model.monoFont, typography.dataPx, "collection-meta", order++,
				textRects.second, model.collection.meta, model.style.mutedText,
				CalypsoHdHAlign::Right);
	}
	for (const auto& control : model.controls)
		collectControl(builder, model, source, typography.inputPx, typography.actionPx,
			control, order);
	for (const auto& action : model.toolbarActions)
		collectAction(builder, model, source, typography.actionPx, action, order);

	collectCollection(builder, model, source, heading,
		typography.titlePx, typography.bodyPx, typography.labelPx, typography.dataPx,
		model.collection, g.collectionViewport, g.collectionColumns, g.collectionRows,
		g.collectionScrollTrack, g.collectionScrollThumb, "workspace", order);
	const CalypsoHdOperationsRect detailPanel = g.detailPanel.valid()
		? g.detailPanel : model.detail.panel;
	addPanel(builder, model, "detail", order++, detailPanel,
		model.style.regionFill, model.style.regionFill);
	const CalypsoHdOperationsRect identityRect = g.detailIdentity.valid()
		? g.detailIdentity : model.detail.identity.rect;
	const CalypsoHdOperationsRect identityTitleRect = g.detailIdentityTitle.valid()
		? g.detailIdentityTitle : model.detail.identity.titleRect;
	const CalypsoHdOperationsRect identitySubtitleRect = g.detailIdentitySubtitle.valid()
		? g.detailIdentitySubtitle : model.detail.identity.subtitleRect;
	addText(builder, model, source, typography.labelPx, "detail-label", order++, identityRect,
		model.detail.identity.label, model.style.mutedText, CalypsoHdHAlign::Left);
	addText(builder, model, heading, typography.detailTitlePx, "detail-title", order++,
		identityTitleRect, model.detail.identity.title, model.style.text,
		CalypsoHdHAlign::Left);
	addText(builder, model, source, typography.bodyPx, "detail-subtitle", order++,
		identitySubtitleRect, model.detail.identity.subtitle, model.style.mutedText,
		CalypsoHdHAlign::Left);
	for (std::size_t i = 0; i < model.detail.metrics.size(); ++i)
	{
		const auto& metric = model.detail.metrics[i];
		const CalypsoHdOperationsRect rect = i < g.detailMetrics.size()
			&& g.detailMetrics[i].valid() ? g.detailMetrics[i] : metric.rect;
		const auto textRects = stackedTextRects(rect);
		const std::uint32_t valueColor = metric.state.disabled
			? model.style.disabled : model.style.text;
		addText(builder, model, source, typography.labelPx,
			"metric-label/" + metric.id, order++, textRects.first, metric.label,
			metric.state.disabled ? model.style.disabled : model.style.mutedText,
			CalypsoHdHAlign::Left);
		addText(builder, model, model.monoFont, typography.dataPx,
			"metric-value/" + metric.id, order++, textRects.second, metric.value,
			valueColor, CalypsoHdHAlign::Left);
	}
	for (const auto& action : model.detail.actions)
		collectAction(builder, model, source, typography.actionPx, action, order);
	for (const auto& action : model.footerActions)
		collectAction(builder, model, source, typography.actionPx, action, order);
}

void collectWideDetail(CalypsoHdFrameBuilder& builder,
	const CalypsoHdOperationsModel& model, const CalypsoTtfSourceDescriptor& source,
	const CalypsoTtfSourceDescriptor& heading, const OperationsTypography& typography,
	int& order)
{
	const auto& g = model.geometry;
	addText(builder, model, heading, typography.titlePx, "title", order++, g.title, model.title,
		model.style.text, CalypsoHdHAlign::Left);
	addPanel(builder, model, "control-bar", order++, g.controlBar,
		model.style.regionFill, model.style.regionFill);
	for (const auto& control : model.controls)
		collectControl(builder, model, source, typography.inputPx, typography.actionPx,
			control, order);
	for (const auto& action : model.toolbarActions)
		collectAction(builder, model, source, typography.actionPx, action, order);
	for (const auto& region : model.regions)
	{
		if (!region.state.visible) continue;
		addPanel(builder, model, "region/" + region.id, order++, region.rect,
			model.style.regionFill, model.style.regionFill);
		addText(builder, model, source, typography.bodyPx, "region-label/" + region.id,
			order++, region.labelRect, region.label, model.style.mutedText,
			CalypsoHdHAlign::Left);
		if (region.kind == CalypsoHdOperationsRegionKind::Preview)
			addText(builder, model, source, typography.bodyPx,
				"region-preview/" + region.id, order++, region.previewRect,
				region.previewContent, model.style.text, CalypsoHdHAlign::Left);
		else if (region.kind == CalypsoHdOperationsRegionKind::Collection)
			collectCollection(builder, model, source, heading,
				typography.titlePx, typography.bodyPx, typography.labelPx, typography.dataPx,
				region.collection, region.rect, std::vector<CalypsoHdOperationsRect>(),
				region.collection.rowSlots, CalypsoHdOperationsRect(),
				CalypsoHdOperationsRect(), "region/" + region.id, order);
		else if (region.kind == CalypsoHdOperationsRegionKind::Fields)
			for (const auto& field : region.fields)
			{
				const std::uint32_t valueColor = field.state.disabled
					? model.style.disabled : model.style.text;
				addText(builder, model, source, typography.bodyPx,
					"region-field-label/" + region.id + "/" + field.id, order++,
					regionFieldLabelRect(region, field.rect), field.label,
					field.state.disabled ? model.style.disabled : model.style.mutedText,
					CalypsoHdHAlign::Left);
				addText(builder, model, model.monoFont, typography.dataPx,
					"region-field-value/" + region.id + "/" + field.id, order++,
					field.rect, field.value, valueColor, CalypsoHdHAlign::Left);
			}
		for (const auto& action : region.actions)
			collectAction(builder, model, source, typography.actionPx, action, order);
	}
	addPanel(builder, model, "footer", order++, g.footer,
		model.style.regionFill, model.style.regionFill);
	for (const auto& action : model.footerActions)
		collectAction(builder, model, source, typography.actionPx, action, order);
}

} // namespace

CalypsoHdOperationsRenderer::CalypsoHdOperationsRenderer(
	const void* state, CalypsoHdOperationsModel model)
	: _state(state), _model(std::move(model))
{
	if (_model.ownerState == nullptr) _model.ownerState = state;
}

CalypsoHdOperationsRenderer::~CalypsoHdOperationsRenderer()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

const void* CalypsoHdOperationsRenderer::topState() const
{
	return _state;
}

void CalypsoHdOperationsRenderer::collectLogicalSuppression(
	CalypsoHdLogicalSuppression& suppression) const
{
	for (const auto& action : _model.toolbarActions) suppression.add(action.widget);
	for (const auto& action : _model.detail.actions) suppression.add(action.widget);
	for (const auto& action : _model.footerActions) suppression.add(action.widget);
	for (const auto& control : _model.controls)
	{
		suppression.add(control.widget);
		suppression.add(control.decrement.widget);
		suppression.add(control.increment.widget);
	}
	for (const auto& field : _model.summaryFields) suppression.add(field.widget);
	for (const auto& row : _model.collection.rows) suppression.add(row.widget);
	for (const auto& region : _model.regions)
	{
		for (const auto& action : region.actions) suppression.add(action.widget);
		for (const auto& row : region.collection.rows) suppression.add(row.widget);
	}
	for (const void* widget : _model.suppressedWidgets) suppression.add(widget);
}

bool CalypsoHdOperationsRenderer::physicalFontsPresent() const
{
	const auto& heading = _model.headingFont;
	const auto& body = _model.bodyFont;
	const auto& mono = _model.monoFont;
	return !heading.canonicalVfsPath.empty() && !body.canonicalVfsPath.empty()
		&& !mono.canonicalVfsPath.empty() && heading.logicalDesignSize > 0
		&& body.logicalDesignSize > 0 && mono.logicalDesignSize > 0;
}

bool CalypsoHdOperationsRenderer::physicalReady() const
{
	if (_state == nullptr || !_model.readiness.fontsReady || !physicalFontsPresent())
		return false;
	Game* game = getCurrentGame();
	return CommandCenter::calypsoCcResolveFonts(game ? game->getMod() : nullptr).ready;
}

bool CalypsoHdOperationsRenderer::completeFrameReady() const
{
	return physicalReady() && calypsoHdOperationsModelReady(_model)
		&& CalypsoHdUiOverlay::instance().resourcesReadyForFrame();
}

bool CalypsoHdOperationsRenderer::retryableReadiness() const
{
	return _model.readiness.retryable && !completeFrameReady();
}

void CalypsoHdOperationsRenderer::setModel(CalypsoHdOperationsModel model)
{
	if (model.ownerState == nullptr) model.ownerState = _state;
	_model = std::move(model);
}

void CalypsoHdOperationsRenderer::setModelProvider(
	std::function<CalypsoHdOperationsModel()> provider)
{
	_modelProvider = std::move(provider);
}

void CalypsoHdOperationsRenderer::collect(CalypsoHdFrameBuilder& builder) const
{
	if (_modelProvider)
	{
		auto model = _modelProvider();
		if (model.ownerState == nullptr) model.ownerState = _state;
		_model = std::move(model);
	}
	if (!completeFrameReady()) return;
	builder.beginSubgroup();
	int order = 1;
	const OperationsTypography typography = operationsTypography(_model);
	if (_model.archetype == CalypsoHdOperationsArchetype::OperationsWorkspace)
	{
		collectOperationsWorkspace(builder, _model, _model.bodyFont, _model.headingFont,
			typography, order);
		collectCanonicalBasescapeChrome(builder, _model, order);
	}
	else
	{
		collectOperationsShell(builder, _model, typography, order);
		collectWideDetail(builder, _model, _model.bodyFont, _model.headingFont,
			typography, order);
		collectCanonicalBasescapeChrome(builder, _model, order);
	}
}

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
