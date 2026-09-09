#ifdef __EMSCRIPTEN__
#include "CalypsoF10ProductionUi.h"
#include "CalypsoHdFontSource.h"
#include "CalypsoHdOperationsRenderer.h"
#include "CalypsoHdUiOverlay.h"
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
	surface->setX(p.x); surface->setY(p.y); surface->setWidth(p.w); surface->setHeight(p.h);
}

template <typename R>
void setWindow(Window *window, const R &rect)
{
	if (!window) return;
	window->setX(rect.x); window->setY(rect.y); window->setWidth(rect.w); window->setHeight(rect.h);
}

void setFonts(CalypsoHdOperationsModel &model, const Mod *mod)
{
	model.readiness.contractReady = true;
	model.readiness.uploadsReady = true;
	model.readiness.retryable = true;
	model.readiness.fontsReady =
		calypsoHdResolveFontDescriptor(mod, "FONT_F34_SAIRA_700", model.headingFont)
		&& calypsoHdResolveFontDescriptor(mod, "FONT_F33_BODY", model.bodyFont)
		&& calypsoHdResolveFontDescriptor(mod, "FONT_F34_MONO", model.monoFont);
}

CalypsoHdOperationsAction action(const std::string &id, const std::string &label,
	const CalypsoHdOperationsRect &rect, const void *widget, bool visible = true)
{
	CalypsoHdOperationsAction out;
	out.id = id; out.label = label; out.component = "management-action-group";
	out.slotRole = "action"; out.coordinateSpace = "logical";
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

void finishModel(CalypsoHdOperationsModel &model, const Mod *mod)
{
	setFonts(model, mod);
	calypsoHdOperationsClampSelectionAndScroll(model);
}
} // namespace

CalypsoF10ProductionUi::CalypsoF10ProductionUi(ManufactureState *state)
	: _kind(Kind::Queue), _queue(state)
{
	_renderer = new CalypsoHdOperationsRenderer(state, CalypsoHdOperationsModel{});
	_renderer->setModelProvider([this]() { return buildModel(); });
}
CalypsoF10ProductionUi::CalypsoF10ProductionUi(NewManufactureListState *state)
	: _kind(Kind::Catalogue), _catalogue(state)
{
	_renderer = new CalypsoHdOperationsRenderer(state, CalypsoHdOperationsModel{});
	_renderer->setModelProvider([this]() { return buildModel(); });
}
CalypsoF10ProductionUi::CalypsoF10ProductionUi(ManufactureStartState *state)
	: _kind(Kind::Requirements), _requirements(state)
{
	_renderer = new CalypsoHdOperationsRenderer(state, CalypsoHdOperationsModel{});
	_renderer->setModelProvider([this]() { return buildModel(); });
}
CalypsoF10ProductionUi::CalypsoF10ProductionUi(ManufactureInfoState *state)
	: _kind(Kind::Controls), _controls(state)
{
	_renderer = new CalypsoHdOperationsRenderer(state, CalypsoHdOperationsModel{});
	_renderer->setModelProvider([this]() { return buildModel(); });
}
CalypsoF10ProductionUi::CalypsoF10ProductionUi(ManufactureDependenciesTreeState *state)
	: _kind(Kind::Dependencies), _dependencies(state)
{
	_renderer = new CalypsoHdOperationsRenderer(state, CalypsoHdOperationsModel{});
	_renderer->setModelProvider([this]() { return buildModel(); });
}

CalypsoF10ProductionUi::~CalypsoF10ProductionUi() { delete _renderer; }

