#ifdef __EMSCRIPTEN__
#include "CalypsoInventoryLedger.h"
#include "CommandCenter/CommandCenterRenderer.h"
#include "CalypsoHdOperationsFluid.h"
#include "CalypsoHdOperationsLayout.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoHdFontSource.h"
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
CalypsoHdOperationsModel ledgerModel(State &state, Text &title, TextList &list, Base *base,
	const Layout &g, const Collection &table, const Style &style, const Typography &typography,
	const std::vector<Summary> &summary, const std::vector<Column> &columns,
	const std::vector<Button> &buttons,
	const char *emptyKey, const char *emptyBodyKey)
{
	CalypsoHdOperationsModel model;
	model.archetype = CalypsoHdOperationsArchetype::OperationsWorkspace;
	model.ownerState = &state;
	model.familyId = 12;
	model.visualShell = "base-operations";
	model.headerArtId = "logistics-group";
	model.presentation = "table-context";
	model.title = CommandCenter::calypsoHdNormalizeTtfDisplayText(title.getText());
	model.sectionLabel = state.tr("STR_CAL_INVENTORY_SECTION");
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

void CalypsoInventoryLedger::open()
{
	_renderer.setModelProvider(_collect);
	CalypsoHdUiOverlay::instance().registerAdapter(&_renderer);
	refresh();
	calypsoHdOperationsPublishHarnessVisibility();
}

void CalypsoInventoryLedger::refresh()
{
	_renderer.setModel(_collect());
}

CalypsoInventoryLedger::CalypsoInventoryLedger(StoresState &s)
	: _chrome(s), _renderer(&s, {})
{
	s._hdCategory = new TextButton(1,1,0,0);
	s.add(s._hdCategory, "button", "storesInfo");
	s._hdCategory->onMouseClick((ActionHandler)&StoresState::nextCategory);
	_collect = [this, &s]()
	{
		namespace Gen = CalypsoF12StoresGen;
		const bool wide = wideLayout();
		const auto *g = Gen::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
		const auto &table = wide ? Gen::kCollectionsWide[0] : Gen::kCollectionsCompact[0];
		auto model = ledgerModel(s, *s._txtTitle, *s._lstStores, s._base, *g, table,
			Gen::kProfileStyle, wide ? Gen::kTypographyWide : Gen::kTypographyCompact,
			{{"scope", s.tr("STR_CAL_SCOPE"), s._btnGrandTotal->getPressed() ? std::string(s.tr("STR_BASES")) : s._base->getName(), design(g->summary_scope)},
			 {"capacity", s.tr("STR_CAL_LEDGER_BASE_CAPACITY"), s.hdStorageCapacity(false), design(g->summary_capacity)},
			 {"over", s.tr("STR_CAL_LEDGER_OVER"), s.hdStorageCapacity(true), design(g->summary_over)}},
			{{s._txtItem->getText(), s._sortName}, {s._txtQuantity->getText(), s._sortQuantity},
			 {s._txtSize->getText(), s._sortSize}, {s._txtSpaceUsed->getText(), s._sortSpaceUsed}},
			{{"category", s._hdCategory, s.tr(s._category.empty() ? "STR_ALL_ITEMS" : s._category), design(g->toolbar_category), false, true},
			 {"quick-search", s._btnQuickSearch, s._btnQuickSearch->getText().empty() ? std::string(s.tr("STR_QUICK_SEARCH")) : s._btnQuickSearch->getText(), design(g->action_quick_search)},
			 {"scope", s._btnGrandTotal, s.tr(s._btnGrandTotal->getPressed() ? "STR_CAL_LEDGER_THIS_BASE" : "STR_CAL_LEDGER_ALL_BASES"), design(g->action_scope), s._btnGrandTotal->getPressed()},
			 {"done", s._btnOk, s._btnOk->getText(), design(g->action_done)}},
			"STR_CAL_EMPTY_STORES", "STR_CAL_EMPTY_STORES_HINT");
		model.profileId = Gen::kProfileId;
		model.profileVersion = Gen::kProfileVersion;
		model.provenance = Gen::kProvenanceTemplate;
		_chrome.applyGeometry();
		_chrome.populateModel(model);
		return model;
	};
	open();
}

CalypsoInventoryLedger::CalypsoInventoryLedger(TransfersState &s)
	: _chrome(s), _renderer(&s, {})
{
	_collect = [this, &s]()
	{
		namespace Gen = CalypsoF12ActiveTransfersGen;
		const bool wide = wideLayout();
		const auto *g = Gen::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
		const auto &table = wide ? Gen::kCollectionsWide[0] : Gen::kCollectionsCompact[0];
		auto model = ledgerModel(s, *s._txtTitle, *s._lstTransfers, s._base, *g, table,
			Gen::kProfileStyle, wide ? Gen::kTypographyWide : Gen::kTypographyCompact,
			{{"base", s.tr("STR_BASE"), s._allBases ? std::string(s.tr("STR_CAL_LEDGER_ALL_BASES")) : s._base->getName(), design(g->summary_base)},
			 {"inbound", s.tr("STR_CAL_SHIPMENTS"), std::to_string(s._lstTransfers->getTexts()), design(g->summary_inbound)},
			 {"next", s.tr("STR_CAL_NEXT_ARRIVAL_HOURS"), s._nextArrivalHours < 0 ? "–" : std::to_string(s._nextArrivalHours), design(g->summary_next)}},
			{{s._txtItem->getText()}, {s._txtQuantity->getText()}, {s._txtArrivalTime->getText()}, {s.tr("STR_BASE")}, {s.tr("STR_CAL_LEDGER_CAPACITY")}},
			{{"scope", s._hdScope, s._hdScope->getText(), design(g->toolbar_scope), s._allBases, true},
			 {"category", s._hdCategory, s._hdCategory->getText(), design(g->toolbar_category), false, true},
			 {"sort", s._hdSort, s._hdSort->getText(), design(g->toolbar_sort), false, true},
			 {"search", s._hdSearch, s._hdSearch->getText().empty() ? std::string(s.tr("STR_QUICK_SEARCH")) : s._hdSearch->getText(), design(g->toolbar_search), false, true},
			 {"done", s._btnOk, s._btnOk->getText(), design(g->action_done)}},
			"STR_CAL_EMPTY_TRANSFERS", "STR_CAL_EMPTY_TRANSFERS_HINT");
		model.profileId = Gen::kProfileId;
		model.profileVersion = Gen::kProfileVersion;
		model.provenance = Gen::kProvenanceTemplate;
		_chrome.applyGeometry();
		_chrome.populateModel(model);
		return model;
	};
	open();
}

CalypsoInventoryLedger::CalypsoInventoryLedger(ItemLocationsState &s)
	: _chrome(s), _renderer(&s, {})
{
	_collect = [this, &s]()
	{
		namespace Gen = CalypsoF12ItemLocationsGen;
		const bool wide = wideLayout();
		const auto *g = Gen::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
		const auto &table = wide ? Gen::kCollectionsWide[0] : Gen::kCollectionsCompact[0];
		auto model = ledgerModel(s, *s._txtTitle, *s._lstLocations, nullptr, *g, table,
			Gen::kProfileStyle, wide ? Gen::kTypographyWide : Gen::kTypographyCompact,
			{{"total", s.tr("STR_CAL_LEDGER_TOTAL"), std::to_string(s._scopeTotal), design(g->summary_total)},
			 {"inbound", s.tr("STR_CAL_INBOUND"), std::to_string(s._scopeInbound), design(g->summary_inbound)}},
			{{s._txtBase->getText()}, {s._txtQuantity->getText()}, {std::string(s.tr("STR_CAL_INBOUND"))}},
			{{"scope", s._btnScope, s._btnScope->getText(), design(g->action_scope), s._btnScope->getPressed()},
			 {"done", s._btnOk, s._btnOk->getText(), design(g->action_done)}},
			"STR_CAL_EMPTY_LOCATIONS", "STR_CAL_EMPTY_LOCATIONS_HINT");
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
