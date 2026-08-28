#ifdef __EMSCRIPTEN__
#include "CalypsoGeoscapeHdShell.h"

#include <string>
#include <vector>
#include "../Engine/Action.h"
#include "../Geoscape/GeoscapeState.h"
#include "CalypsoGeoscapeHdRuntime.h"
#include "CalypsoGeoscapeActionContract.h"

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
	/// Row availability from the shared route registry (audit §13 item 6):
	/// the same gate vocabulary as the semantic recipe; no local row table.
	bool rowAvailable(const char* availability, bool ironman)
	{
		if (availability == nullptr) return true;
		if (std::string(availability) == "extended-links") return Options::oxceLinks;
		if (std::string(availability) == "debug-only") return Options::debug;
		if (std::string(availability) == "non-ironman")
			return !ironman;
		return true;
	}
} // anonymous namespace

struct CalypsoGeoscapeHdShellState
{
	Calypso::CalypsoGeoscapeHdDrawerState drawer;
	// Reason-aware pause ledger (audit §13 item 1): user/drawer/session
	// pauses are counted tokens; vanilla popup/dogfight/system reasons stay
	// owned by GeoscapeState's own latch.
	Calypso::GeoscapeTimePolicyState policy;
	std::vector<std::pair<TextButton*, const char*>> rows;
	TextButton* sessionChip = nullptr;
	TextButton* pauseControl = nullptr;
	TextButton* speedBeforeOpen = nullptr;
	bool sessionWasFocused = false;
	// Most recent authoritative system reason observed by the timeAdvance
	// recompute hook; reused by HD pause operations between simulation ticks.
	bool vanillaSystemReason = false;
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
	for (const auto& def : Calypso::calypsoGeoscapeHdLiveDrawerRows())
	{
		TextButton* row = nullptr;
		for (auto& entry : shell->rows)
			if (entry.second != nullptr && std::string(def.actionId) == entry.second) { row = entry.first; break; }
		if (row == nullptr)
		{
			row = new TextButton(320, 48, 0, 0);
			row->onMouseClick((ActionHandler)&GeoscapeState::calypsoDrawerDispatch);
			s->add(row, "button", "geoscape");
			shell->rows.emplace_back(row, def.actionId);
		}
		const auto r = projection.project(def.actionId);
		row->setX(r.x); row->setY(r.y); row->setWidth(r.w); row->setHeight(r.h);
		row->setText(s->tr(def.labelKey));   // G-1: localized drawer labels
		const bool ironman = s->_game->getSavedGame() != nullptr && s->_game->getSavedGame()->isIronman();
		row->setVisible(shell->drawer.open && rowAvailable(def.availability, ironman));
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
		shell->drawer.open = true;
		shell->policy.acquire(Calypso::GeoscapePauseReason::MoreDrawer);
	}
	else
	{
		shell->drawer.open = false;
		shell->policy.release(Calypso::GeoscapePauseReason::MoreDrawer);
	}
	syncPause(s, shell->vanillaSystemReason);
	if (!shell->drawer.open && shell->speedBeforeOpen != nullptr)
		s->_timeSpeed = shell->speedBeforeOpen;
	if (!shell->drawer.open && shell->sessionChip != nullptr)
		shell->sessionChip->setFocus(shell->sessionWasFocused);
	apply(s);
	if (shell->drawer.open)
	{
		for (const auto& entry : shell->rows)
		{
			if (entry.first != nullptr && entry.first->getVisible())
			{
				entry.first->setFocus(true);
				break;
			}
		}
	}
}