#define F10_CONFIGURE(TYPE) \
void CalypsoF10ProductionUi::configure(TYPE &state) \
{ \
	if (state._hdAdapter) return; \
	if (!state._game || !state._game->getMod() || !state._game->getMod()->isHdUiFamilyEnabled("F10")) \
	{ state._hdLayout = false; return; } \
	state._hdLayout = true; state._hdWideLayout = Options::baseXResolution >= 1000; \
	auto *adapter = new CalypsoF10ProductionUi(&state); state._hdAdapter = adapter; \
	CalypsoHdUiOverlay::instance().registerAdapter(adapter->_renderer); adapter->refresh(); \
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
	state._hdWideLayout = Options::baseXResolution >= 1000; state._hdAdapter->refresh(); return true; \
}
F10_RESIZE(ManufactureState)
F10_RESIZE(NewManufactureListState)
F10_RESIZE(ManufactureStartState)
F10_RESIZE(ManufactureInfoState)
F10_RESIZE(ManufactureDependenciesTreeState)
#undef F10_RESIZE

void CalypsoF10ProductionUi::refresh()
{
	if (!_renderer) return;
	switch (_kind)
	{
	case Kind::Queue: applyQueueGeometry(); break;
	case Kind::Catalogue: applyCatalogueGeometry(); break;
	case Kind::Requirements: applyRequirementsGeometry(); break;
	case Kind::Controls: applyControlsGeometry(); break;
	case Kind::Dependencies: applyDependenciesGeometry(); break;
	}
	_renderer->setModel(buildModel());
}

CalypsoHdOperationsModel CalypsoF10ProductionUi::buildModel() const
{
	switch (_kind)
	{
	case Kind::Queue: return buildQueueModel();
	case Kind::Catalogue: return buildCatalogueModel();
	case Kind::Requirements: return buildRequirementsModel();
	case Kind::Controls: return buildControlsModel();
	case Kind::Dependencies: return buildDependenciesModel();
	}
	return {};
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
	setWindow(_queue->_window, g->window);
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
	_queue->_lstManufacture->configureCalypsoHdSelectionList(
		std::max(1, static_cast<int>(g->collection_scroll_track.w * sx)),
		std::max(1, static_cast<int>(44 * sy)),
		std::max(1, static_cast<int>(g->collection_row_slot_1.h * sy)),
		wide ? 5 : 2);
}

void CalypsoF10ProductionUi::applyCatalogueGeometry()
{
	if (!_catalogue || !_catalogue->_window) return;
	ensureCatalogueOwners();
	const bool wide = _catalogue->_hdWideLayout;
	const auto *g = CalypsoF10ProductionCatalogueGen::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) return;
	setWindow(_catalogue->_window, g->window);
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
	_catalogue->_lstManufacture->configureCalypsoHdSelectionList(
		std::max(1, static_cast<int>(g->collection_scroll_track.w * sx)),
		std::max(1, static_cast<int>(44 * sy)),
		std::max(1, static_cast<int>(g->collection_row_slot_1.h * sy)),
		wide ? 5 : 2);
}

void CalypsoF10ProductionUi::applyRequirementsGeometry()
{
	if (!_requirements || !_requirements->_window) return;
	const bool wide = _requirements->_hdWideLayout;
	const auto *g = CalypsoF10ProductionRequirementsGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) return;
	setWindow(_requirements->_window, g->window);
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
	_requirements->_lstRequiredItems->configureCalypsoHdSelectionList(
		std::max(1, static_cast<int>(g->region_requirements_collection_scroll_track.w * sx)),
		std::max(1, static_cast<int>(44 * sy)),
		std::max(1, static_cast<int>(g->region_requirements_collection_row_slot_1.h * sy)),
		wide ? 5 : 2);
}

