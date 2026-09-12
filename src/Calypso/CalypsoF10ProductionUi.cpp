#ifdef __EMSCRIPTEN__
#include "CalypsoF10ProductionUi.h"
#include "CalypsoHdFontSource.h"
#include "CalypsoHdOperationsChrome.h"
#include "CalypsoHdOperationsRenderer.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoHdOperationsLayout.h"
#include "CalypsoViewportRuntime.h"
#include "Generated/CalypsoF10ProductionQueue.generated.h"
#include "Generated/CalypsoF10ProductionCatalogue.generated.h"
#include "Generated/CalypsoF10ProductionRequirements.generated.h"
#include "Generated/CalypsoF10ProductionControls.generated.h"
#include "Generated/CalypsoF10ProductionDependencies.generated.h"
#include "../Basescape/ManufactureState.h"
#include "../Basescape/NewManufactureListState.h"
#include "../Basescape/ManufactureStartState.h"
#include "../Basescape/ManufactureInfoState.h"
#include "../Basescape/ManufactureDependenciesTreeState.h"
#include "../Engine/Game.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Options.h"
#include "../Engine/Unicode.h"
#include "../Interface/ArrowButton.h"
#include "../Interface/ComboBox.h"
#include "../Engine/Surface.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextEdit.h"
#include "../Interface/TextList.h"
#include "../Interface/ToggleTextButton.h"
#include "../Interface/Window.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleManufacture.h"
#include "../Savegame/Base.h"
#include "../Savegame/Production.h"
#include "../Savegame/SavedGame.h"
#include "../Ufopaedia/Ufopaedia.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace OpenXcom { namespace Calypso {
namespace {

/// Shared responsive input for the eleven R&D forms (re-review P1): the
/// layout class comes from the canonical logical viewport, never from
/// Options::baseXResolution or the physical backing size. Below the 740x360
/// minimum the registered route fails closed with an explicit size error.
bool calypsoHdRdWideLayout(Game *game)
{
	(void)game;
	const auto &viewport = Calypso::calypsoViewportRuntime().current();
	const auto layoutClass = Calypso::classifyCalypsoHdOperationsLayout(
		std::max(1, viewport.logicalWidth), std::max(1, viewport.logicalHeight));
	if (layoutClass == Calypso::CalypsoHdOperationsLayoutClass::Unsupported)
	{
		CalypsoHdUiOverlay::instance().failHdRoute(
			"R&D HD viewport is below the 740x360 minimum");
	}
	return layoutClass == Calypso::CalypsoHdOperationsLayoutClass::Wide;
}

template <typename R>
CalypsoHdOperationsRect projectRect(const R &r, int wx, int wy, double sx, double sy, int windowX, int windowY)
{
	return {wx + static_cast<int>(std::lround((r.x - windowX) * sx)),
		wy + static_cast<int>(std::lround((r.y - windowY) * sy)),
		std::max(1, static_cast<int>(std::lround(r.w * sx))),
		std::max(1, static_cast<int>(std::lround(r.h * sy)))};
}

template <typename R>
void place(Surface *surface, const R &rect, int wx, int wy, double sx, double sy, int windowX, int windowY)
{
	if (!surface) return;
	const auto p = projectRect(rect, wx, wy, sx, sy, windowX, windowY);
	if (surface->getX() != p.x) surface->setX(p.x);
	if (surface->getY() != p.y) surface->setY(p.y);
	if (surface->getWidth() != p.w) surface->setWidth(p.w);
	if (surface->getHeight() != p.h) surface->setHeight(p.h);
}

template <typename R>
void setWindow(Window *window, const R &rect)
{
	if (!window) return;
	if (window->getX() != rect.x) window->setX(rect.x);
	if (window->getY() != rect.y) window->setY(rect.y);
	if (window->getWidth() != rect.w) window->setWidth(rect.w);
	if (window->getHeight() != rect.h) window->setHeight(rect.h);
}

template <typename R>
void setOperationsWindow(Window *window, const R &rect)
{
	const auto projected = calypsoHdOperationsProjectForCurrentPresentation(
		{rect.x, rect.y, rect.w, rect.h}, rect.w, rect.h);
	setWindow(window, projected);
}

/// One descriptor for paint, hit-testing and the native scrollbar, projected
/// from the generated track itself: the data viewport height is exactly the
/// painted track height (the row origin is a separate Y offset, never part of
/// the height) plus the emitted visible slot capacity — never a hardcoded
/// wide/compact guess (re-review P2 parity).
template <typename R, typename Collection>
void configureHdList(TextList &list, const R &parent, const R &rowSlot1,
	const R &scrollTrack, const Collection &generated, double sx, double sy)
{
	const auto descriptor = Calypso::calypsoSelectionListDescriptorFor(
		static_cast<int>(std::lround(parent.y * sy)),
		static_cast<int>(std::lround(scrollTrack.y * sy)),
		static_cast<int>(std::lround(scrollTrack.w * sx)),
		static_cast<int>(std::lround(scrollTrack.h * sy)),
		static_cast<int>(std::lround(rowSlot1.h * sy)),
		static_cast<std::size_t>(generated.rowSlotCount),
		static_cast<int>(std::lround(44 * sy)));
	list.configureCalypsoHdSelectionList(
		descriptor.scrollBarWidth, descriptor.minThumbHeight, descriptor.rowStride,
		descriptor.rowOriginY, descriptor.dataViewportH, descriptor.visibleRows);
}


void setFonts(CalypsoHdOperationsModel &model, const Mod *mod)
{
	model.readiness.contractReady = true;
	model.readiness.uploadsReady = true;
	model.readiness.retryable = true;
	model.readiness.fontsReady =
		mod != nullptr
		&& calypsoHdResolveFontDescriptor(mod, "FONT_CC_INTER_SB", model.headingFont)
		&& calypsoHdResolveFontDescriptor(mod, "FONT_CC_INTER_R", model.bodyFont)
		&& calypsoHdResolveFontDescriptor(mod, "FONT_CC_PLEX_R", model.monoFont);
}

void setContractMetadata(CalypsoHdOperationsModel &model,
	const char *presentation, const char *profileId, const char *profileVersion,
	const char *provenance)
{
	model.presentation = presentation ? presentation : "";
	model.profileId = profileId ? profileId : "";
	model.profileVersion = profileVersion ? profileVersion : "";
	model.provenance = provenance ? provenance : "";
	model.readiness.contractReady = !model.presentation.empty()
		&& !model.profileId.empty() && !model.profileVersion.empty()
		&& !model.provenance.empty();
}

void setBaseCaption(CalypsoHdOperationsModel &model, const std::string &base)
{
	model.baseCaption = base.empty() ? std::string("BASES") : base;
}

CalypsoHdOperationsAction action(const std::string &id, const std::string &label,
	const CalypsoHdOperationsRect &rect, const void *widget, bool visible = true,
	const std::string &tone = "normal")
{
	CalypsoHdOperationsAction out;
	out.id = id; out.label = label; out.component = "management-action-group";
	out.slotRole = "action"; out.coordinateSpace = "logical"; out.tone = tone;
	out.visible = rect; out.hit = rect; out.widget = widget; out.state.visible = visible;
	return out;
}

CalypsoHdOperationsMetric metric(const std::string &id, const std::string &label,
	const std::string &value, const CalypsoHdOperationsRect &rect)
{
	CalypsoHdOperationsMetric out;
	out.id = id; out.label = label; out.value = value; out.rect = rect;
	return out;
}

CalypsoHdOperationsSummaryField summary(const std::string &id, const std::string &label,
	const std::string &value, const CalypsoHdOperationsRect &rect, const void *widget)
{
	CalypsoHdOperationsSummaryField out;
	out.id = id; out.label = label; out.value = value; out.rect = rect; out.widget = widget;
	return out;
}


std::string productionTimeLeft(const Production *prod)
{
	if (prod->getInfiniteAmount()) return "∞";
	if (prod->getAssignedEngineers() <= 0) return "-";
	const int timeLeft = prod->getAmountTotal() * prod->getRules()->getManufactureTime()
		- prod->getTimeSpent();
	const int hoursLeft = (timeLeft + prod->getAssignedEngineers() - 1)
		/ prod->getAssignedEngineers();
	return std::to_string(hoursLeft / 24) + "/" + std::to_string(hoursLeft % 24);
}

std::string productionProgress(const Production *prod)
{
	std::string progress = std::to_string(prod->getAmountProduced()) + " / ";
	progress += prod->getInfiniteAmount() ? "∞" : std::to_string(prod->getAmountTotal());
	if (prod->getSellItems()) progress += " $";
	return progress;
}

template <typename Collection, typename Project>
void setGeneratedCollectionRows(CalypsoHdOperationsModel &model,
	const Collection &collection, const Project &project)
{
	model.geometry.collectionRows.clear();
	for (int i = 0; i < collection.rowSlotCount; ++i)
		model.geometry.collectionRows.push_back(project(collection.rowSlots[i].rect));
}

template <typename GeneratedProfileStyle,
	typename GeneratedTypographyWide, typename GeneratedTypographyCompact>
void finishModel(CalypsoHdOperationsModel &model, const Mod *mod,
	const char *presentation, const char *profileId, const char *profileVersion,
	const char *provenance, const GeneratedProfileStyle &generatedStyle,
	const GeneratedTypographyWide &typographyWide,
	const GeneratedTypographyCompact &typographyCompact, bool wideLayout)
{
	setContractMetadata(model, presentation, profileId, profileVersion, provenance);
	// One shared binder carries the emitted canonical palette; the renderer
	// defaults (e.g. the legacy accent) never survive to the frame (R06).
	calypsoHdOperationsApplyGeneratedStyle(model, generatedStyle);
	calypsoHdOperationsApplyGeneratedTypography(model,
		wideLayout ? typographyWide : typographyCompact);
	setFonts(model, mod);
}
} // namespace

