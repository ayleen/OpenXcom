#ifdef __EMSCRIPTEN__
#include "CalypsoF09ResearchUi.h"

#include "CalypsoHdFontSource.h"
#include "CalypsoHdOperationsChrome.h"
#include "CalypsoHdOperationsRenderer.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoHdOperationsFluid.h"
#include "CalypsoHdOperationsLayout.h"
#include "CalypsoViewportRuntime.h"
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

bool g_harnessQueueListSetupPending = false;
size_t g_harnessQueueListSetupSelected = 0;
size_t g_harnessQueueListSetupOffset = 0;
bool g_harnessCatalogueListSetupPending = false;
size_t g_harnessCatalogueListSetupSelected = 0;

/// Shared responsive input for the eleven R&D forms (re-review P1): the
/// layout class comes from the canonical logical viewport, never from
/// Options::baseXResolution or the physical backing size. Below the 740x360
/// minimum the registered route fails closed with an explicit size error.
bool calypsoHdRdWideLayout(Game *game)
{
	(void)game;
	const auto &viewport = Calypso::calypsoViewportRuntime().current();
	const auto effective = Calypso::calypsoHdOperationsEffectiveViewport();
	const auto layoutClass = Calypso::classifyCalypsoHdOperationsLayout(
		effective.cssWidth, std::max(1, viewport.logicalHeight));
	if (layoutClass == Calypso::CalypsoHdOperationsLayoutClass::Unsupported)
	{
		CalypsoHdUiOverlay::instance().failHdRoute(
			"R&D HD viewport is below the 740x360 minimum");
	}
	return layoutClass == Calypso::CalypsoHdOperationsLayoutClass::Wide;
}

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

using FluidAnchor = CalypsoHdOperationsFluidPolicy::Anchor;
using FluidVRole = CalypsoHdOperationsFluidPolicy::VerticalRole;


void setOperationsWindow(Window *window, const CalypsoHdOperationsRect &rect)
{
	setSurfaceRect(window, calypsoHdOperationsProjectForCurrentPresentation(
		rect, rect.w, rect.h));
}


} // namespace

template <typename Collection, typename Project>
void setGeneratedCollectionRows(CalypsoHdOperationsModel &model,
	const Collection &collection, const Project &project)
{
	model.geometry.collectionRows.clear();
	for (int i = 0; i < collection.rowSlotCount; ++i)
		model.geometry.collectionRows.push_back(project(collection.rowSlots[i].rect));
}
void setGeneratedMetadata(CalypsoHdOperationsModel &model,
	const char *presentation, const char *profileId, const char *profileVersion,
	const char *provenance)
{
	model.presentation = presentation;
	model.profileId = profileId;
	model.profileVersion = profileVersion;
	model.provenance = provenance;
}

std::size_t nativeSelection(unsigned int row, std::size_t count)
{
	if (count == 0) return 0;
	return row == static_cast<unsigned int>(-1)
		? count : static_cast<std::size_t>(row);
}




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
	state._hdWideLayout = calypsoHdRdWideLayout(state._game);
	auto *adapter = new CalypsoF09ResearchUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter->_renderer);
	adapter->refresh();
	// HD owns the presentation from the first eligible frame.  Complete the
	// legacy popup's temporary input lock after the adapter is registered;
	// otherwise a real pointer action during the 200 ms POPUP_BOTH animation is
	// discarded by InteractiveSurface while the HD model already advertises it.
	if (state._window) state._window->finishPopupForHd();
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
	state._hdWideLayout = calypsoHdRdWideLayout(state._game);
	auto *adapter = new CalypsoF09ResearchUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter->_renderer);
	adapter->refresh();
	if (state._window) state._window->finishPopupForHd();
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
	state._hdWideLayout = calypsoHdRdWideLayout(state._game);
	auto *adapter = new CalypsoF09ResearchUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter->_renderer);
	adapter->refresh();
	if (state._window) state._window->finishPopupForHd();
	calypsoHdOperationsPublishHarnessVisibility();
}

bool CalypsoF09ResearchUi::resize(ResearchState &state)
{
	if (!state._hdLayout || !state._hdAdapter) return false;
	state._hdWideLayout = calypsoHdRdWideLayout(state._game);
	state._hdAdapter->refresh();
	return true;
}

bool CalypsoF09ResearchUi::resize(NewResearchListState &state)
{
	if (!state._hdLayout || !state._hdAdapter) return false;
	state._hdWideLayout = calypsoHdRdWideLayout(state._game);
	state._hdAdapter->refresh();
	return true;
}

bool CalypsoF09ResearchUi::resize(ResearchInfoState &state)
{
	if (!state._hdLayout || !state._hdAdapter) return false;
	state._hdWideLayout = calypsoHdRdWideLayout(state._game);
	state._hdAdapter->refresh();
	return true;
}

void CalypsoF09ResearchUi::queueHarnessQueueListSetup(size_t selectedRow,
	size_t scrollOffset)
{
	g_harnessQueueListSetupPending = true;
	g_harnessQueueListSetupSelected = selectedRow;
	g_harnessQueueListSetupOffset = scrollOffset;
}

void CalypsoF09ResearchUi::queueHarnessCatalogueListSetup(size_t selectedRow)
{
	g_harnessCatalogueListSetupPending = true;
	g_harnessCatalogueListSetupSelected = selectedRow;
}

void CalypsoF09ResearchUi::clearHarnessQueueListSetup()
{
	g_harnessQueueListSetupPending = false;
	g_harnessCatalogueListSetupPending = false;
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
	if (_kind == Kind::Queue && g_harnessQueueListSetupPending && _queue
		&& _queue->_lstResearch
		&& _queue->_lstResearch->getRowsDoNotUse() > 0)
	{
		_queue->_lstResearch->setSelectedRow(g_harnessQueueListSetupSelected);
		_queue->_lstResearch->scrollTo(g_harnessQueueListSetupOffset);
		g_harnessQueueListSetupPending = false;
	}
	if (_kind == Kind::Catalogue && g_harnessCatalogueListSetupPending && _catalogue
		&& _catalogue->_lstResearch
		&& _catalogue->_lstResearch->getRowsDoNotUse() > 0)
	{
		_catalogue->_lstResearch->setSelectedRow(g_harnessCatalogueListSetupSelected);
		g_harnessCatalogueListSetupPending = false;
	}
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
		_queue->_btnOpenProject->setVisible(false);
		_queue->_btnOpenProject->onMouseClick(
			(ActionHandler)&ResearchState::onSelectProject);
	}
	if (!_queue->_btnTechTree)
	{
		_queue->_btnTechTree = new TextButton(1, 1, 0, 0);
		_queue->_btnTechTree->setText(_queue->tr("STR_TECH_TREE"));
		_queue->add(_queue->_btnTechTree, "button", "researchMenu");
		_queue->_btnTechTree->setVisible(false);
		_queue->_btnTechTree->onMouseClick(
			(ActionHandler)&ResearchState::onOpenTechTreeViewer);
	}
	if (Options::oxceBaseResearchReorder && !_moveUp)
	{
		_moveUp = new TextButton(44, 44, 0, 0);
		_moveUp->setText("");
		_queue->add(_moveUp, "button", "researchMenu");
		_moveUp->setVisible(false);
		_moveUp->onMouseClick((ActionHandler)&ResearchState::lstResearchLeftArrowClick);
		_moveUp->onMouseClick((ActionHandler)&ResearchState::lstResearchLeftArrowClick,
			SDL_BUTTON_RIGHT);
	}
	if (Options::oxceBaseResearchReorder && !_moveDown)
	{
		_moveDown = new TextButton(44, 44, 0, 0);
		_moveDown->setText("");
		_queue->add(_moveDown, "button", "researchMenu");
		_moveDown->setVisible(false);
		_moveDown->onMouseClick((ActionHandler)&ResearchState::lstResearchRightArrowClick);
		_moveDown->onMouseClick((ActionHandler)&ResearchState::lstResearchRightArrowClick,
			SDL_BUTTON_RIGHT);
	}
}