void CalypsoF10ProductionUi::applyControlsGeometry()
{
	if (!_controls || !_controls->_window) return;
	const bool wide = _controls->_hdWideLayout;
	const auto *g = CalypsoF10ProductionControlsGen::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) return;
	setWindow(_controls->_window, g->window);
	const int wx = _controls->_window->getX(), wy = _controls->_window->getY();
	const double sx = static_cast<double>(_controls->_window->getWidth()) / g->window.w;
	const double sy = static_cast<double>(_controls->_window->getHeight()) / g->window.h;
	auto put = [&](Surface *s, const auto &r) { place(s, r, wx, wy, sx, sy, g->window.x, g->window.y); };
	put(_controls->_txtTitle, g->title); put(_controls->_txtAvailableEngineer, g->region_result_field_assigned_value);
	put(_controls->_txtAvailableSpace, g->region_result_field_assigned_label);
	put(_controls->_txtHoursPerUnit, g->region_result_field_produced_label);
	put(_controls->_txtMonthlyProfit, g->region_result_field_monthly_profit_value);
	put(_controls->_txtAllocatedEngineer, g->region_result_label); put(_controls->_txtAllocated, g->control_engineers_value);
	put(_controls->_txtUnitToProduce, g->region_output_label); put(_controls->_txtTodo, g->control_units_value);
	put(_controls->_txtEngineerUp, g->control_engineers_increment); put(_controls->_txtEngineerDown, g->control_engineers_decrement);
	put(_controls->_btnEngineerUp, g->control_engineers_increment); put(_controls->_btnEngineerDown, g->control_engineers_decrement);
	put(_controls->_txtUnitUp, g->control_units_increment); put(_controls->_txtUnitDown, g->control_units_decrement);
	put(_controls->_btnUnitUp, g->control_units_increment); put(_controls->_btnUnitDown, g->control_units_decrement);
	put(_controls->_btnFallback, g->region_sell_action_slot_2);
	put(_controls->_btnUnitInfinity, g->region_sell_action_slot_3);
	put(_controls->_btnUnitMinimum, g->region_sell_action_slot_4);
	put(_controls->_surfaceEngineers, g->control_engineers);
	put(_controls->_surfaceUnits, g->control_units);
	put(_controls->_btnSell, g->region_sell_action_slot_1);
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
	setWindow(_dependencies->_window, g->window);
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
	_dependencies->_lstTopics->configureCalypsoHdSelectionList(
		std::max(1, static_cast<int>(g->region_tree_collection_scroll_track.w * sx)),
		std::max(1, static_cast<int>(44 * sy)),
		std::max(1, static_cast<int>(g->region_tree_collection_row_slot_1.h * sy)),
		wide ? 5 : 2);
}