CalypsoF10ProductionUi::CalypsoF10ProductionUi(ManufactureState *state)
	: _kind(Kind::Queue), _queue(state),
	  _chrome(new CalypsoHdOperationsChrome(*state))
{
	_renderer = new CalypsoHdOperationsRenderer(state, CalypsoHdOperationsModel{});
	_renderer->setModelProvider([this]() { syncGeometry(); return buildModel(); });
}
CalypsoF10ProductionUi::CalypsoF10ProductionUi(NewManufactureListState *state)
	: _kind(Kind::Catalogue), _catalogue(state),
	  _chrome(new CalypsoHdOperationsChrome(*state))
{
	_renderer = new CalypsoHdOperationsRenderer(state, CalypsoHdOperationsModel{});
	_renderer->setModelProvider([this]() { syncGeometry(); return buildModel(); });
}
CalypsoF10ProductionUi::CalypsoF10ProductionUi(ManufactureStartState *state)
	: _kind(Kind::Requirements), _requirements(state),
	  _chrome(new CalypsoHdOperationsChrome(*state))
{
	_renderer = new CalypsoHdOperationsRenderer(state, CalypsoHdOperationsModel{});
	_renderer->setModelProvider([this]() { syncGeometry(); return buildModel(); });
}
CalypsoF10ProductionUi::CalypsoF10ProductionUi(ManufactureInfoState *state)
	: _kind(Kind::Controls), _controls(state),
	  _chrome(new CalypsoHdOperationsChrome(*state))
{
	_renderer = new CalypsoHdOperationsRenderer(state, CalypsoHdOperationsModel{});
	_renderer->setModelProvider([this]() { syncGeometry(); return buildModel(); });
}
CalypsoF10ProductionUi::CalypsoF10ProductionUi(ManufactureDependenciesTreeState *state)
	: _kind(Kind::Dependencies), _dependencies(state),
	  _chrome(new CalypsoHdOperationsChrome(*state))
{
	_renderer = new CalypsoHdOperationsRenderer(state, CalypsoHdOperationsModel{});
	_renderer->setModelProvider([this]() { syncGeometry(); return buildModel(); });
}

CalypsoF10ProductionUi::~CalypsoF10ProductionUi()
{
	delete _renderer;
	delete _chrome;
}

#define F10_CONFIGURE(TYPE) \
void CalypsoF10ProductionUi::configure(TYPE &state) \
{ \
	if (state._hdAdapter) return; \
	if (!calypsoHdOperationsRouteEnabled(state._game, "F10")) \
	{ state._hdLayout = false; return; } \
	state._screen = true; \
	state._hdLayout = true; state._hdWideLayout = calypsoHdRdWideLayout(state._game); \
	auto *adapter = new CalypsoF10ProductionUi(&state); state._hdAdapter = adapter; \
	CalypsoHdUiOverlay::instance().registerAdapter(adapter->_renderer); adapter->refresh(); \
	calypsoHdOperationsPublishHarnessVisibility(); \
}
F10_CONFIGURE(ManufactureState)
F10_CONFIGURE(NewManufactureListState)
F10_CONFIGURE(ManufactureStartState)
F10_CONFIGURE(ManufactureInfoState)
F10_CONFIGURE(ManufactureDependenciesTreeState)
#undef F10_CONFIGURE

#define F10_RESIZE(TYPE) \
bool CalypsoF10ProductionUi::resize(TYPE &state) \
{ \
	if (!state._hdLayout || !state._hdAdapter) return false; \
	state._hdWideLayout = calypsoHdRdWideLayout(state._game); state._hdAdapter->refresh(); return true; \
}
F10_RESIZE(ManufactureState)
F10_RESIZE(NewManufactureListState)
F10_RESIZE(ManufactureStartState)
F10_RESIZE(ManufactureInfoState)
F10_RESIZE(ManufactureDependenciesTreeState)
#undef F10_RESIZE

void CalypsoF10ProductionUi::syncGeometry()
{
	switch (_kind)
	{
	case Kind::Queue: applyQueueGeometry(); break;
	case Kind::Catalogue: applyCatalogueGeometry(); break;
	case Kind::Requirements: applyRequirementsGeometry(); break;
	case Kind::Controls: applyControlsGeometry(); break;
	case Kind::Dependencies: applyDependenciesGeometry(); break;
	}
	if (_chrome) _chrome->applyGeometry();
}

void CalypsoF10ProductionUi::refresh()
{
	if (!_renderer) return;
	syncGeometry();
	_renderer->setModel(buildModel());
}

CalypsoHdOperationsModel CalypsoF10ProductionUi::buildModel() const
{
	CalypsoHdOperationsModel model;
	switch (_kind)
	{
	case Kind::Queue: model = buildQueueModel(); break;
	case Kind::Catalogue: model = buildCatalogueModel(); break;
	case Kind::Requirements: model = buildRequirementsModel(); break;
	case Kind::Controls: model = buildControlsModel(); break;
	case Kind::Dependencies: model = buildDependenciesModel(); break;
	}
	if (_chrome) _chrome->populateModel(model);
	return model;
}

void CalypsoF10ProductionUi::ensureQueueOwners()
{
	if (!_queue) return;
	auto make = [&](TextButton *&button, ActionHandler handler)
	{
		if (button) return;
		button = new TextButton(1, 1, 0, 0); button->setText("");
		_queue->add(button, "button", "manufactureMenu"); button->onMouseClick(handler);
	};
	make(_queue->_btnGlobalOverview, (ActionHandler)&ManufactureState::onCurrentGlobalProductionClick);
	make(_queue->_btnOpenProduction, (ActionHandler)&ManufactureState::lstManufactureClickLeft);
	make(_queue->_btnTechTree, (ActionHandler)&ManufactureState::lstManufactureClickMiddle);
	_queue->_btnGlobalOverview->setText(_queue->tr("STR_GLOBAL_OVERVIEW"));
	_queue->_btnOpenProduction->setText(_queue->tr("STR_OPEN_PRODUCTION"));
	_queue->_btnTechTree->setText(_queue->tr("STR_TECH_TREE"));
}

void CalypsoF10ProductionUi::ensureCatalogueOwners()
{
	if (!_catalogue) return;
	auto make = [&](TextButton *&button, ActionHandler handler)
	{
		if (button) return;
		button = new TextButton(1, 1, 0, 0); button->setText("");
		_catalogue->add(button, "button", "selectNewManufacture"); button->onMouseClick(handler);
	};
	make(_catalogue->_btnReview, (ActionHandler)&NewManufactureListState::lstProdClickLeft);
	make(_catalogue->_btnTechTree, (ActionHandler)&NewManufactureListState::lstProdClickMiddle);
	make(_catalogue->_btnUfopaedia, (ActionHandler)&NewManufactureListState::onUfopaedia);
	make(_catalogue->_btnMarkAllSeen, (ActionHandler)&NewManufactureListState::btnMarkAllAsSeenClick);
}

void CalypsoF10ProductionUi::applyQueueGeometry()
{
	if (!_queue || !_queue->_window) return;
	ensureQueueOwners();
	const bool wide = _queue->_hdWideLayout;
	const auto *g = CalypsoF10ProductionQueueGen::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) return;
	const auto &generated = wide
		? CalypsoF10ProductionQueueGen::kCollectionsWide[0]
		: CalypsoF10ProductionQueueGen::kCollectionsCompact[0];
	setOperationsWindow(_queue->_window, g->window);
	const int wx = _queue->_window->getX(), wy = _queue->_window->getY();
	const double sx = static_cast<double>(_queue->_window->getWidth()) / g->window.w;
	const double sy = static_cast<double>(_queue->_window->getHeight()) / g->window.h;
	_queue->_lstManufacture->rebaseNativeSize(
		g->collectionViewport.w, g->collectionViewport.h);
	auto p = [&](const auto &r) { return projectRect(r, wx, wy, sx, sy, g->window.x, g->window.y); };
	place(_queue->_txtTitle, g->title, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_queue->_txtAvailable, g->summary_available, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_queue->_txtAllocated, g->summary_allocated, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_queue->_txtSpace, g->summary_workshop_space, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_queue->_txtItem, g->collection_column_item, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_queue->_txtEngineers, g->collection_column_engineers, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_queue->_txtProduced, g->collection_column_produced, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_queue->_txtCost, g->collection_column_cost, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_queue->_txtTimeLeft, g->collection_column_time, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_queue->_lstManufacture, g->collectionViewport, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_queue->_btnGlobalOverview, g->action_global_overview, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_queue->_btnNew, g->action_new_production, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_queue->_btnOk, g->action_done, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_queue->_btnOpenProduction, g->detail_selected_production_action_open_production, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_queue->_btnTechTree, g->detail_selected_production_action_tech_tree, wx, wy, sx, sy, g->window.x, g->window.y);
	configureHdList(*_queue->_lstManufacture, g->collectionViewport,
		g->collection_row_slot_1, g->collection_scroll_track, generated, sx, sy);
}

