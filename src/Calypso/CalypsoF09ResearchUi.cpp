#ifdef __EMSCRIPTEN__
#include "CalypsoF09ResearchUi.h"

#include "CalypsoHdFontSource.h"
#include "CalypsoHdOperationsChrome.h"
#include "CalypsoHdOperationsRenderer.h"
#include "CalypsoHdUiOverlay.h"
#include "Generated/CalypsoF09ResearchCatalogue.generated.h"
#include "Generated/CalypsoF09ResearchQueue.generated.h"
#include "Generated/CalypsoF09ResearchStaffing.generated.h"
#include "../Basescape/NewResearchListState.h"
#include "../Basescape/ResearchInfoState.h"
#include "../Basescape/ResearchState.h"
#include "../Engine/Game.h"
#include "../Engine/Language.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Options.h"
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
#include "../Mod/RuleResearch.h"
#include "../Savegame/Base.h"
#include "../Savegame/ResearchProject.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/GameTime.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

namespace OpenXcom { namespace Calypso {
namespace {

template <typename R>
CalypsoHdOperationsRect projectRect(const R &r, int wx, int wy, double sx, double sy,
	int windowX, int windowY)
{
	return {
		wx + static_cast<int>(std::lround((r.x - windowX) * sx)),
		wy + static_cast<int>(std::lround((r.y - windowY) * sy)),
		std::max(1, static_cast<int>(std::lround(r.w * sx))),
		std::max(1, static_cast<int>(std::lround(r.h * sy)))};
}

template <typename R>
CalypsoHdOperationsRect rawRect(const R &r)
{
	return {r.x, r.y, r.w, r.h};
}

template <typename R>
void setSurfaceRect(Surface *surface, const R &rect)
{
	if (!surface) return;
	if (surface->getX() != rect.x) surface->setX(rect.x);
	if (surface->getY() != rect.y) surface->setY(rect.y);
	if (surface->getWidth() != rect.w) surface->setWidth(rect.w);
	if (surface->getHeight() != rect.h) surface->setHeight(rect.h);
}

template <typename R>
void setProjectedSurfaceRect(Surface *surface, const R &rect,
	int wx, int wy, double sx, double sy, int windowX, int windowY)
{
	if (!surface) return;
	const auto projected = projectRect(rect, wx, wy, sx, sy, windowX, windowY);
	setSurfaceRect(surface, projected);
}


void setFonts(CalypsoHdOperationsModel &model, const Mod *mod)
{
	model.readiness.contractReady = true;
	model.readiness.uploadsReady = true;
	model.readiness.retryable = true;
	model.readiness.fontsReady =
		calypsoHdResolveFontDescriptor(mod, "FONT_CC_INTER_SB", model.headingFont)
		&& calypsoHdResolveFontDescriptor(mod, "FONT_CC_INTER_R", model.bodyFont)
		&& calypsoHdResolveFontDescriptor(mod, "FONT_CC_PLEX_R", model.monoFont);
}
void setClock(CalypsoHdOperationsModel &model, Game *game)
{
	if (!game || !game->getSavedGame()) return;
	const GameTime *time = game->getSavedGame()->getTime();
	if (!time) return;
	std::ostringstream clock;
	clock << time->getHour() << ":" << std::setfill('0') << std::setw(2)
		<< time->getMinute();
	model.clockTime = clock.str();
	model.clockDate = time->getDayString(game->getLanguage()) + " "
		+ std::string(game->getLanguage()->getString(time->getMonthString())) + " "
		+ std::to_string(time->getYear());
}


CalypsoHdOperationsAction action(const std::string &id, const std::string &label,
	const CalypsoHdOperationsRect &rect, const void *widget, bool visible = true,
	const std::string &tone = "normal")
{
	CalypsoHdOperationsAction out;
	out.id = id;
	out.label = label;
	out.component = "management-action-group";
	out.slotRole = "action";
	out.coordinateSpace = "logical";
	out.tone = tone;
	out.visible = rect;
	out.hit = rect;
	out.widget = widget;
	out.state.visible = visible;
	return out;
}

CalypsoHdOperationsMetric metric(const std::string &id, const std::string &label,
	const std::string &value, const CalypsoHdOperationsRect &rect)
{
	CalypsoHdOperationsMetric out;
	out.id = id;
	out.label = label;
	out.value = value;
	out.rect = rect;
	return out;
}

CalypsoHdOperationsSummaryField summary(const std::string &id, const std::string &label,
	const std::string &value, const CalypsoHdOperationsRect &rect, const void *widget)
{
	CalypsoHdOperationsSummaryField out;
	out.id = id;
	out.label = label;
	out.value = value;
	out.rect = rect;
	out.widget = widget;
	return out;
}

void setWindow(Window *window, const CalypsoHdOperationsRect &rect)
{
	setSurfaceRect(window, rect);
}

void setOperationsWindow(Window *window, const CalypsoHdOperationsRect &rect)
{
	setSurfaceRect(window, calypsoHdOperationsProjectForCurrentPresentation(
		rect, rect.w, rect.h));
}


} // namespace



CalypsoF09ResearchUi::CalypsoF09ResearchUi(ResearchState *state)
	: _kind(Kind::Queue), _queue(state),
	  _chrome(new CalypsoHdOperationsChrome(*state))
{
	_renderer = new CalypsoHdOperationsRenderer(state, CalypsoHdOperationsModel{});
	_renderer->setModelProvider([this]() { syncGeometry(); return buildModel(); });
}

CalypsoF09ResearchUi::CalypsoF09ResearchUi(NewResearchListState *state)
	: _kind(Kind::Catalogue), _catalogue(state),
	  _chrome(new CalypsoHdOperationsChrome(*state))
{
	_renderer = new CalypsoHdOperationsRenderer(state, CalypsoHdOperationsModel{});
	_renderer->setModelProvider([this]() { syncGeometry(); return buildModel(); });
}

CalypsoF09ResearchUi::CalypsoF09ResearchUi(ResearchInfoState *state)
	: _kind(Kind::Staffing), _staffing(state),
	  _chrome(new CalypsoHdOperationsChrome(*state))
{
	_renderer = new CalypsoHdOperationsRenderer(state, CalypsoHdOperationsModel{});
	_renderer->setModelProvider([this]() { syncGeometry(); return buildModel(); });
}

CalypsoF09ResearchUi::~CalypsoF09ResearchUi()
{
	delete _renderer;
	delete _chrome;
}

void CalypsoF09ResearchUi::configure(ResearchState &state)
{
	if (state._hdAdapter != nullptr) return;
	if (!calypsoHdOperationsRouteEnabled(state._game, "F09"))
	{
		state._hdLayout = false;
		return;
	}
	state._hdLayout = true;
	state._hdWideLayout = Options::baseXResolution >= 1000;
	auto *adapter = new CalypsoF09ResearchUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter->_renderer);
	adapter->refresh();
	calypsoHdOperationsPublishHarnessVisibility();
}

void CalypsoF09ResearchUi::configure(NewResearchListState &state)
{
	if (state._hdAdapter != nullptr) return;
	if (!calypsoHdOperationsRouteEnabled(state._game, "F09"))
	{
		state._hdLayout = false;
		return;
	}
	state._hdLayout = true;
	state._hdWideLayout = Options::baseXResolution >= 1000;
	auto *adapter = new CalypsoF09ResearchUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter->_renderer);
	adapter->refresh();
	calypsoHdOperationsPublishHarnessVisibility();
}

