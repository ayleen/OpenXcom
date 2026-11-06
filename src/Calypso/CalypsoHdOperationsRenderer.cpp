#include "CalypsoHdOperationsRenderer.h"

#ifdef __EMSCRIPTEN__

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <typeinfo>
#include <utility>

#include "CalypsoHdUiOverlay.h"
#include "CalypsoBasescapeHdLayout.h"
#include "CalypsoHdHarnessHostState.h"
#include "CalypsoHdRdVisualStateFixture.h"
#include "CalypsoHdOperationsChrome.h"
#include "CalypsoHdOperationsLayout.h"
#include "CalypsoViewportRuntime.h"
#include "CommandCenter/CommandCenterLayout.h"
#include "CommandCenter/CommandCenterRenderer.h"
#include "CommandCenter/CommandCenterTheme.h"
#include "../Engine/Game.h"
#include "../Engine/Surface.h"
#include "../Engine/InteractiveSurface.h"
#include "../Mod/Mod.h"
#include "Generated/CalypsoHdTheme.generated.h"

namespace OpenXcom
{
namespace Calypso
{
namespace
{

CalypsoHdOperationsRenderer* g_activeOperationsRenderer = nullptr;

void applyVisualStateFixture(CalypsoHdOperationsModel& model)
{
	const int profile = calypsoHarnessSession().rdActiveProfile;
	const auto* spec = calypsoHdRdVisualStateProfileSpec(profile);
	if (!spec || !model.geometry.footer.valid()) return;
	const auto footer = model.geometry.footer;
	const int gap = 8;
	const int width = std::max(44, (footer.w - gap * 3) / 4);
	model.footerActions.clear();
	for (int index = 0; index < 4; ++index)
	{
		CalypsoHdOperationsAction action;
		action.id = std::string("l07-state-") + (index == 0 ? "safe" : index == 1 ? "primary" : index == 2 ? "danger" : "warning");
		action.label = action.id;
		action.component = "l07-state-fixture";
		action.slotRole = "visual-state";
		action.coordinateSpace = "logical";
		action.tone = index == 0 ? "safe" : index == 1 ? "primary" : index == 2 ? "danger" : "warning";
		action.visible = {footer.x + index * (width + gap), footer.y, width, footer.h};
		action.hit = action.visible;
	action.state.visible = true;
		if (profile == 325) action.state.hovered = true;
		if (profile == 326) action.state.focused = true;
		if (profile == 327) action.state.selected = true;
		if (profile == 328) action.state.disabled = true;
		model.footerActions.push_back(std::move(action));
	}
}

std::string jsonEscape(const std::string& value)
{
	std::string escaped;
	escaped.reserve(value.size() + 2);
	for (unsigned char ch : value)
	{
		switch (ch)
		{
		case '"': escaped += "\\\""; break;
		case '\\': escaped += "\\\\"; break;
		case '\b': escaped += "\\b"; break;
		case '\f': escaped += "\\f"; break;
		case '\n': escaped += "\\n"; break;
		case '\r': escaped += "\\r"; break;
		case '\t': escaped += "\\t"; break;
		default:
			if (ch < 0x20)
			{
				static const char hex[] = "0123456789abcdef";
				escaped += "\\u00";
				escaped += hex[(ch >> 4) & 0xf];
				escaped += hex[ch & 0xf];
			}
			else escaped += static_cast<char>(ch);
		}
	}
	return escaped;
}

void appendString(std::ostringstream& out, const char* key,
	const std::string& value, bool& first)
{
	if (!first) out << ',';
	first = false;
	out << '"' << key << "\":\"" << jsonEscape(value) << '"';
}

void appendBool(std::ostringstream& out, const char* key, bool value, bool& first)
{
	if (!first) out << ',';
	first = false;
	out << '"' << key << "\":" << (value ? "true" : "false");
}

void appendSize(std::ostringstream& out, const char* key, std::size_t value, bool& first)
{
	if (!first) out << ',';
	first = false;
	out << '"' << key << "\":" << value;
}

void appendInt(std::ostringstream& out, const char* key, int value, bool& first)
{
	if (!first) out << ',';
	first = false;
	out << '"' << key << "\":" << value;
}

void appendRect(std::ostringstream& out, const char* key,
	const CalypsoHdOperationsRect& rect, bool& first)
{
	if (!first) out << ',';
	first = false;
	out << '"' << key << "\":{\"x\":" << rect.x << ",\"y\":" << rect.y
		<< ",\"w\":" << rect.w << ",\"h\":" << rect.h << '}';
}

void appendRects(std::ostringstream& out, const char* key,
	const std::vector<CalypsoHdOperationsRect>& rects, bool& first)
{
	if (!first) out << ',';
	first = false;
	out << '"' << key << "\":[";
	for (std::size_t i = 0; i < rects.size(); ++i)
	{
		if (i) out << ',';
		const auto& rect = rects[i];
		out << "{\"x\":" << rect.x << ",\"y\":" << rect.y
			<< ",\"w\":" << rect.w << ",\"h\":" << rect.h << '}';
	}
	out << ']';
}

void appendState(std::ostringstream& out, const CalypsoHdOperationsState& state)
{
	out << "{\"visible\":" << (state.visible ? "true" : "false")
		<< ",\"hovered\":" << (state.hovered ? "true" : "false")
		<< ",\"focused\":" << (state.focused ? "true" : "false")
		<< ",\"selected\":" << (state.selected ? "true" : "false")
		<< ",\"disabled\":" << (state.disabled ? "true" : "false") << '}';
}

void appendAction(std::ostringstream& out, const CalypsoHdOperationsAction& action)
{
	out << '{';
	bool first = true;
	appendString(out, "id", action.id, first);
	appendString(out, "label", action.label, first);
	appendString(out, "component", action.component, first);
	appendString(out, "slotRole", action.slotRole, first);
	appendString(out, "coordinateSpace", action.coordinateSpace, first);
	appendString(out, "tone", action.tone, first);
	appendRect(out, "visible", action.visible, first);
	appendRect(out, "hit", action.hit, first);
	appendBool(out, "widget", action.widget != nullptr, first);
	if (const auto* owner = static_cast<const Surface*>(action.widget))
	{
		appendRect(out, "ownerRectEngineLogical", {owner->getX(), owner->getY(),
			owner->getWidth(), owner->getHeight()}, first);
		appendBool(out, "ownerVisible", owner->getVisible(), first);
		appendBool(out, "ownerHidden", owner->getHidden(), first);
		if (const auto* input = dynamic_cast<const InteractiveSurface*>(owner))
			appendBool(out, "ownerHovered", input->isHovered(), first);
	}
	if (!first) out << ',';
	out << "\"state\":";
	appendState(out, action.state);
	out << '}';
}

void appendChooser(std::ostringstream& out,
	const CalypsoHdOperationsChooser& chooser)
{
	out << '{';
	bool first = true;
	appendBool(out, "open", chooser.open, first);
	appendRect(out, "panel", chooser.panel, first);
	appendRect(out, "bounds", chooser.bounds, first);
	appendInt(out, "optionInsetX", chooser.optionInsetX, first);
	appendInt(out, "optionInsetY", chooser.optionInsetY, first);
	appendInt(out, "selectedIndex", chooser.selectedIndex, first);
	appendInt(out, "hoveredIndex", chooser.hoveredIndex, first);
	appendBool(out, "owner", chooser.owner != nullptr, first);
	if (!first) out << ',';
	out << "\"options\":[";
	for (std::size_t i = 0; i < chooser.options.size(); ++i)
	{
		if (i) out << ',';
		const auto& option = chooser.options[i];
		out << '{';
		bool optionFirst = true;
		appendString(out, "id", option.id, optionFirst);
		appendString(out, "label", option.label, optionFirst);
		appendRect(out, "rect", option.rect, optionFirst);
		appendBool(out, "widget", option.widget != nullptr, optionFirst);
		if (!optionFirst) out << ',';
		out << "\"state\":";
		appendState(out, option.state);
		out << '}';
	}
	out << "]}";
}

void appendCollection(std::ostringstream& out,
	const CalypsoHdOperationsCollection& collection)
{
	out << '{';
	bool first = true;
	appendString(out, "heading", collection.heading, first);
	appendString(out, "meta", collection.meta, first);
	appendString(out, "emptyTitle", collection.emptyTitle, first);
	appendString(out, "emptyBody", collection.emptyBody, first);
	appendSize(out, "count", calypsoHdOperationsCollectionCount(collection), first);
	if (!first) out << ',';
	first = false;
	out << "\"selectedIndex\":"
		<< (collection.selectedIndex == std::numeric_limits<std::size_t>::max()
			? -1 : static_cast<long long>(collection.selectedIndex));
	appendSize(out, "scrollOffset", calypsoHdOperationsScrollOffset(collection), first);
	appendSize(out, "visibleRows", calypsoHdOperationsVisibleRows(collection), first);
	appendInt(out, "rowHeight", collection.rowHeight, first);
	appendRect(out, "viewport", collection.scroll.viewport, first);
	const auto visibleThumb = calypsoHdOperationsVisibleScrollThumb(collection);
	appendRect(out, "scrollTrack", visibleThumb.valid()
		? calypsoHdOperationsScrollTrack(collection) : CalypsoHdOperationsRect{}, first);
	appendRect(out, "scrollThumb", visibleThumb, first);
	if (!first) out << ',';
	out << "\"rows\":[";
	for (std::size_t i = 0; i < collection.rows.size(); ++i)
	{
		if (i) out << ',';
		const auto& row = collection.rows[i];
		out << '{';
		bool rowFirst = true;
		appendString(out, "id", row.id, rowFirst);
		appendString(out, "kind", row.kind == CalypsoHdOperationsRowKind::GroupHeader
			? "group-header" : row.kind == CalypsoHdOperationsRowKind::Empty
			? "empty" : "item", rowFirst);
		appendRect(out, "rect", row.rect, rowFirst);
		appendBool(out, "widget", row.widget != nullptr, rowFirst);
		appendBool(out, "selected", calypsoHdOperationsRowSelected(collection, i), rowFirst);
		if (!rowFirst) out << ',';
		out << "\"values\":[";
		for (std::size_t value = 0; value < row.values.size(); ++value)
		{
			if (value) out << ',';
			out << '"' << jsonEscape(row.values[value]) << '"';
		}
		out << ']';
		if (!rowFirst) out << ',';
		out << "\"state\":";
		appendState(out, row.state);
		out << '}';
	}
	out << "],\"rowSlots\":[";
	for (std::size_t i = 0; i < collection.rowSlots.size(); ++i)
	{
		if (i) out << ',';
		const auto& rect = collection.rowSlots[i];
		out << "{\"x\":" << rect.x << ",\"y\":" << rect.y
			<< ",\"w\":" << rect.w << ",\"h\":" << rect.h << '}';
	}
	out << "]}";
}

void appendProgress(std::ostringstream& out,
	const std::optional<CalypsoHdOperationsProgress>& progress)
{
	out << "\"progressBar\":{";
	if (progress)
	{
		out << "\"visible\":true,\"ratio\":"
			<< std::setprecision(17) << progress->ratio << ",\"rect\":";
		out << '{' << "\"x\":" << progress->rect.x
			<< ",\"y\":" << progress->rect.y
			<< ",\"w\":" << progress->rect.w
			<< ",\"h\":" << progress->rect.h << '}';
	}
	else
	{
		out << "\"visible\":false,\"ratio\":null,\"rect\":{";
		out << "\"x\":0,\"y\":0,\"w\":0,\"h\":0}";
	}
	out << '}';
}

void appendGeometry(std::ostringstream& out, const CalypsoHdOperationsGeometry& geometry,
	const CalypsoHdOperationsCollection& collection)
{
	out << '{';
	bool first = true;
	appendInt(out, "designWidth", geometry.designWidth, first);
	appendInt(out, "designHeight", geometry.designHeight, first);
	appendRect(out, "window", geometry.window, first);
	appendRect(out, "screenHeader", geometry.screenHeader, first);
	appendRect(out, "headerArt", geometry.headerArt, first);
	appendRect(out, "status", geometry.status, first);
	appendRect(out, "title", geometry.title, first);
	appendRect(out, "summaryBar", geometry.summaryBar, first);
	appendRect(out, "toolbarBar", geometry.toolbarBar, first);
	appendRect(out, "controlBar", geometry.controlBar, first);
	appendRect(out, "collectionViewport", geometry.collectionViewport, first);
	appendRect(out, "detailPanel", geometry.detailPanel, first);
	appendRect(out, "footer", geometry.footer, first);
	appendRect(out, "detailIdentity", geometry.detailIdentity, first);
	appendRect(out, "detailIdentityTitle", geometry.detailIdentityTitle, first);
	appendRect(out, "detailIdentitySubtitle", geometry.detailIdentitySubtitle, first);
	const auto visibleThumb = calypsoHdOperationsVisibleScrollThumb(collection,
		geometry.collectionScrollTrack, geometry.collectionScrollThumb);
	appendRect(out, "collectionScrollTrack", visibleThumb.valid()
		? calypsoHdOperationsScrollTrack(collection, geometry.collectionScrollTrack)
		: CalypsoHdOperationsRect{}, first);
	appendRect(out, "collectionScrollThumb", visibleThumb, first);
	appendRects(out, "collectionColumns", geometry.collectionColumns, first);
	appendRects(out, "detailMetrics", geometry.detailMetrics, first);
	appendRects(out, "detailActions", geometry.detailActions, first);
	appendRects(out, "footerActions", geometry.footerActions, first);
	if (!first) out << ',';
	out << "\"collectionRows\":[";
	for (std::size_t i = 0; i < geometry.collectionRows.size(); ++i)
	{
		if (i) out << ',';
		const auto& rect = geometry.collectionRows[i];
		out << "{\"x\":" << rect.x << ",\"y\":" << rect.y
			<< ",\"w\":" << rect.w << ",\"h\":" << rect.h << '}';
	}
	out << "]}";
}

void appendControl(std::ostringstream& out, const CalypsoHdOperationsControl& control)
{
	out << '{';
	bool first = true;
	appendString(out, "id", control.id, first);
	appendString(out, "label", control.label, first);
	appendString(out, "displayValue", control.displayValue, first);
	appendInt(out, "selectedOption", control.selectedOption, first);
	appendBool(out, "toggled", control.toggled, first);
	appendRect(out, "rect", control.rect, first);
	appendRect(out, "valueRect", control.valueRect, first);
	appendRect(out, "labelRect", control.labelRect, first);
	appendBool(out, "widget", control.widget != nullptr, first);
	if (const auto* owner = static_cast<const Surface*>(control.widget))
		appendRect(out, "ownerRectEngineLogical", {owner->getX(), owner->getY(),
			owner->getWidth(), owner->getHeight()}, first);
	if (!first) out << ',';
	out << "\"decrement\":";
	appendAction(out, control.decrement);
	out << ",\"increment\":";
	appendAction(out, control.increment);
	out << ",\"state\":";
	appendState(out, control.state);
	out << '}';
}

void appendRegion(std::ostringstream& out, const CalypsoHdOperationsRegion& region)
{
	out << '{';
	bool first = true;
	appendString(out, "id", region.id, first);
	appendString(out, "label", region.label, first);
	appendString(out, "previewContent", region.previewContent, first);
	appendRect(out, "rect", region.rect, first);
	appendRect(out, "labelRect", region.labelRect, first);
	appendRect(out, "previewRect", region.previewRect, first);
	if (!first) out << ',';
	out << "\"collection\":";
	appendCollection(out, region.collection);
	out << ",\"fields\":[";
	for (std::size_t i = 0; i < region.fields.size(); ++i)
	{
		if (i) out << ',';
		const auto& field = region.fields[i];
		out << '{';
		bool fieldFirst = true;
		appendString(out, "id", field.id, fieldFirst);
		appendString(out, "label", field.label, fieldFirst);
		appendString(out, "value", field.value, fieldFirst);
		appendRect(out, "rect", field.rect, fieldFirst);
		if (!fieldFirst) out << ',';
		appendProgress(out, field.progress);
		out << '}';
	}
	out << "],\"actions\":[";
	for (std::size_t i = 0; i < region.actions.size(); ++i)
	{
		if (i) out << ',';
		appendAction(out, region.actions[i]);
	}
	out << "],\"state\":";
	appendState(out, region.state);
	out << '}';
}

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
	// Workspace typography is authored per canonical design class and must not
	// inherit the F01 chrome downscale: the compact 740x360 canvas is its own
	// CSS canvas, not a half-scale 1280x720 shell (external review R06).
	// Values bound from the generated profile win; the literals below are a
	// fallback for models built without the generated binding.
	const auto& metrics = CalypsoHdUiOverlay::instance().frozenMetrics();
	const bool wide = classifyCalypsoHdOperationsLayout(model.geometry.designWidth,
		model.geometry.designHeight) == CalypsoHdOperationsLayoutClass::Wide;
	auto px = [&](int designPx) {
		return calypsoHdOperationsPhysicalFontPx(
			designPx, model.geometry.designHeight, metrics.physicalHeight);
	};
	const auto& bound = model.typography;
	if (bound.body > 0)
	{
		return {
			px(bound.title), px(bound.detailTitle), px(bound.body),
			px(bound.label), px(bound.data), px(bound.input), px(bound.action)
		};
	}
	if (wide)
	{
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
	// Compact CSS targets per the approved guide: title 18, detail title 16,
	// body/button 12, label 10, data 13.
	return {
		px(18),
		px(16),
		px(12),
		px(10),
		px(13),
		px(12),
		px(12)
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

// The operations shell is layered over the canonical Basescape chrome.  Its
// root surface is a coverage quad, not a card: using the shared styled-panel
// helper here adds a rounded frame around the whole HD screen and changes the
// pixels behind the canonical rail/header.  Keep this item deliberately plain
// while retaining the normal panel ownership/ordering metadata.
void addRootSurface(CalypsoHdFrameBuilder& builder,
	const CalypsoHdOperationsModel& model, int order,
	const CalypsoHdOperationsRect& rect)
{
	if (!rect.valid()) return;
	CalypsoHdItem item = baseItem(model, "window", order, rect, nullptr);
	item.kind = CalypsoHdItemKind::Panel;
	// Match the canonical F01 flat root pass without reintroducing its
	// rounded window-card silhouette.
	item.colorRgba = CommandCenterTheme::packed(CommandCenterTheme::BgRoot);
	item.panelStyle.styled = false;
	builder.add(item);
}

void addProgressBar(CalypsoHdFrameBuilder& builder,
	const CalypsoHdOperationsModel& model, const std::string& name, int& order,
	const CalypsoHdOperationsProgress& progress)
{
	if (!progress.rect.valid()) return;
	const double ratio = std::max(0.0, std::min(1.0, progress.ratio));
	addPanel(builder, model, name + "/track", order++, progress.rect,
		model.style.panelFillTop, model.style.panelFillBottom);
	const int fillWidth = static_cast<int>(std::lround(progress.rect.w * ratio));
	if (fillWidth <= 0) return;
	addPanel(builder, model, name + "/fill", order++,
		{progress.rect.x, progress.rect.y, fillWidth, progress.rect.h},
		model.style.accent, model.style.accent);
}

void addText(CalypsoHdFrameBuilder& builder, const CalypsoHdOperationsModel& model,
	const CalypsoTtfSourceDescriptor& source, int physicalPixelHeight,
	const std::string& name, int order, const CalypsoHdOperationsRect& rect,
	const std::string& text, std::uint32_t color,
	CalypsoHdHAlign align, const void* widget = nullptr, bool autoWrap = false)
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
	item.rasterKey.explicitBreaksOnly = !autoWrap;
	item.rasterKey.guardRows = !autoWrap;
	if (autoWrap)
	{
		const auto& metrics = CalypsoHdUiOverlay::instance().frozenMetrics();
		item.rasterKey.wrapWidth = metrics.valid()
			? std::max(1, calypsoHdRoundToInt(rect.w * metrics.scaleX))
			: rect.w;
		// Action cells are the approved two-line exception. Use an explicit
		// skip so the guarded compositor cannot clip the second line.
		item.rasterKey.lineHeightPx = std::max(1, physicalPixelHeight);
	}
	else
	{
		// Other generated cells are fixed row bounds: preserve authored '\n'
		// breaks but never create implicit metric-wrapped lines.
		item.rasterKey.wrapWidth = 0;
	}
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
	const bool wide = classifyCalypsoHdOperationsLayout(g.designWidth, g.designHeight)
		== CalypsoHdOperationsLayoutClass::Wide;
	addRootSurface(builder, model, order++, g.window);
	const CalypsoBasescapeHdFitParams fitParams;
	const auto authored = calypsoBasescapeHdAuthoredSize(
		g.designWidth, g.designHeight, fitParams);
	const auto fitted = [&](int value) {
		return static_cast<int>(std::lround(value * authored.fitScale));
	};
	const CalypsoHdOperationsRect canonicalWorkspace{
		fitted(fitParams.railWidth), fitted(fitParams.headerHeight),
		fitted(authored.w - fitParams.railWidth),
		fitted(authored.h - fitParams.headerHeight)};
	const auto flatBackdrop = [&](const char* name,
		const CalypsoHdOperationsRect& rect, std::uint32_t color) {
		CalypsoHdItem item = baseItem(model, name, order++, rect, nullptr);
		item.kind = CalypsoHdItemKind::Panel;
		item.colorRgba = color;
		item.panelStyle.styled = false;
		builder.add(item);
	};
	// Shared chrome edge antialiasing must sample the same plate as F01.
	addImage(builder, model, "canonical-workspace-background", order++,
		canonicalWorkspace, "Resources/basescape/background.png");
	flatBackdrop("canonical-workspace-dim", canonicalWorkspace, 0x0000002Eu);
	const CalypsoHdOperationsRect workspace{
		g.screenHeader.x, g.screenHeader.y,
		g.designWidth - g.screenHeader.x,
		g.designHeight - g.screenHeader.y};
	// Replace only the inset Operations interior, preserving its existing tone.
	flatBackdrop("operations-workspace-root", workspace,
		CommandCenterTheme::packed(CommandCenterTheme::BgRoot));
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
	const auto effective = calypsoHdOperationsEffectiveViewport();
	const int cssWidth = effective.cssWidth;
	const int cssHeight = effective.cssHeight;
	const CommandCenter::CommandCenterLayout layout = CommandCenter::computeLayout(
		CommandCenter::Size2{static_cast<float>(cssWidth), static_cast<float>(cssHeight)},
		false, CommandCenter::InsetsF{
			static_cast<float>(viewport.safeX), static_cast<float>(viewport.safeY),
			static_cast<float>(cssWidth - viewport.safeX - std::min(viewport.safeWidth,
				std::max(0, cssWidth - viewport.safeX))),
			static_cast<float>(cssHeight - viewport.safeY - viewport.safeHeight)});
	const double densityX = static_cast<double>(effective.physicalWidth) / cssWidth;
	const double densityY = static_cast<double>(effective.physicalHeight) / cssHeight;
	const double logicalPerCssX = densityX / metrics.scaleX;
	const double logicalPerCssY = densityY / metrics.scaleY;
	CalypsoF21Painter painter{builder, model.familyId,
		reinterpret_cast<std::uintptr_t>(model.ownerState), order, 1.0f, 1.0,
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
	// F21Painter owns the monotonic item order.  `role` is only the shared
	// stable visual id passed to CommandCenter; it must not reset draw order.
	order = painter.ord;
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
		action.visible, action.label, palette.text, CalypsoHdHAlign::Center,
		action.widget, true);
}

void collectChooser(CalypsoHdFrameBuilder& builder,
	const CalypsoHdOperationsModel& model, const CalypsoTtfSourceDescriptor& source,
	int bodyPx, int& order)
{
	const auto& chooser = model.chooser;
	if (!chooser.open || !chooser.panel.valid()) return;
	addPanel(builder, model, "chooser/panel", order++, chooser.panel,
		model.style.panelFillTop, model.style.panelFillTop, chooser.owner,
		model.style.frame);
	const int insetX = std::max(0, chooser.optionInsetX);
	const int insetY = std::max(0, chooser.optionInsetY);
	for (const auto& option : chooser.options)
	{
		if (!option.state.visible || !option.rect.valid()) continue;
		const std::uint32_t fill = option.state.selected || option.state.hovered
			? model.style.selection : model.style.panelFillTop;
		addPanel(builder, model, "chooser/option/" + option.id, order++, option.rect,
			fill, fill, option.widget,
			option.state.selected ? model.style.accent : model.style.divider);
		CalypsoHdOperationsRect textRect = insetHorizontal(option.rect, insetX);
		textRect.y += insetY;
		textRect.h = std::max(1, textRect.h - insetY * 2);
		addText(builder, model, source, bodyPx,
			"chooser/label/" + option.id, order++,
			textRect, option.label,
			option.state.disabled ? model.style.disabledText : model.style.text,
			CalypsoHdHAlign::Left, option.widget);
	}
}

void collectControl(CalypsoHdFrameBuilder& builder,
	const CalypsoHdOperationsModel& model, const CalypsoTtfSourceDescriptor& source,
	const OperationsTypography& typography, const CalypsoHdOperationsControl& control,
	int& order)
{
	const int inputPx = typography.inputPx;
	const int actionPx = typography.actionPx;
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
		if (control.labelRect.valid())
		{
			// Visible localized caption for the quantity being stepped; the
			// value cell stays numeric-only (external review R07).
			addText(builder, model, source, typography.labelPx,
				"control-caption/" + control.id, order++,
				control.labelRect, control.label,
				model.style.mutedText, CalypsoHdHAlign::Left, control.widget);
		}
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
		const int requestedInset = classifyCalypsoHdOperationsLayout(model.geometry.designWidth,
		model.geometry.designHeight) == CalypsoHdOperationsLayoutClass::Wide ? 48 : 24;
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
			const CalypsoHdOperationsRect textCell =
				calypsoHdOperationsCollectionCellTextRect(
					column, col == 0 ? nullptr : &collection.columns[col - 1], cell);
			addText(builder, model, data ? model.monoFont : source,
				data ? dataPx : bodyPx,
				prefix + "/cell/" + row.id + "/" + column.id, order++, textCell, value,
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
		collectControl(builder, model, source, typography, control, order);
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
		if (metric.progress) addProgressBar(builder, model,
			"metric-progress/" + metric.id, order, *metric.progress);
		auto textRects = stackedTextRects(rect);
		if (metric.progress)
			textRects.second.h = std::max(1, textRects.second.h - metric.progress->rect.h);
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
	collectChooser(builder, model, source, typography.bodyPx, order);
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
		collectControl(builder, model, source, typography, control, order);
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
				if (field.progress) addProgressBar(builder, model,
					"region-field-progress/" + region.id + "/" + field.id,
					order, *field.progress);
				const std::uint32_t valueColor = field.state.disabled
					? model.style.disabled : model.style.text;
				addText(builder, model, source, typography.bodyPx,
					"region-field-label/" + region.id + "/" + field.id, order++,
					regionFieldLabelRect(region, field.rect), field.label,
					field.state.disabled ? model.style.disabled : model.style.mutedText,
					CalypsoHdHAlign::Left);
				CalypsoHdOperationsRect valueRect = field.rect;
				if (field.progress)
					valueRect.h = std::max(1, valueRect.h - field.progress->rect.h);
				addText(builder, model, model.monoFont, typography.dataPx,
					"region-field-value/" + region.id + "/" + field.id, order++,
					valueRect, field.value, valueColor, CalypsoHdHAlign::Left);
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
	if (g_activeOperationsRenderer == this) g_activeOperationsRenderer = nullptr;
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

const CalypsoHdOperationsRenderer* calypsoHdOperationsActiveRenderer()
{
	return g_activeOperationsRenderer;
}

std::string calypsoHdOperationsModelSnapshotJson(
	const CalypsoHdOperationsModel& model)
{
	std::ostringstream out;
	out << '{';
	bool first = true;
	appendString(out, "presentation", model.presentation, first);
	appendString(out, "profileId", model.profileId, first);
	appendString(out, "profileVersion", model.profileVersion, first);
	appendString(out, "provenance", model.provenance, first);
	appendSize(out, "familyId", model.familyId, first);
	appendString(out, "title", model.title, first);
	if (!first) out << ',';
	first = false;
	out << "\"shellIdentity\":{";
	bool shellFirst = true;
	appendString(out, "baseCaption", model.baseCaption, shellFirst);
	appendString(out, "baseName", model.baseName, shellFirst);
	appendString(out, "clockTime", model.clockTime, shellFirst);
	appendString(out, "clockDate", model.clockDate, shellFirst);
	appendString(out, "sectionLabel", model.sectionLabel, shellFirst);
	out << '}';
	if (!first) out << ',';
	first = false;
	out << "\"coordinateSpace\":\"css-fluid\",\"projection\":{";
	const auto& presentation = CalypsoHdUiOverlay::instance().frozenMetrics();
	const auto& viewport = calypsoViewportRuntime().current();
	const auto effective = calypsoHdOperationsEffectiveViewport();
	out << "\"logicalWidth\":" << effective.projectionLogicalWidth
		<< ",\"logicalHeight\":" << effective.projectionLogicalHeight
		<< ",\"physicalWidth\":" << effective.physicalWidth
		<< ",\"physicalHeight\":" << effective.physicalHeight
		<< ",\"engineLogicalWidth\":" << effective.projectionLogicalWidth
		<< ",\"engineLogicalHeight\":" << effective.projectionLogicalHeight
		<< ",\"effectiveCssWidth\":" << effective.cssWidth
		<< ",\"effectiveCssHeight\":" << effective.cssHeight
		<< ",\"sideBySide\":" << (effective.sideBySide ? "true" : "false")
		<< std::setprecision(17)
		<< ",\"scaleX\":" << presentation.scaleX
		<< ",\"scaleY\":" << presentation.scaleY
		<< ",\"contentOffsetX\":" << presentation.contentOffsetX
		<< ",\"contentOffsetY\":" << presentation.contentOffsetY
		<< ",\"dpr\":" << presentation.dpr
		<< ",\"generation\":" << presentation.generation
		<< ",\"runtimeLogicalWidth\":" << viewport.logicalWidth
		<< ",\"runtimeLogicalHeight\":" << viewport.logicalHeight
		<< ",\"runtimeGeneration\":" << calypsoViewportRuntime().generation()
		<< "},\"geometry\":";
	appendGeometry(out, model.geometry, model.collection);
	out << ",\"collection\":";
	appendCollection(out, model.collection);
	out << ",\"summaryFields\":[";
	for (std::size_t i = 0; i < model.summaryFields.size(); ++i)
	{
		if (i) out << ',';
		const auto& field = model.summaryFields[i];
		out << '{';
		bool fieldFirst = true;
		appendString(out, "id", field.id, fieldFirst);
		appendString(out, "label", field.label, fieldFirst);
		appendString(out, "value", field.value, fieldFirst);
		appendRect(out, "rect", field.rect, fieldFirst);
		appendBool(out, "widget", field.widget != nullptr, fieldFirst);
		if (!fieldFirst) out << ',';
		out << "\"state\":";
		appendState(out, field.state);
		out << '}';
	}
	out << "],\"toolbarActions\":[";
	for (std::size_t i = 0; i < model.toolbarActions.size(); ++i)
	{
		if (i) out << ',';
		appendAction(out, model.toolbarActions[i]);
	}
	out << "],\"chooser\":";
	appendChooser(out, model.chooser);
	out << ",\"footerActions\":[";
	for (std::size_t i = 0; i < model.footerActions.size(); ++i)
	{
		if (i) out << ',';
		appendAction(out, model.footerActions[i]);
	}
	out << "],\"detail\":{";
	bool detailFirst = true;
	appendString(out, "id", model.detail.id, detailFirst);
	appendString(out, "identityId", model.detail.identity.id, detailFirst);
	appendString(out, "identityTitle", model.detail.identity.title, detailFirst);
	appendString(out, "identitySubtitle", model.detail.identity.subtitle, detailFirst);
	appendRect(out, "panel", model.detail.panel, detailFirst);
	appendBool(out, "identityVisible", model.detail.identity.state.visible, detailFirst);
	if (!detailFirst) out << ',';
	out << "\"metrics\":[";
	for (std::size_t i = 0; i < model.detail.metrics.size(); ++i)
	{
		if (i) out << ',';
		const auto& metric = model.detail.metrics[i];
		out << '{';
		bool metricFirst = true;
		appendString(out, "id", metric.id, metricFirst);
		appendString(out, "label", metric.label, metricFirst);
		appendString(out, "value", metric.value, metricFirst);
		appendRect(out, "rect", metric.rect, metricFirst);
		if (!metricFirst) out << ',';
		appendProgress(out, metric.progress);
		if (!metricFirst) out << ',';
		out << "\"state\":";
		appendState(out, metric.state);
		out << '}';
	}
	out << "],\"actions\":[";
	for (std::size_t i = 0; i < model.detail.actions.size(); ++i)
	{
		if (i) out << ',';
		appendAction(out, model.detail.actions[i]);
	}
	out << "]},\"inputOwnerCounts\":{";
	out << "\"summaryFields\":" << model.summaryFields.size()
		<< ",\"toolbarActions\":" << model.toolbarActions.size()
		<< ",\"footerActions\":" << model.footerActions.size()
		<< ",\"collectionRows\":" << model.collection.rows.size()
		<< ",\"suppressedWidgets\":" << model.suppressedWidgets.size() << '}';
	out << ",\"controls\":[";
	for (std::size_t i = 0; i < model.controls.size(); ++i)
	{
		if (i) out << ',';
		appendControl(out, model.controls[i]);
	}
	out << "],\"regions\":[";
	for (std::size_t i = 0; i < model.regions.size(); ++i)
	{
		if (i) out << ',';
		appendRegion(out, model.regions[i]);
	}
	out << "],\"navigation\":{";
	bool navigationFirst = true;
	appendString(out, "origin", model.navigation.origin, navigationFirst);
	if (!navigationFirst) out << ',';
	out << "\"actions\":[";
	for (std::size_t i = 0; i < model.navigation.actions.size(); ++i)
	{
		if (i) out << ',';
		appendAction(out, model.navigation.actions[i]);
	}
	out << "]}";
	out << '}';
	return out.str();
}

void CalypsoHdOperationsRenderer::collect(CalypsoHdFrameBuilder& builder) const
{
	// Publish only the renderer that actually participates in the current
	// presented frame. Constructor order is not an ownership signal when a
	// stacked state is popped while an underlay remains alive.
	g_activeOperationsRenderer = const_cast<CalypsoHdOperationsRenderer*>(this);
	if (_modelProvider)
	{
		auto model = _modelProvider();
		if (model.ownerState == nullptr) model.ownerState = _state;
		_model = std::move(model);
	}
	applyVisualStateFixture(_model);
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