void CalypsoF10ProductionUi::applyCatalogueGeometry()
{
	if (!_catalogue || !_catalogue->_window) return;
	ensureCatalogueOwners();
	const bool wide = _catalogue->_hdWideLayout;
	const auto *g = CalypsoF10ProductionCatalogueGen::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) return;
	const auto &generated = wide
		? CalypsoF10ProductionCatalogueGen::kCollectionsWide[0]
		: CalypsoF10ProductionCatalogueGen::kCollectionsCompact[0];
	setOperationsWindow(_catalogue->_window, g->window);
	const int wx = _catalogue->_window->getX(), wy = _catalogue->_window->getY();
	const double sx = static_cast<double>(_catalogue->_window->getWidth()) / g->window.w;
	const double sy = static_cast<double>(_catalogue->_window->getHeight()) / g->window.h;
	_catalogue->_lstManufacture->rebaseNativeSize(
		g->collectionViewport.w, g->collectionViewport.h);
	auto put = [&](Surface *s, const auto &r) { place(s, r, wx, wy, sx, sy, g->window.x, g->window.y); };
	put(_catalogue->_txtTitle, g->title); put(_catalogue->_txtItem, g->collection_column_item);
	put(_catalogue->_txtCategory, g->collection_column_category); put(_catalogue->_lstManufacture, g->collectionViewport);
	put(_catalogue->_cbxFilter, g->toolbar_availability_default); put(_catalogue->_cbxCategory, g->toolbar_category_all);
	put(_catalogue->_btnShowOnlyNew, g->toolbar_show_only_new); put(_catalogue->_btnQuickSearch, g->toolbar_quick_search);
	put(_catalogue->_btnReview, g->detail_selected_item_action_review_production);
	put(_catalogue->_btnTechTree, g->detail_selected_item_action_tech_tree);
	put(_catalogue->_btnUfopaedia, g->detail_selected_item_action_ufopaedia);
	put(_catalogue->_btnMarkAllSeen, g->action_mark_all_seen); put(_catalogue->_btnOk, g->action_done);
	configureHdList(*_catalogue->_lstManufacture, g->collectionViewport,
		g->collection_row_slot_1, g->collection_scroll_track, generated, sx, sy);
}

void CalypsoF10ProductionUi::applyRequirementsGeometry()
{
	if (!_requirements || !_requirements->_window) return;
	const bool wide = _requirements->_hdWideLayout;
	const auto *g = CalypsoF10ProductionRequirementsGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) return;
	const auto &generated = wide
		? CalypsoF10ProductionRequirementsGen::kCollectionsWide[0]
		: CalypsoF10ProductionRequirementsGen::kCollectionsCompact[0];
	setOperationsWindow(_requirements->_window, g->window);
	const int wx = _requirements->_window->getX(), wy = _requirements->_window->getY();
	const double sx = static_cast<double>(_requirements->_window->getWidth()) / g->window.w;
	const double sy = static_cast<double>(_requirements->_window->getHeight()) / g->window.h;
	_requirements->_lstRequiredItems->rebaseNativeSize(
		g->region_requirements_collection.w, g->region_requirements_collection.h);
	auto put = [&](Surface *s, const auto &r) {
		place(s, r, wx, wy, sx, sy, g->window.x, g->window.y);
	};
	put(_requirements->_txtTitle, g->title);
	put(_requirements->_txtManHour, g->region_facts_field_hours_value);
	put(_requirements->_txtCost, g->region_facts_field_cost_value);
	put(_requirements->_txtWorkSpace, g->region_context_content);
	put(_requirements->_txtRequiredItemsTitle, g->region_requirements_label);
	put(_requirements->_txtItemNameColumn, g->region_requirements_collection_column_item);
	put(_requirements->_txtUnitRequiredColumn, g->region_requirements_collection_column_required);
	put(_requirements->_txtUnitAvailableColumn, g->region_requirements_collection_column_available);
	put(_requirements->_lstRequiredItems, g->region_requirements_collection);
	put(_requirements->_btnCancel, g->action_cancel);
	put(_requirements->_btnStart, g->action_start_production);
	configureHdList(*_requirements->_lstRequiredItems, g->region_requirements_collection,
		g->region_requirements_collection_row_slot_1,
		g->region_requirements_collection_scroll_track, generated, sx, sy);
}

void CalypsoF10ProductionUi::applyControlsGeometry()
{
	if (!_controls || !_controls->_window) return;
	const bool wide = _controls->_hdWideLayout;
	const auto *g = CalypsoF10ProductionControlsGen::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) return;
	// Four simultaneously-visible commands each own one slot — they must never
	// share a rect (external review R05): sell, fallback, infinity, minimum.
	const auto &sellGroup = wide
		? CalypsoF10ProductionControlsGen::kStrictActionSlotGroupsWide[0]
		: CalypsoF10ProductionControlsGen::kStrictActionSlotGroupsCompact[0];
	if (sellGroup.slotCount < 4) return;
	setOperationsWindow(_controls->_window, g->window);
	const int wx = _controls->_window->getX(), wy = _controls->_window->getY();
	const double sx = static_cast<double>(_controls->_window->getWidth()) / g->window.w;
	const double sy = static_cast<double>(_controls->_window->getHeight()) / g->window.h;
	auto put = [&](Surface *s, const auto &r) { place(s, r, wx, wy, sx, sy, g->window.x, g->window.y); };
	put(_controls->_txtTitle, g->title); put(_controls->_txtAvailableEngineer, g->region_resources_field_available_engineers_value);
	put(_controls->_txtAvailableSpace, g->region_resources_field_workshop_space_value);
	put(_controls->_txtHoursPerUnit, g->region_resources_field_hours_per_unit_value);
	put(_controls->_txtMonthlyProfit, g->region_result_field_monthly_profit_value);
	put(_controls->_txtAllocatedEngineer, g->region_result_label); put(_controls->_txtAllocated, g->control_engineers_value);
	put(_controls->_txtUnitToProduce, g->region_output_label); put(_controls->_txtTodo, g->control_units_value);
	put(_controls->_txtEngineerUp, g->control_engineers_increment); put(_controls->_txtEngineerDown, g->control_engineers_decrement);
	put(_controls->_btnEngineerUp, g->control_engineers_increment); put(_controls->_btnEngineerDown, g->control_engineers_decrement);
	put(_controls->_txtUnitUp, g->control_units_increment); put(_controls->_txtUnitDown, g->control_units_decrement);
	put(_controls->_btnUnitUp, g->control_units_increment); put(_controls->_btnUnitDown, g->control_units_decrement);
	put(_controls->_btnSell, sellGroup.slots[0].rect);
	put(_controls->_btnFallback, sellGroup.slots[1].rect);
	put(_controls->_btnUnitInfinity, sellGroup.slots[2].rect);
	put(_controls->_btnUnitMinimum, sellGroup.slots[3].rect);
	put(_controls->_surfaceEngineers, g->control_engineers);
	put(_controls->_surfaceUnits, g->control_units);
	put(_controls->_btnStop, g->action_stop_production);
	put(_controls->_btnOk, g->action_ok);
}

void CalypsoF10ProductionUi::applyDependenciesGeometry()
{
	if (!_dependencies || !_dependencies->_window) return;
	const bool wide = _dependencies->_hdWideLayout;
	const auto *g = CalypsoF10ProductionDependenciesGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) return;
	const auto &generated = wide
		? CalypsoF10ProductionDependenciesGen::kCollectionsWide[0]
		: CalypsoF10ProductionDependenciesGen::kCollectionsCompact[0];
	setOperationsWindow(_dependencies->_window, g->window);
	const int wx = _dependencies->_window->getX(), wy = _dependencies->_window->getY();
	const double sx = static_cast<double>(_dependencies->_window->getWidth()) / g->window.w;
	const double sy = static_cast<double>(_dependencies->_window->getHeight()) / g->window.h;
	_dependencies->_lstTopics->rebaseNativeSize(
		g->region_tree_collection.w, g->region_tree_collection.h);
	auto put = [&](Surface *s, const auto &r) {
		place(s, r, wx, wy, sx, sy, g->window.x, g->window.y);
	};
	put(_dependencies->_txtTitle, g->title);
	put(_dependencies->_lstTopics, g->region_tree_collection);
	put(_dependencies->_btnShowAll, g->action_show_all);
	put(_dependencies->_btnOk, g->action_ok);
	configureHdList(*_dependencies->_lstTopics, g->region_tree_collection,
		g->region_tree_collection_row_slot_1,
		g->region_tree_collection_scroll_track, generated, sx, sy);
}

