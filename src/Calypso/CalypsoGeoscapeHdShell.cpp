#ifdef __EMSCRIPTEN__
#include "CalypsoGeoscapeHdShell.h"

#include <string>
#include <vector>
#include "../Geoscape/GeoscapeState.h"
#include "CalypsoGeoscapeHdRuntime.h"
#include "CalypsoViewportRuntime.h"
#include "../Engine/Game.h"
#include "../Engine/Screen.h"
#include "../Engine/Logger.h"
#include "../Engine/Language.h"
#include "../Mod/Mod.h"
#include "../Interface/TextButton.h"
#include "../Menu/NotesState.h"
#include "../Menu/SaveGameState.h"
#include "../Menu/LoadGameState.h"

namespace OpenXcom
{
Surface* CalypsoGeoscapeHdShell::resolveWidget(GeoscapeState *s, const std::string& member)
	{
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


namespace
{
	bool s_drawerOpen = false;
	std::vector<std::pair<TextButton*, const char*>> s_drawerRows;
	TextButton* s_sessionChip = nullptr;

	struct RowDef { const char* id; const char* labelKey; ActionHandler handler; };

	const std::vector<RowDef>& rowDefs()
	{
		static const std::vector<RowDef> rows = {
			{ "drawer.tech-tree", "STR_TECH_TREE_VIEWER", (ActionHandler)&GeoscapeState::btnTechTreeViewerClick },
			{ "drawer.ufo-tracker", "STR_UFO_TRACKER", (ActionHandler)&GeoscapeState::btnUfoTrackerClick },
			{ "drawer.music", "STR_SELECT_MUSIC_TRACK", (ActionHandler)&GeoscapeState::btnSelectMusicTrackClick },
			{ "drawer.global-production", "STR_GLOBAL_MANUFACTURE", (ActionHandler)&GeoscapeState::btnGlobalProductionClick },
			{ "drawer.global-research", "STR_GLOBAL_RESEARCH", (ActionHandler)&GeoscapeState::btnGlobalResearchClick },
			{ "drawer.global-containment", "STR_GLOBAL_ALIEN_CONTAINMENT", (ActionHandler)&GeoscapeState::btnGlobalAlienContainmentClick },
		{ "drawer.quick-save", "STR_QUICK_SAVE", (ActionHandler)&GeoscapeState::calypsoDrawerQuickSave },
		{ "drawer.instant-save", "STR_INSTANT_SAVE", (ActionHandler)&GeoscapeState::calypsoDrawerInstantSave },
		{ "drawer.quick-load", "STR_QUICK_LOAD", (ActionHandler)&GeoscapeState::calypsoDrawerQuickLoad },
		{ "drawer.notes", "STR_NOTES", (ActionHandler)&GeoscapeState::calypsoDrawerNotes },
		};
		return rows;
	}
} // anonymous namespace


const char* CalypsoGeoscapeHdShell::apply(GeoscapeState *s)
{
	using namespace OpenXcom::Calypso;
	using namespace OpenXcom::Calypso::CalypsoGeoscapeCommandShellGen;
	const bool listed = s->_game != nullptr && s->_game->getMod() != nullptr && s->_game->getMod()->isHdUiFamilyEnabled("F16");
	const auto decision = calypsoGeoscapeHdGateDecision(true, listed, true, true);
	if (!decision.enabled)
	{
		for (auto& row : s_drawerRows) row.first->setVisible(false);
		if (s_sessionChip) s_sessionChip->setVisible(false);
		s->_sidebar->setVisible(true);
		s->_sideLine->setVisible(true);
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
	if (s_sessionChip == nullptr)
	{
		s_sessionChip = new TextButton(122, 46, 18, 16);
	s->add(s_sessionChip, "button", "geoscape");
	s_sessionChip->onMouseClick((ActionHandler)&GeoscapeState::calypsoToggleDrawer);
	}
	const auto sess = projection.project("action.session");
	s_sessionChip->setX(sess.x); s_sessionChip->setY(sess.y);
	s_sessionChip->setWidth(sess.w); s_sessionChip->setHeight(sess.h);
	s_sessionChip->setVisible(true);
	int deferred = 0;
	for (const auto& def : rowDefs())
	{
		TextButton* row = nullptr;
		for (auto& entry : s_drawerRows)
			if (entry.second != nullptr && def.id == entry.second) { row = entry.first; break; }
		if (row == nullptr)
		{
			row = new TextButton(320, 48, 0, 0);
			row->onMouseClick(def.handler);
			s->add(row, "button", "geoscape");
			s_drawerRows.emplace_back(row, def.id);
		}
		const auto r = projection.project(def.id);
		row->setX(r.x); row->setY(r.y); row->setWidth(r.w); row->setHeight(r.h);
		row->setText(tr(def.labelKey));   // G-1: localized drawer labels
		row->setVisible(s_drawerOpen);
	}
	Log(LOG_INFO) << "[HD] geoscape shell: layout=" << (wide ? "wide" : "compact") << " projected=" << projected << " drawer=" << (s_drawerOpen ? "open" : "closed");
	return decision.reason;
}

void CalypsoGeoscapeHdShell::toggleDrawer(GeoscapeState *s)
{
	s_drawerOpen = !s_drawerOpen;
	apply(s);
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