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
	: _state(state), _renderer(nullptr), _ready(false)
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
	return _state != nullptr && _state->_game != nullptr
		&& _state->_game->getMod() != nullptr;
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
	if (_state == nullptr)
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