bool CalypsoGeoscapeHdShell::closeDrawer(GeoscapeState *s)
{
	if (s == nullptr || s->_calypsoHdShell == nullptr || !s->_calypsoHdShell->drawer.open)
		return false;
	auto* shell = s->_calypsoHdShell;
	shell->drawer.open = false;
	// Closing releases only the drawer's own reason token; a user, session,
	// popup, dogfight, or system pause can never be resumed over it.
	shell->policy.release(Calypso::GeoscapePauseReason::MoreDrawer);
	syncPause(s, shell->vanillaSystemReason);
	if (shell->speedBeforeOpen != nullptr) s->_timeSpeed = shell->speedBeforeOpen;
	if (shell->sessionChip != nullptr) shell->sessionChip->setFocus(shell->sessionWasFocused);
	apply(s);
	return true;
}

/* Stage 8–9 closure: reason-aware explicit pause. The vanilla `_pause` latch
 * stays the single gameplay gate; this only flips the User ledger token and
 * re-derives the latch through the same recompute rule as timeAdvance. No
 * destructive toggle of the latch remains anywhere in the shell. */
void CalypsoGeoscapeHdShell::togglePause(GeoscapeState *s)
{
	auto* shell = state(s);
	shell->policy.toggleUser();
	syncPause(s, shell->vanillaSystemReason);
}

void CalypsoGeoscapeHdShell::syncPause(GeoscapeState *s, bool systemReason)
{
	auto* shell = state(s);
	shell->vanillaSystemReason = systemReason;
	s->_pause = Calypso::calypsoGeoscapeEffectivePause(systemReason, shell->policy);
}

bool CalypsoGeoscapeHdShell::effectivePause(const GeoscapeState *s)
{
	if (s == nullptr) return false;
	const auto* shell = s->_calypsoHdShell;
	if (shell == nullptr) return s->_pause;
	// The derived `_pause` latch must never be re-fed as a system input: it
	// already includes ledger tokens and vanilla writes, so using it here
	// would make every pause sticky. The last authoritative system reason
	// observed by timeAdvance is the only system side of the OR.
	return Calypso::calypsoGeoscapeEffectivePause(shell->vanillaSystemReason, shell->policy);
}

bool CalypsoGeoscapeHdShell::isDrawerOpen(const GeoscapeState *s)
{
	return s != nullptr && s->_calypsoHdShell != nullptr && s->_calypsoHdShell->drawer.open;
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

/* Stage 8–9 closure: `time.pause` is a counted User ledger token synced into
 * the authoritative latch — no destructive latch flip remains anywhere. */
void GeoscapeState::calypsoTogglePause(Action *)
{
	CalypsoGeoscapeHdShell::togglePause(this);
}

void GeoscapeState::calypsoDrawerDispatch(Action *action)
{
	if (action == nullptr) return;
	const char* actionId = nullptr;
	const auto* shell = _calypsoHdShell;
	if (shell != nullptr)
	{
		for (const auto& entry : shell->rows)
			if (entry.first == action->getSender())
			{
				actionId = entry.second;
				break;
			}
	}
	if (actionId == nullptr) return;
	CalypsoGeoscapeHdShell::closeDrawer(this);
	const std::string id(actionId);
	if (id == "drawer.funding") return calypsoDrawerFunding(action);
	if (id == "drawer.tech-tree") return btnTechTreeViewerClick(action);
	if (id == "drawer.global-research") return btnGlobalResearchClick(action);
	if (id == "drawer.global-production") return btnGlobalProductionClick(action);
	if (id == "drawer.global-containment") return btnGlobalAlienContainmentClick(action);
	if (id == "drawer.ufo-tracker") return btnUfoTrackerClick(action);
	if (id == "drawer.pilot-experience") return btnDogfightExperienceClick(action);
	if (id == "drawer.notes") return calypsoDrawerNotes(action);
	if (id == "drawer.music") return btnSelectMusicTrackClick(action);
	if (id == "drawer.debug")
	{
		if (Options::debug) btnDebugClick(action);
		else _game->pushState(new TestState);
		return;
	}
	if (id == "drawer.quick-save") return calypsoDrawerQuickSave(action);
	if (id == "drawer.instant-save") return calypsoDrawerInstantSave(action);
	if (id == "drawer.quick-load") return calypsoDrawerQuickLoad(action);
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