CalypsoHdOperationsModel CalypsoF10ProductionUi::buildQueueModel() const
{
	CalypsoHdOperationsModel model;
	if (!_queue || !_queue->_window || !_queue->_game) return model;
	const auto tr = [this](const std::string &key) { return _queue->tr(key); };
	const bool wide = _queue->_hdWideLayout;
	const auto *g = CalypsoF10ProductionQueueGen::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) return model;
	const int wx = _queue->_window->getX(), wy = _queue->_window->getY();
	const double sx = static_cast<double>(_queue->_window->getWidth()) / g->window.w;
	const double sy = static_cast<double>(_queue->_window->getHeight()) / g->window.h;
	auto p = [&](const auto &r) { return projectRect(r, wx, wy, sx, sy, g->window.x, g->window.y); };
	model.archetype = CalypsoHdOperationsArchetype::OperationsWorkspace;
	model.familyId = 10;
	model.ownerState = _queue;
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
	model.geometry.collectionRows = {
		p(g->collection_row_slot_1), p(g->collection_row_slot_2),
		p(g->collection_row_slot_3), p(g->collection_row_slot_4),
		p(g->collection_row_slot_5)};
	model.geometry.collectionRows.resize(wide ? 5 : 2);
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
	model.collection.columns = {
		{"item", tr("STR_ITEM"), p(g->collection_column_item), {}},
		{"engineers", tr("STR_CALYPSO_ENGINEERS_ALLOCATED"),
			p(g->collection_column_engineers), {}},
		{"produced", tr("STR_CALYPSO_PRODUCED"),
			p(g->collection_column_produced), {}},
		{"cost", tr("STR_CALYPSO_COST_PER_UNIT"),
			p(g->collection_column_cost), {}},
		{"time", _queue->_txtTimeLeft->getText(),
			p(g->collection_column_time), {}}};
	const auto &productions = _queue->_base->getProductions();
	model.collection.heading = tr("STR_CALYPSO_PRODUCTION_LINES");
	model.collection.meta = tr("STR_CALYPSO_FUNDS_VALUE").arg(
		Unicode::formatFunding(_queue->_game->getSavedGame()->getFunds()));
	model.collection.emptyTitle = tr("STR_CALYPSO_NO_ACTIVE_PRODUCTION");
	model.collection.emptyBody = tr("STR_CALYPSO_START_PRODUCTION_PROMPT");
	for (std::size_t i = 0; i < productions.size(); ++i)
	{
		const Production *prod = productions[i];
		const std::string produced = productionProgress(prod);
		const std::string time = productionTimeLeft(prod);
		CalypsoHdOperationsRow row;
		row.id = prod->getRules()->getName();
		row.values = {tr(prod->getRules()->getName()), std::to_string(prod->getAssignedEngineers()),
			produced, Unicode::formatFunding(prod->getRules()->getManufactureCost()), time};
		row.rect = model.geometry.collectionRows.empty()
			? model.geometry.collectionViewport
			: model.geometry.collectionRows[
				std::min(i, model.geometry.collectionRows.size() - 1)];
		row.state.selected = i == _queue->_lstManufacture->getSelectedRow();
		row.widget = _queue->_lstManufacture;
		model.collection.rows.push_back(std::move(row));
	}
	model.collection.selectedIndex = productions.empty() ? 0 : _queue->_lstManufacture->getSelectedRow();
	model.collection.scrollOffset = _queue->_lstManufacture->getScroll();
	model.collection.visibleRows = model.geometry.collectionRows.size();
	model.collection.rowHeight = model.geometry.collectionRows.empty()
		? 0 : model.geometry.collectionRows.front().h;
	model.collection.rowSlots = model.geometry.collectionRows;
	const bool selected = !productions.empty()
		&& model.collection.selectedIndex < productions.size();
	_queue->_btnOpenProduction->setVisible(selected);
	_queue->_btnTechTree->setVisible(selected);
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
		p(g->action_global_overview), _queue->_btnGlobalOverview));
	auto newProduction = action("new-production", _queue->_btnNew->getText(),
		p(g->action_new_production), _queue->_btnNew);
	newProduction.state.selected = productions.empty();
	model.footerActions.push_back(std::move(newProduction));
	model.footerActions.push_back(action("done", _queue->_btnOk->getText(),
		p(g->action_done), _queue->_btnOk));
	finishModel(model, _queue->_game->getMod());
	return model;
}