CalypsoHdOperationsModel CalypsoF10ProductionUi::buildQueueModel() const
{
	CalypsoHdOperationsModel model;
	if (!_queue || !_queue->_window || !_queue->_game || !_queue->_base
		|| !_queue->_lstManufacture || !_queue->_txtTitle
		|| !_queue->_btnNew || !_queue->_btnOk || !_queue->_btnGlobalOverview
		|| !_queue->_btnOpenProduction || !_queue->_btnTechTree
		|| !_queue->_game->getSavedGame()) return model;
	const auto tr = [this](const std::string &key) { return _queue->tr(key); };
	const bool wide = _queue->_hdWideLayout;
	const auto *g = CalypsoF10ProductionQueueGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) return model;
	const auto &generated = wide
		? CalypsoF10ProductionQueueGen::kCollectionsWide[0]
		: CalypsoF10ProductionQueueGen::kCollectionsCompact[0];
	auto p = [&](const auto &r) {
		return CalypsoHdOperationsRect{r.x, r.y, r.w, r.h};
	};
	model.archetype = CalypsoHdOperationsArchetype::OperationsWorkspace;
	setBaseCaption(model, model.baseName);
	model.familyId = 10;
	model.ownerState = _queue;
	model.visualShell = CalypsoF10ProductionQueueGen::kVisualShell;
	model.headerArtId = CalypsoF10ProductionQueueGen::kHeaderArt;
	model.baseName = _queue->_base->getName();
	model.sectionLabel = tr("STR_MANUFACTURE");
	model.title = _queue->_txtTitle->getText();
	model.suppressedWidgets = {
		_queue->_window, _queue->_btnNew, _queue->_btnOk,
		_queue->_txtTitle, _queue->_txtAvailable, _queue->_txtAllocated,
		_queue->_txtSpace, _queue->_txtFunds, _queue->_txtItem,
		_queue->_txtEngineers, _queue->_txtProduced, _queue->_txtCost,
		_queue->_txtTimeLeft, _queue->_lstManufacture,
		_queue->_btnGlobalOverview, _queue->_btnOpenProduction, _queue->_btnTechTree};
	model.geometry.designWidth = g->designWidth;
	model.geometry.designHeight = g->designHeight;
	model.geometry.window = p(g->window);
	model.geometry.title = p(g->title);
	model.geometry.screenHeader = p(g->screenHeader);
	model.geometry.headerArt = p(g->headerArt);
	model.geometry.summaryBar = p(g->summaryBar);
	model.geometry.toolbarBar = p(g->toolbarBar);
	model.geometry.collectionViewport = p(g->collectionViewport);
	model.geometry.detailPanel = p(g->detailPanel);
	model.geometry.footer = p(g->footer);
	model.geometry.collectionScrollTrack = p(g->collection_scroll_track);
	model.geometry.collectionScrollThumb = p(g->collection_scroll_thumb);
	model.geometry.collectionColumns = {
		p(g->collection_column_item), p(g->collection_column_engineers),
		p(g->collection_column_produced), p(g->collection_column_cost),
		p(g->collection_column_time)};
	setGeneratedCollectionRows(model, generated, p);
	model.geometry.detailIdentity = p(g->detail_selected_production_label);
	model.geometry.detailIdentityTitle = p(g->detail_selected_production_identity_title);
	model.geometry.detailIdentitySubtitle = p(g->detail_selected_production_identity_subtitle);
	model.geometry.detailMetrics = {
		p(g->detail_selected_production_metric_engineers),
		p(g->detail_selected_production_metric_produced),
		p(g->detail_selected_production_metric_unit_cost),
		p(g->detail_selected_production_metric_time_left)};
	model.geometry.detailActions = {
		p(g->detail_selected_production_action_open_production),
		p(g->detail_selected_production_action_tech_tree)};
	model.geometry.footerActions = {
		p(g->action_global_overview), p(g->action_new_production), p(g->action_done)};
	model.summaryFields.push_back(summary("available",
		tr("STR_CALYPSO_ENGINEERS_AVAILABLE"),
		std::to_string(_queue->_base->getAvailableEngineers()),
		p(g->summary_available), _queue->_txtAvailable));
	model.summaryFields.push_back(summary("allocated",
		tr("STR_CALYPSO_ENGINEERS_ALLOCATED"),
		std::to_string(_queue->_base->getAllocatedEngineers()),
		p(g->summary_allocated), _queue->_txtAllocated));
	model.summaryFields.push_back(summary("workshop-space",
		tr("STR_CALYPSO_WORKSHOP_SPACE"),
		std::to_string(_queue->_base->getFreeWorkshops()),
		p(g->summary_workshop_space), _queue->_txtSpace));
	const std::string columnLabels[] = {
		tr("STR_ITEM"), tr("STR_CALYPSO_ENGINEERS_ALLOCATED"),
		tr("STR_CALYPSO_PRODUCED"), tr("STR_CALYPSO_COST_PER_UNIT"),
		_queue->_txtTimeLeft->getText()};
	for (int i = 0; i < generated.columnCount; ++i)
		model.collection.columns.push_back({
			generated.columns[i].id, columnLabels[i],
			p(generated.columns[i].rect), {}, generated.columns[i].contentRole});
	const auto &productions = _queue->_base->getProductions();
	model.collection.heading = tr("STR_CALYPSO_PRODUCTION_LINES");
	model.collection.meta = tr("STR_CALYPSO_FUNDS_VALUE").arg(
		Unicode::formatFunding(_queue->_game->getSavedGame()->getFunds()));
	model.collection.emptyTitle = tr("STR_CALYPSO_NO_ACTIVE_PRODUCTION");
	model.collection.emptyBody = tr("STR_CALYPSO_START_PRODUCTION_PROMPT");
	const std::size_t nativeSelected =
		static_cast<std::size_t>(_queue->_lstManufacture->getSelectedRow());
	const std::size_t nativeOffset = _queue->_lstManufacture->getScroll();
	for (std::size_t i = 0; i < productions.size(); ++i)
	{
		const Production *prod = productions[i];
		const std::string produced = productionProgress(prod);
		const std::string time = productionTimeLeft(prod);
		CalypsoHdOperationsRow row;
		row.id = prod->getRules()->getName();
		row.values = {tr(prod->getRules()->getName()), std::to_string(prod->getAssignedEngineers()),
			produced, Unicode::formatFunding(prod->getRules()->getManufactureCost()), time};
		const std::size_t slot = generated.rowSlotCount == 0 ? 0
			: (i >= nativeOffset ? i - nativeOffset : 0) % generated.rowSlotCount;
		row.rect = generated.rowSlotCount == 0
			? model.geometry.collectionViewport : p(generated.rowSlots[slot].rect);
		for (std::size_t c = 0; c < row.values.size(); ++c)
			row.cells.push_back({row.values[c], generated.columns[c].contentRole, {}});
		row.state.selected = i == nativeSelected;
		row.widget = _queue->_lstManufacture;
		model.collection.rows.push_back(std::move(row));
	}
	model.collection.selectedIndex = nativeSelected;
	model.collection.scrollOffset = nativeOffset;
	model.collection.visibleRows = generated.rowSlotCount;
	model.collection.rowHeight = generated.rowSlotCount == 0
		? 0 : p(generated.rowSlots[0].rect).h;
	model.collection.rowSlots = model.geometry.collectionRows;
	model.collection.count = productions.size();
	model.collection.viewport = model.geometry.collectionViewport;
	model.collection.scroll = {
		nativeOffset, productions.size(), static_cast<std::size_t>(generated.rowSlotCount),
		model.geometry.collectionViewport, model.geometry.collectionScrollTrack,
		model.geometry.collectionScrollThumb};
	const bool selected = !productions.empty()
		&& model.collection.selectedIndex < productions.size();
	const Production *prod = selected ? productions[model.collection.selectedIndex] : nullptr;
	model.detail.id = "selected-production";
	model.detail.panel = p(g->detailPanel);
	model.detail.identity.id = "selected-production";
	model.detail.identity.label = tr("STR_CALYPSO_SELECTED_PRODUCTION");
	model.detail.identity.title = prod
		? std::string(tr(prod->getRules()->getName()))
		: std::string(tr("STR_NONE"));
	model.detail.identity.subtitle = prod
		? std::string(tr(prod->getRules()->getCategory()))
		: _queue->_btnNew->getText();
	model.detail.identity.rect = p(g->detail_selected_production);
	model.detail.identity.titleRect = p(g->detail_selected_production_identity_title);
	model.detail.identity.subtitleRect = p(g->detail_selected_production_identity_subtitle);
	if (prod)
	{
		model.detail.metrics.push_back(metric("engineers",
			tr("STR_CALYPSO_ENGINEERS_ALLOCATED"),
			std::to_string(prod->getAssignedEngineers()),
			p(g->detail_selected_production_metric_engineers)));
		model.detail.metrics.push_back(metric("produced",
			tr("STR_CALYPSO_PRODUCED"),
			productionProgress(prod),
			p(g->detail_selected_production_metric_produced)));
		model.detail.metrics.push_back(metric("unit-cost",
			tr("STR_CALYPSO_COST_PER_UNIT"),
			Unicode::formatFunding(prod->getRules()->getManufactureCost()),
			p(g->detail_selected_production_metric_unit_cost)));
		model.detail.metrics.push_back(metric("time-left",
			tr("STR_DAYS_HOURS_LEFT"),
			productionTimeLeft(prod),
			p(g->detail_selected_production_metric_time_left)));
	}
	model.detail.actions.push_back(action("open-production",
		_queue->_btnOpenProduction->getText(),
		p(g->detail_selected_production_action_open_production),
		_queue->_btnOpenProduction, selected));
	model.detail.actions.push_back(action("tech-tree", _queue->_btnTechTree->getText(),
		p(g->detail_selected_production_action_tech_tree), _queue->_btnTechTree,
		selected));
	model.footerActions.push_back(action("global-overview",
		_queue->_btnGlobalOverview->getText(),
		p(g->action_global_overview), _queue->_btnGlobalOverview, true, "safe"));
	auto newProduction = action("new-production", _queue->_btnNew->getText(),
		p(g->action_new_production), _queue->_btnNew, true, "primary");
	newProduction.state.selected = productions.empty();
	model.footerActions.push_back(std::move(newProduction));
	model.footerActions.push_back(action("done", _queue->_btnOk->getText(),
		p(g->action_done), _queue->_btnOk));
	finishModel(model, _queue->_game->getMod(),
		CalypsoF10ProductionQueueGen::kPresentationProfile,
		CalypsoF10ProductionQueueGen::kProfileId,
		CalypsoF10ProductionQueueGen::kProfileVersion,
		CalypsoF10ProductionQueueGen::kProvenanceTemplate,
		CalypsoF10ProductionQueueGen::kProfileStyle,
		CalypsoF10ProductionQueueGen::kTypographyWide, CalypsoF10ProductionQueueGen::kTypographyCompact,
		wide);
	return model;
}