void CalypsoF09ResearchUi::configure(ResearchInfoState &state)
{
	if (state._hdAdapter != nullptr) return;
	if (!calypsoHdOperationsRouteEnabled(state._game, "F09"))
	{
		state._hdLayout = false;
		return;
	}
	state._screen = true;
	state._hdLayout = true;
	state._hdWideLayout = Options::baseXResolution >= 1000;
	auto *adapter = new CalypsoF09ResearchUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter->_renderer);
	adapter->refresh();
	calypsoHdOperationsPublishHarnessVisibility();
}

bool CalypsoF09ResearchUi::resize(ResearchState &state)
{
	if (!state._hdLayout || !state._hdAdapter) return false;
	state._hdWideLayout = Options::baseXResolution >= 1000;
	state._hdAdapter->refresh();
	return true;
}

bool CalypsoF09ResearchUi::resize(NewResearchListState &state)
{
	if (!state._hdLayout || !state._hdAdapter) return false;
	state._hdWideLayout = Options::baseXResolution >= 1000;
	state._hdAdapter->refresh();
	return true;
}

bool CalypsoF09ResearchUi::resize(ResearchInfoState &state)
{
	if (!state._hdLayout || !state._hdAdapter) return false;
	state._hdWideLayout = Options::baseXResolution >= 1000;
	state._hdAdapter->refresh();
	return true;
}

void CalypsoF09ResearchUi::syncGeometry()
{
	switch (_kind)
	{
	case Kind::Queue: applyQueueGeometry(); break;
	case Kind::Catalogue: applyCatalogueGeometry(); break;
	case Kind::Staffing: applyStaffingGeometry(); break;
	}
	if (_chrome) _chrome->applyGeometry();
}

void CalypsoF09ResearchUi::refresh()
{
	if (!_renderer) return;
	syncGeometry();
	_renderer->setModel(buildModel());
}

CalypsoHdOperationsModel CalypsoF09ResearchUi::buildModel() const
{
	CalypsoHdOperationsModel model;
	switch (_kind)
	{
	case Kind::Queue: model = buildQueueModel(); break;
	case Kind::Catalogue: model = buildCatalogueModel(); break;
	case Kind::Staffing: model = buildStaffingModel(); break;
	}
	if (_chrome) _chrome->populateModel(model);
	return model;
}

void CalypsoF09ResearchUi::ensureQueueOwners()
{
	if (!_queue) return;
	if (!_queue->_btnGlobalOverview)
	{
		_queue->_btnGlobalOverview = new TextButton(1, 1, 0, 0);
		_queue->_btnGlobalOverview->setText(_queue->tr("STR_GLOBAL_OVERVIEW"));
		_queue->add(_queue->_btnGlobalOverview, "button", "researchMenu");
		_queue->_btnGlobalOverview->onMouseClick(
			(ActionHandler)&ResearchState::onCurrentGlobalResearchClick);
	}
	if (!_queue->_btnOpenProject)
	{
		_queue->_btnOpenProject = new TextButton(1, 1, 0, 0);
		_queue->_btnOpenProject->setText(_queue->tr("STR_OPEN_PROJECT"));
		_queue->add(_queue->_btnOpenProject, "button", "researchMenu");
		_queue->_btnOpenProject->onMouseClick(
			(ActionHandler)&ResearchState::onSelectProject);
	}
	if (!_queue->_btnTechTree)
	{
		_queue->_btnTechTree = new TextButton(1, 1, 0, 0);
		_queue->_btnTechTree->setText(_queue->tr("STR_TECH_TREE"));
		_queue->add(_queue->_btnTechTree, "button", "researchMenu");
		_queue->_btnTechTree->onMouseClick(
			(ActionHandler)&ResearchState::onOpenTechTreeViewer);
	}
}

