#ifdef __EMSCRIPTEN__
#include "Generated/CalypsoF17ContactTracker.generated.h"
#include "Generated/CalypsoF17TargetInfo.generated.h"
#include "../Geoscape/TargetInfoState.h"
#include "Generated/CalypsoF17OverlappingTargets.generated.h"
#include "../Geoscape/MultipleTargetsState.h"
#include "../Geoscape/UfoTrackerState.h"
#include "../Savegame/Target.h"
#include "Generated/CalypsoF15TopicSelector.generated.h"
#include "../Basescape/TechTreeSelectState.h"
#include "CalypsoBaseLedger.h"
#include "CalypsoContractDeliverState.h"
#include "Generated/CalypsoF37ContractDelivery.generated.h"
#include "CalypsoEconomyState.h"
#include "Generated/CalypsoF36Economy.generated.h"
#include "CommandCenter/CommandCenterRenderer.h"
#include "CalypsoHdOperationsFluid.h"
#include "CalypsoHdOperationsLayout.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoHdFontSource.h"
#include "Generated/CalypsoF13ManageContainment.generated.h"
#include "Generated/CalypsoF13ContainmentReview.generated.h"
#include "../Basescape/ManageAlienContainmentState.h"
#include "../Basescape/ContainmentReviewState.h"
#include "../Engine/Unicode.h"
#include "Generated/CalypsoF13GlobalContainment.generated.h"
#include "../Basescape/GlobalAlienContainmentState.h"
#include "Generated/CalypsoF02BaseInformation.generated.h"
#include "Generated/CalypsoF02MonthlyCosts.generated.h"
#include "../Basescape/BaseInfoState.h"
#include "../Basescape/MonthlyCostsState.h"
#include "Generated/CalypsoF12Stores.generated.h"
#include "Generated/CalypsoF12ActiveTransfers.generated.h"
#include "Generated/CalypsoF12ItemLocations.generated.h"
#include "../Basescape/StoresState.h"
#include "../Basescape/TransfersState.h"
#include "../Geoscape/ItemsArrivingState.h"
#include "../Geoscape/GeoscapeEventState.h"
#include "../Mod/RuleEvent.h"
#include "../Interface/ToggleTextButton.h"
#include "Generated/CalypsoF24StrategicEvent.generated.h"
#include "../Geoscape/ProductionCompleteState.h"
#include "CalypsoReportLines.h"
#include "Generated/CalypsoF24ProductionComplete.generated.h"
#include "Generated/CalypsoF24ItemsArriving.generated.h"
#include "Generated/CalypsoF31ArchiveSections.generated.h"
#include "Generated/CalypsoF31ArchiveTopics.generated.h"
#include "../Mod/ArticleDefinition.h"
#include "../Ufopaedia/UfopaediaStartState.h"
#include "../Ufopaedia/UfopaediaSelectState.h"
#include "Generated/CalypsoF24Unlocks.generated.h"
#include "../Basescape/ItemLocationsState.h"
#include "../Engine/Game.h"
#include "../Interface/ArrowButton.h"
#include "../Interface/Text.h"
#include "../Interface/TextEdit.h"
#include "../Interface/TextList.h"
#include "../Interface/ToggleTextButton.h"
#include "../Interface/Window.h"
#include "../Savegame/Base.h"
#include "../Mod/Mod.h"
#include "../Savegame/SavedGame.h"

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
	bool disabled = false;
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
	model.headerArtId = (familyId == 15 || familyId == 13) ? "base-research" : familyId == 2 ? "base-build" : "logistics-group";
	model.presentation = "table-context";
	model.title = CommandCenter::calypsoHdNormalizeTtfDisplayText(title);
	model.sectionLabel = state.tr(familyId == 15 ? "STR_TECH_TREE" : familyId == 36 || familyId == 37 ? "STR_CAL_ECON_TITLE" : familyId == 13 ? "STR_ALIEN_CONTAINMENT" : familyId == 2 ? "STR_BASE_INFORMATION" : "STR_CAL_INVENTORY_SECTION");
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
		action.state.disabled = binding.disabled;
		action.tone = action.id == "confirm" || action.id == "cleanup" ? "danger" : action.id == "done" ? "primary" : "normal";
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
			{{"defense", s.tr("STR_CAL_DEFENSE_SHORT"), s._numDefense->getText(), design(g->summary_defense)},
			 {"short-range", s.tr("STR_CAL_SONAR_SHORT"), s._numShortRange->getText(), design(g->summary_short_range)},
			 {"long-range", s.tr("STR_CAL_SONAR_WIDE"), s._numLongRange->getText(), design(g->summary_long_range)}},
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
CalypsoBaseLedger::CalypsoBaseLedger(GlobalAlienContainmentState &s)
	: _chrome(s), _renderer(&s, {})
{
	_collect = [this, &s]()
	{
		namespace Gen = CalypsoF13GlobalContainmentGen;
		const bool wide = wideLayout();
		const auto *g = Gen::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
		const auto &table = wide ? Gen::kCollectionsWide[0] : Gen::kCollectionsCompact[0];
		auto model = ledgerModel(s, s._txtTitle->getText(), *s._lstPrisoners, nullptr, *g, table,
			Gen::kProfileStyle, wide ? Gen::kTypographyWide : Gen::kTypographyCompact,
			{{"held", s.tr("STR_CAL_HELD"), std::to_string(s._hdHeld), design(g->summary_held)},
			 {"capacity", s.tr("STR_CAL_CAPACITY"), std::to_string(s._hdCapacity), design(g->summary_capacity)},
			 {"study", s.tr("STR_CAL_IN_STUDY"), std::to_string(s._hdStudy), design(g->summary_study)}},
			{{s.tr("STR_BASE")}, {s.tr("STR_CAL_HELD")}, {s.tr("STR_CAL_CAPACITY")}, {s.tr("STR_CAL_FREE_CELLS")}, {s.tr("STR_CAL_IN_STUDY")}},
			{{"done", s._btnOk, s._btnOk->getText(), design(g->action_done)}},
			"STR_PRISONER_OVERVIEW", "STR_CAL_NO_SPECIMENS", 13);
		model.profileId = Gen::kProfileId;
		model.profileVersion = Gen::kProfileVersion;
		model.provenance = Gen::kProvenanceTemplate;
		_chrome.applyGeometry();
		_chrome.populateModel(model);
		return model;
	};
	open();
}
CalypsoBaseLedger::CalypsoBaseLedger(ManageAlienContainmentState &s)
	: _chrome(s), _renderer(&s, {})
{
	_collect = [this, &s]()
	{
		namespace Gen = CalypsoF13ManageContainmentGen;
		const bool wide = wideLayout();
		const auto *g = Gen::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
		const auto &table = wide ? Gen::kCollectionsWide[0] : Gen::kCollectionsCompact[0];
		const int held = s._base->getUsedContainment(s._prisonType);
		const int capacity = s._base->getAvailableContainment(s._prisonType);
		auto model = ledgerModel(s, s._txtTitle->getText(), *s._lstAliens, s._base, *g, table,
			Gen::kProfileStyle, wide ? Gen::kTypographyWide : Gen::kTypographyCompact,
			{{"held", s.tr("STR_CAL_HELD"), std::to_string(held), design(g->summary_held)},
			 {"capacity", s.tr("STR_CAL_CAPACITY"), std::to_string(capacity), design(g->summary_capacity)},
			 {"balance", s.tr("STR_CAL_FREE_CELLS"), std::to_string(capacity - held + s._aliensSold), design(g->summary_balance)}},
			{{s._txtItem->getText()}, {s.tr("STR_COST_PER_UNIT")}, {s.tr("STR_CAL_REMAINING")}, {s.tr("STR_CAL_MARKED")}, {s.tr("STR_CAL_IN_STUDY")}},
			{{"global", s._hdGlobal, s._hdGlobal->getText(), design(g->toolbar_global), false, true},
			 {"decrease", s._hdDecrease, "−", design(g->toolbar_decrease), false, true},
			 {"increase", s._hdIncrease, "+", design(g->toolbar_increase), false, true},
			 {"transfer", s._btnTransfer, s._btnTransfer->getText(), design(g->toolbar_transfer), false, true},
			 {"review", s._btnOk, s.tr("STR_CAL_CONTAINMENT_REVIEW"), design(g->action_review), false, false, s._aliensSold == 0},
			 {"cleanup", s._btnCleanup, s._btnCleanup->getText(), design(g->action_review)},
			 {"sell", s._btnSell, s._btnSell->getText(), design(g->action_sell), false, false, s._aliensSold == 0},
			 {"done", s._btnCancel, s._btnCancel->getText(), design(g->action_done)}},
			"STR_CAL_NO_SPECIMENS", "STR_CAL_EMPTY_CELLS_HINT", 13);
		if (s._sel < model.collection.rows.size()) model.collection.selectedIndex = s._sel;
		model.profileId = Gen::kProfileId;
		model.profileVersion = Gen::kProfileVersion;
		model.provenance = Gen::kProvenanceTemplate;
		_chrome.applyGeometry();
		_chrome.populateModel(model);
		return model;
	};
	open();
}
CalypsoBaseLedger::CalypsoBaseLedger(ContainmentReviewState &s)
	: _chrome(s), _renderer(&s, {})
{
	_collect = [this, &s]()
	{
		namespace Gen = CalypsoF13ContainmentReviewGen;
		const bool wide = wideLayout();
		const auto *g = Gen::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
		const auto &table = wide ? Gen::kCollectionsWide[0] : Gen::kCollectionsCompact[0];
		auto model = ledgerModel(s, s._title->getText(), *s._rows, s._owner._base, *g, table,
			Gen::kProfileStyle, wide ? Gen::kTypographyWide : Gen::kTypographyCompact,
			{{"capacity", s.tr("STR_CAL_FREE_CELLS"), std::to_string(s._free), design(g->summary_capacity)},
			 {"proceeds", s.tr("STR_CAL_PROCEEDS"), Unicode::formatFunding(s._proceeds), design(g->summary_proceeds)}},
			{{s.tr("STR_ITEM")}, {s.tr("STR_QUANTITY_UC")}, {s.tr("STR_CAL_RESULT")}},
			{{"confirm", s._confirm, s._confirm->getText(), design(g->action_confirm)},
			 {"done", s._cancel, s._cancel->getText(), design(g->action_done)}},
			"STR_CAL_NO_SPECIMENS", "STR_CAL_EMPTY_CELLS_HINT", 13);
		model.profileId = Gen::kProfileId;
		model.profileVersion = Gen::kProfileVersion;
		model.provenance = Gen::kProvenanceTemplate;
		_chrome.applyGeometry();
		_chrome.populateModel(model);
		return model;
	};
	open();
}
CalypsoBaseLedger::CalypsoBaseLedger(CalypsoEconomyState &s)
	: _chrome(s), _renderer(&s, {})
{
	_collect = [this, &s]()
	{
		namespace Gen = CalypsoF36EconomyGen;
		const bool wide = wideLayout();
		const auto *g = Gen::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
		const auto &table = wide ? Gen::kCollectionsWide[0] : Gen::kCollectionsCompact[0];
		const char *headers[][3] = {
			{"STR_CAL_COUNTERPARTY", "STR_CAL_GRANT", "STR_CAL_MONTHS"},
			{"STR_CAL_CONTRACT", "STR_CAL_REWARD", "STR_CAL_STATUS"},
			{"STR_CAL_COUNTERPARTY", "STR_CAL_TIER", "STR_CAL_ECON_TAB_STANDING"}
		};
		const auto &col = headers[s._tab];
		auto *save = getCurrentGame()->getSavedGame();
		const auto *economy = save->getCalypsoEconomy();
		const char *empty = economy && economy->active() ? "STR_CAL_NO_RECORDS" : "STR_CAL_ECON_UNAVAILABLE";
		auto model = ledgerModel(s, s._txtTitle->getText(), *s._lstMain, nullptr, *g, table,
			Gen::kProfileStyle, wide ? Gen::kTypographyWide : Gen::kTypographyCompact,
			{{"funds", s.tr("STR_CAL_FUNDS"), Unicode::formatFunding(save->getFunds()), design(g->summary_funds)}},
			{{s.tr(col[0])}, {s.tr(col[1])}, {s.tr(col[2])}},
			{{"grants", s._btnGrants, s._btnGrants->getText(), design(g->toolbar_grants), s._tab == 0, true},
			 {"contracts", s._btnContracts, s._btnContracts->getText(), design(g->toolbar_contracts), s._tab == 1, true},
			 {"standing", s._btnStanding, s._btnStanding->getText(), design(g->toolbar_standing), s._tab == 2, true},
			 {"done", s._btnOk, s._btnOk->getText(), design(g->action_done)}},
			empty, "STR_CAL_ECON_EMPTY_HINT", 36);
		model.profileId = Gen::kProfileId;
		model.profileVersion = Gen::kProfileVersion;
		model.provenance = Gen::kProvenanceTemplate;
		_chrome.applyGeometry();
		_chrome.populateModel(model);
		return model;
	};
	open();
}
CalypsoBaseLedger::CalypsoBaseLedger(CalypsoContractDeliverState &s)
	: _chrome(s), _renderer(&s, {})
{
	_collect = [this, &s]()
	{
		namespace Gen = CalypsoF37ContractDeliveryGen;
		const bool wide = wideLayout();
		const auto *g = Gen::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
		const auto &table = wide ? Gen::kCollectionsWide[0] : Gen::kCollectionsCompact[0];
		const bool review = s._selected != nullptr;
		auto model = ledgerModel(s, s._txtTitle->getText(), *s._lstBases, nullptr, *g, table,
			Gen::kProfileStyle, wide ? Gen::kTypographyWide : Gen::kTypographyCompact,
			{{"quantity", s.tr("STR_QUANTITY_UC"), std::to_string(s._quote.qty), design(g->summary_quantity)},
			 {"reward", s.tr("STR_CAL_REWARD"), Unicode::formatFunding(s._quote.rewardTotal), design(g->summary_reward)}},
			{{s.tr(review ? "STR_CAL_BASE_MEASURE" : "STR_BASE")}, {s.tr(review ? "STR_CAL_BEFORE" : "STR_CAL_HELD")}, {s.tr("STR_CAL_AFTER")}},
			{{"deliver", s._btnDeliver, s._btnDeliver->getText(), design(g->action_deliver)},
			 {"done", s._btnCancel, s._btnCancel->getText(), design(g->action_done)}},
			s._notice.empty() ? "STR_CAL_ECON_DELIVER_NONE" : s._notice.c_str(), "STR_CAL_ECON_EMPTY_HINT", 37);
		model.profileId = Gen::kProfileId;
		model.profileVersion = Gen::kProfileVersion;
		model.provenance = Gen::kProvenanceTemplate;
		_chrome.applyGeometry();
		_chrome.populateModel(model);
		return model;
	};
	open();
}
CalypsoBaseLedger::CalypsoBaseLedger(TargetInfoState &s)
	: _chrome(s), _renderer(&s, {})
{
	_collect = [this, &s]()
	{
		namespace Gen = CalypsoF17TargetInfoGen;
		const bool wide = wideLayout();
		const auto *g = Gen::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
		if (!g) CalypsoHdUiOverlay::instance().failHdRoute("Target information layout is unavailable");
		const auto &table = wide ? Gen::kCollectionsWide[0] : Gen::kCollectionsCompact[0];
		auto *lang = getCurrentGame()->getLanguage();
		const auto name = s._target->getName(lang);
		const auto draft = s._edtTitle->getText();
		const auto preview = draft.empty() ? s._target->getDefaultName(lang) : draft;
		auto model = ledgerModel(s, s.tr("STR_CAL_TARGET_INFO"), *s._facts, nullptr, *g, table,
			Gen::kProfileStyle, wide ? Gen::kTypographyWide : Gen::kTypographyCompact,
			{{"current", s.tr("STR_CAL_CURRENT_NAME"), name, design(g->summary_current)},
			 {"preview", s.tr("STR_CAL_NAME_PREVIEW"), preview, design(g->summary_preview)}},
			{{s.tr("STR_CAL_CONTACT_FIELD")}, {s.tr("STR_CAL_CONTACT_VALUE")}},
			{{"name", s._edtTitle, draft.empty() ? std::string(s.tr("STR_NAME_UC")) : draft, design(g->toolbar_name), false, true},
			 {"apply", s._apply, s._apply->getText(), design(g->toolbar_apply), false, true, draft == name},
			 {"info", s._btnInfo, s._btnInfo->getText(), design(g->action_info)},
			 {"intercept", s._btnIntercept, s._btnIntercept->getText(), design(g->action_intercept)},
			 {"done", s._btnOk, s._btnOk->getText(), design(g->action_done)}},
			"STR_CAL_NO_CONTACTS", "STR_CAL_CONTACTS_EMPTY", 17);
		model.headerArtId = Gen::kHeaderArt;
		model.sectionLabel = s.tr("STR_CAL_CONTACTS");
		model.profileId = Gen::kProfileId;
		model.profileVersion = Gen::kProfileVersion;
		model.provenance = Gen::kProvenanceTemplate;
		_chrome.applyGeometry();
		_chrome.populateModel(model);
		model.baseName = s.tr("STR_GEOSCAPE");
		return model;
	};
	open();
}