CalypsoHdOperationsModel CalypsoF10ProductionUi::buildCatalogueModel() const
{
	CalypsoHdOperationsModel model;
	if (!_catalogue || !_catalogue->_window || !_catalogue->_game) return model;
	const auto tr = [this](const std::string &key) { return _catalogue->tr(key); };
	const bool wide = _catalogue->_hdWideLayout;
	const auto *g = CalypsoF10ProductionCatalogueGen::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) return model;
	const int wx = _catalogue->_window->getX(), wy = _catalogue->_window->getY();
	const double sx = static_cast<double>(_catalogue->_window->getWidth()) / g->window.w;
	const double sy = static_cast<double>(_catalogue->_window->getHeight()) / g->window.h;
	auto p = [&](const auto &r) { return projectRect(r, wx, wy, sx, sy, g->window.x, g->window.y); };
	model.archetype = CalypsoHdOperationsArchetype::OperationsWorkspace;
	model.familyId = 10;
	model.ownerState = _catalogue;
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
	model.geometry.toolbarBar = p(g->toolbarBar); model.geometry.collectionViewport = p(g->collectionViewport); model.geometry.detailPanel = p(g->detailPanel); model.geometry.footer = p(g->footer);
	model.geometry.collectionScrollTrack = p(g->collection_scroll_track); model.geometry.collectionScrollThumb = p(g->collection_scroll_thumb);
	model.geometry.collectionColumns = {p(g->collection_column_item),p(g->collection_column_category),p(g->collection_column_status)};
	model.geometry.collectionRows = {p(g->collection_row_slot_1),p(g->collection_row_slot_2),p(g->collection_row_slot_3),p(g->collection_row_slot_4),p(g->collection_row_slot_5)};
	model.geometry.collectionRows.resize(wide ? 5 : 2);
	model.geometry.detailIdentity = p(g->detail_selected_item_label); model.geometry.detailIdentityTitle = p(g->detail_selected_item_identity_title); model.geometry.detailIdentitySubtitle = p(g->detail_selected_item_identity_subtitle);
	model.geometry.detailMetrics = {p(g->detail_selected_item_metric_category)};
	model.geometry.detailActions = {p(g->detail_selected_item_action_review_production),p(g->detail_selected_item_action_tech_tree),p(g->detail_selected_item_action_ufopaedia)};
	model.geometry.footerActions = {p(g->action_mark_all_seen),p(g->action_done)};
	const auto &items = _catalogue->_displayedStrings;
	model.collection.columns = {{"item",_catalogue->_txtItem->getText(),p(g->collection_column_item),{}},{"category",_catalogue->_txtCategory->getText(),p(g->collection_column_category),{}},{"status",tr("STR_STATUS"),p(g->collection_column_status),{}}};
	model.collection.emptyTitle = tr("STR_CALYPSO_NO_AVAILABLE_PRODUCTION");
	model.collection.emptyBody = tr("STR_CALYPSO_NO_AVAILABLE_PRODUCTION_PROMPT");
	for (std::size_t i = 0; i < items.size(); ++i)
	{
		CalypsoHdOperationsRow row; row.id = items[i]; row.values = {_catalogue->_lstManufacture->getCellText(i,0), _catalogue->_lstManufacture->getCellText(i,1), _catalogue->_lstManufacture->getCellText(i,2)};
		row.rect = model.geometry.collectionRows.empty() ? model.geometry.collectionViewport : model.geometry.collectionRows[std::min(i, model.geometry.collectionRows.size()-1)];
		row.state.selected = i == _catalogue->_lstManufacture->getSelectedRow(); row.widget = _catalogue->_lstManufacture; model.collection.rows.push_back(std::move(row));
	}
	model.collection.selectedIndex = items.empty() ? 0 : _catalogue->_lstManufacture->getSelectedRow(); model.collection.scrollOffset = _catalogue->_lstManufacture->getScroll();
	model.collection.visibleRows = model.geometry.collectionRows.size(); model.collection.rowHeight = model.geometry.collectionRows.empty() ? 0 : model.geometry.collectionRows.front().h; model.collection.rowSlots = model.geometry.collectionRows;
	const bool selected = !items.empty() && model.collection.selectedIndex < items.size(); const RuleManufacture *rule = selected ? _catalogue->_game->getMod()->getManufacture(items[model.collection.selectedIndex]) : nullptr;
	_catalogue->_btnReview->setVisible(selected);
	_catalogue->_btnTechTree->setVisible(selected);
	_catalogue->_btnUfopaedia->setVisible(selected);
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
		selected));
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
	model.footerActions.push_back(action("mark-all-seen", tr("STR_MARK_ALL_AS_SEEN"), p(g->action_mark_all_seen), _catalogue->_btnMarkAllSeen));
	model.footerActions.push_back(action("done", tr("STR_DONE"),
		p(g->action_done), _catalogue->_btnOk));
	finishModel(model, _catalogue->_game->getMod());
	return model;
}