void CalypsoF09ResearchUi::ensureCatalogueOwners()
{
	if (!_catalogue) return;
	auto make = [&](TextButton *&button, ActionHandler handler)
	{
		if (button) return;
		button = new TextButton(1, 1, 0, 0);
		button->setText("");
		_catalogue->add(button, "button", "selectNewResearch");
		button->onMouseClick(handler);
	};
	make(_catalogue->_btnReview,
		(ActionHandler)&NewResearchListState::onSelectProject);
	make(_catalogue->_btnChangeVisibility,
		(ActionHandler)&NewResearchListState::onToggleProjectStatus);
	make(_catalogue->_btnTechTree,
		(ActionHandler)&NewResearchListState::onOpenTechTreeViewer);
	make(_catalogue->_btnMarkAllSeen,
		(ActionHandler)&NewResearchListState::btnMarkAllAsSeenClick);
}

void CalypsoF09ResearchUi::applyQueueGeometry()
{
	if (!_queue || !_queue->_window) return;
	ensureQueueOwners();
	const bool wide = _queue->_hdWideLayout;
	const auto *g = CalypsoF09ResearchQueueGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) return;
	setOperationsWindow(_queue->_window, rawRect(g->window));
	const int wx = _queue->_window->getX(), wy = _queue->_window->getY();
	const double sx = static_cast<double>(_queue->_window->getWidth()) / g->window.w;
	const double sy = static_cast<double>(_queue->_window->getHeight()) / g->window.h;
	_queue->_lstResearch->rebaseNativeSize(
		g->collectionViewport.w, g->collectionViewport.h);
	auto place = [&](Surface *surface, const auto &rect)
	{
		setProjectedSurfaceRect(surface, rect, wx, wy, sx, sy,
			g->window.x, g->window.y);
	};
	place(_queue->_txtTitle, g->title);
	place(_queue->_txtAvailable, g->summary_available);
	place(_queue->_txtAllocated, g->summary_allocated);
	place(_queue->_txtSpace, g->summary_lab_space);
	place(_queue->_txtProject, g->collection_column_project);
	place(_queue->_txtScientists, g->collection_column_scientists);
	place(_queue->_txtProgress, g->collection_column_progress);
	place(_queue->_lstResearch, g->collectionViewport);
	place(_queue->_btnGlobalOverview, g->action_global_overview);
	place(_queue->_btnNew, g->action_new_project);
	place(_queue->_btnOk, g->action_done);
	place(_queue->_btnOpenProject, g->detail_selected_project_action_open_project);
	place(_queue->_btnTechTree, g->detail_selected_project_action_tech_tree);
	_queue->_lstResearch->configureCalypsoHdSelectionList(
		std::max(1, static_cast<int>(g->collection_scroll_track.w * sx)),
		std::max(1, static_cast<int>(44 * sy)),
		std::max(1, static_cast<int>(g->collection_row_slot_1.h * sy)),
		wide ? 5 : 2);
}

void CalypsoF09ResearchUi::applyCatalogueGeometry()
{
	if (!_catalogue || !_catalogue->_window) return;
	ensureCatalogueOwners();
	const bool wide = _catalogue->_hdWideLayout;
	const auto *g = CalypsoF09ResearchCatalogueGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) return;
	setOperationsWindow(_catalogue->_window, rawRect(g->window));
	const int wx = _catalogue->_window->getX(), wy = _catalogue->_window->getY();
	const double sx = static_cast<double>(_catalogue->_window->getWidth()) / g->window.w;
	const double sy = static_cast<double>(_catalogue->_window->getHeight()) / g->window.h;
	_catalogue->_lstResearch->rebaseNativeSize(
		g->collectionViewport.w, g->collectionViewport.h);
	auto place = [&](Surface *surface, const auto &rect)
	{
		setProjectedSurfaceRect(surface, rect, wx, wy, sx, sy,
			g->window.x, g->window.y);
	};
	place(_catalogue->_txtTitle, g->title);
	place(_catalogue->_lstResearch, g->collectionViewport);
	place(_catalogue->_btnReview, g->detail_selected_project_action_review_project);
	place(_catalogue->_btnChangeVisibility,
		g->detail_selected_project_action_change_visibility);
	place(_catalogue->_btnTechTree, g->detail_selected_project_action_tech_tree);
	place(_catalogue->_btnMarkAllSeen, g->action_mark_all_seen);
	place(_catalogue->_btnOK, g->action_done);
	place(_catalogue->_btnQuickSearch, g->toolbar_quick_search);
	place(_catalogue->_cbxSort, g->toolbar_sort_default);
	place(_catalogue->_btnShowOnlyNew, g->toolbar_show_only_new);
	_catalogue->_lstResearch->configureCalypsoHdSelectionList(
		std::max(1, static_cast<int>(g->collection_scroll_track.w * sx)),
		std::max(1, static_cast<int>(44 * sy)),
		std::max(1, static_cast<int>(g->collection_row_slot_1.h * sy)),
		wide ? 5 : 2);
}