CalypsoBaseLedger::CalypsoBaseLedger(UfoTrackerState &s)
	: _chrome(s), _renderer(&s, {})
{
	_collect = [this, &s]()
	{
		namespace Gen = CalypsoF17ContactTrackerGen;
		const bool wide = wideLayout();
		const auto *g = Gen::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
		if (!g) CalypsoHdUiOverlay::instance().failHdRoute("Contact table layout is unavailable");
		const auto &table = wide ? Gen::kCollectionsWide[0] : Gen::kCollectionsCompact[0];
		const bool picked = s._picked < s._objects.size();
		std::string label = picked ? s._objects[s._picked]->getName(getCurrentGame()->getLanguage()) : std::string(s.tr("STR_NONE_UC"));
		auto model = ledgerModel(s, s._txtTitle->getText(), *s._lstObjects, nullptr, *g, table,
			Gen::kProfileStyle, wide ? Gen::kTypographyWide : Gen::kTypographyCompact,
			{{"count", s.tr("STR_CAL_CONTACTS"), std::to_string(s._objects.size()), design(g->summary_count)},
			 {"selected", s.tr("STR_CAL_SELECTED_CONTACT"), label, design(g->summary_selected)}},
			{{s._txtObject->getText()}, {s._txtSize->getText()}, {s._txtAltitude->getText()}, {s._txtHeading->getText()}, {s._txtSpeed->getText()}},
			{{"detail", s._open, s._open->getText(), design(g->action_detail), false, false, !picked},
			 {"center", s._center, s._center->getText(), design(g->action_center), false, false, !picked},
			 {"reference", s._reference, s._reference->getText(), design(g->toolbar_reference), false, true, !s.canReference()},
			 {"done", s._btnCancel, s._btnCancel->getText(), design(g->action_done)}},
			"STR_CAL_NO_CONTACTS", "STR_CAL_CONTACTS_EMPTY", 17);
		model.headerArtId = Gen::kHeaderArt;
		model.sectionLabel = s.tr("STR_CAL_CONTACTS");
		model.provenance = Gen::kProvenanceTemplate;
		model.collection.selectedIndex = s._picked;
		for (size_t i = 0; i < model.collection.rows.size(); ++i)
		{
			auto &row = model.collection.rows[i];
			row.state.selected = i == s._picked;
			for (auto &cell : row.cells) cell.state.selected = row.state.selected;
		}
		_chrome.applyGeometry();
		_chrome.populateModel(model);
		model.baseName = s.tr("STR_GEOSCAPE");
		return model;
	};
	open();
}