bool CalypsoF09ResearchUi::ownsQueueReorderPoint(int x, int y) const
{
	if (!_queue || !Options::oxceBaseResearchReorder) return false;
	// Reserve the whole row-action lane: a reorder moves the button to its new
	// row before the underlying TextList sees the same release event.
	if (y < _queue->_lstResearch->getY()
		|| y >= _queue->_lstResearch->getY() + _queue->_lstResearch->getHeight()) return false;
	for (const TextButton *owner : {_moveUp, _moveDown})
		if (owner && owner->getVisible() && x >= owner->getX()
			&& x < owner->getX() + owner->getWidth())
			return true;
	return false;
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
		button->setVisible(false);
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
	_catalogue->_btnMarkAllSeen->setVisible(true);
}

void CalypsoF09ResearchUi::applyQueueGeometry()
{
	if (!_queue || !_queue->_window) return;
	ensureQueueOwners();
	const bool wide = _queue->_hdWideLayout;
	const auto *g = CalypsoF09ResearchQueueGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) return;
	const auto &generated = wide
		? CalypsoF09ResearchQueueGen::kCollectionsWide[0]
		: CalypsoF09ResearchQueueGen::kCollectionsCompact[0];
	setOperationsWindow(_queue->_window, rawRect(g->window));
	// RR3-001: fluid resolution — authored reference geometry stays CSS px;
	// the surplus flows into the data area while 44px targets and typography
	// keep their authored size. The rect is projected into surface space once.
	const auto fluid = CalypsoHdOperationsFluidFrame::forReference(
		g->window.w, g->window.h, g->footer.x, g->footer.w);
	_queue->_lstResearch->rebaseNativeSize(
		g->collectionViewport.w, g->collectionViewport.h);
	auto place = [&](Surface *surface, const auto &rect,
		FluidVRole vrole = FluidVRole::Fixed, FluidAnchor anchor = FluidAnchor::Geometry)
	{
		setSurfaceRect(surface, fluid.resolve(
			CalypsoHdOperationsRectf{rect.x, rect.y, rect.w, rect.h}, vrole, anchor));
	};
	place(_queue->_txtTitle, g->title);
	place(_queue->_txtAvailable, g->summary_available, FluidVRole::Fixed, FluidAnchor::Right);
	place(_queue->_txtAllocated, g->summary_allocated, FluidVRole::Fixed, FluidAnchor::Right);
	place(_queue->_txtSpace, g->summary_lab_space, FluidVRole::Fixed, FluidAnchor::Right);
	place(_queue->_txtProject, g->collection_column_project);
	place(_queue->_txtScientists, g->collection_column_scientists);
	place(_queue->_txtProgress, g->collection_column_progress);
	place(_queue->_lstResearch, g->collectionViewport, FluidVRole::Stretch);
	place(_queue->_btnGlobalOverview, g->action_global_overview, FluidVRole::BottomShift);
	place(_queue->_btnNew, g->action_new_project, FluidVRole::BottomShift);
	place(_queue->_btnOk, g->action_done, FluidVRole::BottomShift, FluidAnchor::Right);
	place(_queue->_btnOpenProject, g->detail_selected_project_action_open_project, FluidVRole::BottomShift, FluidAnchor::Right);
	place(_queue->_btnTechTree, g->detail_selected_project_action_tech_tree, FluidVRole::BottomShift, FluidAnchor::Right);
	// Staffing wheel zone: the scientists column of the resolved collection,
	// not the HD-moved summary texts. Without scroll speeds the column scrolls
	// the list like every other position.
	if (Options::oxceResearchScrollSpeed > 0 || Options::oxceResearchScrollSpeedWithCtrl > 0)
	{
		CalypsoHdOperationsRectf columns[3] = {
			{g->collection_column_project.x, g->collection_column_project.y,
				g->collection_column_project.w, g->collection_column_project.h},
			{g->collection_column_scientists.x, g->collection_column_scientists.y,
				g->collection_column_scientists.w, g->collection_column_scientists.h},
			{g->collection_column_progress.x, g->collection_column_progress.y,
				g->collection_column_progress.w, g->collection_column_progress.h}};
		fluid.policy.mapXStretchGroup(columns, 3, fluid.width());
		const auto scientistsCss = fluid.cssY(columns[1], FluidVRole::Fixed);
		const auto scientistsColumn = calypsoHdOperationsProjectForCurrentPresentation(
			{scientistsCss.x, scientistsCss.y, scientistsCss.w, scientistsCss.h},
			fluid.width(), fluid.height());
		_queue->_lstResearch->setNoScrollArea(
			scientistsColumn.x, scientistsColumn.x + scientistsColumn.w);
	}
	else
	{
		_queue->_lstResearch->setNoScrollArea(0, 0);
	}
	// Row capacity comes from the resolved viewport; the input rail equals the
	// painted track (RR3-001 parity with the generated geometry). The generated
	// row slot is the single source for both native input and HD paint.
	const int tableRowHeight = g->collection_row_slot_1.h;
	const CalypsoHdOperationsRectf authoredQueueRow{
		g->collection_row_slot_1.x,
		g->collection_row_slot_1.y,
		g->collection_row_slot_1.w,
		tableRowHeight};
	const auto descriptor = fluid.selectionListGeometry(
		{g->collectionViewport.x, g->collectionViewport.y,
			g->collectionViewport.w, g->collectionViewport.h},
		authoredQueueRow,
		{g->collection_scroll_track.x, g->collection_scroll_track.y,
			g->collection_scroll_track.w, g->collection_scroll_track.h});
	_queue->_lstResearch->configureCalypsoHdSelectionList(
		descriptor.scrollBarWidth, descriptor.minThumbHeight,
		descriptor.rowStride, descriptor.rowOriginY,
		descriptor.dataViewportH, descriptor.visibleRows);
}