CalypsoHdOperationsModel CalypsoF09ResearchUi::buildQueueModel() const
{
	CalypsoHdOperationsModel model;
	if (!_queue || !_queue->_window || !_queue->_game) return model;
	const auto tr = [this](const std::string &key) { return _queue->tr(key); };
	const bool wide = _queue->_hdWideLayout;
	const auto *g = CalypsoF09ResearchQueueGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) return model;
	auto p = [&](const auto &r) {
		return rawRect(r);
	};
	model.archetype = CalypsoHdOperationsArchetype::OperationsWorkspace;
	model.familyId = CalypsoF09ResearchQueueGen::kFamilyId;
	model.ownerState = _queue;
	model.visualShell = CalypsoF09ResearchQueueGen::kVisualShell;
	model.headerArtId = CalypsoF09ResearchQueueGen::kHeaderArt;
	model.baseName = _queue->_base->getName();
	model.sectionLabel = tr("STR_RESEARCH");
	setClock(model, _queue->_game);
	model.suppressedWidgets = {
		_queue->_window, _queue->_btnNew, _queue->_btnOk,
		_queue->_txtTitle, _queue->_txtAvailable, _queue->_txtAllocated,
		_queue->_txtSpace, _queue->_txtProject, _queue->_txtScientists,
		_queue->_txtProgress, _queue->_lstResearch,
		_queue->_btnGlobalOverview, _queue->_btnOpenProject, _queue->_btnTechTree};
	model.title = tr("STR_CURRENT_RESEARCH");
	model.geometry.designWidth = g->designWidth;
	model.geometry.designHeight = g->designHeight;
	model.geometry.window = p(g->window);
	model.geometry.title = p(g->title);
	model.geometry.screenHeader = p(g->screenHeader);
	model.geometry.headerArt = p(g->headerArt);
	model.geometry.footer = p(g->footer);
	model.geometry.summaryBar = p(g->summaryBar);
	model.geometry.toolbarBar = p(g->toolbarBar);
	model.geometry.collectionViewport = p(g->collectionViewport);
	model.geometry.detailPanel = p(g->detailPanel);
	model.geometry.detailIdentity = p(g->detail_selected_project_label);
	model.geometry.collectionScrollTrack = p(g->collection_scroll_track);
	model.geometry.collectionScrollThumb = p(g->collection_scroll_thumb);
	model.geometry.collectionColumns = {
		p(g->collection_column_project), p(g->collection_column_scientists),
		p(g->collection_column_progress)};
	model.geometry.collectionRows = {
		p(g->collection_row_slot_1), p(g->collection_row_slot_2),
		p(g->collection_row_slot_3), p(g->collection_row_slot_4),
		p(g->collection_row_slot_5)};
	model.geometry.collectionRows.resize(wide ? 5 : 2);
	model.geometry.detailIdentityTitle = p(g->detail_selected_project_identity_title);
	model.geometry.detailIdentitySubtitle = p(g->detail_selected_project_identity_subtitle);
	model.geometry.detailMetrics = {
		p(g->detail_selected_project_metric_scientists),
		p(g->detail_selected_project_metric_progress)};
	model.geometry.detailActions = {
		p(g->detail_selected_project_action_open_project),
		p(g->detail_selected_project_action_tech_tree)};
	model.geometry.footerActions = {
		p(g->action_global_overview), p(g->action_new_project), p(g->action_done)};
	model.summaryFields.push_back(summary("available",
		tr("STR_CALYPSO_RESEARCH_SCIENTISTS_AVAILABLE"),
		std::to_string(_queue->_base->getAvailableScientists()),
		p(g->summary_available), _queue->_txtAvailable));
	model.summaryFields.push_back(summary("allocated",
		tr("STR_CALYPSO_RESEARCH_SCIENTISTS_ALLOCATED"),
		std::to_string(_queue->_base->getAllocatedScientists()),
		p(g->summary_allocated), _queue->_txtAllocated));
	model.summaryFields.push_back(summary("lab-space",
		tr("STR_CALYPSO_RESEARCH_LABORATORY_SPACE_AVAILABLE"),
		std::to_string(_queue->_base->getFreeLaboratories()),
		p(g->summary_lab_space), _queue->_txtSpace));
	model.collection.columns.push_back(
		{"project", tr("STR_RESEARCH_PROJECT"), p(g->collection_column_project), {}});
	model.collection.columns.push_back(
		{"scientists", tr("STR_CALYPSO_RESEARCH_SCIENTISTS_ALLOCATED_UC"),
			p(g->collection_column_scientists), {}});
	model.collection.columns.push_back(
		{"progress", tr("STR_PROGRESS"), p(g->collection_column_progress), {}});
	const auto &projects = _queue->_base->getResearch();
	model.collection.heading = tr("STR_CALYPSO_RESEARCH_PROJECTS");
	model.collection.meta = tr("STR_CALYPSO_ACTIVE_PROJECT_COUNT").arg(projects.size());
	model.collection.emptyTitle = tr("STR_CALYPSO_NO_ACTIVE_RESEARCH");
	model.collection.emptyBody = tr("STR_CALYPSO_START_RESEARCH_PROMPT");
	for (std::size_t i = 0; i < projects.size(); ++i)
	{
		const auto *project = projects[i];
		CalypsoHdOperationsRow row;
		row.id = project->getRules()->getName();
		row.values = {
			tr(project->getRules()->getName()),
			std::to_string(project->getAssigned()),
			tr(project->getResearchProgress())};
		row.rect = model.geometry.collectionRows.empty()
			? model.geometry.collectionViewport
			: model.geometry.collectionRows[
				std::min(i, model.geometry.collectionRows.size() - 1)];
		row.state.selected = i == _queue->_lstResearch->getSelectedRow();
		row.widget = _queue->_lstResearch;
		model.collection.rows.push_back(std::move(row));
	}
	model.collection.selectedIndex = projects.empty() ? 0 : _queue->_lstResearch->getSelectedRow();
	model.collection.scrollOffset = _queue->_lstResearch->getScroll();
	model.collection.visibleRows = model.geometry.collectionRows.size();
	model.collection.rowHeight = model.geometry.collectionRows.empty()
		? 0 : model.geometry.collectionRows.front().h;
	model.collection.rowSlots = model.geometry.collectionRows;
	const bool hasSelection = !projects.empty()
		&& model.collection.selectedIndex < projects.size();
	_queue->_btnOpenProject->setVisible(hasSelection);
	_queue->_btnTechTree->setVisible(hasSelection);
	const auto *selected = hasSelection ? projects[model.collection.selectedIndex] : nullptr;
	model.detail.id = "selected-project";
	model.detail.panel = p(g->detailPanel);
	model.detail.identity.id = "selected-project";
	model.detail.identity.label = tr("STR_CALYPSO_RESEARCH_SELECTED_PROJECT");
	model.detail.identity.title = selected
		? std::string(tr(selected->getRules()->getName()))
		: std::string(tr("STR_NONE"));
	model.detail.identity.subtitle = selected
		? std::string(tr("STR_RESEARCH_PROJECT"))
		: _queue->_btnNew->getText();
	model.detail.identity.rect = p(g->detail_selected_project);
	model.detail.identity.titleRect = p(g->detail_selected_project_identity_title);
	model.detail.identity.subtitleRect = p(g->detail_selected_project_identity_subtitle);
	if (selected)
	{
		model.detail.metrics.push_back(metric("scientists",
			tr("STR_CALYPSO_RESEARCH_SCIENTISTS_ALLOCATED"),
			std::to_string(selected->getAssigned()),
			p(g->detail_selected_project_metric_scientists)));
		model.detail.metrics.push_back(metric("progress", tr("STR_PROGRESS"),
			tr(selected->getResearchProgress()),
			p(g->detail_selected_project_metric_progress)));
	}
	model.detail.actions.push_back(action("open-project", _queue->_btnOpenProject->getText(),
		p(g->detail_selected_project_action_open_project), _queue->_btnOpenProject,
		hasSelection, "primary"));
	model.detail.actions.push_back(action("tech-tree", _queue->_btnTechTree->getText(),
		p(g->detail_selected_project_action_tech_tree), _queue->_btnTechTree,
		hasSelection));
	model.footerActions.push_back(action("global-overview",
		_queue->_btnGlobalOverview->getText(),
		p(g->action_global_overview), _queue->_btnGlobalOverview));
	auto newProject = action("new-project", _queue->_btnNew->getText(),
		p(g->action_new_project), _queue->_btnNew);
	model.footerActions.push_back(std::move(newProject));
	model.footerActions.push_back(action("done", _queue->_btnOk->getText(),
		p(g->action_done), _queue->_btnOk, true, "primary"));
	setFonts(model, _queue->_game->getMod());
	calypsoHdOperationsClampSelectionAndScroll(model);
	return model;
}