CalypsoBaseLedger::CalypsoBaseLedger(MultipleTargetsState &s)
	: _chrome(s), _renderer(&s, {})
{
	_collect = [this, &s]()
	{
		namespace Gen = CalypsoF17OverlappingTargetsGen;
		const bool wide = wideLayout();
		const auto *g = Gen::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
		if (!g) CalypsoHdUiOverlay::instance().failHdRoute("Contact table layout is unavailable");
		const auto &table = wide ? Gen::kCollectionsWide[0] : Gen::kCollectionsCompact[0];
		const bool picked = s._picked < s._targets.size();
		std::string label = picked ? s._targets[s._picked]->getName(getCurrentGame()->getLanguage()) : std::string(s.tr("STR_NONE_UC"));
		if (picked && s._showDetails)
			label += " · " + std::string(s.tr("STR_CAL_CONTACT_FOLLOWERS")) + " " +
				std::to_string(s._targets[s._picked]->getFollowers()->size());
		auto model = ledgerModel(s, s.tr("STR_CAL_SELECT_CONTACT"), *s._contacts, nullptr, *g, table,
			Gen::kProfileStyle, wide ? Gen::kTypographyWide : Gen::kTypographyCompact,
			{{"count", s.tr("STR_CAL_CONTACTS"), std::to_string(s._targets.size()), design(g->summary_count)},
			 {"selected", s.tr("STR_CAL_SELECTED_CONTACT"), label, design(g->summary_selected)}},
			{{s.tr("STR_NAME_UC")}, {s.tr("STR_TYPE")}},
			{{"detail", s._details, s._details->getText(), design(g->action_detail), s._showDetails, false, !picked},
			 {"open", s._open, s._open->getText(), design(g->action_open), false, false, !picked},
			 {"done", s._cancel, s._cancel->getText(), design(g->action_done)}},
			"STR_CAL_NO_CONTACTS", "STR_CAL_CONTACTS_EMPTY", 17);
		model.headerArtId = Gen::kHeaderArt;
		model.sectionLabel = s.tr("STR_CAL_CONTACTS");
		model.provenance = Gen::kProvenanceTemplate;
		model.collection.selectedIndex = s._picked;
		for (size_t i = 0; i < model.collection.rows.size(); ++i)
		{
			auto &row = model.collection.rows[i];
			row.state.selected = i == s._picked;
			for (auto &cell : row.cells) cell.state.selected = row.state.selected;
		}
		_chrome.applyGeometry();
		_chrome.populateModel(model);
		model.baseName = s.tr("STR_GEOSCAPE");
		return model;
	};
	open();
}