CalypsoHdOperationsModel CalypsoF10ProductionUi::buildCatalogueModel() const
{
	CalypsoHdOperationsModel model;
	if (!_catalogue || !_catalogue->_window || !_catalogue->_game
		|| !_catalogue->_base || !_catalogue->_lstManufacture
		|| !_catalogue->_txtTitle || !_catalogue->_txtItem
		|| !_catalogue->_txtCategory || !_catalogue->_btnQuickSearch
		|| !_catalogue->_btnOk || !_catalogue->_btnShowOnlyNew
		|| !_catalogue->_btnReview || !_catalogue->_btnTechTree
		|| !_catalogue->_btnUfopaedia || !_catalogue->_btnMarkAllSeen
		|| !_catalogue->_cbxFilter || !_catalogue->_cbxCategory
		|| !_catalogue->_game->getSavedGame() || !_catalogue->_game->getMod())
		return model;
	const auto tr = [this](const std::string &key) { return _catalogue->tr(key); };
	const bool wide = _catalogue->_hdWideLayout;
	const auto *g = CalypsoF10ProductionCatalogueGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) return model;
	const auto &generated = wide
		? CalypsoF10ProductionCatalogueGen::kCollectionsWide[0]
		: CalypsoF10ProductionCatalogueGen::kCollectionsCompact[0];
	auto p = [&](const auto &r) {
		return CalypsoHdOperationsRect{r.x, r.y, r.w, r.h};
	};
	model.archetype = CalypsoHdOperationsArchetype::OperationsWorkspace;
	model.familyId = 10;
	model.ownerState = _catalogue;
	model.visualShell = CalypsoF10ProductionCatalogueGen::kVisualShell;
	model.headerArtId = CalypsoF10ProductionCatalogueGen::kHeaderArt;
	model.baseName = _catalogue->_base->getName();
	setBaseCaption(model, model.baseName);
	model.sectionLabel = tr("STR_MANUFACTURE");
	model.title = _catalogue->_txtTitle->getText();
	model.suppressedWidgets = {
		_catalogue->_window, _catalogue->_btnQuickSearch, _catalogue->_btnOk,
		_catalogue->_btnShowOnlyNew, _catalogue->_txtTitle, _catalogue->_txtItem,
		_catalogue->_txtCategory, _catalogue->_lstManufacture, _catalogue->_cbxFilter,
		_catalogue->_cbxCategory, _catalogue->_btnReview, _catalogue->_btnTechTree,
		_catalogue->_btnUfopaedia, _catalogue->_btnMarkAllSeen};
	model.geometry.designWidth = g->designWidth;
	model.geometry.designHeight = g->designHeight;
	model.geometry.window = p(g->window); model.geometry.title = p(g->title); model.geometry.summaryBar = p(g->summaryBar);
	model.geometry.screenHeader = p(g->screenHeader);
	model.geometry.headerArt = p(g->headerArt);
	model.geometry.toolbarBar = p(g->toolbarBar); model.geometry.collectionViewport = p(g->collectionViewport); model.geometry.detailPanel = p(g->detailPanel); model.geometry.footer = p(g->footer);
	setGeneratedCollectionRows(model, generated, p);
	model.geometry.detailIdentity = p(g->detail_selected_item_label); model.geometry.detailIdentityTitle = p(g->detail_selected_item_identity_title); model.geometry.detailIdentitySubtitle = p(g->detail_selected_item_identity_subtitle);
	model.geometry.detailMetrics = {p(g->detail_selected_item_metric_category)};
	model.geometry.detailActions = {p(g->detail_selected_item_action_review_production),p(g->detail_selected_item_action_tech_tree),p(g->detail_selected_item_action_ufopaedia)};
	model.geometry.footerActions = {p(g->action_mark_all_seen),p(g->action_done)};
	const std::string searchQuery = _catalogue->_btnQuickSearch->getText();
	const bool filtered = !searchQuery.empty() || _catalogue->_cbxFilter->getSelected() != 0
		|| _catalogue->_cbxCategory->getSelected() != 0
		|| _catalogue->_btnShowOnlyNew->getPressed();
	model.collection.emptyKind = filtered ? "no-match" : "empty";
	const auto &items = _catalogue->_displayedStrings;
	const std::string columnLabels[] = {
		_catalogue->_txtItem->getText(),
		_catalogue->_txtCategory->getText(), tr("STR_STATUS")};
	for (int i = 0; i < generated.columnCount; ++i)
		model.collection.columns.push_back({
			generated.columns[i].id, columnLabels[i],
			p(generated.columns[i].rect), {}, generated.columns[i].contentRole});
	model.collection.emptyTitle = tr("STR_CALYPSO_NO_AVAILABLE_PRODUCTION");
	model.collection.emptyBody = tr("STR_CALYPSO_NO_AVAILABLE_PRODUCTION_PROMPT");
	const auto &nativeRows = _catalogue->_lstManufacture->getCellTextsSnapshot();
	const std::size_t nativeCount = nativeRows.size();
	const std::size_t nativeSelected =
		static_cast<std::size_t>(_catalogue->_lstManufacture->getSelectedRow());
	const std::size_t nativeOffset = _catalogue->_lstManufacture->getScroll();
	for (std::size_t i = 0; i < nativeCount; ++i)
	{
		CalypsoHdOperationsRow row;
		row.id = i < _catalogue->_displayedStrings.size()
			? _catalogue->_displayedStrings[i] : std::to_string(i);
		for (std::size_t c = 0; c < generated.columnCount; ++c)
			row.values.push_back(c < nativeRows[i].size() && nativeRows[i][c]
				? nativeRows[i][c]->getText() : std::string());
		const std::size_t slot = generated.rowSlotCount == 0 ? 0
			: (i >= nativeOffset ? i - nativeOffset : 0) % generated.rowSlotCount;
		row.rect = generated.rowSlotCount == 0
			? model.geometry.collectionViewport : p(generated.rowSlots[slot].rect);
		for (std::size_t c = 0; c < row.values.size(); ++c)
			row.cells.push_back({row.values[c], generated.columns[c].contentRole, {}});
		row.state.selected = i == nativeSelected;
		row.widget = _catalogue->_lstManufacture;
		model.collection.rows.push_back(std::move(row));
	}
	model.collection.selectedIndex = nativeSelected;
	model.collection.scrollOffset = nativeOffset;
	model.collection.visibleRows = generated.rowSlotCount;
	model.collection.rowHeight = generated.rowSlotCount == 0
		? 0 : p(generated.rowSlots[0].rect).h;
	model.collection.rowSlots = model.geometry.collectionRows;
	model.collection.count = nativeCount;
	model.collection.viewport = model.geometry.collectionViewport;
	model.collection.scroll = {
		nativeOffset, nativeCount, static_cast<std::size_t>(generated.rowSlotCount),
		model.geometry.collectionViewport, model.geometry.collectionScrollTrack,
		model.geometry.collectionScrollThumb};
	const bool selected = nativeSelected < nativeCount
		&& nativeSelected < items.size();
	const RuleManufacture *rule = selected
		? _catalogue->_game->getMod()->getManufacture(items[nativeSelected]) : nullptr;
	model.detail.id = "selected-item";
	model.detail.panel = p(g->detailPanel);
	model.detail.identity.id = "selected-item";
	model.detail.identity.label = tr("STR_CALYPSO_SELECTED_ITEM");
	model.detail.identity.title = rule ? tr(rule->getName()) : tr("STR_NONE");
	model.detail.identity.subtitle = rule ? tr(rule->getCategory()) : tr("STR_NONE");
	model.detail.identity.rect = p(g->detail_selected_item);
	model.detail.identity.titleRect = p(g->detail_selected_item_identity_title);
	model.detail.identity.subtitleRect = p(g->detail_selected_item_identity_subtitle);
	if (rule)
		model.detail.metrics.push_back(metric("category", _catalogue->_txtCategory->getText(),
			tr(rule->getCategory()), p(g->detail_selected_item_metric_category)));
	model.detail.actions.push_back(action("review-production", tr("STR_REVIEW_PRODUCTION"),
		p(g->detail_selected_item_action_review_production), _catalogue->_btnReview,
		selected, "primary"));
	model.detail.actions.push_back(action("tech-tree", tr("STR_TECH_TREE"),
		p(g->detail_selected_item_action_tech_tree), _catalogue->_btnTechTree, selected));
	model.detail.actions.push_back(action("ufopaedia", tr("STR_UFOPAEDIA"),
		p(g->detail_selected_item_action_ufopaedia), _catalogue->_btnUfopaedia, selected));
	static constexpr const char *filterKeys[] = {
		"STR_FILTER_DEFAULT", "STR_FILTER_DEFAULT_SUPPLIES_OK",
		"STR_FILTER_DEFAULT_NO_SUPPLIES", "STR_FILTER_FACILITY_REQUIRED",
		"STR_FILTER_HIDDEN"};
	const std::size_t filterIndex = _catalogue->_cbxFilter->getSelected();
	const std::string filterLabel = tr(filterKeys[
		std::min(filterIndex, sizeof(filterKeys) / sizeof(filterKeys[0]) - 1)]);
	const std::size_t categoryIndex = _catalogue->_cbxCategory->getSelected();
	const std::string categoryLabel = categoryIndex < _catalogue->_catStrings.size()
		? tr(_catalogue->_catStrings[categoryIndex]) : tr("STR_ALL_ITEMS");
	model.toolbarActions.push_back(action("availability-filter", filterLabel,
		p(g->toolbar_availability_default), _catalogue->_cbxFilter));
	model.toolbarActions.push_back(action("category-filter", categoryLabel,
		p(g->toolbar_category_all), _catalogue->_cbxCategory));
	auto showOnlyNew = action("show-only-new",
		_catalogue->_btnShowOnlyNew->getText(), p(g->toolbar_show_only_new),
		_catalogue->_btnShowOnlyNew, _catalogue->_btnShowOnlyNew->getVisible());
	showOnlyNew.state.selected = _catalogue->_btnShowOnlyNew->getPressed();
	model.toolbarActions.push_back(std::move(showOnlyNew));
	const std::string query = _catalogue->_btnQuickSearch->getText();
	model.toolbarActions.push_back(action("quick-search",
		query.empty() ? std::string(tr("STR_TOGGLE_QUICK_SEARCH")) : query,
		p(g->toolbar_quick_search), _catalogue->_btnQuickSearch,
		_catalogue->_btnQuickSearch->getVisible()));
	model.footerActions.push_back(action("mark-all-seen", tr("STR_MARK_ALL_AS_SEEN"), p(g->action_mark_all_seen), _catalogue->_btnMarkAllSeen, true, "safe"));
	model.footerActions.push_back(action("done", tr("STR_DONE"),
		p(g->action_done), _catalogue->_btnOk));
	finishModel(model, _catalogue->_game->getMod(),
		CalypsoF10ProductionCatalogueGen::kPresentationProfile,
		CalypsoF10ProductionCatalogueGen::kProfileId,
		CalypsoF10ProductionCatalogueGen::kProfileVersion,
		CalypsoF10ProductionCatalogueGen::kProvenanceTemplate,
		CalypsoF10ProductionCatalogueGen::kProfileStyle,
		CalypsoF10ProductionCatalogueGen::kTypographyWide, CalypsoF10ProductionCatalogueGen::kTypographyCompact,
		wide);
	return model;
}