CalypsoHdOperationsModel CalypsoF09ResearchUi::buildCatalogueModel() const
{
	CalypsoHdOperationsModel model;
	if (!_catalogue || !_catalogue->_window || !_catalogue->_game) return model;
	const auto tr = [this](const std::string &key) { return _catalogue->tr(key); };
	const bool wide = _catalogue->_hdWideLayout;
	const auto *g = CalypsoF09ResearchCatalogueGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) return model;
	auto p = [&](const auto &r) {
		return rawRect(r);
	};
	model.archetype = CalypsoHdOperationsArchetype::OperationsWorkspace;
	model.familyId = CalypsoF09ResearchCatalogueGen::kFamilyId;
	model.ownerState = _catalogue;
	model.visualShell = CalypsoF09ResearchCatalogueGen::kVisualShell;
	model.headerArtId = CalypsoF09ResearchCatalogueGen::kHeaderArt;
	model.baseName = _catalogue->_base->getName();
	model.sectionLabel = tr("STR_RESEARCH");
	setClock(model, _catalogue->_game);
	model.suppressedWidgets = {
		_catalogue->_window, _catalogue->_btnQuickSearch, _catalogue->_btnOK,
		_catalogue->_cbxSort, _catalogue->_btnShowOnlyNew, _catalogue->_txtTitle,
		_catalogue->_lstResearch, _catalogue->_btnReview,
		_catalogue->_btnChangeVisibility, _catalogue->_btnTechTree,
		_catalogue->_btnMarkAllSeen};
	model.title = tr("STR_NEW_RESEARCH_PROJECTS");
	model.geometry.designWidth = g->designWidth;
	model.geometry.designHeight = g->designHeight;
	model.geometry.window = p(g->window);
	model.geometry.title = p(g->title);
	model.geometry.screenHeader = p(g->screenHeader);
	model.geometry.headerArt = p(g->headerArt);
	model.geometry.summaryBar = p(g->summaryBar);
	model.geometry.collectionScrollThumb = p(g->collection_scroll_thumb);
	model.geometry.toolbarBar = p(g->toolbarBar);
	model.geometry.collectionViewport = p(g->collectionViewport);
	model.geometry.detailPanel = p(g->detailPanel);
	model.geometry.footer = p(g->footer);
	model.geometry.collectionScrollTrack = p(g->collection_scroll_track);
	model.geometry.detailIdentity = p(g->detail_selected_project_label);
	model.geometry.collectionColumns = {
		p(g->collection_column_project), p(g->collection_column_status)};
	model.geometry.collectionRows = {
		p(g->collection_row_slot_1), p(g->collection_row_slot_2),
		p(g->collection_row_slot_3), p(g->collection_row_slot_4),
		p(g->collection_row_slot_5)};
	model.geometry.collectionRows.resize(wide ? 5 : 2);
	model.geometry.detailIdentityTitle = p(g->detail_selected_project_identity_title);
	model.geometry.detailIdentitySubtitle = p(g->detail_selected_project_identity_subtitle);
	model.geometry.detailMetrics = {p(g->detail_selected_project_metric_status)};
	model.geometry.detailActions = {
		p(g->detail_selected_project_action_review_project),
		p(g->detail_selected_project_action_change_visibility),
		p(g->detail_selected_project_action_tech_tree)};
	model.geometry.footerActions = {p(g->action_mark_all_seen), p(g->action_done)};
	model.collection.columns.push_back(
		{"project", tr("STR_RESEARCH_PROJECT"), p(g->collection_column_project), {}});
	model.collection.columns.push_back(
		{"status", tr("STR_CALYPSO_RESEARCH_STATUS"), p(g->collection_column_status), {}});
	model.collection.emptyTitle = tr("STR_CALYPSO_NO_AVAILABLE_RESEARCH");
	model.collection.emptyBody = tr("STR_CALYPSO_NO_AVAILABLE_RESEARCH_PROMPT");
	for (std::size_t i = 0; i < _catalogue->_projects.size(); ++i)
	{
		const RuleResearch *rule = _catalogue->_projects[i];
		const int status = _catalogue->_game->getSavedGame()
			->getResearchRuleStatus(rule->getName());
		const std::string statusText = status == RuleResearch::RESEARCH_STATUS_NEW
			? tr("STR_NEW_PROJECT")
			: status == RuleResearch::RESEARCH_STATUS_HIDDEN
				? tr("STR_FILTER_HIDDEN") : tr("STR_NORMAL");
		CalypsoHdOperationsRow row;
		row.id = rule->getName();
		row.values = {tr(rule->getName()), statusText};
		row.rect = model.geometry.collectionRows.empty()
			? model.geometry.collectionViewport
			: model.geometry.collectionRows[
				std::min(i, model.geometry.collectionRows.size() - 1)];
		row.state.selected = i == _catalogue->_lstResearch->getSelectedRow();
		row.widget = _catalogue->_lstResearch;
		model.collection.rows.push_back(std::move(row));
	}
	model.collection.selectedIndex = _catalogue->_projects.empty()
		? 0 : _catalogue->_lstResearch->getSelectedRow();
	model.collection.scrollOffset = _catalogue->_lstResearch->getScroll();
	model.collection.visibleRows = model.geometry.collectionRows.size();
	model.collection.rowHeight = model.geometry.collectionRows.empty()
		? 0 : model.geometry.collectionRows.front().h;
	model.collection.rowSlots = model.geometry.collectionRows;

	const bool hasSelection = !_catalogue->_projects.empty()
		&& model.collection.selectedIndex < _catalogue->_projects.size();
	_catalogue->_btnReview->setVisible(hasSelection);
	_catalogue->_btnChangeVisibility->setVisible(hasSelection);
	_catalogue->_btnTechTree->setVisible(hasSelection);
	const auto *selected = hasSelection
		? _catalogue->_projects[model.collection.selectedIndex] : nullptr;
	const int selectedStatus = selected
		? _catalogue->_game->getSavedGame()->getResearchRuleStatus(selected->getName()) : 0;
	const std::string selectedStatusText = selectedStatus == RuleResearch::RESEARCH_STATUS_NEW
		? tr("STR_NEW_PROJECT")
		: selectedStatus == RuleResearch::RESEARCH_STATUS_HIDDEN
			? tr("STR_FILTER_HIDDEN") : tr("STR_NORMAL");
	model.detail.identity.title = selected ? tr(selected->getName()) : tr("STR_NONE");
	model.detail.panel = p(g->detailPanel);
	model.detail.identity.id = "selected-project";
	model.detail.identity.label = tr("STR_CALYPSO_RESEARCH_SELECTED_PROJECT");
	model.detail.identity.subtitle = tr("STR_RESEARCH_PROJECT");
	model.detail.identity.rect = p(g->detail_selected_project);
	model.detail.identity.titleRect = p(g->detail_selected_project_identity_title);
	model.detail.identity.subtitleRect = p(g->detail_selected_project_identity_subtitle);
	if (selected)
		model.detail.metrics.push_back(metric("status",
			tr("STR_CALYPSO_RESEARCH_VISIBILITY_STATUS"),
			selectedStatusText, p(g->detail_selected_project_metric_status)));
	model.detail.actions.push_back(action("review-project", tr("STR_REVIEW_PROJECT"),
		p(g->detail_selected_project_action_review_project), _catalogue->_btnReview,
		hasSelection, "primary"));
	auto visibility = action("change-visibility", tr("STR_CHANGE_VISIBILITY"),
		p(g->detail_selected_project_action_change_visibility),
		_catalogue->_btnChangeVisibility, hasSelection);
	visibility.state.disabled = !_catalogue->_isSortingEnabled
		&& !Options::oxceHighlightNewTopics;
	model.detail.actions.push_back(std::move(visibility));
	model.detail.actions.push_back(action("tech-tree", tr("STR_TECH_TREE"),
		p(g->detail_selected_project_action_tech_tree), _catalogue->_btnTechTree,
		hasSelection));
	const char *sortLabelKey = "STR_SORT_DEFAULT";
	switch (_catalogue->_cbxSort->getSelected())
	{
	case 1: sortLabelKey = "STR_SORT_BY_COST"; break;
	case 2: sortLabelKey = "STR_SORT_BY_NAME"; break;
	case 3: sortLabelKey = "STR_SHOW_ONLY_NEW"; break;
	case 4: sortLabelKey = "STR_FILTER_HIDDEN"; break;
	default: break;
	}
	if (_catalogue->_isSortingEnabled)
		model.toolbarActions.push_back(action("sort", tr(sortLabelKey),
			p(g->toolbar_sort_default), _catalogue->_cbxSort,
			_catalogue->_cbxSort->getVisible()));
	auto showOnlyNew = action("show-only-new",
		_catalogue->_btnShowOnlyNew->getText(),
		p(g->toolbar_show_only_new), _catalogue->_btnShowOnlyNew,
		_catalogue->_btnShowOnlyNew->getVisible());
	showOnlyNew.state.selected = _catalogue->_btnShowOnlyNew->getPressed();
	model.toolbarActions.push_back(std::move(showOnlyNew));
	const std::string query = _catalogue->_btnQuickSearch->getText();
	model.toolbarActions.push_back(action("quick-search",
		query.empty() ? std::string(tr("STR_TOGGLE_QUICK_SEARCH")) : query,
		p(g->toolbar_quick_search), _catalogue->_btnQuickSearch,
		_catalogue->_btnQuickSearch->getVisible()));
	model.footerActions.push_back(action("mark-all-seen", tr("STR_MARK_ALL_AS_SEEN"),
		p(g->action_mark_all_seen), _catalogue->_btnMarkAllSeen));
	model.footerActions.push_back(action("done", tr("STR_DONE"),
		p(g->action_done), _catalogue->_btnOK, true, "primary"));
	setFonts(model, _catalogue->_game->getMod());
	calypsoHdOperationsClampSelectionAndScroll(model);
	return model;
}