CalypsoBaseLedger::CalypsoBaseLedger(TechTreeSelectState &s)
	: _chrome(s), _renderer(&s, {})
{
	_collect = [this, &s]()
	{
		namespace Gen = CalypsoF15TopicSelectorGen;
		const bool wide = wideLayout();
		const auto *g = Gen::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
		const auto &table = wide ? Gen::kCollectionsWide[0] : Gen::kCollectionsCompact[0];
		const auto &query = s._btnQuickSearch->getText();
		const bool picked = s._chosen < s._availableTopics.size();
		auto model = ledgerModel(s, s._txtTitle->getText(), *s._lstTopics, nullptr, *g, table,
			Gen::kProfileStyle, wide ? Gen::kTypographyWide : Gen::kTypographyCompact,
			{{"matches", s.tr("STR_CAL_TOPIC_RESULTS"), std::to_string(s._displayed.size()), design(g->summary_matches)}},
			{{s.tr("STR_CAL_TOPIC_NAME")}, {s.tr("STR_CAL_TOPIC_TYPE")}, {s.tr("STR_CAL_TOPIC_STATUS")}},
			{{"query", s._btnQuickSearch, query.empty() ? std::string(s.tr("STR_QUICK_SEARCH")) : query, design(g->toolbar_query), false, true},
			 {"category", s._category, s._category->getText(), design(g->toolbar_category), false, true},
			 {"status", s._status, s._status->getText(), design(g->toolbar_status), false, true},
			 {"open", s._open, s._open->getText(), design(g->action_open), false, false, !picked},
			 {"done", s._btnOk, s._btnOk->getText(), design(g->action_done)}},
			query.size() < 3 ? "STR_QS_THREE_LETTERS_A" : "STR_CAL_TOPIC_NO_MATCHES",
			query.size() < 3 ? "STR_QS_THREE_LETTERS_B" : "STR_CAL_TOPIC_CHANGE_QUERY", 15);
		auto &collection = model.collection;
		for (size_t i = 0; i < collection.rows.size(); ++i)
		{
			auto &row = collection.rows[i];
			const auto idx = s._displayed[i];
			row.id = std::to_string(s.topicType(idx)) + ":" + s._availableTopics[idx];
			row.state.selected = idx == s._chosen;
			for (auto &cell : row.cells) cell.state.selected = row.state.selected;
			if (row.state.selected) collection.selectedIndex = i;
		}
		model.profileId = Gen::kProfileId;
		model.profileVersion = Gen::kProfileVersion;
		model.provenance = Gen::kProvenanceTemplate;
		_chrome.applyGeometry();
		_chrome.populateModel(model);
		return model;
	};
	open();
}
CalypsoBaseLedger::CalypsoBaseLedger(UfopaediaStartState &s)
	: _chrome(s, !getCurrentGame()->getSavedGame()->getSavedBattle()), _renderer(&s, {})
{
	_collect = [this, &s]()
	{
		namespace Gen = CalypsoF31ArchiveSectionsGen;
		const bool wide = wideLayout();
		const auto &g = *Gen::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
		const auto &table = wide ? Gen::kCollectionsWide[0] : Gen::kCollectionsCompact[0];
		auto model = ledgerModel(s, s.tr("STR_UFOPAEDIA"), *s._hdSections, nullptr, g, table,
			Gen::kProfileStyle, wide ? Gen::kTypographyWide : Gen::kTypographyCompact,
			{{"matches", s.tr("STR_CAL_ARCHIVE_SECTIONS"), std::to_string(s._cats.size()), design(g.summary_matches)}},
			{{s.tr("STR_CAL_ARCHIVE_SECTION")}, {s.tr("STR_CAL_ARCHIVE_TOPICS")}, {s.tr("STR_CAL_ARCHIVE_NEW")}},
			{{"done", s._btnOk, s.tr("STR_OK"), design(g.action_done)}},
			"STR_CAL_NO_RECORDS", "STR_UFOPAEDIA", 31);
		for (size_t i = 0; i < model.collection.rows.size(); ++i)
			model.collection.rows[i].id = s._cats[i];
		model.profileId = Gen::kProfileId;
		model.profileVersion = Gen::kProfileVersion;
		model.provenance = Gen::kProvenanceTemplate;
		model.sectionLabel = s.tr("STR_UFOPAEDIA");
		model.headerArtId = "base-research";
		_chrome.applyGeometry();
		_chrome.populateModel(model);
		return model;
	};
	open();
}