CalypsoHdOperationsModel CalypsoF10ProductionUi::buildRequirementsModel() const
{
	CalypsoHdOperationsModel model;
	if (!_requirements || !_requirements->_window || !_requirements->_game || !_requirements->_item) return model;
	const auto tr = [this](const std::string &key) { return _requirements->tr(key); };
	const bool wide = _requirements->_hdWideLayout; const auto *g = CalypsoF10ProductionRequirementsGen::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) return model;
	const int wx = _requirements->_window->getX(), wy = _requirements->_window->getY(); const double sx = static_cast<double>(_requirements->_window->getWidth()) / g->window.w; const double sy = static_cast<double>(_requirements->_window->getHeight()) / g->window.h;
	auto p = [&](const auto &r) { return projectRect(r, wx, wy, sx, sy, g->window.x, g->window.y); };
	model.archetype = CalypsoHdOperationsArchetype::WideDetail;
	model.familyId = 10;
	model.ownerState = _requirements;
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
	model.geometry.controlBar = p(g->controlBar);
	model.geometry.footer = p(g->footer);
	CalypsoHdOperationsRegion requirements;
	requirements.id = "requirements";
	requirements.label = _requirements->_txtRequiredItemsTitle->getText();
	requirements.kind = CalypsoHdOperationsRegionKind::Collection;
	requirements.rect = p(g->region_requirements);
	requirements.labelRect = p(g->region_requirements_label);
	requirements.collection.viewport = p(g->region_requirements_collection);
	requirements.collection.scrollTrack = p(g->region_requirements_collection_scroll_track);
	requirements.collection.scrollThumb = p(g->region_requirements_collection_scroll_thumb);
	requirements.collection.columns = {
		{"item", _requirements->_txtItemNameColumn->getText(),
			p(g->region_requirements_collection_column_item), {}},
		{"required", _requirements->_txtUnitRequiredColumn->getText(),
			p(g->region_requirements_collection_column_required), {}},
		{"available", _requirements->_txtUnitAvailableColumn->getText(),
			p(g->region_requirements_collection_column_available), {}}};
	requirements.collection.rowSlots = {
		p(g->region_requirements_collection_row_slot_1),
		p(g->region_requirements_collection_row_slot_2),
		p(g->region_requirements_collection_row_slot_3),
		p(g->region_requirements_collection_row_slot_4),
		p(g->region_requirements_collection_row_slot_5)};
	requirements.collection.rowSlots.resize(wide ? 5 : 2);
	const auto &requiredRows = _requirements->_lstRequiredItems->getCellTextsSnapshot();
	for (std::size_t i = 0; i < requiredRows.size(); ++i)
	{
		CalypsoHdOperationsRow row;
		row.id = std::to_string(i);
		for (std::size_t c = 0; c < requiredRows[i].size() && c < 3; ++c)
			row.values.push_back(requiredRows[i][c] ? requiredRows[i][c]->getText() : std::string());
		while (row.values.size() < 3) row.values.push_back(std::string());
		row.rect = requirements.collection.rowSlots.empty()
			? requirements.collection.viewport
			: requirements.collection.rowSlots[
				std::min(i, requirements.collection.rowSlots.size() - 1)];
		row.widget = _requirements->_lstRequiredItems;
		requirements.collection.rows.push_back(std::move(row));
	}
	requirements.collection.visibleRows = requirements.collection.rowSlots.size();
	requirements.collection.rowHeight = requirements.collection.rowSlots.empty()
		? 0 : requirements.collection.rowSlots.front().h;
	requirements.collection.selectedIndex = 0;
	requirements.collection.scrollOffset = _requirements->_lstRequiredItems->getScroll();
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
	facts.rect = p(g->region_facts);
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
		_requirements->_btnStart->getVisible()));
	finishModel(model, _requirements->_game->getMod());
	return model;
}