void CalypsoF09ResearchUi::ensureStaffingOwners()
{
	if (!_staffing) return;
	if (!_staffing->_btnAllAvailable)
	{
		_staffing->_btnAllAvailable = new TextButton(1, 1, 0, 0);
		_staffing->_btnAllAvailable->setText("");
		_staffing->add(_staffing->_btnAllAvailable, "button2", "allocateResearch");
		_staffing->_btnAllAvailable->onMouseClick(
			(ActionHandler)&ResearchInfoState::allAvailableClick);
	}
	if (!_staffing->_btnRemoveAll)
	{
		_staffing->_btnRemoveAll = new TextButton(1, 1, 0, 0);
		_staffing->_btnRemoveAll->setText("");
		_staffing->add(_staffing->_btnRemoveAll, "button2", "allocateResearch");
		_staffing->_btnRemoveAll->onMouseClick(
			(ActionHandler)&ResearchInfoState::removeAllClick);
	}
}
void CalypsoF09ResearchUi::applyStaffingGeometry()
{
	if (!_staffing || !_staffing->_window) return;
	ensureStaffingOwners();
	const bool wide = _staffing->_hdWideLayout;
	const auto *g = CalypsoF09ResearchStaffingGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) return;
	setOperationsWindow(_staffing->_window, rawRect(g->window));
	const int wx = _staffing->_window->getX(), wy = _staffing->_window->getY();
	const double sx = static_cast<double>(_staffing->_window->getWidth()) / g->window.w;
	const double sy = static_cast<double>(_staffing->_window->getHeight()) / g->window.h;
	auto place = [&](Surface *surface, const auto &rect)
	{
		setProjectedSurfaceRect(surface, rect, wx, wy, sx, sy,
			g->window.x, g->window.y);
	};
	place(_staffing->_txtTitle, g->title);
	place(_staffing->_txtAvailableScientist,
		g->region_capacity_field_available_value);
	place(_staffing->_txtAvailableSpace, g->region_capacity_field_labs_value);
	place(_staffing->_txtAllocatedScientist,
		g->region_capacity_field_allocated_value);
	place(_staffing->_txtMore, g->control_scientists_increment);
	place(_staffing->_txtLess, g->control_scientists_decrement);
	place(_staffing->_btnMore, g->control_scientists_increment);
	place(_staffing->_btnLess, g->control_scientists_decrement);
	place(_staffing->_surfaceScientists, g->control_scientists);
	place(_staffing->_btnRemoveAll,
		g->region_staffing_actions_action_slot_1);
	place(_staffing->_btnAllAvailable,
		g->region_staffing_actions_action_slot_2);
	place(_staffing->_btnCancel, g->action_cancel);
	place(_staffing->_btnOk, g->action_start_project);
}