CalypsoBaseLedger::CalypsoBaseLedger(UfopaediaSelectState &s)
	: _chrome(s, !getCurrentGame()->getSavedGame()->getSavedBattle()), _renderer(&s, {})
{
	_collect = [this, &s]()
	{
		namespace Gen = CalypsoF31ArchiveTopicsGen;
		const bool wide = wideLayout();
		const auto &g = *Gen::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
		const auto &table = wide ? Gen::kCollectionsWide[0] : Gen::kCollectionsCompact[0];
		const auto &query = s._btnQuickSearch->getText();
		auto model = ledgerModel(s, s.tr(s._section), *s._lstSelection, nullptr, g, table,
			Gen::kProfileStyle, wide ? Gen::kTypographyWide : Gen::kTypographyCompact,
			{{"matches", s.tr("STR_CAL_TOPIC_RESULTS"), std::to_string(s._filtered_article_list.size()), design(g.summary_matches)}},
			{{s.tr("STR_CAL_TOPIC_NAME")}, {s.tr("STR_CAL_TOPIC_STATUS")}},
			{{"query", s._btnQuickSearch, query.empty() ? std::string(s.tr("STR_QUICK_SEARCH")) : query, design(g.toolbar_query), false, true},
			 {"filter", s._hdFilter, s.tr(s.filterKey()), design(g.toolbar_filter), false, true},
			 {"seen", s._hdSeen, s.tr("STR_CAL_ARCHIVE_SEEN"), design(g.action_seen)},
			 {"done", s._btnOk, s.tr("STR_OK"), design(g.action_done)}},
			"STR_CAL_TOPIC_NO_MATCHES", "STR_CAL_TOPIC_CHANGE_QUERY", 31);
		const auto frame = CalypsoHdOperationsFluidFrame::forReference(g.designWidth, g.designHeight, g.footer.x, g.footer.w);
		s._hdStatusX = frame.resolve(design(table.columns[1].rect), Vertical::Fixed).x;
		for (size_t i = 0; i < model.collection.rows.size(); ++i)
			model.collection.rows[i].id = s._filtered_article_list[i]->id;
		model.profileId = Gen::kProfileId;
		model.profileVersion = Gen::kProfileVersion;
		model.provenance = Gen::kProvenanceTemplate;
		model.sectionLabel = s.tr("STR_UFOPAEDIA");
		model.headerArtId = "base-research";
		_chrome.applyGeometry();
		_chrome.populateModel(model);
		return model;
	};
	open();
}

