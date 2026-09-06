// F01 Basescape HD presentation holder (T12). Whole-file Emscripten guard.
#ifdef __EMSCRIPTEN__

#include "CalypsoBasescapeHdUi.h"
#include "CalypsoHdScreenRenderer.h"
#include "CalypsoHdUiOverlay.h"

#include "../Basescape/BasescapeState.h"
#include "../Basescape/BaseView.h"
#include "../Basescape/MiniBaseView.h"
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
#include <string>
#include <utility>

#include "../Interface/TextEdit.h"
#include "CalypsoTextEdit.h"

#include "../Engine/Game.h"
#include "../Mod/Mod.h"

namespace OpenXcom
{
namespace Calypso
{

CalypsoBasescapeHdUi::CalypsoBasescapeHdUi(BasescapeState *state)
	: _state(state), _renderer(nullptr), _ready(false), _scalingApplied(false)
{
	CalypsoHdScreenRenderModel model;
	model.archetype = "base-command-shell";
	_renderer = new CalypsoHdScreenRenderer(state, std::move(model),
		CalypsoHdScreenRenderMode::BasescapeLiveChrome);
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
	if (_state == nullptr || _state->_game == nullptr)
	{
		return false;
	}
	const Mod *mod = _state->_game->getMod();
	if (mod == nullptr || !mod->isHdUiFamilyEnabled("F01"))
	{
		return false;
	}
	return true;
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

bool CalypsoBasescapeHdUi::applyGeometry()
{
	// T14: position every surface at its generated authored rect (compare-
	// first: setWidth/setHeight recreate SDL surfaces), wire the T10/T11
	// input extents to the same numbers, then capture/refresh the one-shot
	// UI scaling against the authored canvas (no vanilla-center shift: rects
	// are already authored).
	if (_state == nullptr)
	{
		return false;
	}
	const auto *layout = CalypsoBasescapeCommandShellGen::layoutForDesign(1280, 720);
	if (layout == nullptr)
	{
		return false;
	}
	bool changed = false;
	const auto place = [&](Surface *widget, int x, int y, int w, int h) {
		if (widget == nullptr)
		{
			return;
		}
		if (widget->getX() != x)
		{
			widget->setX(x);
			changed = true;
		}
		if (widget->getY() != y)
		{
			widget->setY(y);
			changed = true;
		}
		if (widget->getWidth() != w)
		{
			widget->setWidth(w);
			changed = true;
		}
		if (widget->getHeight() != h)
		{
			widget->setHeight(h);
			changed = true;
		}
	};
	for (int i = 0; i < layout->actionCount; ++i)
	{
		const auto &gen = layout->actions[i];
		place(resolveActionWidget(*_state, gen.id),
			gen.visible.x, gen.visible.y, gen.visible.w, gen.visible.h);
	}
	const auto region = [&](const char *id) -> CalypsoBasescapeCommandShellGen::CalypsoBasescapeCommandShellGenRect {
		for (int i = 0; i < layout->regionCount; ++i)
		{
			if (std::string(layout->regions[i].id) == id)
			{
				return layout->regions[i].rect;
			}
		}
		return {0, 0, 0, 0};
	};
	const auto deck = region("deckSquare");
	place(_state->_view, deck.x, deck.y, deck.w, deck.h);
	if (_state->_view != nullptr)
	{
		_state->_view->setCalypsoHdGridExtent(deck.w, deck.h);
	}
	const auto selector = region("titleSelector");
	place(_state->_mini, selector.x, selector.y, selector.w, selector.h);
	if (_state->_mini != nullptr)
	{
		_state->_mini->setCalypsoHdMiniGeometry(44.0, 44.0);
	}
	const auto name = region("titleName");
	place(_state->_edtBase, name.x, name.y, name.w, name.h);
	const auto regionText = region("titleRegion");
	place(_state->_txtLocation, regionText.x, regionText.y, regionText.w, regionText.h);
	const auto funds = region("headerFunds");
	place(_state->_txtFunds, funds.x, funds.y, funds.w, funds.h);
	const auto hover = region("hoverLine");
	place(_state->_txtFacility, hover.x, hover.y, hover.w, hover.h);
	ensureRailButtons();
	{
		// Rail input owners sit on the shared chrome rects (single source
		// with the painter). Owned by the state surfaces; never deleted here.
		const CommandCenter::CommandCenterLayout baseCc =
			CommandCenter::computeDesktopLayout(CommandCenter::Size2{1280.0f, 720.0f}, false);
		const int slots[4] = {2, 3, 4, -1};
		for (size_t k = 0; k < _railButtons.size() && k < 4; ++k)
		{
			const CommandCenter::RectF r = slots[k] >= 0
				? CommandCenter::calypsoCcRailItemRect(baseCc.navigationRail, slots[k])
				: CommandCenter::calypsoCcRailSettingsRect(baseCc.navigationRail);
			place(_railButtons[k], (int)r.x, (int)r.y, (int)r.width, (int)r.height);
		}
	}
	if (!_scalingApplied)
	{
		_state->enableUiScaling(1280, 720, 1.0f, false);
		_scalingApplied = true;
	}
	else if (changed)
	{
		_state->recaptureUiScaling(1280, 720, 1.0f, false);
	}
	else
	{
		_state->applyUiScaling();
	}
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
	return _ready && _state != nullptr && _state->_game != nullptr
		&& _state->_game->getTopState() != _state;
}

void CalypsoBasescapeHdUi::refresh()
{
	// Re-check the family gate on every init (cheap, static per session) and
	// register the renderer once ready. Geometry and model stay untouched
	// while the gate is off: zero behavior change on the legacy route.
	if (!_ready)
	{
		_ready = checkReadiness();
	}
	if (_ready && _renderer != nullptr)
	{
		CalypsoHdUiOverlay::instance().registerAdapter(_renderer);
	}
	if (!_ready || _renderer == nullptr || _state == nullptr)
	{
		return;
	}
	applyGeometry();
	feedModel();
}

void CalypsoBasescapeHdUi::feedModel()
{
	// T14a: project live widgets through the generated desktop contract into
	// the shared render model. Read-only: strings via getText, placements via
	// Base, input owners via resolveActionWidget. No handler calls.
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
		const TextButton *button = dynamic_cast<const TextButton *>(widget);
		if (button != nullptr && !button->getText().empty())
		{
			visual.label = button->getText();
		}
		else
		{
			visual.label = gen.label;
		}
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

	CalypsoBasescapeHdSnapshot snapshot;
	snapshot.baseName = _state->_edtBase != nullptr ? _state->_edtBase->getText() : std::string();
	snapshot.region = textOf(_state->_txtLocation);
	snapshot.funds = textOf(_state->_txtFunds);
	snapshot.hoverFacility = textOf(_state->_txtFacility);
	snapshot.baseCount = 0;
	snapshot.selectedBase = 0;
	if (_state->_game != nullptr && _state->_game->getSavedGame() != nullptr
		&& _state->_game->getSavedGame()->getBases() != nullptr)
	{
		const std::vector<Base *> *bases = _state->_game->getSavedGame()->getBases();
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

	const auto findRegion = [&](const char *id) {
		for (int i = 0; i < layout->regionCount; ++i)
		{
			if (std::string(layout->regions[i].id) == id)
			{
				return layout->regions[i].rect;
			}
		}
		return CalypsoBasescapeCommandShellGen::CalypsoBasescapeCommandShellGenRect{0, 0, 0, 0};
	};
	{
		const auto deck = findRegion("deckSquare");
		snapshot.deckRect = {deck.x, deck.y, deck.w, deck.h};
		snapshot.deckCell = std::min(deck.w, deck.h) / 6;
		const auto sel = findRegion("titleSelector");
		snapshot.selectorRect = {sel.x, sel.y, sel.w, sel.h};
	}
	if (_state->_view != nullptr && _state->_view->getSelectedFacility() != nullptr)
	{
		const BaseFacility *hovered = _state->_view->getSelectedFacility();
		snapshot.hasHoverCell = true;
		snapshot.hoverX = _state->_view->getGridX();
		snapshot.hoverY = _state->_view->getGridY();
		snapshot.hoverSizeX = hovered->getRules()->getSizeX();
		snapshot.hoverSizeY = hovered->getRules()->getSizeY();
	}

	std::vector<BaseCraftDrawing> drawings;
	if (_state->_view != nullptr)
	{
		_state->_view->assignCraftsForDrawing(drawings);
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
				craft.name = row.craft->getName(_state->_game->getLanguage());
				craft.away = row.craft->getStatus() == "STR_OUT";
				craft.artKey = row.craft->getRules()->getType();
				snapshot.crafts.push_back(std::move(craft));
				break;
			}
		}
		snapshot.facilities.push_back(std::move(visual));
	}

	model.baseSnapshot = std::move(snapshot);
	_renderer->setModel(std::move(model));
}

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
