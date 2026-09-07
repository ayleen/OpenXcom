// F01 Basescape HD presentation holder (T12). Whole-file Emscripten guard.
#ifdef __EMSCRIPTEN__

#include "CalypsoBasescapeHdUi.h"
#include "CalypsoBasescapeHdLayout.h"
#include "CalypsoHdScreenRenderer.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoViewportRuntime.h"
#include "CalypsoViewportMailbox.h"
#include "../Engine/Options.h"
#include "../Engine/TTFFont.h"

#include "../Basescape/BasescapeState.h"
#include "../Basescape/PlaceFacilityState.h"
#include "../Basescape/BaseView.h"
#include "../Basescape/MiniBaseView.h"
#include "../Engine/Unicode.h"
#include "../Savegame/Region.h"
#include "../Mod/RuleRegion.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "CommandCenter/CommandCenterInteraction.h"
#include "CommandCenter/CommandCenterLayout.h"
#include "../Mod/RuleBaseFacility.h"
#include "../Mod/RuleCraft.h"
#include "../Savegame/Base.h"
#include "../Savegame/BaseFacility.h"
#include "../Savegame/Craft.h"
#include "../Savegame/SavedGame.h"
#include "Generated/CalypsoBasescapeCommandShell.generated.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

#include "../Interface/TextEdit.h"
#include "CalypsoTextEdit.h"

#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Savegame/GameTime.h"