CalypsoBaseLedger::CalypsoBaseLedger(ItemsArrivingState &s)
	: _chrome(s), _renderer(&s, {})
{
	_collect = [this, &s]() {
		namespace Gen = CalypsoF24ItemsArrivingGen;
		const bool wide = wideLayout();
		const auto &g = Gen::kLayouts[wide ? 0 : 1];
		const auto &table = wide ? Gen::kCollectionsWide[0] : Gen::kCollectionsCompact[0];
		auto model = ledgerModel(s, s._txtTitle->getText(), *s._lstTransfers, nullptr, g, table,
			Gen::kProfileStyle, wide ? Gen::kTypographyWide : Gen::kTypographyCompact,
			{{"arrivals", s._txtTitle->getText(), std::to_string(s._destinations.size()), design(g.summary_arrivals)}},
			{{s._txtItem->getText()}, {s._txtQuantity->getText()}, {s._txtDestination->getText()}},
			{{"goto", s._btnGotoBase, s._btnGotoBase->getText(), design(g.action_goto)},
			 {"done", s._btnOk, s._btnOk->getText(), design(g.action_done)}},
			"STR_CAL_NO_RECORDS", "STR_ITEMS_ARRIVING", 24);
		model.sectionLabel = s._txtTitle->getText();
		model.headerArtId = Gen::kHeaderArt;
		model.profileId = Gen::kProfileId;
		model.profileVersion = Gen::kProfileVersion;
		model.provenance = Gen::kProvenanceTemplate;
		_chrome.applyGeometry();
		_chrome.populateModel(model);
		return model;
	};
	open();
}
CalypsoBaseLedger::CalypsoBaseLedger(ProductionCompleteState &s)
	: _chrome(s), _renderer(&s, {})
{
	_collect = [this, &s]() {
		namespace Gen = CalypsoF24ProductionCompleteGen;
		const bool wide = wideLayout();
		const auto &g = Gen::kLayouts[wide ? 0 : 1];
		const auto &table = wide ? Gen::kCollectionsWide[0] : Gen::kCollectionsCompact[0];
		const auto &type = wide ? Gen::kTypographyWide : Gen::kTypographyCompact;
		const bool summary = s._lstSummary->getVisible();
		const int width = table.columns[1].rect.w;
		if (!summary && (s._noticeWidth != width || s._noticeFont != type.body))
		{
			s._noticeRows->clearList();
			calypsoAppendReportText(*s._noticeRows, *getCurrentGame()->getMod(), s.tr("STR_CAL_RESULT"),
				s._txtMessage->getText(), width, type.body);
			s._noticeWidth = width;
			s._noticeFont = type.body;
		}
		auto model = ledgerModel(s, s.tr(summary ? "STR_RANDOM_PRODUCTION_SUMMARY" : "STR_PRODUCTION_OVERVIEW"),
			*(summary ? s._lstSummary : s._noticeRows), s._base, g, table, Gen::kProfileStyle, type,
			{{"base", s.tr("STR_BASE"), s._base->getName(), design(g.summary_base)},
			 {"funds", s.tr("STR_CAL_FUNDS"), Unicode::formatFunding(getCurrentGame()->getSavedGame()->getFunds()), design(g.summary_funds)}},
			{{s.tr(summary ? "STR_ITEM" : "STR_TYPE")}, {s.tr(summary ? "STR_QUANTITY_UC" : "STR_VALUE")}},
			{{"summary", s._btnSummary, s._btnSummary->getText(), design(g.toolbar_summary), false, true},
			 {"base", s._btnGotoBase, s._btnGotoBase->getText(), design(g.action_base)},
			 {"done", s._btnOk, s._btnOk->getText(), design(g.action_done)}},
			"STR_CAL_NO_RECORDS", "STR_RANDOM_PRODUCTION_SUMMARY", 24);
		model.sectionLabel = s.tr("STR_PRODUCTION_OVERVIEW");
		model.headerArtId = Gen::kHeaderArt;
		model.profileId = Gen::kProfileId;
		model.profileVersion = Gen::kProfileVersion;
		model.provenance = Gen::kProvenanceTemplate;
		_chrome.applyGeometry();
		_chrome.populateModel(model);
		return model;
	};
	open();
}