CalypsoHdOperationsModel CalypsoF10ProductionUi::buildRequirementsModel() const
{
	CalypsoHdOperationsModel model;
	if (!_requirements || !_requirements->_window || !_requirements->_game
		|| !_requirements->_item || !_requirements->_lstRequiredItems
		|| !_requirements->_txtTitle || !_requirements->_txtManHour
		|| !_requirements->_txtCost || !_requirements->_txtWorkSpace
		|| !_requirements->_txtRequiredItemsTitle
		|| !_requirements->_txtItemNameColumn
		|| !_requirements->_txtUnitRequiredColumn
		|| !_requirements->_txtUnitAvailableColumn
		|| !_requirements->_btnCancel || !_requirements->_btnStart
		|| !_requirements->_game->getMod()) return model;
	const auto tr = [this](const std::string &key) { return _requirements->tr(key); };
	const bool wide = _requirements->_hdWideLayout;
	const auto *g = CalypsoF10ProductionRequirementsGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) return model;
	const auto &generated = wide
		? CalypsoF10ProductionRequirementsGen::kCollectionsWide[0]
		: CalypsoF10ProductionRequirementsGen::kCollectionsCompact[0];
	auto p = [&](const auto &r) {
		return CalypsoHdOperationsRect{r.x, r.y, r.w, r.h};
	};
	model.archetype = CalypsoHdOperationsArchetype::WideDetail;
	model.familyId = 10;
	model.ownerState = _requirements;
	model.visualShell = CalypsoF10ProductionRequirementsGen::kVisualShell;
	model.headerArtId = CalypsoF10ProductionRequirementsGen::kHeaderArt;
	model.title = _requirements->_txtTitle->getText();
	model.suppressedWidgets = {
		_requirements->_window, _requirements->_btnCancel, _requirements->_btnStart,
		_requirements->_txtTitle, _requirements->_txtManHour, _requirements->_txtCost,
		_requirements->_txtWorkSpace, _requirements->_txtRequiredItemsTitle,
		_requirements->_txtItemNameColumn, _requirements->_txtUnitRequiredColumn,
		_requirements->_txtUnitAvailableColumn, _requirements->_lstRequiredItems};
	model.geometry.designWidth = g->designWidth;
	model.geometry.designHeight = g->designHeight;
	model.geometry.window = p(g->window);
	model.geometry.status = p(g->status);
	model.geometry.title = p(g->title);
	model.geometry.screenHeader = p(g->status);
	model.geometry.headerArt = p(g->headerArt);
	model.geometry.controlBar = p(g->controlBar);
	model.geometry.footer = p(g->footer);
	CalypsoHdOperationsRegion requirements;
	setBaseCaption(model, _requirements->_base ? _requirements->_base->getName()
		: std::string());
	requirements.id = "requirements";
	requirements.label = _requirements->_txtRequiredItemsTitle->getText();
	requirements.kind = CalypsoHdOperationsRegionKind::Collection;
	requirements.rect = p(g->region_requirements);
	requirements.labelRect = p(g->region_requirements_label);
	requirements.collection.viewport = p(g->region_requirements_collection);
	requirements.collection.scrollTrack = p(g->region_requirements_collection_scroll_track);
	requirements.collection.scrollThumb = p(g->region_requirements_collection_scroll_thumb);
	requirements.collection.emptyTitle = _requirements->_txtRequiredItemsTitle->getText();
	requirements.collection.emptyBody = tr("STR_NONE");
	for (int c = 0; c < generated.columnCount; ++c)
	{
		const std::string label = c == 0 ? _requirements->_txtItemNameColumn->getText()
			: c == 1 ? _requirements->_txtUnitRequiredColumn->getText()
			: _requirements->_txtUnitAvailableColumn->getText();
		requirements.collection.columns.push_back({
			generated.columns[c].id, label, p(generated.columns[c].rect), {},
			generated.columns[c].contentRole});
	}
	for (int i = 0; i < generated.rowSlotCount; ++i)
		requirements.collection.rowSlots.push_back(p(generated.rowSlots[i].rect));
	const auto &requiredRows = _requirements->_lstRequiredItems->getCellTextsSnapshot();
	const std::size_t nativeSelected =
		static_cast<std::size_t>(_requirements->_lstRequiredItems->getSelectedRow());
	const std::size_t nativeOffset = _requirements->_lstRequiredItems->getScroll();
	for (std::size_t i = 0; i < requiredRows.size(); ++i)
	{
		CalypsoHdOperationsRow row;
		const std::size_t nativeColumns = requiredRows[i].size();
		const char *rowKind = nativeColumns >= 3 ? "requirement"
			: nativeColumns == 2 ? "output" : "separator";
		row.id = std::string(rowKind) + "-" + std::to_string(i);
		for (std::size_t c = 0; c < generated.columnCount; ++c)
			row.values.push_back(c < requiredRows[i].size() && requiredRows[i][c]
				? requiredRows[i][c]->getText() : std::string());
		const std::size_t slot = generated.rowSlotCount == 0 ? 0
			: (i >= nativeOffset ? i - nativeOffset : 0) % generated.rowSlotCount;
		row.rect = generated.rowSlotCount == 0
			? requirements.collection.viewport
			: requirements.collection.rowSlots[slot];
		for (std::size_t c = 0; c < row.values.size(); ++c)
			row.cells.push_back({row.values[c], generated.columns[c].contentRole, {}});
		row.state.selected = i == nativeSelected;
		row.widget = _requirements->_lstRequiredItems;
		requirements.collection.rows.push_back(std::move(row));
	}
	requirements.collection.visibleRows = generated.rowSlotCount;
	requirements.collection.rowHeight = generated.rowSlotCount == 0
		? 0 : requirements.collection.rowSlots.front().h;
	requirements.collection.selectedIndex = nativeSelected;
	requirements.collection.scrollOffset = nativeOffset;
	requirements.collection.count = requiredRows.size();
	requirements.collection.scroll = {
		nativeOffset, requiredRows.size(), static_cast<std::size_t>(generated.rowSlotCount),
		requirements.collection.viewport, requirements.collection.scrollTrack,
		requirements.collection.scrollThumb};
	model.regions.push_back(std::move(requirements));
	CalypsoHdOperationsRegion context;
	context.id = "context";
	context.label = tr("STR_CALYPSO_SELECTED_ITEM");
	context.kind = CalypsoHdOperationsRegionKind::Preview;
	context.rect = p(g->region_context);
	context.labelRect = p(g->region_context_label);
	context.previewRect = p(g->region_context_content);
	context.previewContent = tr(_requirements->_item->getName());
	model.regions.push_back(std::move(context));
	CalypsoHdOperationsRegion facts;
	facts.id = "facts";
	facts.label = tr("STR_CALYPSO_SELECTED_ITEM");
	facts.labelRect = p(g->region_facts_label);
	facts.fields.push_back(metric("hours", tr("STR_CALYPSO_ENGINEER_HOURS"),
		std::to_string(_requirements->_item->getManufactureTime()),
		p(g->region_facts_field_hours_value)));
	facts.fields.push_back(metric("cost", tr("STR_CALYPSO_COST_PER_UNIT"),
		Unicode::formatFunding(_requirements->_item->getManufactureCost()),
		p(g->region_facts_field_cost_value)));
	facts.fields.push_back(metric("space", tr("STR_CALYPSO_REQUIRED_WORKSPACE"),
		std::to_string(_requirements->_item->getRequiredSpace()),
		p(g->region_facts_field_items_value)));
	model.regions.push_back(std::move(facts));
	model.geometry.footerActions = {p(g->action_cancel), p(g->action_start_production)};
	model.footerActions.push_back(action("cancel", _requirements->_btnCancel->getText(),
		p(g->action_cancel), _requirements->_btnCancel));
	model.footerActions.push_back(action("start-production", _requirements->_btnStart->getText(),
		p(g->action_start_production), _requirements->_btnStart,
		_requirements->_btnStart->getVisible(), "primary"));
	finishModel(model, _requirements->_game->getMod(),
		CalypsoF10ProductionRequirementsGen::kPresentationProfile,
		CalypsoF10ProductionRequirementsGen::kProfileId,
		CalypsoF10ProductionRequirementsGen::kProfileVersion,
		CalypsoF10ProductionRequirementsGen::kProvenanceTemplate,
		CalypsoF10ProductionRequirementsGen::kProfileStyle,
		CalypsoF10ProductionRequirementsGen::kTypographyWide, CalypsoF10ProductionRequirementsGen::kTypographyCompact,
		wide);
	return model;
}

