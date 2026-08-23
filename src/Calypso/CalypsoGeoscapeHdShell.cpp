#ifdef __EMSCRIPTEN__
#include "CalypsoGeoscapeHdShell.h"

#include <string>
#include <vector>
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
	struct RowDef { const char* id; const char* labelKey; const char* availability; ActionHandler handler; };

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
			{ "drawer.funding", "STR_FUNDING", "extended-links", (ActionHandler)&GeoscapeState::calypsoDrawerFunding },
			{ "drawer.tech-tree", "STR_TECH_TREE_VIEWER", "extended-links", (ActionHandler)&GeoscapeState::btnTechTreeViewerClick },
			{ "drawer.global-research", "STR_GLOBAL_RESEARCH", "extended-links", (ActionHandler)&GeoscapeState::btnGlobalResearchClick },
			{ "drawer.global-production", "STR_GLOBAL_MANUFACTURE", "extended-links", (ActionHandler)&GeoscapeState::btnGlobalProductionClick },
			{ "drawer.global-containment", "STR_GLOBAL_ALIEN_CONTAINMENT", "extended-links", (ActionHandler)&GeoscapeState::btnGlobalAlienContainmentClick },
			{ "drawer.ufo-tracker", "STR_UFO_TRACKER", "extended-links", (ActionHandler)&GeoscapeState::btnUfoTrackerClick },
			{ "drawer.pilot-experience", "STR_DAILY_PILOT_EXPERIENCE", "extended-links", (ActionHandler)&GeoscapeState::btnDogfightExperienceClick },
			{ "drawer.notes", "STR_NOTES", "extended-links", (ActionHandler)&GeoscapeState::calypsoDrawerNotes },
			{ "drawer.music", "STR_SELECT_MUSIC_TRACK", "extended-links", (ActionHandler)&GeoscapeState::btnSelectMusicTrackClick },
			{ "drawer.debug", "STR_DEBUG", "debug-only", (ActionHandler)&GeoscapeState::btnDebugClick },
			{ "drawer.quick-save", "STR_QUICK_SAVE", "non-ironman", (ActionHandler)&GeoscapeState::calypsoDrawerQuickSave },
			{ "drawer.instant-save", "STR_INSTANT_SAVE", "non-ironman", (ActionHandler)&GeoscapeState::calypsoDrawerInstantSave },
			{ "drawer.quick-load", "STR_QUICK_LOAD", "non-ironman", (ActionHandler)&GeoscapeState::calypsoDrawerQuickLoad },
		};
		return rows;
	}
} // anonymous namespace

struct CalypsoGeoscapeHdShellState
{
	Calypso::CalypsoGeoscapeHdDrawerState drawer;
	std::vector<std::pair<TextButton*, const char*>> rows;
	TextButton* sessionChip = nullptr;
};

const Surface* CalypsoGeoscapeHdShell::resolveLiveWidget(const GeoscapeState* s, const std::string& actionId)
{
	if (s == nullptr || s->_calypsoHdShell == nullptr) return nullptr;
	const auto* shell = s->_calypsoHdShell;
	if (actionId == "action.session") return shell->sessionChip;
	for (const auto& entry : shell->rows)
		if (entry.second != nullptr && actionId == entry.second)
			return entry.first != nullptr && entry.first->getVisible() ? entry.first : nullptr;
	return nullptr;
}

Surface* CalypsoGeoscapeHdShell::resolveLiveWidget(GeoscapeState* s, const std::string& actionId)
{
	return const_cast<Surface*>(resolveLiveWidget(static_cast<const GeoscapeState*>(s), actionId));
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
		s->_sidebar->setVisible(true);
		s->_sideLine->setVisible(true);
		s->_sideTop->setVisible(true);
		s->_sideBottom->setVisible(true);
		return decision.reason;
	}
	const auto& metrics = calypsoViewportRuntime().current();
	const bool wide = metrics.layoutClass == CalypsoLayoutClass::Wide;
	const auto* layout = layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
	if (layout == nullptr) return "layout-missing";
	const auto projection = calypsoGeoscapeHdProjection(*layout, metrics);
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
	s->_sidebar->setVisible(false);
	s->_sideLine->setVisible(false);
	s->_sideTop->setVisible(false);
	s->_sideBottom->setVisible(false);
	if (shell->sessionChip == nullptr)
	{
		shell->sessionChip = new TextButton(122, 46, 18, 16);
		s->add(shell->sessionChip, "button", "geoscape");
		shell->sessionChip->onMouseClick((ActionHandler)&GeoscapeState::calypsoToggleDrawer);
	}
	const auto sess = projection.project("action.session");
	shell->sessionChip->setX(sess.x); shell->sessionChip->setY(sess.y);
	shell->sessionChip->setWidth(sess.w); shell->sessionChip->setHeight(sess.h);
	shell->sessionChip->setVisible(true);
	int deferred = 0;
	for (const auto& def : rowDefs())
	{
		TextButton* row = nullptr;
		for (auto& entry : shell->rows)
			if (entry.second != nullptr && def.id == entry.second) { row = entry.first; break; }
		if (row == nullptr)
		{
			row = new TextButton(320, 48, 0, 0);
			row->onMouseClick(def.handler);
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
	shell->drawer.toggle(s->_pause);
	s->_pause = shell->drawer.open ? true : shell->drawer.pauseBeforeOpen;
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