CalypsoHdOperationsModel CalypsoF10ProductionUi::buildControlsModel() const
{
	CalypsoHdOperationsModel model;
	if (!_controls || !_controls->_window || !_controls->_game || !_controls->_production) return model;
	const auto tr = [this](const std::string &key) { return _controls->tr(key); };
	const bool wide = _controls->_hdWideLayout;
	const auto *g = CalypsoF10ProductionControlsGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) return model;
	const int wx = _controls->_window->getX();
	const int wy = _controls->_window->getY();
	const double sx = static_cast<double>(_controls->_window->getWidth()) / g->window.w;
	const double sy = static_cast<double>(_controls->_window->getHeight()) / g->window.h;
	auto p = [&](const auto &r) {
		return projectRect(r, wx, wy, sx, sy, g->window.x, g->window.y);
	};
	model.archetype = CalypsoHdOperationsArchetype::WideDetail;
	model.familyId = 10;
	model.ownerState = _controls;
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
	model.geometry.designWidth = g->designWidth;
	model.geometry.designHeight = g->designHeight;
	model.geometry.window = p(g->window);
	model.geometry.status = p(g->status);
	model.geometry.title = p(g->title);
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
	engineers.widget = _controls->_surfaceEngineers;
	engineers.decrement = action("engineers-decrement", "−",
		p(g->control_engineers_decrement), _controls->_btnEngineerDown);
	engineers.increment = action("engineers-increment", "+",
		p(g->control_engineers_increment), _controls->_btnEngineerUp);
	model.controls.push_back(std::move(engineers));

	CalypsoHdOperationsControl units;
	units.id = "units";
	units.label = tr("STR_UNITS_TO_PRODUCE");
	units.kind = CalypsoHdOperationsControlKind::Stepper;
	units.displayValue = _controls->_production->getInfiniteAmount()
		? "∞" : std::to_string(_controls->_production->getAmountTotal());
	units.rect = p(g->control_units);
	units.valueRect = p(g->control_units_value);
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
		p(g->region_sell_action_slot_1), _controls->_btnSell,
		_controls->_btnSell->getVisible());
	sellAction.state.selected = _controls->_btnSell->getPressed();
	sell.actions.push_back(std::move(sellAction));
	auto fallbackAction = action("fallback-production",
		_controls->_btnFallback->getText() + ": "
			+ std::string(tr(_controls->_btnFallback->getPressed() ? "STR_YES" : "STR_NO")),
		p(g->region_sell_action_slot_2), _controls->_btnFallback,
		_controls->_btnFallback->getVisible());
	fallbackAction.state.selected = _controls->_btnFallback->getPressed();
	sell.actions.push_back(std::move(fallbackAction));
	sell.actions.push_back(action("infinity", tr("STR_CALYPSO_INFINITY"),
		p(g->region_sell_action_slot_3), _controls->_btnUnitInfinity,
		_controls->_btnUnitInfinity->getVisible()));
	sell.actions.push_back(action("minimum", tr("STR_CALYPSO_MINIMUM"),
		p(g->region_sell_action_slot_4), _controls->_btnUnitMinimum,
		_controls->_btnUnitMinimum->getVisible()));
	sell.actions.push_back(action("sell-slot-5", "", p(g->region_sell_action_slot_5),
		nullptr, false));
	sell.actions.push_back(action("sell-slot-6", "", p(g->region_sell_action_slot_6),
		nullptr, false));
	sell.actions.push_back(action("sell-slot-7", "", p(g->region_sell_action_slot_7),
		nullptr, false));
	sell.actions.push_back(action("sell-slot-8", "", p(g->region_sell_action_slot_8),
		nullptr, false));
	sell.actions.push_back(action("sell-slot-9", "", p(g->region_sell_action_slot_9),
		nullptr, false));
	sell.actions.push_back(action("sell-slot-10", "", p(g->region_sell_action_slot_10),
		nullptr, false));
	model.regions.push_back(std::move(sell));

	model.footerActions.push_back(action("stop-production", _controls->_btnStop->getText(),
		p(g->action_stop_production), _controls->_btnStop));
	model.footerActions.push_back(action("ok", _controls->_btnOk->getText(),
		p(g->action_ok), _controls->_btnOk));
	finishModel(model, _controls->_game->getMod());
	return model;
}

