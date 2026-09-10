#ifdef __EMSCRIPTEN__
#include "CalypsoF14GlobalOperationsUi.h"

#include "CalypsoHdFontSource.h"
#include "CalypsoHdOperationsChrome.h"
#include "CalypsoHdOperationsRenderer.h"
#include "CalypsoHdUiOverlay.h"
#include "Generated/CalypsoF14GlobalProduction.generated.h"
#include "Generated/CalypsoF14GlobalResearch.generated.h"
#include "Generated/CalypsoF14ResearchDiary.generated.h"
#include "../Basescape/GlobalManufactureState.h"
#include "../Basescape/GlobalResearchDiaryState.h"
#include "../Basescape/GlobalResearchState.h"
#include "../Engine/Game.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Options.h"
#include "../Interface/ArrowButton.h"
#include "../Engine/Surface.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextEdit.h"
#include "../Interface/TextList.h"
#include "../Interface/Window.h"
#include "../Mod/RuleManufacture.h"
#include "../Mod/RuleResearch.h"
#include "../Savegame/ResearchDiary.h"
#include "../Mod/Mod.h"
#include "../Savegame/Base.h"
#include "../Savegame/SavedGame.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace OpenXcom { namespace Calypso {
namespace {

template <typename R>
CalypsoHdOperationsRect projectRect(const R &rect, int wx, int wy,
	double sx, double sy, int windowX, int windowY)
{
	return {
		wx + static_cast<int>(std::lround((rect.x - windowX) * sx)),
		wy + static_cast<int>(std::lround((rect.y - windowY) * sy)),
		std::max(1, static_cast<int>(std::lround(rect.w * sx))),
		std::max(1, static_cast<int>(std::lround(rect.h * sy)))};
}

template <typename R>
void place(Surface *surface, const R &rect, int wx, int wy, double sx,
	double sy, int windowX, int windowY)
{
	if (!surface) return;
	const auto projected = projectRect(rect, wx, wy, sx, sy, windowX, windowY);
	if (surface->getX() != projected.x) surface->setX(projected.x);
	if (surface->getY() != projected.y) surface->setY(projected.y);
	if (surface->getWidth() != projected.w) surface->setWidth(projected.w);
	if (surface->getHeight() != projected.h) surface->setHeight(projected.h);
}

template <typename R>
void setWindow(Window *window, const R &rect)
{
	if (!window) return;
	const auto projected = calypsoHdOperationsProjectForCurrentPresentation(
		{rect.x, rect.y, rect.w, rect.h}, rect.w, rect.h);
	if (window->getX() != projected.x) window->setX(projected.x);
	if (window->getY() != projected.y) window->setY(projected.y);
	if (window->getWidth() != projected.w) window->setWidth(projected.w);
	if (window->getHeight() != projected.h) window->setHeight(projected.h);
}



CalypsoHdOperationsAction action(const std::string &id, const std::string &label,
	const CalypsoHdOperationsRect &rect, const void *widget, bool visible = true)
{
	CalypsoHdOperationsAction result;
	result.id = id;
	result.label = label;
	result.component = "management-action-group";
	result.slotRole = "action";
	result.coordinateSpace = "logical";
	result.visible = rect;
	result.hit = rect;
	result.widget = widget;
	result.state.visible = visible;
	return result;
}

CalypsoHdOperationsSummaryField summary(const std::string &id,
	const std::string &label, const std::string &value,
	const CalypsoHdOperationsRect &rect, const void *widget)
{
	CalypsoHdOperationsSummaryField result;
	result.id = id;
	result.label = label;
	result.value = value;
	result.rect = rect;
	result.widget = widget;
	return result;
}

CalypsoHdOperationsMetric metric(const std::string &id, const std::string &label,
	const std::string &value, const CalypsoHdOperationsRect &rect)
{
	CalypsoHdOperationsMetric result;
	result.id = id;
	result.label = label;
	result.value = value;
	result.rect = rect;
	return result;
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

void setFonts(CalypsoHdOperationsModel &model, const Mod *mod)
{
	model.readiness.uploadsReady = true;
	model.readiness.retryable = true;
	model.readiness.fontsReady =
		mod != nullptr
		&& calypsoHdResolveFontDescriptor(mod, "FONT_CC_INTER_SB", model.headingFont)
		&& calypsoHdResolveFontDescriptor(mod, "FONT_CC_INTER_R", model.bodyFont)
		&& calypsoHdResolveFontDescriptor(mod, "FONT_CC_PLEX_R", model.monoFont);
}

template <typename Collection, typename Project>
void setGeneratedCollectionRows(CalypsoHdOperationsModel &model,
	const Collection &collection, const Project &project)
{
	model.geometry.collectionRows.clear();
	for (int i = 0; i < collection.rowSlotCount; ++i)
		model.geometry.collectionRows.push_back(project(collection.rowSlots[i].rect));
}

void finish(CalypsoHdOperationsModel &model, const Mod *mod,
	const char *presentation, const char *profileId, const char *profileVersion,
	const char *provenance)
{
	setContractMetadata(model, presentation, profileId, profileVersion, provenance);
	setFonts(model, mod);
}

void suppress(CalypsoHdOperationsModel &model, const void *widget)
{
	if (widget) model.suppressedWidgets.push_back(widget);
}

template <typename G, typename Project>
void setWorkspaceGeometry(CalypsoHdOperationsModel &model, const G &g,
	const Project &project)
{
	model.geometry.designWidth = g.designWidth;
	model.geometry.designHeight = g.designHeight;
	model.geometry.window = project(g.window);
	model.geometry.screenHeader = project(g.screenHeader);
	model.geometry.headerArt = project(g.headerArt);
	model.geometry.title = project(g.title);
	model.geometry.summaryBar = project(g.summaryBar);
	model.geometry.toolbarBar = project(g.toolbarBar);
	model.geometry.collectionViewport = project(g.collectionViewport);
	model.geometry.detailPanel = project(g.detailPanel);
	model.geometry.footer = project(g.footer);
	model.geometry.collectionRows.clear();
	model.geometry.collectionScrollTrack = project(g.collection_scroll_track);
	model.geometry.collectionScrollThumb = project(g.collection_scroll_thumb);
}

template <typename G, typename Project>
void setResearchGeometry(CalypsoHdOperationsModel &model, const G &g,
	const Project &project)
{
	setWorkspaceGeometry(model, g, project);
	model.geometry.detailIdentity = project(g.detail_selected_project_label);
	model.geometry.detailIdentityTitle =
		project(g.detail_selected_project_identity_title);
	model.geometry.detailIdentitySubtitle =
		project(g.detail_selected_project_identity_subtitle);
	model.geometry.detailMetrics = {
		project(g.detail_selected_project_metric_base),
		project(g.detail_selected_project_metric_research_project)};
	model.geometry.detailActions = {
		project(g.detail_selected_project_action_open_base_research),
		project(g.detail_selected_project_action_tech_tree)};
	model.geometry.footerActions = {
		project(g.action_research_diary), project(g.action_done)};
}

template <typename G, typename Project>
void setProductionGeometry(CalypsoHdOperationsModel &model, const G &g,
	const Project &project)
{
	setWorkspaceGeometry(model, g, project);
	model.geometry.detailIdentity = project(g.detail_selected_production_label);
	model.geometry.detailIdentityTitle =
		project(g.detail_selected_production_identity_title);
	model.geometry.detailIdentitySubtitle =
		project(g.detail_selected_production_identity_subtitle);
	model.geometry.detailMetrics = {
		project(g.detail_selected_production_metric_base),
		project(g.detail_selected_production_metric_item)};
	model.geometry.detailActions = {
		project(g.detail_selected_production_action_open_base_production),
		project(g.detail_selected_production_action_tech_tree)};
	model.geometry.footerActions = {project(g.action_done)};
}

template <typename G, typename Project>
void setDiaryGeometry(CalypsoHdOperationsModel &model, const G &g,
	const Project &project)
{
	setWorkspaceGeometry(model, g, project);
	model.geometry.detailIdentity = project(g.detail_selected_entry_label);
	model.geometry.detailIdentityTitle =
		project(g.detail_selected_entry_identity_title);
	model.geometry.detailIdentitySubtitle =
		project(g.detail_selected_entry_identity_subtitle);
	model.geometry.collectionColumns = {
		project(g.collection_column_name), project(g.collection_column_type),
		project(g.collection_column_date)};
	model.geometry.detailMetrics = {
		project(g.detail_selected_entry_metric_name),
		project(g.detail_selected_entry_metric_type),
		project(g.detail_selected_entry_metric_date)};
	model.geometry.detailActions = {
		project(g.detail_selected_entry_action_open_tech_tree),
		project(g.detail_selected_entry_action_open_ufopaedia)};
	model.geometry.footerActions = {project(g.action_done)};
}

template <typename G, typename Project>
void setCollectionGeometry(CalypsoHdOperationsModel &model, const G &g,
	const Project &project)
{
	model.collection.viewport = project(g.collectionViewport);
	model.collection.scrollTrack = project(g.collection_scroll_track);
	model.collection.scrollThumb = project(g.collection_scroll_thumb);
	model.collection.rowSlots = model.geometry.collectionRows;
	model.collection.visibleRows = model.geometry.collectionRows.size();
	model.collection.rowHeight = model.collection.rowSlots.empty()
		? 0 : model.collection.rowSlots.front().h;
}

} // namespace

CalypsoF14GlobalOperationsUi::CalypsoF14GlobalOperationsUi(GlobalResearchState *state)
	: _kind(Kind::Research), _research(state),
	  _chrome(new CalypsoHdOperationsChrome(*state))
{
	_renderer = new CalypsoHdOperationsRenderer(state, CalypsoHdOperationsModel{});
	_renderer->setModelProvider([this]() { syncGeometry(); return buildModel(); });
}

CalypsoF14GlobalOperationsUi::CalypsoF14GlobalOperationsUi(
	GlobalResearchDiaryState *state)
	: _kind(Kind::Diary), _diary(state),
	  _chrome(new CalypsoHdOperationsChrome(*state))
{
	_renderer = new CalypsoHdOperationsRenderer(state, CalypsoHdOperationsModel{});
	_renderer->setModelProvider([this]() { syncGeometry(); return buildModel(); });
}

CalypsoF14GlobalOperationsUi::CalypsoF14GlobalOperationsUi(
	GlobalManufactureState *state)
	: _kind(Kind::Manufacture), _manufacture(state),
	  _chrome(new CalypsoHdOperationsChrome(*state))
{
	_renderer = new CalypsoHdOperationsRenderer(state, CalypsoHdOperationsModel{});
	_renderer->setModelProvider([this]() { syncGeometry(); return buildModel(); });
}

CalypsoF14GlobalOperationsUi::~CalypsoF14GlobalOperationsUi()
{
	delete _renderer;
	delete _chrome;
}

#define F14_CONFIGURE(TYPE) \
void CalypsoF14GlobalOperationsUi::configure(TYPE &state) \
{ \
	if (state._hdAdapter) return; \
	if (!calypsoHdOperationsRouteEnabled(state._game, "F14")) \
	{ \
		state._hdLayout = false; \
		return; \
	} \
	state._hdLayout = true; \
	state._hdWideLayout = Options::baseXResolution >= 1000; \
	auto *adapter = new CalypsoF14GlobalOperationsUi(&state); \
	state._hdAdapter = adapter; \
	CalypsoHdUiOverlay::instance().registerAdapter(adapter->_renderer); \
	adapter->refresh(); \
	calypsoHdOperationsPublishHarnessVisibility(); \
}
F14_CONFIGURE(GlobalResearchState)
F14_CONFIGURE(GlobalResearchDiaryState)
F14_CONFIGURE(GlobalManufactureState)
#undef F14_CONFIGURE

#define F14_RESIZE(TYPE) \
bool CalypsoF14GlobalOperationsUi::resize(TYPE &state) \
{ \
	if (!state._hdLayout || !state._hdAdapter) return false; \
	state._hdWideLayout = Options::baseXResolution >= 1000; \
	state._hdAdapter->refresh(); \
	return true; \
}
F14_RESIZE(GlobalResearchState)
F14_RESIZE(GlobalResearchDiaryState)
F14_RESIZE(GlobalManufactureState)
#undef F14_RESIZE

void CalypsoF14GlobalOperationsUi::syncGeometry()
{
	switch (_kind)
	{
	case Kind::Research: applyResearchGeometry(); break;
	case Kind::Diary: applyDiaryGeometry(); break;
	case Kind::Manufacture: applyManufactureGeometry(); break;
	}
	_chrome->applyGeometry();
}

void CalypsoF14GlobalOperationsUi::refresh()
{
	if (!_renderer) return;
	syncGeometry();
	_renderer->setModel(buildModel());
}

CalypsoHdOperationsModel CalypsoF14GlobalOperationsUi::buildModel() const
{
	CalypsoHdOperationsModel model;
	switch (_kind)
	{
	case Kind::Research: model = buildResearchModel(); break;
	case Kind::Diary: model = buildDiaryModel(); break;
	case Kind::Manufacture: model = buildManufactureModel(); break;
	}
	_chrome->populateModel(model);
	return model;
}

void CalypsoF14GlobalOperationsUi::ensureResearchOwners()
{
	if (!_research) return;
	auto make = [&](TextButton *&button, ActionHandler handler)
	{
		if (button) return;
		button = new TextButton(1, 1, 0, 0);
		button->setText("");
		_research->add(button, "button", "globalResearchMenu");
		button->onMouseClick(handler);
	};
	make(_research->_btnOpenBaseResearch,
		(ActionHandler)&GlobalResearchState::onSelectBase);
	make(_research->_btnTechTree,
		(ActionHandler)&GlobalResearchState::onOpenTechTreeViewer);
}

void CalypsoF14GlobalOperationsUi::ensureManufactureOwners()
{
	if (!_manufacture) return;
	auto make = [&](TextButton *&button, ActionHandler handler)
	{
		if (button) return;
		button = new TextButton(1, 1, 0, 0);
		button->setText("");
		_manufacture->add(button, "button", "globalManufactureMenu");
		button->onMouseClick(handler);
	};
	make(_manufacture->_btnOpenBaseProduction,
		(ActionHandler)&GlobalManufactureState::onSelectBase);
	make(_manufacture->_btnTechTree,
		(ActionHandler)&GlobalManufactureState::onOpenTechTreeViewer);
}

void CalypsoF14GlobalOperationsUi::ensureDiaryOwners()
{
	if (!_diary) return;
	auto make = [&](TextButton *&button, ActionHandler handler)
	{
		if (button) return;
		button = new TextButton(1, 1, 0, 0);
		button->setText("");
		_diary->add(button, "button", "globalResearchDiary");
		button->onMouseClick(handler);
	};
	make(_diary->_btnOpenTechTree,
		(ActionHandler)&GlobalResearchDiaryState::lstItemLClick);
	make(_diary->_btnOpenUfopaedia,
		(ActionHandler)&GlobalResearchDiaryState::lstItemMClick);
	make(_diary->_btnQuickSearchToggle,
		(ActionHandler)&GlobalResearchDiaryState::btnQuickSearchToggle);
	if (_diary->_btnQuickSearchToggle)
		_diary->_btnQuickSearchToggle->setVisible(
			!_diary->_btnQuickSearch->getVisible());
}

void CalypsoF14GlobalOperationsUi::applyResearchGeometry()
{
	if (!_research || !_research->_window) return;
	ensureResearchOwners();
	const auto *g = CalypsoF14GlobalResearchGen::layoutForDesign(
		_research->_hdWideLayout ? 1280 : 740,
		_research->_hdWideLayout ? 720 : 360);
	if (!g) return;
	setWindow(_research->_window, g->window);
	const int wx = _research->_window->getX(), wy = _research->_window->getY();
	const double sx = static_cast<double>(_research->_window->getWidth()) / g->window.w;
	const double sy = static_cast<double>(_research->_window->getHeight()) / g->window.h;
	auto p = [&](const auto &rect) {
		return projectRect(rect, wx, wy, sx, sy, g->window.x, g->window.y);
	};
	_research->_lstResearch->rebaseNativeSize(g->collectionViewport.w,
		g->collectionViewport.h);
	place(_research->_txtTitle, g->title, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_research->_txtAvailable, g->summary_available, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_research->_txtAllocated, g->summary_allocated, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_research->_txtSpace, g->summary_lab_space, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_research->_txtProject, g->collection_column_project, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_research->_txtScientists, g->collection_column_scientists, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_research->_txtProgress, g->collection_column_progress, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_research->_lstResearch, g->collectionViewport, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_research->_btnDiary, g->action_research_diary, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_research->_btnOk, g->action_done, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_research->_btnOpenBaseResearch, g->detail_selected_project_action_open_base_research, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_research->_btnTechTree, g->detail_selected_project_action_tech_tree, wx, wy, sx, sy, g->window.x, g->window.y);
	_research->_lstResearch->configureCalypsoHdSelectionList(
		std::max(1, p(g->collection_scroll_track).w),
		std::max(1, p(g->collection_row_slot_1).h),
		std::max(1, p(g->collection_row_slot_1).h),
		_research->_hdWideLayout ? 5 : 2);
}

void CalypsoF14GlobalOperationsUi::applyManufactureGeometry()
{
	if (!_manufacture || !_manufacture->_window) return;
	ensureManufactureOwners();
	const auto *g = CalypsoF14GlobalProductionGen::layoutForDesign(
		_manufacture->_hdWideLayout ? 1280 : 740,
		_manufacture->_hdWideLayout ? 720 : 360);
	if (!g) return;
	setWindow(_manufacture->_window, g->window);
	const int wx = _manufacture->_window->getX(), wy = _manufacture->_window->getY();
	const double sx = static_cast<double>(_manufacture->_window->getWidth()) / g->window.w;
	const double sy = static_cast<double>(_manufacture->_window->getHeight()) / g->window.h;
	auto p = [&](const auto &rect) {
		return projectRect(rect, wx, wy, sx, sy, g->window.x, g->window.y);
	};
	_manufacture->_lstManufacture->rebaseNativeSize(g->collectionViewport.w,
		g->collectionViewport.h);
	place(_manufacture->_txtTitle, g->title, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_manufacture->_txtAvailable, g->summary_available, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_manufacture->_txtAllocated, g->summary_allocated, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_manufacture->_txtSpace, g->summary_workshop_space, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_manufacture->_txtFunds, g->summary_workshop_space, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_manufacture->_txtItem, g->collection_column_item, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_manufacture->_txtEngineers, g->collection_column_engineers, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_manufacture->_txtProduced, g->collection_column_produced, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_manufacture->_txtCost, g->collection_column_cost, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_manufacture->_txtTimeLeft, g->collection_column_time_left, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_manufacture->_lstManufacture, g->collectionViewport, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_manufacture->_btnOk, g->action_done, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_manufacture->_btnOpenBaseProduction, g->detail_selected_production_action_open_base_production, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_manufacture->_btnTechTree, g->detail_selected_production_action_tech_tree, wx, wy, sx, sy, g->window.x, g->window.y);
	_manufacture->_lstManufacture->configureCalypsoHdSelectionList(
		std::max(1, p(g->collection_scroll_track).w),
		std::max(1, p(g->collection_row_slot_1).h),
		std::max(1, p(g->collection_row_slot_1).h),
		_manufacture->_hdWideLayout ? 5 : 2);
}

void CalypsoF14GlobalOperationsUi::applyDiaryGeometry()
{
	if (!_diary || !_diary->_window) return;
	ensureDiaryOwners();
	const auto *g = CalypsoF14ResearchDiaryGen::layoutForDesign(
		_diary->_hdWideLayout ? 1280 : 740,
		_diary->_hdWideLayout ? 720 : 360);
	if (!g) return;
	setWindow(_diary->_window, g->window);
	const int wx = _diary->_window->getX(), wy = _diary->_window->getY();
	const double sx = static_cast<double>(_diary->_window->getWidth()) / g->window.w;
	const double sy = static_cast<double>(_diary->_window->getHeight()) / g->window.h;
	auto p = [&](const auto &rect) {
		return projectRect(rect, wx, wy, sx, sy, g->window.x, g->window.y);
	};
	_diary->_lstItems->rebaseNativeSize(g->collectionViewport.w,
		g->collectionViewport.h);
	place(_diary->_txtTitle, g->title, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_diary->_txtName, g->collection_column_name, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_diary->_txtType, g->collection_column_type, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_diary->_txtDate, g->collection_column_date, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_diary->_lstItems, g->collectionViewport, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_diary->_btnQuickSearch, g->toolbar_quick_search, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_diary->_btnQuickSearchToggle, g->toolbar_quick_search, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_diary->_sortName, g->toolbar_sort_name, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_diary->_sortDate, g->toolbar_sort_date, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_diary->_btnOk, g->action_done, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_diary->_btnOpenTechTree, g->detail_selected_entry_action_open_tech_tree, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_diary->_btnOpenUfopaedia, g->detail_selected_entry_action_open_ufopaedia, wx, wy, sx, sy, g->window.x, g->window.y);
	place(_diary->_txtTooltip, g->detail_selected_entry, wx, wy, sx, sy, g->window.x, g->window.y);
	_diary->_lstItems->configureCalypsoHdSelectionList(
		std::max(1, p(g->collection_scroll_track).w),
		std::max(1, p(g->collection_row_slot_1).h),
		std::max(1, p(g->collection_row_slot_1).h),
		_diary->_hdWideLayout ? 5 : 2);
}

CalypsoHdOperationsModel CalypsoF14GlobalOperationsUi::buildResearchModel() const
{
	CalypsoHdOperationsModel model;
	if (!_research || !_research->_window || !_research->_game
		|| !_research->_game->getSavedGame() || !_research->_lstResearch) return model;
	const auto tr = [this](const std::string &key) { return _research->tr(key); };
	const auto *g = CalypsoF14GlobalResearchGen::layoutForDesign(
		_research->_hdWideLayout ? 1280 : 740,
		_research->_hdWideLayout ? 720 : 360);
	if (!g) return model;
	const auto &generated = _research->_hdWideLayout
		? CalypsoF14GlobalResearchGen::kCollectionsWide[0]
		: CalypsoF14GlobalResearchGen::kCollectionsCompact[0];
	auto p = [&](const auto &rect) {
		return CalypsoHdOperationsRect{rect.x, rect.y, rect.w, rect.h};
	};
	model.archetype = CalypsoHdOperationsArchetype::OperationsWorkspace;
	model.familyId = CalypsoF14GlobalResearchGen::kFamilyId;
	model.ownerState = _research;
	model.visualShell = CalypsoF14GlobalResearchGen::kVisualShell;
	model.headerArtId = CalypsoF14GlobalResearchGen::kHeaderArt;
	model.baseCaption = tr("STR_BASES");
	model.baseName = "GLOBAL";
	model.sectionLabel = tr("STR_RESEARCH");
	model.title = _research->_txtTitle->getText();
	setResearchGeometry(model, *g, p);
	setGeneratedCollectionRows(model, generated, p);
	setCollectionGeometry(model, *g, p);
	model.geometry.collectionColumns.clear();
	for (int c = 0; c < generated.columnCount; ++c)
		model.geometry.collectionColumns.push_back(p(generated.columns[c].rect));
	model.collection.heading = tr("STR_CALYPSO_PROJECTS_ACROSS_ALL_BASES");
	model.collection.emptyTitle = tr("STR_CALYPSO_NO_ACTIVE_RESEARCH");
	model.collection.emptyBody = tr("STR_CALYPSO_NO_GLOBAL_RESEARCH_PROMPT");
	model.summaryFields.push_back(summary("available",
		tr("STR_CALYPSO_RESEARCH_SCIENTISTS_AVAILABLE"),
		std::to_string([&]() {
			int value = 0;
			for (const auto *base : *_research->_game->getSavedGame()->getBases())
				value += base->getAvailableScientists();
			return value;
		}()), p(g->summary_available), _research->_txtAvailable));
	model.summaryFields.push_back(summary("allocated",
		tr("STR_CALYPSO_RESEARCH_SCIENTISTS_ALLOCATED"),
		std::to_string([&]() {
			int value = 0;
			for (const auto *base : *_research->_game->getSavedGame()->getBases())
				value += base->getAllocatedScientists();
			return value;
		}()), p(g->summary_allocated), _research->_txtAllocated));
	model.summaryFields.push_back(summary("lab-space",
		tr("STR_CALYPSO_RESEARCH_LABORATORY_SPACE_AVAILABLE"),
		std::to_string([&]() {
			int value = 0;
			for (const auto *base : *_research->_game->getSavedGame()->getBases())
				value += base->getFreeLaboratories();
			return value;
		}()), p(g->summary_lab_space), _research->_txtSpace));
	const std::string columnLabels[] = {
		tr("STR_RESEARCH_PROJECT"),
		tr("STR_CALYPSO_RESEARCH_SCIENTISTS_ALLOCATED_UC"), tr("STR_PROGRESS")};
	for (int c = 0; c < generated.columnCount; ++c)
		model.collection.columns.push_back({
			generated.columns[c].id, columnLabels[c],
			p(generated.columns[c].rect), {}, generated.columns[c].contentRole});
	const auto &nativeRows = _research->_lstResearch->getCellTextsSnapshot();
	const std::size_t nativeSelected =
		static_cast<std::size_t>(_research->_lstResearch->getSelectedRow());
	const std::size_t nativeOffset = _research->_lstResearch->getScroll();
	for (std::size_t i = 0; i < nativeRows.size(); ++i)
	{
		const Base *base = i < _research->_bases.size() ? _research->_bases[i] : nullptr;
		const RuleResearch *topic = i < _research->_topics.size() ? _research->_topics[i] : nullptr;
		CalypsoHdOperationsRow row;
		row.id = "row-" + std::to_string(i);
		row.kind = base == nullptr ? CalypsoHdOperationsRowKind::GroupHeader
			: (topic == nullptr ? CalypsoHdOperationsRowKind::Empty
				: CalypsoHdOperationsRowKind::Item);
		row.state.disabled = row.kind == CalypsoHdOperationsRowKind::GroupHeader;
		row.state.selected = i == nativeSelected;
		for (std::size_t c = 0; c < generated.columnCount; ++c)
			row.values.push_back(c < nativeRows[i].size() && nativeRows[i][c]
				? nativeRows[i][c]->getText() : std::string());
		if (i >= nativeOffset && i - nativeOffset < generated.rowSlotCount)
			row.rect = model.geometry.collectionRows[i - nativeOffset];
		for (std::size_t c = 0; c < row.values.size(); ++c)
			row.cells.push_back({row.values[c], generated.columns[c].contentRole,
				{true, false, row.state.selected, row.state.disabled}});
		row.widget = _research->_lstResearch;
		model.collection.rows.push_back(std::move(row));
	}
	model.collection.selectedIndex = nativeSelected;
	model.collection.scrollOffset = nativeOffset;
	model.collection.count = nativeRows.size();
	model.collection.scroll = {
		nativeOffset, nativeRows.size(), static_cast<std::size_t>(generated.rowSlotCount),
		model.collection.viewport, model.collection.scrollTrack,
		model.collection.scrollThumb};
	const std::size_t selectedIndex = nativeSelected;
	const bool hasSelection = selectedIndex < model.collection.rows.size();
	const Base *selectedBase = hasSelection && selectedIndex < _research->_bases.size()
		? _research->_bases[selectedIndex] : nullptr;
	const RuleResearch *selectedTopic = hasSelection
		&& selectedIndex < _research->_topics.size()
			? _research->_topics[selectedIndex] : nullptr;
	model.detail.id = "selected-project";
	model.detail.panel = p(g->detailPanel);
	model.detail.identity.id = "selected-project";
	model.detail.identity.label = tr("STR_CALYPSO_RESEARCH_SELECTED_PROJECT");
	model.detail.identity.title = selectedTopic ? tr(selectedTopic->getName()) : tr("STR_NONE");
	model.detail.identity.subtitle = std::string();
	model.detail.identity.rect = p(g->detail_selected_project);
	model.detail.identity.titleRect = p(g->detail_selected_project_identity_title);
	model.detail.identity.subtitleRect = p(g->detail_selected_project_identity_subtitle);
	if (selectedBase)
		model.detail.metrics.push_back(metric("base", tr("STR_BASE"),
			selectedBase->getName(_research->_game->getLanguage()),
			p(g->detail_selected_project_metric_base)));
	if (selectedTopic)
		model.detail.metrics.push_back(metric("project",
			tr("STR_CALYPSO_RESEARCH_SELECTED_PROJECT"),
			tr(selectedTopic->getName()),
			p(g->detail_selected_project_metric_research_project)));
	model.detail.actions.push_back(action("open-base-research",
		tr("STR_OPEN_BASE_RESEARCH"), p(g->detail_selected_project_action_open_base_research),
		_research->_btnOpenBaseResearch, selectedBase != nullptr));
	model.detail.actions.push_back(action("tech-tree", tr("STR_TECH_TREE"),
		p(g->detail_selected_project_action_tech_tree), _research->_btnTechTree,
		selectedTopic != nullptr));
	model.footerActions.push_back(action("research-diary", _research->_btnDiary->getText(),
		p(g->action_research_diary), _research->_btnDiary));
	model.footerActions.push_back(action("done", _research->_btnOk->getText(),
		p(g->action_done), _research->_btnOk));
	suppress(model, _research->_window);
	suppress(model, _research->_btnDiary);
	suppress(model, _research->_btnOk);
	suppress(model, _research->_txtTitle);
	suppress(model, _research->_txtAvailable);
	suppress(model, _research->_txtAllocated);
	suppress(model, _research->_txtSpace);
	suppress(model, _research->_txtProject);
	suppress(model, _research->_txtScientists);
	suppress(model, _research->_txtProgress);
	suppress(model, _research->_lstResearch);
	suppress(model, _research->_btnOpenBaseResearch);
	suppress(model, _research->_btnTechTree);
	finish(model, _research->_game->getMod(),
		CalypsoF14GlobalResearchGen::kPresentationProfile,
		CalypsoF14GlobalResearchGen::kProfileId,
		CalypsoF14GlobalResearchGen::kProfileVersion,
		CalypsoF14GlobalResearchGen::kProvenanceTemplate);
	return model;
}

CalypsoHdOperationsModel CalypsoF14GlobalOperationsUi::buildManufactureModel() const
{
	CalypsoHdOperationsModel model;
	if (!_manufacture || !_manufacture->_window || !_manufacture->_game
		|| !_manufacture->_game->getSavedGame() || !_manufacture->_lstManufacture)
		return model;
	const auto tr = [this](const std::string &key) { return _manufacture->tr(key); };
	const auto *g = CalypsoF14GlobalProductionGen::layoutForDesign(
		_manufacture->_hdWideLayout ? 1280 : 740,
		_manufacture->_hdWideLayout ? 720 : 360);
	if (!g) return model;
	const auto &generated = _manufacture->_hdWideLayout
		? CalypsoF14GlobalProductionGen::kCollectionsWide[0]
		: CalypsoF14GlobalProductionGen::kCollectionsCompact[0];
	auto p = [&](const auto &rect) {
		return CalypsoHdOperationsRect{rect.x, rect.y, rect.w, rect.h};
	};
	model.archetype = CalypsoHdOperationsArchetype::OperationsWorkspace;
	model.familyId = CalypsoF14GlobalProductionGen::kFamilyId;
	model.ownerState = _manufacture;
	model.visualShell = CalypsoF14GlobalProductionGen::kVisualShell;
	model.headerArtId = CalypsoF14GlobalProductionGen::kHeaderArt;
	model.baseName = "GLOBAL";
	model.sectionLabel = tr("STR_MANUFACTURE");
	model.title = _manufacture->_txtTitle->getText();
	setProductionGeometry(model, *g, p);
	setGeneratedCollectionRows(model, generated, p);
	setCollectionGeometry(model, *g, p);
	model.geometry.collectionColumns.clear();
	for (int c = 0; c < generated.columnCount; ++c)
		model.geometry.collectionColumns.push_back(p(generated.columns[c].rect));
	model.collection.heading = tr("STR_CALYPSO_PRODUCTION_ACROSS_ALL_BASES");
	model.collection.meta = tr("STR_CALYPSO_FUNDS_VALUE").arg(
		Unicode::formatFunding(_manufacture->_game->getSavedGame()->getFunds()));
	model.collection.emptyTitle = tr("STR_CALYPSO_NO_ACTIVE_PRODUCTION");
	model.collection.emptyBody = tr("STR_CALYPSO_NO_GLOBAL_PRODUCTION_PROMPT");
	model.summaryFields.push_back(summary("available",
		tr("STR_CALYPSO_ENGINEERS_AVAILABLE"),
		std::to_string([&]() {
			int value = 0;
			for (const auto *base : *_manufacture->_game->getSavedGame()->getBases())
				value += base->getAvailableEngineers();
			return value;
		}()), p(g->summary_available), _manufacture->_txtAvailable));
	model.summaryFields.push_back(summary("allocated",
		tr("STR_CALYPSO_ENGINEERS_ALLOCATED"),
		std::to_string([&]() {
			int value = 0;
			for (const auto *base : *_manufacture->_game->getSavedGame()->getBases())
				value += base->getAllocatedEngineers();
			return value;
		}()), p(g->summary_allocated), _manufacture->_txtAllocated));
	model.summaryFields.push_back(summary("workshop-space",
		tr("STR_CALYPSO_WORKSHOP_SPACE"),
		std::to_string([&]() {
			int value = 0;
			for (const auto *base : *_manufacture->_game->getSavedGame()->getBases())
				value += base->getFreeWorkshops();
			return value;
		}()), p(g->summary_workshop_space), _manufacture->_txtSpace));
	const std::string columnLabels[] = {
		tr("STR_ITEM"), tr("STR_ENGINEERS__ALLOCATED"),
		tr("STR_UNITS_PRODUCED"), tr("STR_COST__PER__UNIT"),
		tr("STR_DAYS_HOURS_LEFT")};
	for (int c = 0; c < generated.columnCount; ++c)
		model.collection.columns.push_back({
			generated.columns[c].id, columnLabels[c],
			p(generated.columns[c].rect), {}, generated.columns[c].contentRole});
	const auto &nativeRows = _manufacture->_lstManufacture->getCellTextsSnapshot();
	const std::size_t nativeSelected =
		static_cast<std::size_t>(_manufacture->_lstManufacture->getSelectedRow());
	const std::size_t nativeOffset = _manufacture->_lstManufacture->getScroll();
	for (std::size_t i = 0; i < nativeRows.size(); ++i)
	{
		const Base *base = i < _manufacture->_bases.size()
			? _manufacture->_bases[i] : nullptr;
		const RuleManufacture *topic = i < _manufacture->_topics.size()
			? _manufacture->_topics[i] : nullptr;
		CalypsoHdOperationsRow row;
		row.id = "row-" + std::to_string(i);
		row.kind = base == nullptr ? CalypsoHdOperationsRowKind::GroupHeader
			: (topic == nullptr ? CalypsoHdOperationsRowKind::Empty
				: CalypsoHdOperationsRowKind::Item);
		row.state.disabled = row.kind == CalypsoHdOperationsRowKind::GroupHeader;
		row.state.selected = i == nativeSelected;
		for (std::size_t c = 0; c < generated.columnCount; ++c)
			row.values.push_back(c < nativeRows[i].size() && nativeRows[i][c]
				? nativeRows[i][c]->getText() : std::string());
		if (i >= nativeOffset && i - nativeOffset < generated.rowSlotCount)
			row.rect = model.geometry.collectionRows[i - nativeOffset];
		for (std::size_t c = 0; c < row.values.size(); ++c)
			row.cells.push_back({row.values[c], generated.columns[c].contentRole,
				{true, false, row.state.selected, row.state.disabled}});
		row.widget = _manufacture->_lstManufacture;
		model.collection.rows.push_back(std::move(row));
	}
	model.collection.selectedIndex = nativeSelected;
	model.collection.scrollOffset = nativeOffset;
	model.collection.count = nativeRows.size();
	model.collection.scroll = {
		nativeOffset, nativeRows.size(), static_cast<std::size_t>(generated.rowSlotCount),
		model.collection.viewport, model.collection.scrollTrack,
		model.collection.scrollThumb};
	const std::size_t selectedIndex = nativeSelected;
	const bool hasSelection = selectedIndex < model.collection.rows.size();
	const Base *selectedBase = hasSelection && selectedIndex < _manufacture->_bases.size()
		? _manufacture->_bases[selectedIndex] : nullptr;
	const RuleManufacture *selectedTopic = hasSelection
		&& selectedIndex < _manufacture->_topics.size()
			? _manufacture->_topics[selectedIndex] : nullptr;
	model.detail.id = "selected-production";
	model.detail.panel = p(g->detailPanel);
	model.detail.identity.id = "selected-production";
	model.detail.identity.label = tr("STR_CALYPSO_SELECTED_PRODUCTION");
	model.detail.identity.title = selectedTopic ? tr(selectedTopic->getName()) : tr("STR_NONE");
	model.detail.identity.subtitle = std::string();
	model.detail.identity.rect = p(g->detail_selected_production);
	model.detail.identity.titleRect = p(g->detail_selected_production_identity_title);
	model.detail.identity.subtitleRect = p(g->detail_selected_production_identity_subtitle);
	if (selectedBase)
		model.detail.metrics.push_back(metric("base", tr("STR_BASE"),
			selectedBase->getName(_manufacture->_game->getLanguage()),
			p(g->detail_selected_production_metric_base)));
	if (selectedTopic)
		model.detail.metrics.push_back(metric("item", tr("STR_ITEM"),
			tr(selectedTopic->getName()), p(g->detail_selected_production_metric_item)));
	model.detail.actions.push_back(action("open-base-production",
		tr("STR_OPEN_BASE_PRODUCTION"),
		p(g->detail_selected_production_action_open_base_production),
		_manufacture->_btnOpenBaseProduction, selectedBase != nullptr));
	model.detail.actions.push_back(action("tech-tree", tr("STR_TECH_TREE"),
		p(g->detail_selected_production_action_tech_tree), _manufacture->_btnTechTree,
		selectedTopic != nullptr));
	model.footerActions.push_back(action("done", _manufacture->_btnOk->getText(),
		p(g->action_done), _manufacture->_btnOk));
	suppress(model, _manufacture->_window);
	suppress(model, _manufacture->_btnOk);
	suppress(model, _manufacture->_txtTitle);
	suppress(model, _manufacture->_txtAvailable);
	suppress(model, _manufacture->_txtAllocated);
	suppress(model, _manufacture->_txtSpace);
	suppress(model, _manufacture->_txtFunds);
	suppress(model, _manufacture->_txtItem);
	suppress(model, _manufacture->_txtEngineers);
	suppress(model, _manufacture->_txtProduced);
	suppress(model, _manufacture->_txtCost);
	suppress(model, _manufacture->_txtTimeLeft);
	suppress(model, _manufacture->_lstManufacture);
	suppress(model, _manufacture->_btnOpenBaseProduction);
	suppress(model, _manufacture->_btnTechTree);
	finish(model, _manufacture->_game->getMod(),
		CalypsoF14GlobalProductionGen::kPresentationProfile,
		CalypsoF14GlobalProductionGen::kProfileId,
		CalypsoF14GlobalProductionGen::kProfileVersion,
		CalypsoF14GlobalProductionGen::kProvenanceTemplate);
	return model;
}
CalypsoHdOperationsModel CalypsoF14GlobalOperationsUi::buildDiaryModel() const
{
	CalypsoHdOperationsModel model;
	if (!_diary || !_diary->_window || !_diary->_game
		|| !_diary->_game->getSavedGame() || !_diary->_lstItems)
		return model;
	const auto tr = [this](const std::string &key) { return _diary->tr(key); };
	const auto *g = CalypsoF14ResearchDiaryGen::layoutForDesign(
		_diary->_hdWideLayout ? 1280 : 740,
		_diary->_hdWideLayout ? 720 : 360);
	if (!g) return model;
	const auto &generated = _diary->_hdWideLayout
		? CalypsoF14ResearchDiaryGen::kCollectionsWide[0]
		: CalypsoF14ResearchDiaryGen::kCollectionsCompact[0];
	auto p = [&](const auto &rect) {
		return CalypsoHdOperationsRect{rect.x, rect.y, rect.w, rect.h};
	};
	model.archetype = CalypsoHdOperationsArchetype::OperationsWorkspace;
	model.familyId = CalypsoF14ResearchDiaryGen::kFamilyId;
	model.ownerState = _diary;
	model.visualShell = CalypsoF14ResearchDiaryGen::kVisualShell;
	model.headerArtId = CalypsoF14ResearchDiaryGen::kHeaderArt;
	model.baseCaption = tr("STR_BASES");
	model.baseName = "GLOBAL";
	model.sectionLabel = tr("STR_RESEARCH");
	model.title = _diary->_txtTitle->getText();
	setDiaryGeometry(model, *g, p);
	setGeneratedCollectionRows(model, generated, p);
	setCollectionGeometry(model, *g, p);
	const std::string columnLabels[] = {
		tr("STR_NAME_UC"), tr("STR_TYPE"), tr("STR_DATE_UC")};
	for (int c = 0; c < generated.columnCount; ++c)
		model.collection.columns.push_back({
			generated.columns[c].id, columnLabels[c],
			p(generated.columns[c].rect), {}, generated.columns[c].contentRole});
	model.collection.emptyTitle = tr("STR_CALYPSO_NO_RESEARCH_RECORDS");
	model.collection.emptyBody = tr("STR_CALYPSO_NO_RESEARCH_RECORDS_PROMPT");
	const auto &nativeRows = _diary->_lstItems->getCellTextsSnapshot();
	model.collection.emptyKind = _diary->_btnQuickSearch->getText().empty()
		? "empty" : "no-match";
	const std::size_t nativeSelected =
		static_cast<std::size_t>(_diary->_lstItems->getSelectedRow());
	const std::size_t nativeOffset = _diary->_lstItems->getScroll();
	for (std::size_t i = 0; i < nativeRows.size(); ++i)
	{
		CalypsoHdOperationsRow row;
		row.id = "entry-" + std::to_string(i);
		row.state.selected = i == nativeSelected;
		for (std::size_t c = 0; c < generated.columnCount; ++c)
			row.values.push_back(c < nativeRows[i].size() && nativeRows[i][c]
				? nativeRows[i][c]->getText() : std::string());
		if (i >= nativeOffset && i - nativeOffset < generated.rowSlotCount)
			row.rect = model.geometry.collectionRows[i - nativeOffset];
		for (std::size_t c = 0; c < row.values.size(); ++c)
			row.cells.push_back({row.values[c], generated.columns[c].contentRole,
				{true, false, row.state.selected, false}});
		row.widget = _diary->_lstItems;
		model.collection.rows.push_back(std::move(row));
	}
	model.collection.selectedIndex = nativeSelected;
	model.collection.scrollOffset = nativeOffset;
	model.collection.count = nativeRows.size();
	model.collection.scroll = {
		nativeOffset, nativeRows.size(), static_cast<std::size_t>(generated.rowSlotCount),
		model.collection.viewport, model.collection.scrollTrack,
		model.collection.scrollThumb};
	const std::size_t selectedIndex = nativeSelected;
	const bool hasSelection = selectedIndex < _diary->_filteredItemList.size();
	const auto *selected = hasSelection ? _diary->_filteredItemList[selectedIndex] : nullptr;
	model.detail.id = "selected-entry";
	model.detail.panel = p(g->detailPanel);
	model.detail.identity.id = "selected-entry";
	model.detail.identity.label = tr("STR_CALYPSO_RESEARCH_SELECTED_ENTRY");
	model.detail.identity.title = selected ? selected->name : std::string(tr("STR_NONE"));
	model.detail.identity.subtitle = std::string();
	model.detail.identity.rect = p(g->detail_selected_entry);
	model.detail.identity.titleRect = p(g->detail_selected_entry_identity_title);
	model.detail.identity.subtitleRect = p(g->detail_selected_entry_identity_subtitle);
	if (selected)
	{
		model.detail.metrics.push_back(metric("name", tr("STR_NAME_UC"),
			selected->name, p(g->detail_selected_entry_metric_name)));
		model.detail.metrics.push_back(metric("type", tr("STR_TYPE"),
			std::to_string(static_cast<int>(selected->diaryEntry->source.type)),
			p(g->detail_selected_entry_metric_type)));
		model.detail.metrics.push_back(metric("date", tr("STR_DATE_UC"),
			selected->date, p(g->detail_selected_entry_metric_date)));
	}
	model.detail.actions.push_back(action("open-tech-tree", tr("STR_OPEN_TECH_TREE"),
		p(g->detail_selected_entry_action_open_tech_tree), _diary->_btnOpenTechTree,
		selected != nullptr));
	model.detail.actions.push_back(action("open-ufopaedia", tr("STR_OPEN_UFOPAEDIA"),
		p(g->detail_selected_entry_action_open_ufopaedia), _diary->_btnOpenUfopaedia,
		selected != nullptr));
	const std::string query = _diary->_btnQuickSearch->getText();
	model.toolbarActions.push_back(action("quick-search",
		query.empty() ? std::string(tr("STR_QUICK_SEARCH")) : query,
		p(g->toolbar_quick_search),
		_diary->_btnQuickSearch->getVisible()
			? static_cast<Surface *>(_diary->_btnQuickSearch)
			: static_cast<Surface *>(_diary->_btnQuickSearchToggle)));
	model.toolbarActions.push_back(action("sort-name", tr("STR_SORT_NAME"),
		p(g->toolbar_sort_name), _diary->_sortName));
	model.toolbarActions.push_back(action("sort-date", tr("STR_SORT_DATE"),
		p(g->toolbar_sort_date), _diary->_sortDate));
	model.footerActions.push_back(action("done", _diary->_btnOk->getText(),
		p(g->action_done), _diary->_btnOk));
	suppress(model, _diary->_window);
	suppress(model, _diary->_btnOk);
	suppress(model, _diary->_btnQuickSearch);
	suppress(model, _diary->_btnQuickSearchToggle);
	suppress(model, _diary->_txtTitle);
	suppress(model, _diary->_txtName);
	suppress(model, _diary->_txtType);
	suppress(model, _diary->_txtDate);
	suppress(model, _diary->_lstItems);
	suppress(model, _diary->_sortName);
	suppress(model, _diary->_sortDate);
	suppress(model, _diary->_txtTooltip);
	suppress(model, _diary->_btnOpenTechTree);
	suppress(model, _diary->_btnOpenUfopaedia);
	finish(model, _diary->_game->getMod(),
		CalypsoF14ResearchDiaryGen::kPresentationProfile,
		CalypsoF14ResearchDiaryGen::kProfileId,
		CalypsoF14ResearchDiaryGen::kProfileVersion,
		CalypsoF14ResearchDiaryGen::kProvenanceTemplate);
	return model;
}

} } // namespace OpenXcom::Calypso
#endif // __EMSCRIPTEN__