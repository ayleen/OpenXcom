#ifdef __EMSCRIPTEN__
#include "CalypsoBaseLedger.h"
#include "CommandCenter/CommandCenterRenderer.h"
#include "CalypsoHdOperationsFluid.h"
#include "CalypsoHdOperationsLayout.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoHdFontSource.h"
#include "Generated/CalypsoF02BaseInformation.generated.h"
#include "Generated/CalypsoF02MonthlyCosts.generated.h"
#include "../Basescape/BaseInfoState.h"
#include "../Basescape/MonthlyCostsState.h"
#include "Generated/CalypsoF12Stores.generated.h"
#include "Generated/CalypsoF12ActiveTransfers.generated.h"
#include "Generated/CalypsoF12ItemLocations.generated.h"
#include "../Basescape/StoresState.h"
#include "../Basescape/TransfersState.h"
#include "../Basescape/ItemLocationsState.h"
#include "../Engine/Game.h"
#include "../Interface/ArrowButton.h"
#include "../Interface/Text.h"
#include "../Interface/TextEdit.h"
#include "../Interface/TextList.h"
#include "../Interface/ToggleTextButton.h"
#include "../Interface/Window.h"
#include "../Savegame/Base.h"

namespace OpenXcom { namespace Calypso {
namespace
{
using Vertical = CalypsoHdOperationsFluidPolicy::VerticalRole;

template<typename R>
CalypsoHdOperationsRectf design(const R &r) { return {r.x,r.y,r.w,r.h}; }

void place(Surface &widget, const CalypsoHdOperationsRect &rect)
{
	if (widget.getX() != rect.x) widget.setX(rect.x);
	if (widget.getY() != rect.y) widget.setY(rect.y);
	if (widget.getWidth() != rect.w) widget.setWidth(rect.w);
	if (widget.getHeight() != rect.h) widget.setHeight(rect.h);
}

bool wideLayout()
{
	const auto viewport = calypsoHdOperationsEffectiveViewport();
	const auto layout = classifyCalypsoHdOperationsLayout(viewport.cssWidth, viewport.cssHeight);
	if (layout == CalypsoHdOperationsLayoutClass::Unsupported)
		CalypsoHdUiOverlay::instance().failHdRoute("Inventory ledger viewport is unavailable");
	return layout == CalypsoHdOperationsLayoutClass::Wide;
}

struct Column
{
	std::string label;
	Surface *sort = nullptr;
};

struct Summary
{
	const char *id;
	std::string label;
	std::string value;
	CalypsoHdOperationsRectf rect;
};

struct Button
{
	const char *id;
	Surface *owner;
	std::string text;
	CalypsoHdOperationsRectf designRect;
	bool selected = false;
	bool toolbar = false;
};

template<typename Layout, typename Collection, typename Style, typename Typography>
CalypsoHdOperationsModel ledgerModel(State &state, const std::string &title, TextList &list, Base *base,
	const Layout &g, const Collection &table, const Style &style, const Typography &typography,
	const std::vector<Summary> &summary, const std::vector<Column> &columns,
	const std::vector<Button> &buttons,
	const char *emptyKey, const char *emptyBodyKey, unsigned familyId)
{
	CalypsoHdOperationsModel model;
	model.archetype = CalypsoHdOperationsArchetype::OperationsWorkspace;
	model.ownerState = &state;
	model.familyId = familyId;
	model.visualShell = "base-operations";
	model.headerArtId = familyId == 2 ? "base-build" : "logistics-group";
	model.presentation = "table-context";
	model.title = CommandCenter::calypsoHdNormalizeTtfDisplayText(title);
	model.sectionLabel = state.tr(familyId == 2 ? "STR_BASE_INFORMATION" : "STR_CAL_INVENTORY_SECTION");
	model.baseCaption = state.tr("STR_BASES");
	if (base) model.baseName = base->getName();
	model.readiness.contractReady = true;
	model.readiness.uploadsReady = true;
	model.readiness.retryable = true;
	const auto *mod = getCurrentGame()->getMod();
	model.readiness.fontsReady = calypsoHdResolveFontDescriptor(mod,"FONT_CC_INTER_SB",model.headingFont)
		&& calypsoHdResolveFontDescriptor(mod,"FONT_CC_INTER_R",model.bodyFont)
		&& calypsoHdResolveFontDescriptor(mod,"FONT_CC_PLEX_R",model.monoFont);
	calypsoHdOperationsApplyGeneratedStyle(model, style);
	calypsoHdOperationsApplyGeneratedTypography(model, typography);
	const auto frame = CalypsoHdOperationsFluidFrame::forReference(
		g.designWidth, g.designHeight, g.footer.x, g.footer.w);
	auto css = [&](auto r, Vertical role = Vertical::Fixed)
	{
		const auto resolved = frame.cssY(frame.cssX(design(r)),role);
		return CalypsoHdOperationsRect{resolved.x,resolved.y,resolved.w,resolved.h};
	};
	auto &geometry = model.geometry;
	geometry.designWidth = frame.width();
	geometry.designHeight = frame.height();
	geometry.window = {0,0,frame.width(),frame.height()};
	geometry.screenHeader = css(g.screenHeader);
	geometry.headerArt = css(g.headerArt);
	geometry.title = css(g.title);
	geometry.summaryBar = css(g.summaryBar);
	geometry.toolbarBar = css(g.toolbarBar);
	for (const auto &field : summary)
		model.summaryFields.push_back({field.id, field.label, field.value, css(field.rect), {}, nullptr});
	geometry.footer = css(g.footer, Vertical::BottomShift);
	auto &collection = model.collection;
	const auto viewport = css(g.collectionViewport, Vertical::Stretch);
	const auto firstRow = css(g.collection_row_slot_1);
	calypsoHdOperationsResolveCollectionGeometry(collection, viewport, firstRow,
		css(g.collection_scroll_track), firstRow.y - viewport.y, g.collectionViewport.w);
	geometry.collectionViewport = viewport;
	geometry.collectionRows = collection.rowSlots;
	geometry.collectionScrollTrack = collection.scrollTrack;
	geometry.collectionScrollThumb = collection.scrollThumb;
	place(list, frame.resolve(design(g.collectionViewport), Vertical::Stretch));
	const auto hits = frame.selectionListGeometry(design(g.collectionViewport),
		design(g.collection_row_slot_1), design(g.collection_scroll_track));
	auto edges = frame.projectChooserRowBoundaries(design(viewport),
		firstRow.y - viewport.y, firstRow.h, hits.visibleRows);
	for (int &edge : edges) edge += hits.rowOriginY;
	list.configureCalypsoHdSelectionList(hits.scrollBarWidth, hits.minThumbHeight,
		hits.rowStride, hits.rowOriginY, hits.dataViewportH, hits.visibleRows, edges);
	if (columns.size() != static_cast<std::size_t>(table.columnCount))
		CalypsoHdUiOverlay::instance().failHdRoute("Inventory ledger column mismatch");
	for (int i = 0; i < table.columnCount; ++i)
	{
		const auto &column = table.columns[i];
		collection.columns.push_back({column.id,columns[i].label,css(column.rect),{},column.contentRole});
		if (columns[i].sort)
			place(*columns[i].sort, frame.resolve(design(column.rect),Vertical::Fixed));
	}
	collection.emptyTitle = state.tr(emptyKey);
	collection.emptyBody = state.tr(emptyBodyKey);
	collection.selectedIndex = list.getSelectedRow();
	collection.scrollOffset = list.getScroll();
	const auto &nativeRows = list.getCellTextsSnapshot();
	collection.count = nativeRows.size();
	collection.scroll = {collection.scrollOffset,collection.count,collection.visibleRows,
		viewport,collection.scrollTrack,collection.scrollThumb};
	for (size_t i = 0; i < nativeRows.size(); ++i)
	{
		CalypsoHdOperationsRow row;
		row.id = std::to_string(i);
		row.widget = &list;
		row.state.selected = i == collection.selectedIndex;
		if (i >= collection.scrollOffset && i - collection.scrollOffset < collection.rowSlots.size())
			row.rect = collection.rowSlots[i - collection.scrollOffset];
		for (int c = 0; c < table.columnCount; ++c)
		{
			const auto &cells = nativeRows[i];
			const auto *text = static_cast<std::size_t>(c) < cells.size() ? cells[c] : nullptr;
			row.values.push_back(text ? CommandCenter::calypsoHdNormalizeTtfDisplayText(text->getText()) : std::string());
			row.cells.push_back({row.values.back(),table.columns[c].contentRole,
				{true,false,row.state.selected,false}});
		}
		collection.rows.push_back(std::move(row));
	}
	for (const auto &binding : buttons)
	{
		const auto role = binding.toolbar ? Vertical::Fixed : Vertical::BottomShift;
		place(*binding.owner, frame.resolve(binding.designRect, role));
		if (!binding.owner->getVisible()) continue;
		CalypsoHdOperationsAction action;
		action.id = binding.id;
		action.label = binding.text;
		action.component = "management-action-group";
		action.slotRole = "action";
		action.coordinateSpace = "logical";
		action.visible = css(binding.designRect, role);
		action.hit = action.visible;
		action.widget = binding.owner;
		action.state.selected = binding.selected;
		action.tone = action.id == "done" ? "primary" : "normal";
		if (binding.toolbar)
		{
			model.toolbarActions.push_back(std::move(action));
		}
		else
		{
			geometry.footerActions.push_back(action.visible);
			model.footerActions.push_back(std::move(action));
		}
	}
	return model;
}
}

void CalypsoBaseLedger::open()
{
	_renderer.setModelProvider(_collect);
	CalypsoHdUiOverlay::instance().registerAdapter(&_renderer);
	refresh();
	calypsoHdOperationsPublishHarnessVisibility();
}

void CalypsoBaseLedger::refresh()
{
	_renderer.setModel(_collect());
}

CalypsoBaseLedger::CalypsoBaseLedger(StoresState &s)
	: _chrome(s), _renderer(&s, {})
{
	_collect = [this, &s]()
	{
		namespace Gen = CalypsoF12StoresGen;
		const bool wide = wideLayout();
		const auto *g = Gen::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
		const auto &table = wide ? Gen::kCollectionsWide[0] : Gen::kCollectionsCompact[0];
		auto model = ledgerModel(s, s._txtTitle->getText(), *s._lstStores, s._base, *g, table,
			Gen::kProfileStyle, wide ? Gen::kTypographyWide : Gen::kTypographyCompact,
			{{"scope", s.tr("STR_CAL_SCOPE"), s._btnGrandTotal->getPressed() ? std::string(s.tr("STR_BASES")) : s._base->getName(), design(g->summary_scope)},
			 {"items", s.tr("STR_CAL_STORE_TYPES"), std::to_string(s._itemList.size()), design(g->summary_items)}},
			{{s._txtItem->getText(), s._sortName}, {s._txtQuantity->getText(), s._sortQuantity},
			 {s._txtSize->getText(), s._sortSize}, {s._txtSpaceUsed->getText(), s._sortSpaceUsed}},
			{{"quick-search", s._btnQuickSearch, s._btnQuickSearch->getText().empty() ? std::string(s.tr("STR_QUICK_SEARCH")) : s._btnQuickSearch->getText(), design(g->action_quick_search)},
			 {"scope", s._btnGrandTotal, s._btnGrandTotal->getText(), design(g->action_scope), s._btnGrandTotal->getPressed()},
			 {"done", s._btnOk, s._btnOk->getText(), design(g->action_done)}},
			"STR_CAL_EMPTY_STORES", "STR_CAL_EMPTY_STORES_HINT", 12);
		model.profileId = Gen::kProfileId;
		model.profileVersion = Gen::kProfileVersion;
		model.provenance = Gen::kProvenanceTemplate;
		_chrome.applyGeometry();
		_chrome.populateModel(model);
		return model;
	};
	open();
}

CalypsoBaseLedger::CalypsoBaseLedger(TransfersState &s)
	: _chrome(s), _renderer(&s, {})
{
	_collect = [this, &s]()
	{
		namespace Gen = CalypsoF12ActiveTransfersGen;
		const bool wide = wideLayout();
		const auto *g = Gen::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
		const auto &table = wide ? Gen::kCollectionsWide[0] : Gen::kCollectionsCompact[0];
		auto model = ledgerModel(s, s._txtTitle->getText(), *s._lstTransfers, s._base, *g, table,
			Gen::kProfileStyle, wide ? Gen::kTypographyWide : Gen::kTypographyCompact,
			{{"base", s.tr("STR_BASE"), s._base->getName(), design(g->summary_base)},
			 {"inbound", s.tr("STR_CAL_SHIPMENTS"), std::to_string(s._base->getTransfers()->size()), design(g->summary_inbound)},
			 {"next", s.tr("STR_CAL_NEXT_ARRIVAL_HOURS"), s._nextArrivalHours < 0 ? "–" : std::to_string(s._nextArrivalHours), design(g->summary_next)}},
			{{s._txtItem->getText()}, {s._txtQuantity->getText()}, {s._txtArrivalTime->getText()}},
			{{"done", s._btnOk, s._btnOk->getText(), design(g->action_done)}},
			"STR_CAL_EMPTY_TRANSFERS", "STR_CAL_EMPTY_TRANSFERS_HINT", 12);
		model.profileId = Gen::kProfileId;
		model.profileVersion = Gen::kProfileVersion;
		model.provenance = Gen::kProvenanceTemplate;
		_chrome.applyGeometry();
		_chrome.populateModel(model);
		return model;
	};
	open();
}

CalypsoBaseLedger::CalypsoBaseLedger(ItemLocationsState &s)
	: _chrome(s), _renderer(&s, {})
{
	_collect = [this, &s]()
	{
		namespace Gen = CalypsoF12ItemLocationsGen;
		const bool wide = wideLayout();
		const auto *g = Gen::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
		const auto &table = wide ? Gen::kCollectionsWide[0] : Gen::kCollectionsCompact[0];
		auto model = ledgerModel(s, s._txtTitle->getText(), *s._lstLocations, nullptr, *g, table,
			Gen::kProfileStyle, wide ? Gen::kTypographyWide : Gen::kTypographyCompact,
			{{"total", s.tr("STR_CAL_LEDGER_TOTAL"), std::to_string(s._scopeTotal), design(g->summary_total)},
			 {"inbound", s.tr("STR_CAL_INBOUND"), std::to_string(s._scopeInbound), design(g->summary_inbound)}},
			{{s._txtBase->getText()}, {s._txtQuantity->getText()}, {std::string(s.tr("STR_CAL_INBOUND"))}},
			{{"scope", s._btnScope, s._btnScope->getText(), design(g->action_scope), s._btnScope->getPressed()},
			 {"done", s._btnOk, s._btnOk->getText(), design(g->action_done)}},
			"STR_CAL_EMPTY_LOCATIONS", "STR_CAL_EMPTY_LOCATIONS_HINT", 12);
		model.profileId = Gen::kProfileId;
		model.profileVersion = Gen::kProfileVersion;
		model.provenance = Gen::kProvenanceTemplate;
		_chrome.applyGeometry();
		_chrome.populateModel(model);
		return model;
	};
	open();
}
CalypsoBaseLedger::CalypsoBaseLedger(BaseInfoState &s)
	: _chrome(s), _renderer(&s, {})
{
	_collect = [this, &s]()
	{
		namespace Gen = CalypsoF02BaseInformationGen;
		const bool wide = wideLayout();
		const auto *g = Gen::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
		const auto &table = wide ? Gen::kCollectionsWide[0] : Gen::kCollectionsCompact[0];
		auto model = ledgerModel(s, s._edtBase->getText().empty() ? std::string(s.tr("STR_BASE_INFORMATION")) : s._edtBase->getText(), *s._hdRows, s._base, *g, table,
			Gen::kProfileStyle, wide ? Gen::kTypographyWide : Gen::kTypographyCompact,
			{{"defense", s._txtDefense->getText(), s._numDefense->getText(), design(g->summary_defense)},
			 {"short-range", s._txtShortRange->getText(), s._numShortRange->getText(), design(g->summary_short_range)},
			 {"long-range", s._txtLongRange->getText(), s._numLongRange->getText(), design(g->summary_long_range)}},
			{{s.tr("STR_CAL_BASE_CATEGORY")}, {s.tr("STR_CAL_BASE_MEASURE")}, {s.tr("STR_QUANTITY_UC")}, {s.tr("STR_TOTAL")}, {s.tr("STR_CAL_BASE_OVER")}},
			{{"previous-base", s._previousBase, s._previousBase->getText(), design(g->toolbar_previous_base), false, true},
			 {"next-base", s._nextBase, s._nextBase->getText(), design(g->toolbar_next_base), false, true},
			 {"transfers", s._btnTransfers, s._btnTransfers->getText(), design(g->toolbar_transfers), false, true},
			 {"stores", s._btnStores, s._btnStores->getText(), design(g->action_stores)},
			 {"monthly", s._btnMonthlyCosts, s._btnMonthlyCosts->getText(), design(g->action_monthly)},
			 {"done", s._btnOk, s._btnOk->getText(), design(g->action_done)}},
			"STR_BASE_INFORMATION", "STR_BASE_INFORMATION", 2);
		const auto frame = CalypsoHdOperationsFluidFrame::forReference(g->designWidth, g->designHeight, g->footer.x, g->footer.w);
		place(*s._edtBase, frame.resolve(design(g->title), Vertical::Fixed));
		model.profileId = Gen::kProfileId;
		model.profileVersion = Gen::kProfileVersion;
		model.provenance = Gen::kProvenanceTemplate;
		_chrome.applyGeometry();
		_chrome.populateModel(model);
		return model;
	};
	open();
}

CalypsoBaseLedger::CalypsoBaseLedger(MonthlyCostsState &s)
	: _chrome(s), _renderer(&s, {})
{
	_collect = [this, &s]()
	{
		namespace Gen = CalypsoF02MonthlyCostsGen;
		const bool wide = wideLayout();
		const auto *g = Gen::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
		const auto &table = wide ? Gen::kCollectionsWide[0] : Gen::kCollectionsCompact[0];
		auto model = ledgerModel(s, s._txtTitle->getText(), *s._hdRows, s._base, *g, table,
			Gen::kProfileStyle, wide ? Gen::kTypographyWide : Gen::kTypographyCompact,
			{{"base", s.tr("STR_BASE"), s._base->getName(), design(g->summary_base)}},
			{{s.tr("STR_CAL_BASE_CATEGORY")}, {s._txtCost->getText()}, {s._txtQuantity->getText()}, {s._txtTotal->getText()}},
			{{"done", s._btnOk, s._btnOk->getText(), design(g->action_done)}},
			"STR_MONTHLY_COSTS", "STR_MONTHLY_COSTS", 2);
		model.profileId = Gen::kProfileId;
		model.profileVersion = Gen::kProfileVersion;
		model.provenance = Gen::kProvenanceTemplate;
		_chrome.applyGeometry();
		_chrome.populateModel(model);
		return model;
	};
	open();
}
} } // namespace OpenXcom::Calypso
#endif