void CalypsoF09ResearchUi::applyCatalogueGeometry()
{
	if (!_catalogue || !_catalogue->_window) return;
	ensureCatalogueOwners();
	const bool wide = _catalogue->_hdWideLayout;
	const auto *g = CalypsoF09ResearchCatalogueGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) return;
	const auto &generated = wide
		? CalypsoF09ResearchCatalogueGen::kCollectionsWide[0]
		: CalypsoF09ResearchCatalogueGen::kCollectionsCompact[0];
	setOperationsWindow(_catalogue->_window, rawRect(g->window));
	// RR3-001: fluid resolution — the wide inspector stretches, the compact
	// data area absorbs the height; fixed targets stay authored CSS px.
	const auto fluid = CalypsoHdOperationsFluidFrame::forReference(
		g->window.w, g->window.h, g->footer.x, g->footer.w);
	_catalogue->_lstResearch->rebaseNativeSize(
		g->collectionViewport.w, g->collectionViewport.h);
	auto place = [&](Surface *surface, const auto &rect,
		FluidVRole vrole = FluidVRole::Fixed, FluidAnchor anchor = FluidAnchor::Geometry)
	{
		setSurfaceRect(surface, fluid.resolve(
			CalypsoHdOperationsRectf{rect.x, rect.y, rect.w, rect.h}, vrole, anchor));
	};
	place(_catalogue->_txtTitle, g->title);
	place(_catalogue->_lstResearch, g->collectionViewport,
		FluidVRole::Stretch, FluidAnchor::Stretch);
	place(_catalogue->_btnReview, g->detail_selected_project_action_review_project, FluidVRole::BottomShift, FluidAnchor::Right);
	place(_catalogue->_btnChangeVisibility,
		g->detail_selected_project_action_change_visibility, FluidVRole::BottomShift, FluidAnchor::Right);
	place(_catalogue->_btnTechTree, g->detail_selected_project_action_tech_tree, FluidVRole::BottomShift, FluidAnchor::Right);
	place(_catalogue->_btnMarkAllSeen, g->action_mark_all_seen, FluidVRole::BottomShift);
	place(_catalogue->_btnOK, g->action_done, FluidVRole::BottomShift, FluidAnchor::Right);
	place(_catalogue->_btnQuickSearch, g->toolbar_quick_search, FluidVRole::Fixed, FluidAnchor::Right);
	place(_catalogue->_cbxSort, g->toolbar_sort_default);
	const auto chooserBounds = fluid.resolve(
		CalypsoHdOperationsRectf{g->toolbarChooser.bounds.x,
			g->toolbarChooser.bounds.y, g->toolbarChooser.bounds.w,
			g->toolbarChooser.bounds.h},
		FluidVRole::Stretch, FluidAnchor::Stretch);
	const auto chooserRow = fluid.resolve(
		CalypsoHdOperationsRectf{0, 0, 1, g->toolbarChooser.optionRowHeight},
		FluidVRole::Fixed);
	const auto chooserInsetX = fluid.resolve(
		CalypsoHdOperationsRectf{0, 0, g->toolbarChooser.optionInsetX, 1},
		FluidVRole::Fixed);
	const auto chooserInsetY = fluid.resolve(
		CalypsoHdOperationsRectf{0, 0, 1, g->toolbarChooser.optionInsetY},
		FluidVRole::Fixed);
	const auto chooserMinWidth = fluid.resolve(
		CalypsoHdOperationsRectf{0, 0, g->toolbarChooser.minPanelWidth, 1},
		FluidVRole::Fixed);
	_catalogue->_cbxSort->calypsoHdConfigurePopupSelection(
		chooserRow.h, chooserInsetX.w, chooserInsetY.h,
		chooserBounds.x, chooserBounds.y, chooserBounds.w, chooserBounds.h,
		g->toolbarChooser.maxOptions, chooserMinWidth.w);
	place(_catalogue->_btnShowOnlyNew, g->toolbar_show_only_new);
	const auto descriptor = fluid.selectionListGeometry(
		{g->collectionViewport.x, g->collectionViewport.y,
			g->collectionViewport.w, g->collectionViewport.h},
		{g->collection_row_slot_1.x, g->collection_row_slot_1.y,
			g->collection_row_slot_1.w, g->collection_row_slot_1.h},
		{g->collection_scroll_track.x, g->collection_scroll_track.y,
			g->collection_scroll_track.w, g->collection_scroll_track.h},
		FluidVRole::Stretch, FluidAnchor::Stretch);
	_catalogue->_lstResearch->configureCalypsoHdSelectionList(
		descriptor.scrollBarWidth, descriptor.minThumbHeight,
		descriptor.rowStride, descriptor.rowOriginY,
		descriptor.dataViewportH, descriptor.visibleRows);
}

