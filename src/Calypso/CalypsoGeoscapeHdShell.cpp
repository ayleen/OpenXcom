#ifdef __EMSCRIPTEN__
#include "CalypsoGeoscapeHdShell.h"

#include <string>
#include <vector>
#include "../Engine/Action.h"
#include "../Geoscape/GeoscapeState.h"
#include "CalypsoGeoscapeHdRuntime.h"

extern "C" int g_calypsoGeoscapeHdPreview;
#include "CalypsoViewportRuntime.h"
#include "../Engine/Game.h"
#include "../Engine/Screen.h"
#include "../Engine/Logger.h"
#include "../Engine/Language.h"
#include "../Mod/Mod.h"
#include "../Interface/TextButton.h"
#include "../Engine/Options.h"
#include "../Savegame/SavedGame.h"
#include "../Menu/NotesState.h"
#include "../Menu/SaveGameState.h"
#include "../Menu/LoadGameState.h"
#include "../Menu/TestState.h"
#include "../Geoscape/FundingState.h"

namespace OpenXcom
{
const Surface* CalypsoGeoscapeHdShell::resolveWidget(const GeoscapeState* s, const std::string& member)
{
	if (s == nullptr) return nullptr;
	if (member == "btnIntercept") return s->_btnIntercept;
	if (member == "btnBases") return s->_btnBases;
	if (member == "btnGraphs") return s->_btnGraphs;
	if (member == "btnUfopaedia") return s->_btnUfopaedia;
	if (member == "btnOptions") return s->_btnOptions;
	if (member == "btnFunding") return s->_btnFunding;
	if (member == "btn5Secs") return s->_btn5Secs;
	if (member == "btn1Min") return s->_btn1Min;
	if (member == "btn5Mins") return s->_btn5Mins;
	if (member == "btn30Mins") return s->_btn30Mins;
	if (member == "btn1Hour") return s->_btn1Hour;
	if (member == "btn1Day") return s->_btn1Day;
	if (member == "btnZoomIn") return s->_btnZoomIn;
	if (member == "btnZoomOut") return s->_btnZoomOut;
	return nullptr;
}

Surface* CalypsoGeoscapeHdShell::resolveWidget(GeoscapeState* s, const std::string& member)
{
	return const_cast<Surface*>(resolveWidget(static_cast<const GeoscapeState*>(s), member));
}

namespace
{
	struct RowDef { const char* id; const char* labelKey; const char* availability; const char* route; };

	bool rowAvailable(const RowDef& row, bool ironman)
	{
		if (std::string(row.availability) == "extended-links") return Options::oxceLinks;
		if (std::string(row.availability) == "debug-only") return Options::debug;
		if (std::string(row.availability) == "non-ironman")
			return !ironman;
		return true;
	}