CalypsoHdOperationsModel CalypsoF10ProductionUi::buildDependenciesModel() const
{
	CalypsoHdOperationsModel model;
	if (!_dependencies || !_dependencies->_window || !_dependencies->_game) return model;
	const auto tr = [this](const std::string &key) { return _dependencies->tr(key); };
	const bool wide = _dependencies->_hdWideLayout;
	const auto *g = CalypsoF10ProductionDependenciesGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) return model;
	const int wx = _dependencies->_window->getX(), wy = _dependencies->_window->getY();
	const double sx = static_cast<double>(_dependencies->_window->getWidth()) / g->window.w;
	const double sy = static_cast<double>(_dependencies->_window->getHeight()) / g->window.h;
	auto p = [&](const auto &r) {
		return projectRect(r, wx, wy, sx, sy, g->window.x, g->window.y);
	};
	model.archetype = CalypsoHdOperationsArchetype::WideDetail;
	model.familyId = 10;
	model.ownerState = _dependencies;
	model.title = _dependencies->_txtTitle->getText();
	model.geometry.designWidth = g->designWidth;
	model.geometry.designHeight = g->designHeight;
	model.geometry.window = p(g->window);
	model.geometry.status = p(g->status);
	model.geometry.title = p(g->title);
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
	tree.labelRect = p(g->region_tree_label);
	tree.collection.viewport = p(g->region_tree_collection);
	tree.collection.scrollTrack = p(g->region_tree_collection_scroll_track);
	tree.collection.scrollThumb = p(g->region_tree_collection_scroll_thumb);
	tree.collection.columns = {
		{"direct", tr("STR_DIRECT_DEPENDENCIES"),
			p(g->region_tree_collection_column_direct), {}},
		{"level-two", tr("STR_LEVEL_2_DEPENDENCIES"),
			p(g->region_tree_collection_column_level_two), {}},
		{"level-three", tr("STR_LEVEL_3_DEPENDENCIES"),
			p(g->region_tree_collection_column_level_three), {}}};
	tree.collection.rowSlots = {
		p(g->region_tree_collection_row_slot_1),
		p(g->region_tree_collection_row_slot_2),
		p(g->region_tree_collection_row_slot_3),
		p(g->region_tree_collection_row_slot_4),
		p(g->region_tree_collection_row_slot_5)};
	tree.collection.rowSlots.resize(wide ? 5 : 2);

	int level = 0;
	for (std::size_t i = 0; i < _dependencies->_lstTopics->getTexts(); ++i)
	{
		const std::string text = _dependencies->_lstTopics->getCellText(i, 0);
		if (text == std::string(tr("STR_DIRECT_DEPENDENCIES"))) level = 0;
		else if (text == std::string(tr("STR_LEVEL_2_DEPENDENCIES"))) level = 1;
		else if (text == std::string(tr("STR_LEVEL_3_DEPENDENCIES"))) level = 2;
		CalypsoHdOperationsRow row;
		row.id = std::to_string(i);
		row.values = {"", "", ""};
		row.values[std::min(level, 2)] = text;
		row.rect = tree.collection.rowSlots.empty()
			? tree.collection.viewport
			: tree.collection.rowSlots[
				std::min(i, tree.collection.rowSlots.size() - 1)];
		row.widget = _dependencies->_lstTopics;
		tree.collection.rows.push_back(std::move(row));
	}
	tree.collection.visibleRows = tree.collection.rowSlots.size();
	tree.collection.rowHeight = tree.collection.rowSlots.empty()
		? 0 : tree.collection.rowSlots.front().h;
	tree.collection.selectedIndex = _dependencies->_lstTopics->getSelectedRow();
	tree.collection.scrollOffset = _dependencies->_lstTopics->getScroll();
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
	finishModel(model, _dependencies->_game->getMod());
	return model;
}

} } // namespace OpenXcom::Calypso
#endif