CalypsoHdOperationsModel CalypsoF10ProductionUi::buildControlsModel() const
{
	CalypsoHdOperationsModel model;
	if (!_controls || !_controls->_window || !_controls->_game
		|| !_controls->_base || !_controls->_production
		|| !_controls->_txtTitle || !_controls->_txtAvailableEngineer
		|| !_controls->_txtAvailableSpace || !_controls->_txtHoursPerUnit
		|| !_controls->_txtMonthlyProfit || !_controls->_txtAllocatedEngineer
		|| !_controls->_txtUnitToProduce || !_controls->_txtEngineerUp
		|| !_controls->_txtEngineerDown || !_controls->_txtUnitUp
		|| !_controls->_txtUnitDown || !_controls->_txtAllocated
		|| !_controls->_txtTodo || !_controls->_btnEngineerUp
		|| !_controls->_btnEngineerDown || !_controls->_btnUnitInfinity
		|| !_controls->_btnUnitMinimum || !_controls->_btnUnitUp
		|| !_controls->_btnUnitDown || !_controls->_btnStop || !_controls->_btnOk
		|| !_controls->_btnSell || !_controls->_btnFallback
		|| !_controls->_surfaceEngineers || !_controls->_surfaceUnits
		|| !_controls->_game->getMod() || !_controls->_production->getRules())
		return model;
	const auto tr = [this](const std::string &key) { return _controls->tr(key); };
	const bool wide = _controls->_hdWideLayout;
	const auto *g = CalypsoF10ProductionControlsGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) return model;
	// Four simultaneously-visible commands each own one slot — they must never
	// share a rect (external review R05): sell, fallback, infinity, minimum.
	const auto &sellGroup = wide
		? CalypsoF10ProductionControlsGen::kStrictActionSlotGroupsWide[0]
		: CalypsoF10ProductionControlsGen::kStrictActionSlotGroupsCompact[0];
	if (sellGroup.slotCount < 4) return model;
	auto p = [&](const auto &r) {
		return CalypsoHdOperationsRect{r.x, r.y, r.w, r.h};
	};
	model.archetype = CalypsoHdOperationsArchetype::WideDetail;
	model.familyId = 10;
	model.ownerState = _controls;
	model.visualShell = CalypsoF10ProductionControlsGen::kVisualShell;
	model.headerArtId = CalypsoF10ProductionControlsGen::kHeaderArt;
	model.title = _controls->_txtTitle->getText();
	model.suppressedWidgets = {
		_controls->_window, _controls->_txtTitle, _controls->_txtAvailableEngineer,
		_controls->_txtAvailableSpace, _controls->_txtHoursPerUnit,
		_controls->_txtMonthlyProfit, _controls->_txtAllocatedEngineer,
		_controls->_txtUnitToProduce, _controls->_txtEngineerUp,
		_controls->_txtEngineerDown, _controls->_txtUnitUp, _controls->_txtUnitDown,
		_controls->_txtAllocated, _controls->_txtTodo, _controls->_btnEngineerUp,
		_controls->_btnEngineerDown, _controls->_btnUnitInfinity,
		_controls->_btnUnitMinimum, _controls->_btnUnitUp, _controls->_btnUnitDown,
		_controls->_btnStop, _controls->_btnOk, _controls->_btnSell,
		_controls->_btnFallback, _controls->_surfaceEngineers, _controls->_surfaceUnits};
	setBaseCaption(model, _controls->_base ? _controls->_base->getName()
		: std::string());
	model.baseName = model.baseCaption;
	model.geometry.designWidth = g->designWidth;
	model.geometry.designHeight = g->designHeight;
	model.geometry.window = p(g->window);
	model.geometry.status = p(g->status);
	model.geometry.title = p(g->title);
	model.geometry.screenHeader = p(g->status);
	model.geometry.headerArt = p(g->headerArt);
	model.geometry.controlBar = p(g->controlBar);
	model.geometry.footer = p(g->footer);
	model.geometry.detailPanel = p(g->region_result);
	model.geometry.detailIdentity = p(g->region_result_label);
	model.geometry.detailIdentityTitle = p(g->region_result_field_produced_label);
	model.geometry.detailIdentitySubtitle = p(g->region_result_field_produced_value);
	model.geometry.detailMetrics = {
		p(g->region_result_field_produced_value),
		p(g->region_result_field_assigned_value),
		p(g->region_result_field_monthly_profit_value)};
	model.geometry.footerActions = {p(g->action_stop_production), p(g->action_ok)};

	CalypsoHdOperationsControl engineers;
	engineers.id = "engineers";
	engineers.label = tr("STR_CALYPSO_ENGINEERS_ALLOCATED");
	engineers.kind = CalypsoHdOperationsControlKind::Stepper;
	engineers.displayValue = std::to_string(_controls->_production->getAssignedEngineers());
	engineers.rect = p(g->control_engineers);
	engineers.valueRect = p(g->control_engineers_value);
	engineers.labelRect = p(g->control_engineers_label);
	engineers.widget = _controls->_surfaceEngineers;
	engineers.decrement = action("engineers-decrement", "−",
		p(g->control_engineers_decrement), _controls->_btnEngineerDown);
	engineers.increment = action("engineers-increment", "+",
		p(g->control_engineers_increment), _controls->_btnEngineerUp);
	model.detail.id = "selected-production";
	model.detail.panel = p(g->region_output);
	model.detail.identity.id = "selected-production";
	model.detail.identity.label = tr("STR_CALYPSO_SELECTED_PRODUCTION");
	model.detail.identity.title = tr(_controls->_production->getRules()->getName());
	model.detail.identity.subtitle = tr(_controls->_production->getRules()->getCategory());
	model.detail.identity.rect = p(g->region_output);
	model.detail.identity.titleRect = p(g->region_output_content);
	model.detail.identity.subtitleRect = p(g->region_output_label);
	model.controls.push_back(std::move(engineers));

	CalypsoHdOperationsControl units;
	units.id = "units";
	units.label = tr("STR_UNITS_TO_PRODUCE");
	units.kind = CalypsoHdOperationsControlKind::Stepper;
	units.displayValue = _controls->_production->getInfiniteAmount()
		? "∞" : std::to_string(_controls->_production->getAmountTotal());
	units.rect = p(g->control_units);
	units.valueRect = p(g->control_units_value);
	units.labelRect = p(g->control_units_label);
	units.widget = _controls->_surfaceUnits;
	units.decrement = action("units-decrement", "−",
		p(g->control_units_decrement), _controls->_btnUnitDown);
	units.increment = action("units-increment", "+",
		p(g->control_units_increment), _controls->_btnUnitUp);
	model.controls.push_back(std::move(units));

	CalypsoHdOperationsRegion output;
	output.id = "output";
	output.label = tr("STR_UNITS_TO_PRODUCE");
	output.kind = CalypsoHdOperationsRegionKind::Preview;
	output.rect = p(g->region_output);
	output.labelRect = p(g->region_output_label);
	output.previewRect = p(g->region_output_content);
	output.previewContent = tr(_controls->_production->getRules()->getName());
	model.regions.push_back(std::move(output));

	CalypsoHdOperationsRegion result;
	result.id = "result";
	result.label = tr("STR_CALYPSO_SELECTED_PRODUCTION");
	result.rect = p(g->region_result);
	result.labelRect = p(g->region_result_label);
	result.fields.push_back(metric("produced", tr("STR_CALYPSO_PRODUCED"),
		productionProgress(_controls->_production),
		p(g->region_result_field_produced_value)));
	result.fields.push_back(metric("assigned", tr("STR_CALYPSO_ENGINEERS_ALLOCATED"),
		std::to_string(_controls->_production->getAssignedEngineers()),
		p(g->region_result_field_assigned_value)));
	result.fields.push_back(metric("monthly-profit", tr("STR_CALYPSO_MONTHLY_PROFIT"),
		Unicode::formatFunding(_controls->getMonthlyNetFunds()),
		p(g->region_result_field_monthly_profit_value)));
	model.regions.push_back(std::move(result));

	CalypsoHdOperationsRegion resources;
	resources.id = "resources";
	resources.label = tr("STR_CALYPSO_RESOURCES");
	resources.kind = CalypsoHdOperationsRegionKind::Fields;
	resources.rect = p(g->region_resources);
	resources.labelRect = p(g->region_resources_label);
	resources.fields.push_back(metric("available-engineers",
		tr("STR_CALYPSO_ENGINEERS_AVAILABLE"),
		std::to_string(_controls->_base->getAvailableEngineers()),
		p(g->region_resources_field_available_engineers_value)));
	resources.fields.push_back(metric("workshop-space",
		tr("STR_CALYPSO_WORKSHOP_SPACE"),
		std::to_string(_controls->_base->getFreeWorkshops()),
		p(g->region_resources_field_workshop_space_value)));
	resources.fields.push_back(metric("hours-per-unit",
		tr("STR_CALYPSO_ENGINEER_HOURS"),
		std::to_string(_controls->_production->getRules()->getManufactureTime()),
		p(g->region_resources_field_hours_per_unit_value)));
	model.regions.push_back(std::move(resources));

	CalypsoHdOperationsRegion sell;
	sell.id = "sell";
	sell.label = tr("STR_SELL_PRODUCTION");
	sell.kind = CalypsoHdOperationsRegionKind::Actions;
	sell.rect = p(g->region_sell);
	sell.labelRect = p(g->region_sell_label);
	sell.collection.scrollTrack = p(g->region_sell_scroll_track);
	sell.collection.scrollThumb = p(g->region_sell_scroll_thumb);
	auto sellAction = action("sell-production",
		_controls->_btnSell->getText() + ": "
			+ std::string(tr(_controls->_btnSell->getPressed() ? "STR_YES" : "STR_NO")),
		p(sellGroup.slots[0].rect), _controls->_btnSell,
		_controls->_btnSell->getVisible());
	sellAction.state.selected = _controls->_btnSell->getPressed();
	sell.actions.push_back(std::move(sellAction));
	auto fallbackAction = action("fallback-production",
		_controls->_btnFallback->getText() + ": "
			+ std::string(tr(_controls->_btnFallback->getPressed() ? "STR_YES" : "STR_NO")),
		p(sellGroup.slots[1].rect), _controls->_btnFallback,
		_controls->_btnFallback->getVisible());
	fallbackAction.state.selected = _controls->_btnFallback->getPressed();
	sell.actions.push_back(std::move(fallbackAction));
	sell.actions.push_back(action("infinity", tr("STR_CALYPSO_INFINITY"),
		p(sellGroup.slots[2].rect),
		_controls->_btnUnitInfinity,
		_controls->_btnUnitInfinity->getVisible()));
	sell.actions.push_back(action("minimum", tr("STR_CALYPSO_MINIMUM"),
		p(sellGroup.slots[3].rect),
		_controls->_btnUnitMinimum,
		_controls->_btnUnitMinimum->getVisible()));
	model.regions.push_back(std::move(sell));

	model.footerActions.push_back(action("stop-production", _controls->_btnStop->getText(),
		p(g->action_stop_production), _controls->_btnStop, true, "danger"));
	model.footerActions.push_back(action("ok", _controls->_btnOk->getText(),
		p(g->action_ok), _controls->_btnOk, true, "primary"));
	finishModel(model, _controls->_game->getMod(),
		CalypsoF10ProductionControlsGen::kPresentationProfile,
		CalypsoF10ProductionControlsGen::kProfileId,
		CalypsoF10ProductionControlsGen::kProfileVersion,
		CalypsoF10ProductionControlsGen::kProvenanceTemplate,
		CalypsoF10ProductionControlsGen::kProfileStyle,
		CalypsoF10ProductionControlsGen::kTypographyWide, CalypsoF10ProductionControlsGen::kTypographyCompact,
		wide);
	return model;
}