	const std::vector<RowDef>& rowDefs()
	{
		static const std::vector<RowDef> rows = {
			{ "drawer.funding", "STR_FUNDING", "extended-links", "funding" },
			{ "drawer.tech-tree", "STR_TECH_TREE_VIEWER", "extended-links", "tech-tree" },
			{ "drawer.global-research", "STR_GLOBAL_RESEARCH", "extended-links", "global-research" },
			{ "drawer.global-production", "STR_GLOBAL_MANUFACTURE", "extended-links", "global-production" },
			{ "drawer.global-containment", "STR_GLOBAL_ALIEN_CONTAINMENT", "extended-links", "global-containment" },
			{ "drawer.ufo-tracker", "STR_UFO_TRACKER", "extended-links", "ufo-tracker" },
			{ "drawer.pilot-experience", "STR_DAILY_PILOT_EXPERIENCE", "extended-links", "pilot-experience" },
			{ "drawer.notes", "STR_NOTES", "extended-links", "notes" },
			{ "drawer.music", "STR_SELECT_MUSIC_TRACK", "extended-links", "music" },
			{ "drawer.debug", "STR_DEBUG", "extended-links", "debug" },
			{ "drawer.quick-save", "STR_QUICK_SAVE", "non-ironman", "quick-save" },
			{ "drawer.instant-save", "STR_INSTANT_SAVE", "non-ironman", "instant-save" },
			{ "drawer.quick-load", "STR_QUICK_LOAD", "non-ironman", "quick-load" },
		};
		return rows;
	}
} // anonymous namespace

struct CalypsoGeoscapeHdShellState
{
	Calypso::CalypsoGeoscapeHdDrawerState drawer;
	std::vector<std::pair<TextButton*, const char*>> rows;
	TextButton* sessionChip = nullptr;
	TextButton* pauseControl = nullptr;
	TextButton* speedBeforeOpen = nullptr;
	bool sessionWasFocused = false;
};

const Surface* CalypsoGeoscapeHdShell::resolveLiveWidget(const GeoscapeState* s, const std::string& actionId)
{
	if (s == nullptr || s->_calypsoHdShell == nullptr) return nullptr;
	const auto* shell = s->_calypsoHdShell;
	if (actionId == "action.session") return shell->sessionChip;
	if (actionId == "time.pause") return shell->pauseControl;
	for (const auto& entry : shell->rows)
		if (entry.second != nullptr && actionId == entry.second)
			return entry.first != nullptr && entry.first->getVisible() ? entry.first : nullptr;
	return nullptr;
}

Surface* CalypsoGeoscapeHdShell::resolveLiveWidget(GeoscapeState* s, const std::string& actionId)
{
	return const_cast<Surface*>(resolveLiveWidget(static_cast<const GeoscapeState*>(s), actionId));
}

bool CalypsoGeoscapeHdShell::isLiveActionVisible(const GeoscapeState* s, const std::string& actionId)
{
	if (s == nullptr || s->_calypsoHdShell == nullptr) return false;
	const auto* shell = s->_calypsoHdShell;
	if (actionId == "action.session")
		return shell->sessionChip != nullptr && shell->sessionChip->getVisible();
	if (actionId == "time.pause")
		return shell->pauseControl != nullptr && shell->pauseControl->getVisible();
	for (const auto& entry : shell->rows)
		if (entry.second != nullptr && actionId == entry.second)
			return entry.first != nullptr && entry.first->getVisible();
	return false;
}

CalypsoGeoscapeHdShellState* CalypsoGeoscapeHdShell::state(GeoscapeState* s)
{
	if (s->_calypsoHdShell == nullptr)
		s->_calypsoHdShell = new CalypsoGeoscapeHdShellState;
	return s->_calypsoHdShell;
}


const char* CalypsoGeoscapeHdShell::apply(GeoscapeState *s)
{
	using namespace OpenXcom::Calypso;
	using namespace OpenXcom::Calypso::CalypsoGeoscapeCommandShellGen;
	const bool canonicalListed = s->_game != nullptr && s->_game->getMod() != nullptr
		&& s->_game->getMod()->isHdUiFamilyEnabled("F16");
	const bool listed = calypsoGeoscapeHdPreviewFamilyEnabled(canonicalListed,
		g_calypsoGeoscapeHdPreview != 0);
	const auto decision = calypsoGeoscapeHdGateDecision(true, listed, true, true);
	auto* shell = state(s);
	if (!decision.enabled)
	{
		for (auto& row : shell->rows) row.first->setVisible(false);
		if (shell->sessionChip) shell->sessionChip->setVisible(false);
		if (shell->pauseControl) shell->pauseControl->setVisible(false);
		return decision.reason;
	}
	const auto& metrics = calypsoViewportRuntime().current();
	const bool wide = metrics.layoutClass == CalypsoLayoutClass::Wide;
	const auto* layout = layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
	if (layout == nullptr) return "layout-missing";
	const auto projection = calypsoGeoscapeHdProjection(*layout, metrics,
		Options::baseXResolution, Options::baseYResolution);
	int projected = 0;
	for (const auto& binding : calypsoGeoscapeHdWidgetBindings())
	{
		if (std::string(binding.role).rfind("widget:", 0) != 0) continue;
		Surface* widget = resolveWidget(s, std::string(binding.role).substr(7));
		if (widget == nullptr) continue;
		const auto r = projection.project(binding.actionId);
		widget->setX(r.x); widget->setY(r.y); widget->setWidth(r.w); widget->setHeight(r.h);
		++projected;
	}
	if (shell->sessionChip == nullptr)
	{
		shell->sessionChip = new TextButton(122, 46, 18, 16);
		s->add(shell->sessionChip, "button", "geoscape");
		shell->sessionChip->setText(s->tr("STR_SESSION"));
		shell->sessionChip->onMouseClick((ActionHandler)&GeoscapeState::calypsoToggleDrawer);
	}
	const auto sess = projection.project("action.session");
	shell->sessionChip->setX(sess.x); shell->sessionChip->setY(sess.y);
	shell->sessionChip->setWidth(sess.w); shell->sessionChip->setHeight(sess.h);
	shell->sessionChip->setVisible(true);
	if (shell->pauseControl == nullptr)
	{
		shell->pauseControl = new TextButton(50, 50, 0, 0);
		s->add(shell->pauseControl, "button", "geoscape");
		shell->pauseControl->setText(s->tr("STR_PAUSE"));
		shell->pauseControl->onMouseClick((ActionHandler)&GeoscapeState::calypsoTogglePause);
	}
	const auto pause = projection.project("time.pause");
	shell->pauseControl->setX(pause.x); shell->pauseControl->setY(pause.y);
	shell->pauseControl->setWidth(pause.w); shell->pauseControl->setHeight(pause.h);
	shell->pauseControl->setVisible(true);
	int deferred = 0;
	for (const auto& def : rowDefs())
	{
		TextButton* row = nullptr;
		for (auto& entry : shell->rows)
			if (entry.second != nullptr && def.id == entry.second) { row = entry.first; break; }
		if (row == nullptr)
		{
			row = new TextButton(320, 48, 0, 0);
			row->onMouseClick((ActionHandler)&GeoscapeState::calypsoDrawerDispatch);
			s->add(row, "button", "geoscape");
			shell->rows.emplace_back(row, def.id);
		}
		const auto r = projection.project(def.id);
		row->setX(r.x); row->setY(r.y); row->setWidth(r.w); row->setHeight(r.h);
		row->setText(s->tr(def.labelKey));   // G-1: localized drawer labels
		const bool ironman = s->_game->getSavedGame() != nullptr && s->_game->getSavedGame()->isIronman();
		row->setVisible(shell->drawer.open && rowAvailable(def, ironman));
	}
	Log(LOG_INFO) << "[HD] geoscape shell: layout=" << (wide ? "wide" : "compact") << " projected=" << projected << " drawer=" << (shell->drawer.open ? "open" : "closed");
	return decision.reason;
}

void CalypsoGeoscapeHdShell::toggleDrawer(GeoscapeState *s)
{
	auto* shell = state(s);
	if (!shell->drawer.open)
	{
		shell->speedBeforeOpen = s->_timeSpeed;
		shell->sessionWasFocused = shell->sessionChip != nullptr && shell->sessionChip->isFocused();
	}
	shell->drawer.toggle(s->_pause);
	s->_pause = shell->drawer.open ? true : shell->drawer.pauseBeforeOpen;
	if (!shell->drawer.open && shell->speedBeforeOpen != nullptr)
		s->_timeSpeed = shell->speedBeforeOpen;
	if (!shell->drawer.open && shell->sessionChip != nullptr)
		shell->sessionChip->setFocus(shell->sessionWasFocused);
	apply(s);
}

void CalypsoGeoscapeHdShell::closeDrawer(GeoscapeState *s)
{
	auto* shell = state(s);
	if (!shell->drawer.open) return;
	shell->drawer.open = false;
	s->_pause = shell->drawer.pauseBeforeOpen;
	if (shell->speedBeforeOpen != nullptr) s->_timeSpeed = shell->speedBeforeOpen;
	if (shell->sessionChip != nullptr) shell->sessionChip->setFocus(shell->sessionWasFocused);
	apply(s);
}

void CalypsoGeoscapeHdShell::destroy(GeoscapeState *s)
{
	delete s->_calypsoHdShell;
	s->_calypsoHdShell = nullptr;
}


/* Stage 9.1.3 bridge: the session chip toggles the drawer through the state. */
void GeoscapeState::calypsoToggleDrawer(Action *)
{
	CalypsoGeoscapeHdShell::toggleDrawer(this);
}

void GeoscapeState::calypsoTogglePause(Action *)
{
	_pause = !_pause;
}

void GeoscapeState::calypsoDrawerDispatch(Action *action)
{
	if (action == nullptr) return;
	const char* route = nullptr;
	const auto* shell = _calypsoHdShell;
	if (shell != nullptr)
	{
		for (const auto& entry : shell->rows)
			if (entry.first == action->getSender())
			{
				for (const auto& def : rowDefs())
					if (def.id == entry.second) { route = def.route; break; }
				break;
			}
	}
	if (route == nullptr) return;
	CalypsoGeoscapeHdShell::closeDrawer(this);
	if (std::string(route) == "funding") return calypsoDrawerFunding(action);
	if (std::string(route) == "tech-tree") return btnTechTreeViewerClick(action);
	if (std::string(route) == "global-research") return btnGlobalResearchClick(action);
	if (std::string(route) == "global-production") return btnGlobalProductionClick(action);
	if (std::string(route) == "global-containment") return btnGlobalAlienContainmentClick(action);
	if (std::string(route) == "ufo-tracker") return btnUfoTrackerClick(action);
	if (std::string(route) == "pilot-experience") return btnDogfightExperienceClick(action);
	if (std::string(route) == "notes") return calypsoDrawerNotes(action);
	if (std::string(route) == "music") return btnSelectMusicTrackClick(action);
	if (std::string(route) == "debug")
	{
		if (Options::debug) btnDebugClick(action);
		else _game->pushState(new TestState);
		return;
	}
	if (std::string(route) == "quick-save") return calypsoDrawerQuickSave(action);
	if (std::string(route) == "instant-save") return calypsoDrawerInstantSave(action);
	if (std::string(route) == "quick-load") return calypsoDrawerQuickLoad(action);
}


void GeoscapeState::calypsoDrawerQuickSave(Action *)
{
	_game->pushState(new SaveGameState(OPT_GEOSCAPE, SAVE_QUICK, _game->getScreen()->getPalette()));
}

void GeoscapeState::calypsoDrawerFunding(Action *)
{
	if (buttonsDisabled()) return;
	_game->pushState(new FundingState);
}

void GeoscapeState::calypsoDrawerInstantSave(Action *)
{
	_game->pushState(new SaveGameState(OPT_GEOSCAPE, SAVE_INSTA, _game->getScreen()->getPalette()));
}

void GeoscapeState::calypsoDrawerQuickLoad(Action *)
{
	_game->pushState(new LoadGameState(OPT_GEOSCAPE, SAVE_QUICK, _game->getScreen()->getPalette()));
}

void GeoscapeState::calypsoDrawerNotes(Action *)
{
	_game->pushState(new NotesState(OPT_GEOSCAPE));
}

} /* namespace OpenXcom */

#endif /* __EMSCRIPTEN__ */