CalypsoHdOperationsModel CalypsoF09ResearchUi::buildStaffingModel() const
{
	CalypsoHdOperationsModel model;
	if (!_staffing || !_staffing->_window || !_staffing->_game) return model;
	const auto tr = [this](const std::string &key) { return _staffing->tr(key); };
	const bool wide = _staffing->_hdWideLayout;
	const auto *g = CalypsoF09ResearchStaffingGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) return model;
	auto p = [&](const auto &r) {
		return rawRect(r);
	};
	model.archetype = CalypsoHdOperationsArchetype::WideDetail;
	model.familyId = CalypsoF09ResearchStaffingGen::kFamilyId;
	model.ownerState = _staffing;
	model.visualShell = CalypsoF09ResearchStaffingGen::kVisualShell;
	model.headerArtId = CalypsoF09ResearchStaffingGen::kHeaderArt;
	model.baseName = _staffing->_base ? _staffing->_base->getName() : std::string();
	model.sectionLabel = tr("STR_RESEARCH");
	setClock(model, _staffing->_game);
	model.suppressedWidgets = {
		_staffing->_window, _staffing->_txtTitle, _staffing->_txtAvailableScientist,
		_staffing->_txtAvailableSpace, _staffing->_txtAllocatedScientist,
		_staffing->_txtMore, _staffing->_txtLess, _staffing->_btnCancel,
		_staffing->_btnOk, _staffing->_btnMore, _staffing->_btnLess,
		_staffing->_surfaceScientists, _staffing->_btnAllAvailable,
		_staffing->_btnRemoveAll};
	model.title = _staffing->_txtTitle->getText();
	model.geometry.designWidth = g->designWidth;
	model.geometry.designHeight = g->designHeight;
	model.geometry.window = p(g->window);
	model.geometry.status = p(g->status);
	model.geometry.title = p(g->title);
	model.geometry.screenHeader = p(g->status);
	model.geometry.headerArt = p(g->headerArt);
	model.geometry.controlBar = p(g->controlBar);
	model.geometry.footer = p(g->footer);
	auto addRegion = [&](const std::string &id, const std::string &label,
		const auto &rect, const auto &labelRect)
	{
		CalypsoHdOperationsRegion region;
		region.id = id;
		region.label = label;
		region.rect = p(rect);
		region.labelRect = p(labelRect);
		region.state.visible = true;
		model.regions.push_back(std::move(region));
	};
	addRegion("staffing", tr("STR_CALYPSO_RESEARCH_STAFFING"),
		g->region_staffing, g->region_staffing_label);
	model.regions.back().kind = CalypsoHdOperationsRegionKind::Preview;
	model.regions.back().previewContent = model.title;
	model.regions.back().previewRect = p(g->region_staffing_content);
	addRegion("capacity", tr("STR_CALYPSO_RESEARCH_STAFFING"),
		g->region_capacity, g->region_capacity_label);
	auto &capacity = model.regions.back();
	int availableScientists = _staffing->_base ? _staffing->_base->getAvailableScientists() : 0;
	int availableLaboratories = _staffing->_base ? _staffing->_base->getFreeLaboratories() : 0;
	if (_staffing->_transaction.pending())
	{
		availableScientists -= _staffing->_transaction.assigned();
		availableLaboratories -= _staffing->_transaction.assigned();
	}
	capacity.fields.push_back(metric("available",
		tr("STR_CALYPSO_RESEARCH_SCIENTISTS_AVAILABLE_UC"),
		std::to_string(availableScientists),
		p(g->region_capacity_field_available_value)));
	capacity.fields.push_back(metric("labs",
		tr("STR_CALYPSO_RESEARCH_LABORATORY_SPACE_AVAILABLE_UC"),
		std::to_string(availableLaboratories),
		p(g->region_capacity_field_labs_value)));
	capacity.fields.push_back(metric("allocated",
		tr("STR_CALYPSO_RESEARCH_SCIENTISTS_ALLOCATED"),
		std::to_string(_staffing->_project->getAssigned()),
		p(g->region_capacity_field_allocated_value)));
	addRegion("staffing-actions", tr("STR_CALYPSO_RESEARCH_STAFFING_ACTIONS"),
		g->region_staffing_actions, g->region_staffing_actions_label);
	auto &staffingActions = model.regions.back();
	staffingActions.actions.push_back(action("remove-all", tr("STR_REMOVE_ALL"),
		p(g->region_staffing_actions_action_slot_1), _staffing->_btnRemoveAll));
	staffingActions.actions.push_back(action("all-available", tr("STR_ALL_AVAILABLE"),
		p(g->region_staffing_actions_action_slot_2), _staffing->_btnAllAvailable));
	staffingActions.kind = CalypsoHdOperationsRegionKind::Actions;
	CalypsoHdOperationsControl control;
	control.id = "scientists";
	control.label = tr("STR_CALYPSO_RESEARCH_SCIENTISTS");
	control.kind = CalypsoHdOperationsControlKind::Stepper;
	control.displayValue = std::to_string(_staffing->_project->getAssigned());
	control.rect = p(g->control_scientists);
	control.valueRect = p(g->control_scientists_value);
	control.widget = _staffing->_btnMore;
	control.decrement = action("scientists-decrement", "−",
		p(g->control_scientists_decrement), _staffing->_btnLess);
	control.increment = action("scientists-increment", "+",
		p(g->control_scientists_increment), _staffing->_btnMore);
	model.controls.push_back(std::move(control));
	model.footerActions.push_back(action("cancel", _staffing->_btnCancel->getText(),
		p(g->action_cancel), _staffing->_btnCancel));
	model.footerActions.push_back(action("start-project", _staffing->_btnOk->getText(),
		p(g->action_start_project), _staffing->_btnOk));
	setFonts(model, _staffing->_game->getMod());
	return model;
}


} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__