CalypsoBaseLedger::CalypsoBaseLedger(GeoscapeEventState &s)
	: _chrome(s), _renderer(&s, {})
{
	_collect = [this, &s]() {
		namespace Gen = CalypsoF24StrategicEventGen;
		const bool wide = wideLayout();
		const auto &g = Gen::kLayouts[wide ? 0 : 1];
		const auto &table = wide ? Gen::kCollectionsWide[0] : Gen::kCollectionsCompact[0];
		const auto &type = wide ? Gen::kTypographyWide : Gen::kTypographyCompact;
		const bool summary = s._lstTransfers->getVisible();
		const int width = table.columns[1].rect.w;
		const auto frame = CalypsoHdOperationsFluidFrame::forReference(g.designWidth, g.designHeight, g.footer.x, g.footer.w);
		const int minRows = s._eventRule.alignBottom() ? frame.selectionListGeometry(design(g.collectionViewport),
			design(g.collection_row_slot_1), design(g.collection_scroll_track)).visibleRows : 0;
		if (!summary && (width != s._narrativeWidth || type.body != s._narrativeFont || minRows != s._narrativeMinimumRows))
		{
			s._narrative->clearList();
			calypsoAppendReportText(*s._narrative, *getCurrentGame()->getMod(), "", s._txtMessage->getText(), width, type.body, minRows);
			s._narrativeMinimumRows = minRows;
			s._narrativeWidth = width;
			s._narrativeFont = type.body;
		}
		auto *base = getCurrentGame()->getSavedGame()->getBases()->front();
		auto model = ledgerModel(s, s._txtTitle->getText(), *(summary ? s._lstTransfers : s._narrative),
			base, g, table, Gen::kProfileStyle, type,
			{{"base", s.tr("STR_BASE"), base->getName(), design(g.summary_base)}},
			{{s.tr(summary ? "STR_ITEM" : "STR_TYPE")}, {s.tr(summary ? "STR_QUANTITY_UC" : "STR_CAL_EVENT_REPORT")}},
			{{"summary", s._btnItemsArriving, s._btnItemsArriving->getText(), design(g.action_summary), summary},
			 {"done", s._btnOk, s._btnOk->getText(), design(g.action_done)}},
			"STR_CAL_NO_RECORDS", "STR_CAL_EVENT_REPORT", 24);
		model.sectionLabel = s.tr("STR_CAL_EVENT_REPORT");
		model.headerArtId = Gen::kHeaderArt;
		model.headerImageSource = "ruleset-art:" + s._eventRule.getBackground();
		model.profileId = Gen::kProfileId;
		model.profileVersion = Gen::kProfileVersion;
		model.provenance = Gen::kProvenanceTemplate;
		_chrome.applyGeometry();
		_chrome.populateModel(model);
		return model;
	};
	open();
}