CalypsoHdOperationsModel CalypsoF10ProductionUi::buildDependenciesModel() const
{
	CalypsoHdOperationsModel model;
	if (!_dependencies || !_dependencies->_window || !_dependencies->_game
		|| !_dependencies->_txtTitle || !_dependencies->_lstTopics
		|| !_dependencies->_btnShowAll || !_dependencies->_btnOk
		|| !_dependencies->_game->getMod()) return model;
	const auto tr = [this](const std::string &key) { return _dependencies->tr(key); };
	const bool wide = _dependencies->_hdWideLayout;
	const auto *g = CalypsoF10ProductionDependenciesGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) return model;
	const auto &generated = wide
		? CalypsoF10ProductionDependenciesGen::kCollectionsWide[0]
		: CalypsoF10ProductionDependenciesGen::kCollectionsCompact[0];
	auto p = [&](const auto &r) {
		return CalypsoHdOperationsRect{r.x, r.y, r.w, r.h};
	};
	model.archetype = CalypsoHdOperationsArchetype::WideDetail;
	model.familyId = 10;
	model.ownerState = _dependencies;
	model.visualShell = CalypsoF10ProductionDependenciesGen::kVisualShell;
	model.headerArtId = CalypsoF10ProductionDependenciesGen::kHeaderArt;
	model.title = _dependencies->_txtTitle->getText();
	model.geometry.designWidth = g->designWidth;
	model.geometry.designHeight = g->designHeight;
	model.geometry.window = p(g->window);
	model.geometry.status = p(g->status);
	model.geometry.title = p(g->title);
	model.geometry.screenHeader = p(g->status);
	setBaseCaption(model, "BASES");
	model.geometry.headerArt = p(g->headerArt);
	model.geometry.controlBar = p(g->controlBar);
	model.geometry.footer = p(g->footer);
	model.suppressedWidgets = {
		_dependencies->_window, _dependencies->_txtTitle, _dependencies->_lstTopics,
		_dependencies->_btnShowAll, _dependencies->_btnOk};

	CalypsoHdOperationsRegion tree;
	tree.id = "tree";
	tree.label = tr("STR_TOPIC");
	tree.kind = CalypsoHdOperationsRegionKind::Collection;
	tree.rect = p(g->region_tree);
	tree.collection.emptyTitle = tr("STR_NO_DEPENDENCIES");
	tree.collection.emptyBody = tr("STR_NONE");
	tree.labelRect = p(g->region_tree_label);
	tree.collection.viewport = p(g->region_tree_collection);
	tree.collection.scrollTrack = p(g->region_tree_collection_scroll_track);
	tree.collection.scrollThumb = p(g->region_tree_collection_scroll_thumb);
	for (int c = 0; c < generated.columnCount; ++c)
	{
		tree.collection.columns.push_back({
			generated.columns[c].id, tr("STR_TOPIC"),
			p(generated.columns[c].rect), {}, generated.columns[c].contentRole});
	}
	for (int i = 0; i < generated.rowSlotCount; ++i)
		tree.collection.rowSlots.push_back(p(generated.rowSlots[i].rect));
	// Typed presentation metadata travels with the native rows (R08): the
	// adapter never re-derives structure from translated text.
	const auto &presentationRows = _dependencies->calypsoPresentationRows();
	const std::size_t nativeSelected =
		static_cast<std::size_t>(_dependencies->_lstTopics->getSelectedRow());
	const std::size_t nativeOffset = _dependencies->_lstTopics->getScroll();
	for (std::size_t i = 0; i < presentationRows.size(); ++i)
	{
		const auto &typed = presentationRows[i];
		const std::string &text = typed.text;
		const bool hidden = text == "***";
		const bool structural = typed.kind
			!= ManufactureDependenciesTreeState::RowKind::Item;
		const char *rowKind = "item";
		switch (typed.kind)
		{
		case ManufactureDependenciesTreeState::RowKind::Header: rowKind = "level-header"; break;
		case ManufactureDependenciesTreeState::RowKind::Separator: rowKind = "separator"; break;
		case ManufactureDependenciesTreeState::RowKind::NoDependencies: rowKind = "empty"; break;
		case ManufactureDependenciesTreeState::RowKind::End: rowKind = "end"; break;
		case ManufactureDependenciesTreeState::RowKind::More: rowKind = "more"; break;
		case ManufactureDependenciesTreeState::RowKind::FeatureDisabled: rowKind = "feature-disabled"; break;
		case ManufactureDependenciesTreeState::RowKind::Item: break;
		}
		CalypsoHdOperationsRow row;
		row.id = "depth-" + std::to_string(typed.depth) + "-"
			+ rowKind + "-" + std::to_string(i);
		if (!text.empty())
		{
			row.values.push_back(text);
			CalypsoHdOperationsCell cell;
			cell.value = text;
			cell.contentRole = "name";
			cell.state.disabled = structural || hidden;
			row.cells.push_back(std::move(cell));
		}
		const std::size_t slot = generated.rowSlotCount == 0 ? 0
			: (i >= nativeOffset ? i - nativeOffset : 0) % generated.rowSlotCount;
		row.rect = generated.rowSlotCount == 0
			? tree.collection.viewport : tree.collection.rowSlots[slot];
		row.state.disabled = structural || hidden;
		row.state.selected = i == nativeSelected;
		row.widget = _dependencies->_lstTopics;
		tree.collection.rows.push_back(std::move(row));
	}
	tree.collection.visibleRows = generated.rowSlotCount;
	tree.collection.rowHeight = generated.rowSlotCount == 0
		? 0 : tree.collection.rowSlots.front().h;
	tree.collection.selectedIndex = nativeSelected;
	tree.collection.scrollOffset = nativeOffset;
	tree.collection.count = presentationRows.size();
	tree.collection.scroll = {
		nativeOffset, presentationRows.size(), static_cast<std::size_t>(generated.rowSlotCount),
		tree.collection.viewport, tree.collection.scrollTrack,
		tree.collection.scrollThumb};
	model.regions.push_back(std::move(tree));

	CalypsoHdOperationsRegion context;
	context.id = "context";
	context.label = tr("STR_TOPIC");
	context.kind = CalypsoHdOperationsRegionKind::Preview;
	context.rect = p(g->region_context);
	context.labelRect = p(g->region_context_label);
	context.previewRect = p(g->region_context_content);
	context.previewContent = _dependencies->_selectedItem.empty()
		? tr("STR_NONE") : tr(_dependencies->_selectedItem);
	model.regions.push_back(std::move(context));

	CalypsoHdOperationsRegion summaryRegion;
	summaryRegion.id = "summary";
	summaryRegion.label = tr("STR_TOPIC");
	summaryRegion.rect = p(g->region_summary);
	summaryRegion.labelRect = p(g->region_summary_label);
	summaryRegion.fields.push_back(metric("topic", tr("STR_TOPIC"),
		_dependencies->_selectedItem.empty()
			? tr("STR_NONE") : tr(_dependencies->_selectedItem),
		p(g->region_summary_field_topic_value)));
	model.regions.push_back(std::move(summaryRegion));

	model.geometry.footerActions = {p(g->action_show_all), p(g->action_ok)};
	model.footerActions.push_back(action("show-all", _dependencies->_btnShowAll->getText(),
		p(g->action_show_all), _dependencies->_btnShowAll,
		_dependencies->_btnShowAll->getVisible()));
	model.footerActions.push_back(action("ok", _dependencies->_btnOk->getText(),
		p(g->action_ok), _dependencies->_btnOk));
	finishModel(model, _dependencies->_game->getMod(),
		CalypsoF10ProductionDependenciesGen::kPresentationProfile,
		CalypsoF10ProductionDependenciesGen::kProfileId,
		CalypsoF10ProductionDependenciesGen::kProfileVersion,
		CalypsoF10ProductionDependenciesGen::kProvenanceTemplate,
		CalypsoF10ProductionDependenciesGen::kProfileStyle,
		CalypsoF10ProductionDependenciesGen::kTypographyWide, CalypsoF10ProductionDependenciesGen::kTypographyCompact,
		wide);
	return model;
}

} } // namespace OpenXcom::Calypso
#endif