CalypsoHdOperationsModel CalypsoF09ResearchUi::buildQueueModel() const
{
	CalypsoHdOperationsModel model;
	if (!_queue || !_queue->_window || !_queue->_game || !_queue->_base
		|| !_queue->_lstResearch) return model;
	const auto tr = [this](const std::string &key) { return _queue->tr(key); };
	const bool wide = _queue->_hdWideLayout;
	const auto *g = CalypsoF09ResearchQueueGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) return model;
	const auto &generated = wide
		? CalypsoF09ResearchQueueGen::kCollectionsWide[0]
		: CalypsoF09ResearchQueueGen::kCollectionsCompact[0];
	const auto fluid = CalypsoHdOperationsFluidFrame::forReference(
		g->window.w, g->window.h, g->footer.x, g->footer.w);
	auto p = [&](const auto &r, FluidVRole vr = FluidVRole::Fixed,
		FluidAnchor ha = FluidAnchor::Geometry) {
		const auto css = fluid.cssY(fluid.cssX(
			CalypsoHdOperationsRectf{r.x, r.y, r.w, r.h}, ha), vr);
		return CalypsoHdOperationsRect{css.x, css.y, css.w, css.h};
	};
	model.archetype = CalypsoHdOperationsArchetype::OperationsWorkspace;
	model.familyId = CalypsoF09ResearchQueueGen::kFamilyId;
	setGeneratedMetadata(model,
		CalypsoF09ResearchQueueGen::kPresentationProfile,
		CalypsoF09ResearchQueueGen::kProfileId,
		CalypsoF09ResearchQueueGen::kProfileVersion,
		CalypsoF09ResearchQueueGen::kProvenanceTemplate);
	calypsoHdOperationsApplyGeneratedStyle(model, CalypsoF09ResearchQueueGen::kProfileStyle);
	calypsoHdOperationsApplyGeneratedTypography(model,
		wide ? CalypsoF09ResearchQueueGen::kTypographyWide : CalypsoF09ResearchQueueGen::kTypographyCompact);
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
	if (_moveUp) model.suppressedWidgets.push_back(_moveUp);
	if (_moveDown) model.suppressedWidgets.push_back(_moveDown);
	model.title = tr("STR_CURRENT_RESEARCH");
	model.geometry.designWidth = fluid.width();
	model.geometry.designHeight = fluid.height();
	model.geometry.window = p(g->window);
	model.geometry.title = p(g->title);
	model.geometry.screenHeader = p(g->screenHeader);
	model.geometry.headerArt = p(g->headerArt);
	model.geometry.footer = p(g->footer, FluidVRole::BottomShift);
	model.geometry.summaryBar = p(g->summaryBar);
	model.geometry.toolbarBar = p(g->toolbarBar);
	model.geometry.collectionViewport = p(g->collectionViewport, FluidVRole::Stretch);
	model.geometry.detailPanel = p(g->detailPanel, FluidVRole::BottomShift);
	model.geometry.detailIdentity = p(g->detail_selected_project_label, FluidVRole::BottomShift);
	const auto rowActionArea = p(CalypsoHdOperationsRectf{
		generated.rowActionArea.x,
		generated.rowActionArea.y,
		generated.rowActionArea.w,
		generated.rowActionArea.h},
		FluidVRole::Fixed, FluidAnchor::Right);
	const int authoredActionGap = std::max(0,
		generated.rowActionArea.x
		- (g->collection_column_progress.x + g->collection_column_progress.w));
	const int rowDataRight = rowActionArea.x - authoredActionGap;
	{
		CalypsoHdOperationsRectf cols[3] = {
			{g->collection_column_project.x, g->collection_column_project.y,
				g->collection_column_project.w, g->collection_column_project.h},
			{g->collection_column_scientists.x, g->collection_column_scientists.y,
				g->collection_column_scientists.w, g->collection_column_scientists.h},
			{g->collection_column_progress.x, g->collection_column_progress.y,
				g->collection_column_progress.w, g->collection_column_progress.h}};
		fluid.policy.mapXStretchGroup(cols, 3, fluid.width());
		for (auto &column : cols) column = fluid.cssY(column, FluidVRole::Fixed);
		cols[2].w = std::max(1, rowDataRight - cols[2].x);
		model.geometry.collectionColumns = {
			CalypsoHdOperationsRect{cols[0].x, cols[0].y, cols[0].w, cols[0].h},
			CalypsoHdOperationsRect{cols[1].x, cols[1].y, cols[1].w, cols[1].h},
			CalypsoHdOperationsRect{cols[2].x, cols[2].y, cols[2].w, cols[2].h}};
	}
	const int headerHeight = g->collection_row_slot_1.y - g->collectionViewport.y;
	const auto resolvedViewport = p(g->collectionViewport, FluidVRole::Stretch);
	const int tableRowHeight = g->collection_row_slot_1.h;
	const auto authoredQueueRow = p(CalypsoHdOperationsRectf{
		g->collection_row_slot_1.x,
		g->collection_row_slot_1.y,
		g->collection_row_slot_1.w,
		tableRowHeight});
	const auto resolvedTrack = p(CalypsoHdOperationsRectf{
		g->collection_scroll_track.x,
		g->collectionViewport.y + headerHeight,
		g->collection_scroll_track.w, 0});
	calypsoHdOperationsResolveCollectionGeometry(model.collection,
		resolvedViewport, authoredQueueRow, resolvedTrack, headerHeight,
		g->collectionViewport.w);
	for (auto &slot : model.collection.rowSlots)
		slot.w = std::max(1, rowDataRight - slot.x);
	model.geometry.collectionViewport = model.collection.viewport;
	model.geometry.collectionRows = model.collection.rowSlots;
	model.geometry.collectionScrollTrack = model.collection.scrollTrack;
	model.geometry.collectionScrollThumb = model.collection.scrollThumb;
	model.geometry.detailIdentityTitle = p(g->detail_selected_project_identity_title, FluidVRole::BottomShift);
	model.geometry.detailIdentitySubtitle = p(g->detail_selected_project_identity_subtitle, FluidVRole::BottomShift);
	model.geometry.detailMetrics = {
		p(g->detail_selected_project_metric_scientists, FluidVRole::BottomShift),
		p(g->detail_selected_project_metric_progress, FluidVRole::BottomShift)};
	model.geometry.detailActions = {
		p(g->detail_selected_project_action_open_project, FluidVRole::BottomShift, FluidAnchor::Right),
		p(g->detail_selected_project_action_tech_tree, FluidVRole::BottomShift, FluidAnchor::Right)};
	model.geometry.footerActions = {
		p(g->action_global_overview, FluidVRole::BottomShift),
		p(g->action_new_project, FluidVRole::BottomShift),
		p(g->action_done, FluidVRole::BottomShift, FluidAnchor::Right)};
	model.summaryFields.push_back(summary("available",
		tr("STR_CALYPSO_RESEARCH_SCIENTISTS_AVAILABLE"),
		std::to_string(_queue->_base->getAvailableScientists()),
		p(g->summary_available, FluidVRole::Fixed, FluidAnchor::Right), _queue->_txtAvailable));
	model.summaryFields.push_back(summary("allocated",
		tr("STR_CALYPSO_RESEARCH_SCIENTISTS_ALLOCATED"),
		std::to_string(_queue->_base->getAllocatedScientists()),
		p(g->summary_allocated, FluidVRole::Fixed, FluidAnchor::Right), _queue->_txtAllocated));
	model.summaryFields.push_back(summary("lab-space",
		tr("STR_CALYPSO_RESEARCH_LABORATORY_SPACE_AVAILABLE"),
		std::to_string(_queue->_base->getFreeLaboratories()),
		p(g->summary_lab_space, FluidVRole::Fixed, FluidAnchor::Right), _queue->_txtSpace));
	const std::string columnLabels[] = {
		tr("STR_RESEARCH_PROJECT"),
		tr("STR_CALYPSO_RESEARCH_SCIENTISTS_ALLOCATED_UC"),
		tr("STR_PROGRESS")};
	for (int i = 0; i < generated.columnCount; ++i)
		model.collection.columns.push_back({
			generated.columns[i].id, columnLabels[i],
			model.geometry.collectionColumns.at(i), {}, generated.columns[i].contentRole});
	const auto &projects = _queue->_base->getResearch();
	model.collection.heading = tr("STR_CALYPSO_RESEARCH_PROJECTS");
	model.collection.meta = tr("STR_CALYPSO_ACTIVE_PROJECT_COUNT").arg(projects.size());
	model.collection.emptyTitle = tr("STR_CALYPSO_NO_ACTIVE_RESEARCH");
	model.collection.emptyBody = tr("STR_CALYPSO_START_RESEARCH_PROMPT");
	const std::size_t nativeSelected = nativeSelection(
		_queue->_lstResearch->getSelectedRow(), projects.size());
	const std::size_t nativeOffset = _queue->_lstResearch->getScroll();
	for (std::size_t i = 0; i < projects.size(); ++i)
	{
		const auto *project = projects[i];
		if (!project || !project->getRules()) return CalypsoHdOperationsModel{};
		CalypsoHdOperationsRow row;
		row.id = project->getRules()->getName();
		row.values = {
			tr(project->getRules()->getName()),
			std::to_string(project->getAssigned()),
			tr(project->getResearchProgress())};
		const std::size_t slotCount = model.geometry.collectionRows.size();
		const std::size_t slot = slotCount == 0 ? 0
			: (i >= nativeOffset ? i - nativeOffset : 0) % slotCount;
		row.rect = slotCount == 0
			? model.geometry.collectionViewport
			: model.geometry.collectionRows[slot];
		row.cells.reserve(row.values.size());
		for (std::size_t c = 0; c < row.values.size(); ++c)
			row.cells.push_back({row.values[c], generated.columns[c].contentRole, {}});
		row.state.selected = i == nativeSelected;
		row.widget = _queue->_lstResearch;
		model.collection.rows.push_back(std::move(row));
	}
	model.collection.selectedIndex = nativeSelected;
	model.collection.scrollOffset = nativeOffset;
	model.collection.visibleRows = model.geometry.collectionRows.size();
	model.collection.rowHeight = model.geometry.collectionRows.empty()
		? 0 : model.geometry.collectionRows.front().h;
	model.collection.rowSlots = model.geometry.collectionRows;
	model.collection.count = projects.size();
	model.collection.viewport = model.geometry.collectionViewport;
	model.collection.scrollOffset = nativeOffset;
	model.collection.scroll = {
		nativeOffset, projects.size(), model.collection.visibleRows,
		model.geometry.collectionViewport, model.geometry.collectionScrollTrack,
		model.geometry.collectionScrollThumb};
	const bool hasSelection = !projects.empty()
		&& model.collection.selectedIndex < projects.size();
	const auto *selected = hasSelection ? projects[model.collection.selectedIndex] : nullptr;
	// Native owners remain in the state tree while HD owns the presentation.
	// Keep their hit regions disarmed whenever the list's invalid-selection
	// sentinel is active, including after the last project is removed.
	if (_queue->_btnOpenProject) _queue->_btnOpenProject->setVisible(hasSelection);
	if (_queue->_btnTechTree) _queue->_btnTechTree->setVisible(hasSelection);
	if (Options::oxceBaseResearchReorder && hasSelection && _moveUp && _moveDown)
	{
		_moveUp->setVisible(true);
		_moveDown->setVisible(true);
		// These are real 44px HD owners bound to ResearchState's existing arrow
		// handlers. The legacy TextList arrows remain native, but are too small to
		// serve as a responsive HD hit target directly.
		const auto row = model.collection.rows[nativeSelected].rect;
		const int gap = 0;
		const int width = std::max(1, rowActionArea.w / 2);
		CalypsoHdOperationsRect upRect = row;
		upRect.x = rowActionArea.x;
		upRect.w = width;
		upRect.h = std::max(width, row.h);
		CalypsoHdOperationsRect downRect = upRect;
		downRect.x += width + gap;
		const auto effective = calypsoHdOperationsEffectiveViewport();
		setSurfaceRect(_moveUp, calypsoHdOperationsProjectForCurrentPresentation(
			upRect, effective.cssWidth, effective.cssHeight));
		setSurfaceRect(_moveDown, calypsoHdOperationsProjectForCurrentPresentation(
			downRect, effective.cssWidth, effective.cssHeight));
		auto moveUpAction = action("move-up", "↑", upRect, _moveUp, true);
		moveUpAction.state.disabled = nativeSelected == 0;
		model.detail.actions.push_back(std::move(moveUpAction));
		auto moveDownAction = action("move-down", "↓", downRect, _moveDown, true);
		moveDownAction.state.disabled = nativeSelected + 1 >= projects.size();
		model.detail.actions.push_back(std::move(moveDownAction));
	}
	else if (_moveUp && _moveDown)
	{
		_moveUp->setVisible(false);
		_moveDown->setVisible(false);
	}
	model.detail.id = "selected-project";
	model.detail.panel = model.geometry.detailPanel;
	model.detail.identity.id = "selected-project";
	model.detail.identity.label = tr("STR_CALYPSO_RESEARCH_SELECTED_PROJECT");
	model.detail.identity.title = selected
		? std::string(tr(selected->getRules()->getName()))
		: std::string(tr("STR_NONE"));
	model.detail.identity.subtitle = selected
		? std::string(tr("STR_RESEARCH_PROJECT"))
		: _queue->_btnNew->getText();
	model.detail.identity.rect = p(g->detail_selected_project, FluidVRole::BottomShift);
	model.detail.identity.titleRect = p(g->detail_selected_project_identity_title, FluidVRole::BottomShift);
	model.detail.identity.subtitleRect = p(g->detail_selected_project_identity_subtitle, FluidVRole::BottomShift);
	if (selected)
	{
		model.detail.metrics.push_back(metric("scientists",
			tr("STR_CALYPSO_RESEARCH_SCIENTISTS_ALLOCATED"),
			std::to_string(selected->getAssigned()),
			p(g->detail_selected_project_metric_scientists, FluidVRole::BottomShift)));
		model.detail.metrics.push_back(metric("progress", tr("STR_PROGRESS"),
			tr(selected->getResearchProgress()),
			p(g->detail_selected_project_metric_progress, FluidVRole::BottomShift)));
	}
	model.detail.actions.push_back(action("open-project", _queue->_btnOpenProject->getText(),
		p(g->detail_selected_project_action_open_project, FluidVRole::BottomShift, FluidAnchor::Right), _queue->_btnOpenProject,
		hasSelection));
	model.detail.actions.push_back(action("tech-tree", _queue->_btnTechTree->getText(),
		p(g->detail_selected_project_action_tech_tree, FluidVRole::BottomShift, FluidAnchor::Right), _queue->_btnTechTree,
		hasSelection));
	model.footerActions.push_back(action("global-overview",
		_queue->_btnGlobalOverview->getText(),
		p(g->action_global_overview, FluidVRole::BottomShift), _queue->_btnGlobalOverview, true, "safe"));
	auto newProject = action("new-project", _queue->_btnNew->getText(),
		p(g->action_new_project, FluidVRole::BottomShift), _queue->_btnNew, true, "primary");
	model.footerActions.push_back(std::move(newProject));
	model.footerActions.push_back(action("done", _queue->_btnOk->getText(),
		p(g->action_done, FluidVRole::BottomShift, FluidAnchor::Right), _queue->_btnOk));
	setFonts(model, _queue->_game->getMod());
	return model;
}