void calypsoAddUnlockRow(State &state, TextList &list, const std::string &label,
	RuleBaseFacilityFunctions required)
{
	std::string services;
	for (const auto &key : getCurrentGame()->getMod()->getBaseFunctionNames(required))
	{
		if (!services.empty()) services += ", ";
		services += state.tr(key);
	}
	if (services.empty()) services = state.tr("STR_NONE");
	list.addRow(2, label.c_str(), services.c_str());
}

CalypsoBaseLedger::CalypsoBaseLedger(State &s, Base *base, Text &title, TextList &rows,
	TextButton &openDestination, TextButton &ok) : _chrome(s), _renderer(&s, {})
{
	_collect = [this, &s, base, &title, &rows, &openDestination, &ok]() {
		namespace Gen = CalypsoF24UnlocksGen;
		const bool wide = wideLayout();
		const auto &g = Gen::kLayouts[wide ? 0 : 1];
		const auto &table = wide ? Gen::kCollectionsWide[0] : Gen::kCollectionsCompact[0];
		auto model = ledgerModel(s, title.getText(), rows, base, g, table,
			Gen::kProfileStyle, wide ? Gen::kTypographyWide : Gen::kTypographyCompact,
			{{"base", s.tr("STR_BASE"), base->getName(), design(g.summary_base)}},
			{{s.tr("STR_NAME")}, {s.tr("STR_SERVICES_REQUIRED")}},
			{{"open", &openDestination, openDestination.getText(), design(g.action_open)},
			 {"done", &ok, ok.getText(), design(g.action_done)}},
			"STR_CAL_NO_RECORDS", "STR_NONE", 24);
		model.sectionLabel = title.getText();
		model.headerArtId = Gen::kHeaderArt;
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