namespace OpenXcom
{
namespace Calypso
{

CalypsoBasescapeHdUi::CalypsoBasescapeHdUi(BasescapeState *state)
	: _state(state), _placementState(nullptr), _renderer(nullptr), _ready(false)
{
	CalypsoHdScreenRenderModel model;
	model.archetype = "base-command-shell";
	_renderer = new CalypsoHdScreenRenderer(state, std::move(model),
		CalypsoHdScreenRenderMode::BasescapeLiveChrome);
}

CalypsoBasescapeHdUi::CalypsoBasescapeHdUi(PlaceFacilityState *state)
	: _state(nullptr), _placementState(state), _renderer(nullptr), _ready(false)
{
	CalypsoHdScreenRenderModel model;
	model.archetype = "base-command-shell";
	_renderer = new CalypsoHdScreenRenderer(state, std::move(model),
		CalypsoHdScreenRenderMode::BasescapePlacementChrome);
}

CalypsoBasescapeHdUi::~CalypsoBasescapeHdUi()
{
	// The renderer destructor clears its overlay registration. State members
	// declared after the holder outlive it, and the holder never owns
	// Base/SavedGame (T12.10), so no teardown order hazard.
	delete _renderer;
}

bool CalypsoBasescapeHdUi::checkReadiness() const
{
	Game *game = _placementState != nullptr ? _placementState->_game
		: (_state != nullptr ? _state->_game : nullptr);
	return game != nullptr && game->getMod() != nullptr;
}

void CalypsoBasescapeHdUi::configure(BasescapeState &state)
{
	if (state._calypsoHdUi != nullptr)
	{
		return;
	}
	auto *holder = new CalypsoBasescapeHdUi(&state);
	state._calypsoHdUi = holder;
	holder->refresh();
}

void CalypsoBasescapeHdUi::configure(PlaceFacilityState &state)
{
	if (state._calypsoHdUi != nullptr)
	{
		return;
	}
	auto *holder = new CalypsoBasescapeHdUi(&state);
	state._calypsoHdUi = holder;
	holder->refreshPlacement();
}

void CalypsoBasescapeHdUi::ensureRailButtons()
{
	// T15: visible input-only owners for the rail routes without native
	// buttons (Operations/Analytics/Archive/Settings). World reuses the
	// repositioned Geoscape button; Bases is an indicator. Pixels come from
	// the shared chrome; native blits are claimed via the suppression list.
	if (!_railButtons.empty() || _state == nullptr)
	{
		return;
	}
	struct RailRoute { ActionHandler handler; };
	const RailRoute routes[4] = {
		{ (ActionHandler)&BasescapeState::calypsoRailOperationsClick },
		{ (ActionHandler)&BasescapeState::calypsoRailAnalyticsClick },
		{ (ActionHandler)&BasescapeState::calypsoRailArchiveClick },
		{ (ActionHandler)&BasescapeState::calypsoRailSettingsClick },
	};
	for (const auto &route : routes)
	{
		TextButton *button = new TextButton(72, 72, 0, 0);
		button->setText(std::string());
		_state->add(button, "button", "basescape");
		button->onMouseClick(route.handler);
		_railButtons.push_back(button);
	}
}

namespace
{

/// Frame geometry shared by widget placement (here) and frame paint
/// (CalypsoHdScreenRenderer::collectBasescape): real CSS viewport, frozen
/// presentation metrics, CommandCenter fit at that viewport, and the
/// canonical base derivation. Both consumers run the same pure helpers, so
/// input rects and painted pixels agree on each axis, including resize.
struct BasescapeHdFrameGeometry
{
	bool valid = false;
	CommandCenter::CommandCenterLayout cc;
	CalypsoBasescapeHdProjection proj;
	CalypsoBasescapeHdDerivedLayout derived;
};

BasescapeHdFrameGeometry currentFrameGeometry()
{
	BasescapeHdFrameGeometry out;
	const CalypsoLayoutMetrics& vp = calypsoViewportRuntime().current();
	const CalypsoHdPresentationMetrics metrics = calypsoHdBuildPresentationMetrics(
		Options::baseXResolution, Options::baseYResolution);
	const int cssW = std::max(1, vp.logicalWidth);
	const int cssH = std::max(1, vp.logicalHeight);
	if (!metrics.valid() || metrics.scaleX <= 0.0 || metrics.scaleY <= 0.0)
	{
		return out;
	}
	out.cc = CommandCenter::computeLayout(
		CommandCenter::Size2{static_cast<float>(cssW), static_cast<float>(cssH)},
		false, CommandCenter::InsetsF{
			static_cast<float>(vp.safeX),
			static_cast<float>(vp.safeY),
			static_cast<float>(cssW - vp.safeX - vp.safeWidth),
			static_cast<float>(cssH - vp.safeY - vp.safeHeight)});
	out.proj = calypsoBasescapeHdProjection(cssW, cssH,
		metrics.physicalWidth, metrics.physicalHeight,
		metrics.scaleX, metrics.scaleY,
		static_cast<double>(metrics.contentOffsetX),
		static_cast<double>(metrics.contentOffsetY),
		out.cc.scale);
	const CalypsoBasescapeHdFitParams params;
	const CalypsoBasescapeHdAuthoredSize authored =
		calypsoBasescapeHdAuthoredSize(cssW, cssH, params);
	out.derived = calypsoBasescapeHdDerivedLayout(authored.w, authored.h, params);
	out.valid = true;
	return out;
}

} // namespace

bool CalypsoBasescapeHdUi::applyGeometry()
{
	// T14: position every surface at the shared CSS-viewport projection of
	// the canonical derived rects (compare-first: setWidth/setHeight
	// recreate SDL surfaces). Widgets carry logical pixels directly -- the
	// legacy uniform-fit UI scaling is never enabled, so no second transform
	// can drift input away from paint. Never falls through to legacy
	// State::resize (see resize()): rects are already authored.
	if (_state == nullptr && _placementState == nullptr)
	{
		return false;
	}
	const BasescapeHdFrameGeometry geo = currentFrameGeometry();
	if (!geo.valid)
	{
		CalypsoHdUiOverlay::instance().failHdRoute(
			"Basescape HD requires valid presentation metrics");
		return true;
	}
	bool changed = false;
	const auto place = [&](Surface *widget, const CalypsoBasescapeHdRect &design) {
		const CalypsoBasescapeHdLogicalRect logical =
			calypsoBasescapeHdProjectRect(geo.proj, design);
		if (widget == nullptr)
		{
			return;
		}
		if (widget->getX() != logical.x)
		{
			widget->setX(logical.x);
			changed = true;
		}
		if (widget->getY() != logical.y)
		{
			widget->setY(logical.y);
			changed = true;
		}
		if (widget->getWidth() != logical.w)
		{
			widget->setWidth(logical.w);
			changed = true;
		}
		if (widget->getHeight() != logical.h)
		{
			widget->setHeight(logical.h);
			changed = true;
		}
	};
	if (_placementState != nullptr)
	{
		// Placement mode: the state's own BaseView keeps validation/input
		// ownership at the shared deck rect with instance-local HD extents;
		// Cancel is the only other interactive owner, pinned to the
		// service-row band. The window and detail texts never move: they are
		// suppressed model sources, never painted.
		place(_placementState->_view, geo.derived.deckGrid);
		if (_placementState->_view != nullptr)
		{
			_placementState->_view->setCalypsoHdGridExtent(
				_placementState->_view->getWidth(), _placementState->_view->getHeight());
		}
		const CalypsoBasescapeHdPlacementColumn column =
			calypsoBasescapeHdPlacementColumn(geo.derived, CalypsoBasescapeHdFitParams{});
		place(_placementState->_btnCancel, column.cancel);
		(void)changed;
		return true;
	}
	for (int i = 0; i < 11; ++i)
	{
		place(resolveActionWidget(*_state, geo.derived.rows[i].actionId),
			geo.derived.rows[i].rect);
	}
	place(_state->_view, geo.derived.deckGrid);
	if (_state->_view != nullptr)
	{
		_state->_view->setCalypsoHdGridExtent(
			_state->_view->getWidth(), _state->_view->getHeight());
	}
	place(_state->_mini, geo.derived.titleSelector);
	if (_state->_mini != nullptr)
	{
		const double slot = static_cast<double>(_state->_mini->getWidth()) / 8.0;
		_state->_mini->setCalypsoHdMiniGeometry(slot, slot);
	}
	place(_state->_edtBase, geo.derived.titleName);
	TTFFont* editorFont = _state->_game->getMod()->getTTFFont("FONT_CC_INTER_SB", false);
	if (editorFont == nullptr)
		CalypsoHdUiOverlay::instance().failHdRoute("Basescape title font is unavailable");
	_state->_edtBase->setPhysicalTextMetrics(editorFont,
		CalypsoBasescapeHdFitParams{}.titleFontSize * geo.proj.uiScale / editorFont->pixelSize());
	place(_state->_txtLocation, geo.derived.titleRegion);
	place(_state->_txtFunds, geo.derived.fundsLine);
	place(_state->_txtFacility, geo.derived.hoverLine);
	ensureRailButtons();
	{
		// Rail input owners sit on the shared chrome rects (single source
		// with the painter). Owned by the state surfaces; never deleted here.
		const int slots[4] = {2, 3, 4, -1};
		for (size_t k = 0; k < _railButtons.size() && k < 4; ++k)
		{
			const CommandCenter::RectF r = slots[k] >= 0
				? CommandCenter::calypsoCcRailItemRect(geo.cc.navigationRail, slots[k])
				: CommandCenter::calypsoCcRailSettingsRect(geo.cc.navigationRail);
			place(_railButtons[k], CalypsoBasescapeHdRect{
				static_cast<int>(std::lround(r.x)), static_cast<int>(std::lround(r.y)),
				static_cast<int>(std::lround(r.width)), static_cast<int>(std::lround(r.height))});
		}
	}
	(void)changed;
	return true;
}

bool CalypsoBasescapeHdUi::resize(BasescapeState &state)
{
	// T13: keep the browser text-input overlay on the live name field across
	// resizes. refreshExternalGeometry is a self-guarded no-op unless the
	// field is the focused edit.
	if (state._edtBase != nullptr)
	{
		CalypsoTextEdit::refreshExternalGeometry(*state._edtBase);
	}
	// T14: re-apply the authored geometry and consume the resize. Legacy
	// State::resize would recenter (drag) every surface off its generated
	// rect, so a handled layout must never fall through to it.
	CalypsoBasescapeHdUi *holder = state._calypsoHdUi;
	if (holder != nullptr && holder->_ready)
	{
		holder->applyGeometry();
		return true;
	}
	return false;
}

bool CalypsoBasescapeHdUi::resize(PlaceFacilityState &state)
{
	// Placement consumes resizes the same way: re-apply the authored geometry
	// (deck rect for paint+input, Cancel rect) and never fall through to the
	// legacy recenter. No per-frame Surface reallocations.
	CalypsoBasescapeHdUi *holder = state._calypsoHdUi;
	if (holder != nullptr && holder->_ready)
	{
		holder->applyGeometry();
		return true;
	}
	return false;
}

Surface *CalypsoBasescapeHdUi::resolveActionWidget(BasescapeState &state, const std::string &id)
{
	// T13: one audited table, semantic ID to the existing input owner. No new
	// widgets, no copied gameplay lambdas; the handlers stay on the buttons.
	if (id == "base.new") return state._btnNewBase;
	if (id == "base.info") return state._btnBaseInfo;
	if (id == "base.divers") return state._btnSoldiers;
	if (id == "base.crafts") return state._btnCrafts;
	if (id == "base.build") return state._btnFacilities;
	if (id == "base.research") return state._btnResearch;
	if (id == "base.manufacture") return state._btnManufacture;
	if (id == "base.transfer") return state._btnTransfer;
	if (id == "base.purchase") return state._btnPurchase;
	if (id == "base.sell") return state._btnSell;
	if (id == "navigation.world") return state._btnGeoscape;
	return nullptr;
}

bool CalypsoBasescapeHdUi::covered() const
{
	// T16: covered means another state is on top.
	// Suppression stays applied while covered.
	// blit() paints the neutral backing below.
	if (!_ready)
	{
		return false;
	}
	if (_placementState != nullptr)
	{
		return _placementState->_game != nullptr
			&& _placementState->_game->getTopState() != _placementState;
	}
	return _state != nullptr && _state->_game != nullptr
		&& _state->_game->getTopState() != _state;
}

bool CalypsoBasescapeHdUi::placementMode() const
{
	return _placementState != nullptr;
}

void CalypsoBasescapeHdUi::populateBaseVisuals(CalypsoBasescapeHdSnapshot &snapshot,
	Game *game, Base *base, BaseView *view)
{
	// Single read-only Base -> snapshot population for every native host of
	// the base-command-shell (BasescapeState, PlaceFacilityState, and the
	// chooser underlay): facilities with craft slots, craft visuals, and the
	// base selector entries. No facility/craft painting here; the shared
	// renderer owns that from this snapshot.
	snapshot.baseCount = 0;
	snapshot.selectedBase = 0;
	snapshot.bases.clear();
	snapshot.facilities.clear();
	snapshot.crafts.clear();
	if (game != nullptr && game->getSavedGame() != nullptr
		&& game->getSavedGame()->getBases() != nullptr)
	{
		const std::vector<Base *> *bases = game->getSavedGame()->getBases();
		snapshot.baseCount = static_cast<int>(bases->size());
		for (size_t b = 0; b < bases->size(); ++b)
		{
			if (bases->at(b) == base)
			{
				snapshot.selectedBase = static_cast<int>(b);
			}
			CalypsoBasescapeHdSelectorEntry entry;
			entry.name = bases->at(b) != nullptr ? bases->at(b)->getName() : std::string();
			entry.selected = bases->at(b) == base;
			if (bases->at(b) != nullptr)
			{
				for (const BaseFacility *fac : *bases->at(b)->getFacilities())
				{
					CalypsoBasescapeHdSelectorCell cell;
					cell.x = fac->getX();
					cell.y = fac->getY();
					cell.sizeX = fac->getRules()->getSizeX();
					cell.sizeY = fac->getRules()->getSizeY();
					cell.built = fac->getBuildTime() == 0;
					cell.disabled = fac->getDisabled();
					entry.cells.push_back(cell);
				}
			}
			snapshot.bases.push_back(std::move(entry));
		}
	}
	if (base == nullptr)
	{
		return;
	}

	std::vector<BaseCraftDrawing> drawings;
	if (view != nullptr)
	{
		view->assignCraftsForDrawing(drawings);
	}
	for (const BaseFacility *fac : *base->getFacilities())
	{
		CalypsoBasescapeHdFacilityVisual visual;
		visual.x = fac->getX();
		visual.y = fac->getY();
		visual.sizeX = fac->getRules()->getSizeX();
		visual.sizeY = fac->getRules()->getSizeY();
		visual.ruleType = fac->getRules()->getType();
		visual.buildTime = fac->getBuildTime();
		visual.disabled = fac->getDisabled();
		visual.hadPrevious = fac->getIfHadPreviousFacility();
		visual.connectorsDisabled = fac->getRules()->connectorsDisabled();
		visual.ammo = fac->getAmmo();
		visual.ammoMax = fac->getRules()->getAmmoMax();
		for (const BaseCraftDrawing &row : drawings)
		{
			if (row.pen == fac && row.craft != nullptr)
			{
				visual.craftIndex = static_cast<int>(snapshot.crafts.size());
				visual.craftDrawn = row.drawn;
				CalypsoBasescapeHdCraftVisual craft;
				craft.name = row.craft->getName(game->getLanguage());
				craft.away = row.craft->getStatus() == "STR_OUT";
				craft.artKey = row.craft->getRules()->getType();
				snapshot.crafts.push_back(std::move(craft));
				break;
			}
		}
		snapshot.facilities.push_back(std::move(visual));
	}
}

void CalypsoBasescapeHdUi::refresh()
{
	// Browser Basescape is always HD. Readiness checks prerequisites, not a
	// feature toggle; an unavailable route must never resume native drawing.
	if (!_ready)
	{
		_ready = checkReadiness();
	}
	if (!_ready || _renderer == nullptr)
	{
		CalypsoHdUiOverlay::instance().failHdRoute(
			"Basescape HD prerequisites are unavailable");
	}
	CalypsoHdUiOverlay::instance().registerAdapter(_renderer);
	applyGeometry();
	feedModel();
}

void CalypsoBasescapeHdUi::feedModel()
{
	// T14a: project live widgets through the canonical derived desktop
	// contract into the shared render model. Read-only: strings via getText,
	// placements via Base, input owners via resolveActionWidget. No handler
	// calls, no per-frame setters (the TextEdit keeps rename ownership).
	const BasescapeHdFrameGeometry geo = currentFrameGeometry();
	if (!geo.valid)
	{
		return;
	}
	const CalypsoBasescapeCommandShellGen::CalypsoBasescapeCommandShellGenLayout *layout =
		CalypsoBasescapeCommandShellGen::layoutForDesign(1280, 720);
	if (layout == nullptr)
	{
		return;
	}
	Base *base = _state->_base;
	if (base == nullptr)
	{
		return;
	}
	const auto textOf = [](const Text *widget) -> std::string {
		return widget != nullptr ? widget->getText() : std::string();
	};

	CalypsoHdScreenRenderModel model;
	model.archetype = CalypsoBasescapeCommandShellGen::kArchetype;
	model.designWidth = layout->designWidth;
	model.designHeight = layout->designHeight;
	for (int i = 0; i < layout->actionCount; ++i)
	{
		const auto &gen = layout->actions[i];
		// Placement-only semantic action (template slot "placement-cancel"):
		// the placement holder feeds Cancel from the derived column, never
		// from the live base layout, so it stays out of the live model.
		if (std::string(gen.id) == "base.placement.cancel")
		{
			continue;
		}
		CalypsoHdScreenActionVisual visual;
		visual.id = gen.id;
		visual.component = gen.component;
		visual.slotRole = gen.slotRole;
		visual.coordinateSpace = gen.coordinateSpace;
		visual.visible = {gen.visible.x, gen.visible.y, gen.visible.w, gen.visible.h};
		visual.hit = {gen.hit.x, gen.hit.y, gen.hit.w, gen.hit.h};
		visual.focusOrder = gen.focusOrder;
		visual.zOrder = gen.zOrder;
		Surface *widget = resolveActionWidget(*_state, visual.id);
		visual.widget = widget;
		const auto *binding = calypsoBasescapeHdBindingFor(visual.id);
		if (binding == nullptr)
			CalypsoHdUiOverlay::instance().failHdRoute("Basescape action binding is missing: " + visual.id);
		visual.label = _state->tr(binding->labelKey);
		model.actions.push_back(std::move(visual));
	}
	for (int i = 0; i < layout->regionCount; ++i)
	{
		const auto &gen = layout->regions[i];
		CalypsoHdScreenRegionVisual region;
		region.id = gen.id;
		region.rect = {gen.rect.x, gen.rect.y, gen.rect.w, gen.rect.h};
		model.regions.push_back(std::move(region));
	}
	model.copy.emplace_back("heading.deck", _state->tr("STR_CALYPSO_BASE_LAYOUT"));
	model.copy.emplace_back("heading.column", _state->tr("STR_CALYPSO_BASE_FUNCTIONS"));
	model.copy.emplace_back("heading.logistics", _state->tr("STR_CALYPSO_BASE_LOGISTICS"));

	CalypsoBasescapeHdSnapshot snapshot;
	snapshot.baseName = _state->_edtBase != nullptr ? _state->_edtBase->getText() : std::string();
	snapshot.baseCaption = _state->tr("STR_BASES");
	if (const GameTime *time = _state->_game->getSavedGame()->getTime())
	{
		std::ostringstream clock;
		clock << time->getHour() << ":" << std::setfill('0') << std::setw(2) << time->getMinute();
		snapshot.displayTime = clock.str();
		snapshot.displayDate = time->getDayString(_state->_game->getLanguage()) + " "
			+ std::string(_state->tr(time->getMonthString())) + " " + std::to_string(time->getYear());
	}
	snapshot.region = textOf(_state->_txtLocation);
	snapshot.funds = textOf(_state->_txtFunds);
	snapshot.hoverFacility = textOf(_state->_txtFacility);
	populateBaseVisuals(snapshot, _state->_game, base, _state->_view);

	snapshot.deckRect = {geo.derived.deckGrid.x, geo.derived.deckGrid.y,
		geo.derived.deckGrid.w, geo.derived.deckGrid.h};
	snapshot.deckCell = geo.derived.deckCell;
	snapshot.selectorRect = {geo.derived.titleSelector.x, geo.derived.titleSelector.y,
		geo.derived.titleSelector.w, geo.derived.titleSelector.h};
	if (_state->_view != nullptr && _state->_view->getSelectedFacility() != nullptr)
	{
		const BaseFacility *hovered = _state->_view->getSelectedFacility();
		snapshot.hasHoverCell = true;
		snapshot.hoverX = _state->_view->getGridX();
		snapshot.hoverY = _state->_view->getGridY();
		snapshot.hoverSizeX = hovered->getRules()->getSizeX();
		snapshot.hoverSizeY = hovered->getRules()->getSizeY();
	}

	model.baseSnapshot = std::move(snapshot);
	_renderer->setModel(std::move(model));
}

void CalypsoBasescapeHdUi::refreshPlacement()
{
	// Placement mode model: the same base-command-shell archetype over the
	// same derived geometry. Cancel is the only interactive owner; every
	// other base action and the mini selector stay decorative. Detail strings
	// come from the live native widgets (cost/time/maintenance/resources),
	// guidance from the chooser-owned STR_CALYPSO_BUILD_* keys.
	if (_placementState == nullptr)
	{
		return;
	}
	if (!_ready)
	{
		_ready = checkReadiness();
	}
	if (!_ready || _renderer == nullptr)
	{
		CalypsoHdUiOverlay::instance().failHdRoute(
			"Basescape placement prerequisites are unavailable");
	}
	CalypsoHdUiOverlay::instance().registerAdapter(_renderer);
	applyGeometry();
	feedPlacementModel();
}

void CalypsoBasescapeHdUi::feedPlacementModel()
{
	const BasescapeHdFrameGeometry geo = currentFrameGeometry();
	if (!geo.valid)
	{
		return;
	}
	const CalypsoBasescapeCommandShellGen::CalypsoBasescapeCommandShellGenLayout *layout =
		CalypsoBasescapeCommandShellGen::layoutForDesign(1280, 720);
	if (layout == nullptr)
	{
		return;
	}
	PlaceFacilityState *ps = _placementState;
	if (ps == nullptr || ps->_game == nullptr || ps->_game->getSavedGame() == nullptr)
	{
		return;
	}
	Game *game = ps->_game;
	Base *base = ps->_base;
	const RuleBaseFacility *rule = ps->_rule;
	if (base == nullptr || rule == nullptr)
	{
		return;
	}
	const auto textOf = [](const auto *widget) -> std::string {
		return widget != nullptr ? widget->getText() : std::string();
	};

	CalypsoHdScreenRenderModel model;
	model.archetype = CalypsoBasescapeCommandShellGen::kArchetype;
	model.designWidth = layout->designWidth;
	model.designHeight = layout->designHeight;
	const CalypsoBasescapeHdPlacementColumn column =
		calypsoBasescapeHdPlacementColumn(geo.derived, CalypsoBasescapeHdFitParams{});
	CalypsoHdScreenActionVisual cancel;
	cancel.id = "base.placement.cancel";
	cancel.label = textOf(ps->_btnCancel);
	cancel.component = "management-action-group";
	cancel.slotRole = "placement-cancel";
	cancel.coordinateSpace = "screen";
	cancel.visible = {column.cancel.x, column.cancel.y, column.cancel.w, column.cancel.h};
	cancel.hit = cancel.visible;
	cancel.focusOrder = 120;
	cancel.zOrder = 1;
	cancel.widget = ps->_btnCancel;
	model.actions.push_back(std::move(cancel));
	for (int i = 0; i < layout->regionCount; ++i)
	{
		const auto &gen = layout->regions[i];
		CalypsoHdScreenRegionVisual region;
		region.id = gen.id;
		region.rect = {gen.rect.x, gen.rect.y, gen.rect.w, gen.rect.h};
		model.regions.push_back(std::move(region));
	}
	model.copy.emplace_back("heading.deck", ps->tr("STR_CALYPSO_BASE_LAYOUT"));
	model.copy.emplace_back("heading.column", ps->tr("STR_CALYPSO_BASE_FUNCTIONS"));
	model.copy.emplace_back("heading.logistics", ps->tr("STR_CALYPSO_BASE_LOGISTICS"));

	CalypsoBasescapeHdSnapshot snapshot;
	snapshot.baseName = base->getName();
	snapshot.baseCaption = ps->tr("STR_BASES");
	if (const GameTime *time = game->getSavedGame()->getTime())
	{
		std::ostringstream clock;
		clock << time->getHour() << ":" << std::setfill('0') << std::setw(2) << time->getMinute();
		snapshot.displayTime = clock.str();
		snapshot.displayDate = time->getDayString(game->getLanguage()) + " "
			+ std::string(ps->tr(time->getMonthString())) + " " + std::to_string(time->getYear());
	}
	for (const auto* region : *game->getSavedGame()->getRegions())
	{
		if (region->getRules()->insideRegion(base->getLongitude(), base->getLatitude()))
		{
			snapshot.region = ps->tr(region->getRules()->getType());
			break;
		}
	}
	snapshot.funds = ps->tr("STR_FUNDS").arg(Unicode::formatFunding(game->getSavedGame()->getFunds()));
	populateBaseVisuals(snapshot, game, base, ps->_view);
	snapshot.deckRect = {geo.derived.deckGrid.x, geo.derived.deckGrid.y,
		geo.derived.deckGrid.w, geo.derived.deckGrid.h};
	snapshot.deckCell = geo.derived.deckCell;
	snapshot.selectorRect = {geo.derived.titleSelector.x, geo.derived.titleSelector.y,
		geo.derived.titleSelector.w, geo.derived.titleSelector.h};

	CalypsoBasescapeHdPlacementVisual placement;
	placement.active = true;
	placement.ruleType = rule->getType();
	placement.sizeX = rule->getSizeX();
	placement.sizeY = rule->getSizeY();
	placement.isMove = ps->_origFac != nullptr;
	placement.facilityName = textOf(ps->_txtFacility);
	if (placement.facilityName.empty())
	{
		placement.facilityName = ps->tr(rule->getType());
	}
	snapshot.hoverFacility = placement.facilityName;
	const std::string costLine = textOf(ps->_txtCost) + " " + textOf(ps->_numCost);
	const std::string timeLine = textOf(ps->_txtTime) + " " + textOf(ps->_numTime);
	const std::string maintenanceLine = textOf(ps->_txtMaintenance) + " " + textOf(ps->_numMaintenance);
	if (costLine != " ")
	{
		placement.detailLines.push_back(costLine);
	}
	if (timeLine != " ")
	{
		placement.detailLines.push_back(timeLine);
	}
	if (maintenanceLine != " ")
	{
		placement.detailLines.push_back(maintenanceLine);
	}
	std::istringstream resources(textOf(ps->_numResources));
	std::string resourceLine;
	while (std::getline(resources, resourceLine))
	{
		if (!resourceLine.empty())
		{
			placement.detailLines.push_back(resourceLine);
		}
	}
	placement.guidanceSelect = placement.isMove
		? ps->tr("STR_CALYPSO_BUILD_MOVE_POSITION")
		: ps->tr("STR_CALYPSO_BUILD_SELECT_POSITION");
	placement.guidanceValid = ps->tr("STR_CALYPSO_BUILD_VALID_POSITION");
	placement.guidanceInvalid = ps->tr("STR_CALYPSO_BUILD_INVALID_POSITION");
	placement.cancelLabel = textOf(ps->_btnCancel);
	snapshot.placement = std::move(placement);

	model.baseSnapshot = std::move(snapshot);
	_renderer->setModel(std::move(model));
}

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