CalypsoHdOperationsModel CalypsoF09ResearchUi::buildCatalogueModel() const
{
	CalypsoHdOperationsModel model;
	if (!_catalogue || !_catalogue->_window || !_catalogue->_game
		|| !_catalogue->_base || !_catalogue->_lstResearch
		|| !_catalogue->_game->getSavedGame()) return model;
	const auto tr = [this](const std::string &key) { return _catalogue->tr(key); };
	const bool wide = _catalogue->_hdWideLayout;
	const auto *g = CalypsoF09ResearchCatalogueGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) return model;
	const auto &generated = wide
		? CalypsoF09ResearchCatalogueGen::kCollectionsWide[0]
		: CalypsoF09ResearchCatalogueGen::kCollectionsCompact[0];
	const auto fluid = CalypsoHdOperationsFluidFrame::forReference(
		g->window.w, g->window.h, g->footer.x, g->footer.w);
	auto p = [&](const auto &r, FluidVRole vr = FluidVRole::Fixed,
		FluidAnchor ha = FluidAnchor::Geometry) {
		const auto css = fluid.cssY(fluid.cssX(
			CalypsoHdOperationsRectf{r.x, r.y, r.w, r.h}, ha), vr);
		return CalypsoHdOperationsRect{css.x, css.y, css.w, css.h};
	};
	model.archetype = CalypsoHdOperationsArchetype::OperationsWorkspace;
	model.familyId = CalypsoF09ResearchCatalogueGen::kFamilyId;
	setGeneratedMetadata(model,
		CalypsoF09ResearchCatalogueGen::kPresentationProfile,
		CalypsoF09ResearchCatalogueGen::kProfileId,
		CalypsoF09ResearchCatalogueGen::kProfileVersion,
		CalypsoF09ResearchCatalogueGen::kProvenanceTemplate);
	calypsoHdOperationsApplyGeneratedStyle(model, CalypsoF09ResearchCatalogueGen::kProfileStyle);
	calypsoHdOperationsApplyGeneratedTypography(model,
		wide ? CalypsoF09ResearchCatalogueGen::kTypographyWide : CalypsoF09ResearchCatalogueGen::kTypographyCompact);
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
	_catalogue->_cbxSort->calypsoHdSuppressPopupWidgets(model.suppressedWidgets);
	model.title = tr("STR_NEW_RESEARCH_PROJECTS");
	model.geometry.designWidth = fluid.width();
	model.geometry.designHeight = fluid.height();
	model.geometry.window = p(g->window);
	model.geometry.title = p(g->title);
	model.geometry.screenHeader = p(g->screenHeader);
	model.geometry.headerArt = p(g->headerArt);
	model.geometry.summaryBar = p(g->summaryBar);
	model.geometry.toolbarBar = p(g->toolbarBar, FluidVRole::Fixed, FluidAnchor::Stretch);
	model.geometry.collectionViewport = p(g->collectionViewport,
		FluidVRole::Stretch, FluidAnchor::Stretch);
	model.geometry.detailPanel = p(g->detailPanel,
		wide ? FluidVRole::Stretch : FluidVRole::BottomShift);
	model.geometry.footer = p(g->footer, FluidVRole::BottomShift);
	model.geometry.detailIdentity = p(g->detail_selected_project_label, wide ? FluidVRole::Fixed : FluidVRole::BottomShift, wide ? FluidAnchor::Right : FluidAnchor::Geometry);
	{
		CalypsoHdOperationsRectf cols[2] = {
			{g->collection_column_project.x, g->collection_column_project.y,
				g->collection_column_project.w, g->collection_column_project.h},
			{g->collection_column_status.x, g->collection_column_status.y,
				g->collection_column_status.w, g->collection_column_status.h}};
		fluid.policy.mapXStretchGroup(cols, 2, fluid.width());
		for (auto &column : cols) column = fluid.cssY(column, FluidVRole::Fixed);
		model.geometry.collectionColumns = {
			CalypsoHdOperationsRect{cols[0].x, cols[0].y, cols[0].w, cols[0].h},
			CalypsoHdOperationsRect{cols[1].x, cols[1].y, cols[1].w, cols[1].h}};
	}
	const int headerHeight = g->collection_row_slot_1.y - g->collectionViewport.y;
	const auto resolvedViewport = p(g->collectionViewport,
		FluidVRole::Stretch, FluidAnchor::Stretch);
	const auto resolvedTrack = p(CalypsoHdOperationsRectf{
		g->collection_scroll_track.x,
		g->collectionViewport.y + headerHeight,
		g->collection_scroll_track.w, 0}, FluidVRole::Fixed, FluidAnchor::Right);
	calypsoHdOperationsResolveCollectionGeometry(model.collection,
		resolvedViewport, p(g->collection_row_slot_1), resolvedTrack, headerHeight,
		g->collectionViewport.w);
	model.geometry.collectionViewport = model.collection.viewport;
	model.geometry.collectionRows = model.collection.rowSlots;
	model.geometry.collectionScrollTrack = model.collection.scrollTrack;
	model.geometry.collectionScrollThumb = model.collection.scrollThumb;
	model.geometry.detailIdentityTitle = p(g->detail_selected_project_identity_title, wide ? FluidVRole::Fixed : FluidVRole::BottomShift, wide ? FluidAnchor::Right : FluidAnchor::Geometry);
	model.geometry.detailIdentitySubtitle = p(g->detail_selected_project_identity_subtitle, wide ? FluidVRole::Fixed : FluidVRole::BottomShift, wide ? FluidAnchor::Right : FluidAnchor::Geometry);
	model.geometry.detailMetrics = {p(g->detail_selected_project_metric_status, wide ? FluidVRole::Fixed : FluidVRole::BottomShift, wide ? FluidAnchor::Right : FluidAnchor::Geometry)};
	model.geometry.detailActions = {
		p(g->detail_selected_project_action_review_project, FluidVRole::BottomShift, FluidAnchor::Right),
		p(g->detail_selected_project_action_change_visibility, FluidVRole::BottomShift, FluidAnchor::Right),
		p(g->detail_selected_project_action_tech_tree, FluidVRole::BottomShift, FluidAnchor::Right)};
	model.geometry.footerActions = {
		p(g->action_mark_all_seen, FluidVRole::BottomShift),
		p(g->action_done, FluidVRole::BottomShift, FluidAnchor::Right)};
	const std::string columnLabels[] = {
		tr("STR_RESEARCH_PROJECT"), tr("STR_CALYPSO_RESEARCH_STATUS")};
	for (int i = 0; i < generated.columnCount; ++i)
		model.collection.columns.push_back({
			generated.columns[i].id, columnLabels[i],
			model.geometry.collectionColumns.at(i), {}, generated.columns[i].contentRole});
	model.collection.emptyTitle = tr("STR_CALYPSO_NO_AVAILABLE_RESEARCH");
	model.collection.emptyBody = tr("STR_CALYPSO_NO_AVAILABLE_RESEARCH_PROMPT");
	const std::size_t nativeSelected = nativeSelection(
		_catalogue->_lstResearch->getSelectedRow(), _catalogue->_projects.size());
	const std::size_t nativeOffset = _catalogue->_lstResearch->getScroll();
	for (std::size_t i = 0; i < _catalogue->_projects.size(); ++i)
	{
		const RuleResearch *rule = _catalogue->_projects[i];
		if (!rule) return CalypsoHdOperationsModel{};
		const int status = _catalogue->_game->getSavedGame()
			->getResearchRuleStatus(rule->getName());
		const std::string statusText = status == RuleResearch::RESEARCH_STATUS_NEW
			? tr("STR_NEW_PROJECT")
			: status == RuleResearch::RESEARCH_STATUS_HIDDEN
				? tr("STR_FILTER_HIDDEN") : tr("STR_NORMAL");
		CalypsoHdOperationsRow row;
		row.id = rule->getName();
		row.values = {tr(rule->getName()), statusText};
		const std::size_t slotCount = model.geometry.collectionRows.size();
		const std::size_t slot = slotCount == 0 ? 0
			: (i >= nativeOffset ? i - nativeOffset : 0) % slotCount;
		row.rect = slotCount == 0
			? model.geometry.collectionViewport
			: model.geometry.collectionRows[slot];
		for (std::size_t c = 0; c < row.values.size(); ++c)
			row.cells.push_back({row.values[c], generated.columns[c].contentRole, {}});
		row.state.selected = i == nativeSelected;
		row.widget = _catalogue->_lstResearch;
		model.collection.rows.push_back(std::move(row));
	}
	model.collection.selectedIndex = nativeSelected;
	model.collection.scrollOffset = nativeOffset;
	model.collection.visibleRows = model.geometry.collectionRows.size();
	model.collection.rowHeight = model.geometry.collectionRows.empty()
		? 0 : model.geometry.collectionRows.front().h;
	model.collection.rowSlots = model.geometry.collectionRows;
	model.collection.count = _catalogue->_projects.size();
	model.collection.viewport = model.geometry.collectionViewport;
	model.collection.scroll = {
		nativeOffset, _catalogue->_projects.size(), model.collection.visibleRows,
		model.geometry.collectionViewport, model.geometry.collectionScrollTrack,
		model.geometry.collectionScrollThumb};
	const bool hasSelection = !_catalogue->_projects.empty()
		&& model.collection.selectedIndex < _catalogue->_projects.size();
	const auto *selected = hasSelection
		? _catalogue->_projects[model.collection.selectedIndex] : nullptr;
	// Keep the hidden native owners in sync with the HD selection sentinel. A
	// filtered-out row must not leave its old hit rectangle armed to invoke the
	// newly inserted row zero through the legacy callback.
	if (_catalogue->_btnReview) _catalogue->_btnReview->setVisible(hasSelection);
	if (_catalogue->_btnChangeVisibility) _catalogue->_btnChangeVisibility->setVisible(hasSelection);
	if (_catalogue->_btnTechTree) _catalogue->_btnTechTree->setVisible(hasSelection);
	const int selectedStatus = selected
		? _catalogue->_game->getSavedGame()->getResearchRuleStatus(selected->getName()) : 0;
	const std::string selectedStatusText = selectedStatus == RuleResearch::RESEARCH_STATUS_NEW
		? tr("STR_NEW_PROJECT")
		: selectedStatus == RuleResearch::RESEARCH_STATUS_HIDDEN
			? tr("STR_FILTER_HIDDEN") : tr("STR_NORMAL");
	model.detail.id = "selected-project";
	model.detail.identity.title = selected ? tr(selected->getName()) : tr("STR_NONE");
	model.detail.panel = model.geometry.detailPanel;
	model.detail.identity.id = "selected-project";
	model.detail.identity.label = tr("STR_CALYPSO_RESEARCH_SELECTED_PROJECT");
	model.detail.identity.subtitle = tr("STR_RESEARCH_PROJECT");
	model.detail.identity.rect = p(g->detail_selected_project, wide ? FluidVRole::Fixed : FluidVRole::BottomShift, wide ? FluidAnchor::Right : FluidAnchor::Geometry);
	model.detail.identity.titleRect = p(g->detail_selected_project_identity_title, wide ? FluidVRole::Fixed : FluidVRole::BottomShift, wide ? FluidAnchor::Right : FluidAnchor::Geometry);
	model.detail.identity.subtitleRect = p(g->detail_selected_project_identity_subtitle, wide ? FluidVRole::Fixed : FluidVRole::BottomShift, wide ? FluidAnchor::Right : FluidAnchor::Geometry);
	if (selected)
		model.detail.metrics.push_back(metric("status",
			tr("STR_CALYPSO_RESEARCH_VISIBILITY_STATUS"),
			selectedStatusText, p(g->detail_selected_project_metric_status, wide ? FluidVRole::Fixed : FluidVRole::BottomShift, wide ? FluidAnchor::Right : FluidAnchor::Geometry)));
	model.detail.actions.push_back(action("review-project", tr("STR_REVIEW_PROJECT"),
		p(g->detail_selected_project_action_review_project, FluidVRole::BottomShift, FluidAnchor::Right), _catalogue->_btnReview,
		hasSelection, "primary"));
	auto visibility = action("change-visibility", tr("STR_CHANGE_VISIBILITY"),
		p(g->detail_selected_project_action_change_visibility, FluidVRole::BottomShift, FluidAnchor::Right),
		_catalogue->_btnChangeVisibility, hasSelection);
	visibility.state.disabled = !_catalogue->_isSortingEnabled
		&& !Options::oxceHighlightNewTopics;
	model.detail.actions.push_back(std::move(visibility));
	model.detail.actions.push_back(action("tech-tree", tr("STR_TECH_TREE"),
		p(g->detail_selected_project_action_tech_tree, FluidVRole::BottomShift, FluidAnchor::Right), _catalogue->_btnTechTree,
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
		model.toolbarActions.push_back(action("sort-default", tr(sortLabelKey),
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
		p(g->toolbar_quick_search, FluidVRole::Fixed, FluidAnchor::Right), _catalogue->_btnQuickSearch,
		_catalogue->_btnQuickSearch->getVisible()));
	model.footerActions.push_back(action("mark-all-seen", tr("STR_MARK_ALL_SEEN"),
		p(g->action_mark_all_seen, FluidVRole::BottomShift), _catalogue->_btnMarkAllSeen, true, "safe"));
	model.footerActions.push_back(action("done", tr("STR_DONE"),
		p(g->action_done, FluidVRole::BottomShift, FluidAnchor::Right), _catalogue->_btnOK));
	model.chooser.bounds = p(CalypsoHdOperationsRectf{
		g->toolbarChooser.bounds.x, g->toolbarChooser.bounds.y,
		g->toolbarChooser.bounds.w, g->toolbarChooser.bounds.h},
		FluidVRole::Stretch, FluidAnchor::Stretch);
	model.chooser.optionInsetX = p(CalypsoHdOperationsRectf{
		0, 0, g->toolbarChooser.optionInsetX, 1}).w;
	model.chooser.optionInsetY = p(CalypsoHdOperationsRectf{
		0, 0, 1, g->toolbarChooser.optionInsetY}).h;
	const int chooserRowHeight = p(CalypsoHdOperationsRectf{
		0, 0, 1, g->toolbarChooser.optionRowHeight}).h;
	const int chooserMinWidth = p(CalypsoHdOperationsRectf{
		0, 0, g->toolbarChooser.minPanelWidth, 1}).w;
	const TextList *popupList = _catalogue->_cbxSort->calypsoHdPopupList();
	if (popupList)
	{
		model.chooser.owner = _catalogue->_cbxSort;
		model.chooser.open = _catalogue->_cbxSort->calypsoHdPopupVisible();
		const int rowHeight = std::max(1, chooserRowHeight);
		const int optionCount = static_cast<int>(popupList->getTexts());
		const auto sortRect = p(g->toolbar_sort_default);
		const auto chooserLayout = calypsoHdResolveChooserLayout(
			{sortRect.x, sortRect.y, sortRect.w, sortRect.h},
			{model.chooser.bounds.x, model.chooser.bounds.y,
				model.chooser.bounds.w, model.chooser.bounds.h}, rowHeight,
			model.chooser.optionInsetY, optionCount,
			g->toolbarChooser.maxOptions, _catalogue->_cbxSort->calypsoHdPopupAboveButton(), chooserMinWidth);
		model.chooser.panel = {chooserLayout.panel.x, chooserLayout.panel.y,
			chooserLayout.panel.w, chooserLayout.panel.h};
		model.chooser.scrollOffset = popupList->getScroll();
		model.chooser.visibleRows = popupList->getVisibleRows();
		model.chooser.selectedIndex = popupList->getTexts() > 0
			? static_cast<int>(_catalogue->_cbxSort->getSelected()) : -1;
		model.chooser.hoveredIndex = popupList->getTexts() > 0
			? static_cast<int>(_catalogue->_cbxSort->getHoveredListIdx()) : -1;
		for (std::size_t index = 0; index < popupList->getTexts(); ++index)
		{
			CalypsoHdOperationsChooserOption option;
			option.id = index < static_cast<std::size_t>(
				CalypsoF09ResearchCatalogueGen::kToolbarChooserOptionCount)
				? std::string(CalypsoF09ResearchCatalogueGen::kToolbarChooserOptions[index].id)
				: std::string("sort-option-") + std::to_string(index);
			option.label = popupList->getCellText(index, 0);
			// The native popup is already projected into engine coordinates. Keep
			// the HD chooser model in CSS space so painting and hit testing share
			// one coordinate system; native ownership is configured separately.
			const int popupOffset = static_cast<int>(index) -
				static_cast<int>(popupList->getScroll());
			option.rect = {model.chooser.panel.x + model.chooser.optionInsetX,
				model.chooser.panel.y + model.chooser.optionInsetY
					+ popupOffset * rowHeight,
				model.chooser.panel.w - 2 * model.chooser.optionInsetX, rowHeight};
			option.state.visible = model.chooser.open
				&& popupOffset >= 0
				&& popupOffset < static_cast<int>(model.chooser.visibleRows);
			option.state.selected = static_cast<int>(index) == model.chooser.selectedIndex;
			option.state.hovered = static_cast<int>(index) == model.chooser.hoveredIndex;
			option.widget = popupList;
			model.chooser.options.push_back(std::move(option));
		}
	}
	setFonts(model, _catalogue->_game->getMod());
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
	// RR3-001: fluid resolution — staffing regions absorb the height surplus;
	// the stepper and 44px actions keep their authored CSS size.
	const auto fluid = CalypsoHdOperationsFluidFrame::forReference(
		g->window.w, g->window.h, g->footer.x, g->footer.w);
	auto groupAnchor = [&](const auto &parent) {
		const int contentRight = fluid.policy.contentLeft + fluid.policy.contentWidth;
		return fluidSameEdge(parent.x + parent.w, contentRight)
			&& !fluidSameEdge(parent.x, fluid.policy.contentLeft)
			? FluidAnchor::Right : FluidAnchor::Geometry;
	};
	const auto capacityAnchor = groupAnchor(g->region_capacity);
	const auto staffingActionsAnchor = wide ? FluidAnchor::Right
		: groupAnchor(g->region_staffing_actions);
	auto place = [&](Surface *surface, const auto &rect,
		FluidVRole vrole = FluidVRole::Fixed, FluidAnchor anchor = FluidAnchor::Geometry)
	{
		setSurfaceRect(surface, fluid.resolve(
			CalypsoHdOperationsRectf{rect.x, rect.y, rect.w, rect.h}, vrole, anchor));
	};
	place(_staffing->_txtTitle, g->title);
	place(_staffing->_txtAvailableScientist,
		g->region_capacity_field_available_value, FluidVRole::Fixed, capacityAnchor);
	place(_staffing->_txtAvailableSpace, g->region_capacity_field_labs_value,
		FluidVRole::Fixed, capacityAnchor);
	place(_staffing->_txtAllocatedScientist,
		g->region_capacity_field_allocated_value, FluidVRole::Fixed, capacityAnchor);
	place(_staffing->_txtMore, g->control_scientists_increment);
	place(_staffing->_txtLess, g->control_scientists_decrement);
	place(_staffing->_btnMore, g->control_scientists_increment);
	place(_staffing->_btnLess, g->control_scientists_decrement);
	place(_staffing->_surfaceScientists, g->control_scientists);
	place(_staffing->_btnRemoveAll,
		g->region_staffing_actions_action_slot_1, FluidVRole::Fixed, staffingActionsAnchor);
	place(_staffing->_btnAllAvailable,
		g->region_staffing_actions_action_slot_2, FluidVRole::Fixed, staffingActionsAnchor);
	place(_staffing->_btnCancel, g->action_cancel, FluidVRole::BottomShift);
	place(_staffing->_btnOk, g->action_start_project, FluidVRole::BottomShift);
}

CalypsoHdOperationsModel CalypsoF09ResearchUi::buildStaffingModel() const
{
	CalypsoHdOperationsModel model;
	if (!_staffing || !_staffing->_window || !_staffing->_game
		|| !_staffing->_base || !_staffing->_project
		|| !_staffing->_txtTitle || !_staffing->_txtAvailableScientist
		|| !_staffing->_txtAvailableSpace || !_staffing->_txtAllocatedScientist
		|| !_staffing->_txtMore || !_staffing->_txtLess
		|| !_staffing->_btnCancel || !_staffing->_btnOk
		|| !_staffing->_btnMore || !_staffing->_btnLess
		|| !_staffing->_surfaceScientists || !_staffing->_btnAllAvailable
		|| !_staffing->_btnRemoveAll) return model;
	const auto tr = [this](const std::string &key) { return _staffing->tr(key); };
	const bool wide = _staffing->_hdWideLayout;
	const auto *g = CalypsoF09ResearchStaffingGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) return model;
	const auto fluid = CalypsoHdOperationsFluidFrame::forReference(
		g->window.w, g->window.h, g->footer.x, g->footer.w);
	auto groupAnchor = [&](const auto &parent) {
		const int contentRight = fluid.policy.contentLeft + fluid.policy.contentWidth;
		return fluidSameEdge(parent.x + parent.w, contentRight)
			&& !fluidSameEdge(parent.x, fluid.policy.contentLeft)
			? FluidAnchor::Right : FluidAnchor::Geometry;
	};
	const auto capacityAnchor = groupAnchor(g->region_capacity);
	const auto staffingActionsAnchor = wide ? FluidAnchor::Right
		: groupAnchor(g->region_staffing_actions);
	auto p = [&](const auto &r, FluidVRole vr = FluidVRole::Fixed,
		FluidAnchor ha = FluidAnchor::Geometry) {
		const auto css = fluid.cssY(fluid.cssX(
			CalypsoHdOperationsRectf{r.x, r.y, r.w, r.h}, ha), vr);
		return CalypsoHdOperationsRect{css.x, css.y, css.w, css.h};
	};
	model.archetype = CalypsoHdOperationsArchetype::WideDetail;
	model.familyId = CalypsoF09ResearchStaffingGen::kFamilyId;
	setGeneratedMetadata(model,
		CalypsoF09ResearchStaffingGen::kPresentationProfile,
		CalypsoF09ResearchStaffingGen::kProfileId,
		CalypsoF09ResearchStaffingGen::kProfileVersion,
		CalypsoF09ResearchStaffingGen::kProvenanceTemplate);
	calypsoHdOperationsApplyGeneratedStyle(model, CalypsoF09ResearchStaffingGen::kProfileStyle);
	calypsoHdOperationsApplyGeneratedTypography(model,
		wide ? CalypsoF09ResearchStaffingGen::kTypographyWide : CalypsoF09ResearchStaffingGen::kTypographyCompact);
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
	model.geometry.designWidth = fluid.width();
	model.geometry.designHeight = fluid.height();
	model.geometry.window = p(g->window);
	model.geometry.status = p(g->status);
	model.geometry.title = p(g->title);
	model.geometry.screenHeader = p(g->status);
	model.geometry.headerArt = p(g->headerArt);
	model.geometry.controlBar = p(g->controlBar);
	model.geometry.footer = p(g->footer, FluidVRole::BottomShift);
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
	capacity.rect = p(g->region_capacity, FluidVRole::Fixed, capacityAnchor);
	capacity.labelRect = p(g->region_capacity_label, FluidVRole::Fixed, capacityAnchor);
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
		p(g->region_capacity_field_available_value, FluidVRole::Fixed, capacityAnchor)));
	capacity.fields.push_back(metric("labs",
		tr("STR_CALYPSO_RESEARCH_LABORATORY_SPACE_AVAILABLE_UC"),
		std::to_string(availableLaboratories),
		p(g->region_capacity_field_labs_value, FluidVRole::Fixed, capacityAnchor)));
	capacity.fields.push_back(metric("allocated",
		tr("STR_CALYPSO_RESEARCH_SCIENTISTS_ALLOCATED"),
		std::to_string(_staffing->_project->getAssigned()),
		p(g->region_capacity_field_allocated_value, FluidVRole::Fixed, capacityAnchor)));
	addRegion("staffing-actions", tr("STR_CALYPSO_RESEARCH_STAFFING_ACTIONS"),
		g->region_staffing_actions, g->region_staffing_actions_label);
	auto &staffingActions = model.regions.back();
	staffingActions.rect = p(g->region_staffing_actions, FluidVRole::Fixed, staffingActionsAnchor);
	staffingActions.labelRect = p(g->region_staffing_actions_label, FluidVRole::Fixed, staffingActionsAnchor);
	staffingActions.actions.push_back(action("remove-all", tr("STR_REMOVE_ALL"),
		p(g->region_staffing_actions_action_slot_1, FluidVRole::Fixed, staffingActionsAnchor), _staffing->_btnRemoveAll));
	staffingActions.actions.push_back(action("all-available", tr("STR_ALL_AVAILABLE"),
		p(g->region_staffing_actions_action_slot_2, FluidVRole::Fixed, staffingActionsAnchor), _staffing->_btnAllAvailable));
	staffingActions.kind = CalypsoHdOperationsRegionKind::Actions;
	CalypsoHdOperationsControl control;
	control.id = "scientists";
	control.label = tr("STR_CALYPSO_RESEARCH_SCIENTISTS");
	control.kind = CalypsoHdOperationsControlKind::Stepper;
	control.displayValue = std::to_string(_staffing->_project->getAssigned());
	control.rect = p(g->control_scientists);
	control.valueRect = p(g->control_scientists_value);
	control.labelRect = p(g->control_scientists_label);
	control.widget = _staffing->_btnMore;
	control.decrement = action("scientists-decrement", "−",
		p(g->control_scientists_decrement), _staffing->_btnLess);
	control.increment = action("scientists-increment", "+",
		p(g->control_scientists_increment), _staffing->_btnMore);
	model.controls.push_back(std::move(control));
	model.geometry.footerActions = {
		p(g->action_cancel, FluidVRole::BottomShift),
		p(g->action_start_project, FluidVRole::BottomShift)};
	model.footerActions.push_back(action("cancel", _staffing->_btnCancel->getText(),
		p(g->action_cancel, FluidVRole::BottomShift), _staffing->_btnCancel, true, "danger"));
	model.footerActions.push_back(action("start-project", _staffing->_btnOk->getText(),
		p(g->action_start_project, FluidVRole::BottomShift), _staffing->_btnOk, true, "primary"));
	setFonts(model, _staffing->_game->getMod());
	return model;
}


} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
