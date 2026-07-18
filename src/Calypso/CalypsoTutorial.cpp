#ifdef __EMSCRIPTEN__
/*
 * Phase 37 (Calypso): tutorial manager implementation.
 * Whole file is Emscripten-only — the native desktop build never sees it.
 *
 * pump() drains the queue into a CalypsoTutorialState (37.2) which walks the
 * batch page-by-page; the state's dtor calls notifyPopupClosed() so the next
 * batch can push on the following frame.
 */

#include <emscripten.h>
#include <string>
#include <utility>
#include <vector>
#include <algorithm>

#include "CalypsoTutorial.h"
#include "CalypsoTutorialPolicy.h"
#include "CalypsoAdvisor.h"
#include "../Engine/Surface.h"
#include "CalypsoTutorialState.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Engine/Logger.h"
#include "../Engine/Yaml.h"
#include "../Mod/Mod.h"
#include "../Savegame/SavedGame.h"

namespace OpenXcom
{

CalypsoTutorial& CalypsoTutorial::get()
{
	// Function-local static — first-call construction, no static-init ordering
	// hazard relative to other singletons.
	static CalypsoTutorial instance;
	return instance;
}

bool CalypsoTutorial::isActive(const Game* game) const
{
	if (!game) return false;
	return Calypso::genericTutorialEnabled(Options::calypsoTutorial, _campaignEnabled);
}

bool CalypsoTutorial::guidanceConfigured() const
{
	return Calypso::prologueGuidanceEnabled(Options::calypsoTutorial, _configuredEnabled);
}

void CalypsoTutorial::fire(Game* game, const std::string& event, const std::string& arg)
{
	// FIRST LINE: zero cost when disabled — must run before any allocation/lookup.
	if (!isActive(game)) return;

	// Close postpones the rest of a popup until the player reaches another
	// tutorial event. Moving deferred steps here keeps Close from immediately
	// reopening a new popup on the next frame.
	while (!_deferred.empty())
	{
		const auto* step = _deferred.front();
		_deferred.pop_front();
		bool alreadyQueued = false;
		for (const auto* queued : _queue)
		{
			if (queued->id == step->id) { alreadyQueued = true; break; }
		}
		if (_shown.count(step->id) == 0 && !alreadyQueued)
			_queue.push_back(step);
	}

	const auto& steps = game->getMod()->getCalypsoTutorialSteps();
	for (const auto& step : steps)
	{
		if (step.trigger != event) continue;
		if (!step.triggerArgs.empty())
		{
			bool hit = false;
			for (const auto& a : step.triggerArgs) if (a == arg) { hit = true; break; }
			if (!hit) continue;
		}
		else if (!step.triggerArg.empty() && step.triggerArg != arg) continue;
		if (_shown.count(step.id) != 0) continue;
		// Skip if already queued (match by id).
		bool alreadyQueued = false;
		for (const auto* q : _queue)
		{
			if (q->id == step.id) { alreadyQueued = true; break; }
		}
		if (alreadyQueued) continue;
		_queue.push_back(&step);
	}
}

void CalypsoTutorial::pump(Game* game)
{
	if (_queue.empty()) return;
	if (_holdWhileDogfight) return;
	if (_popupActive) return; // popup-over-popup guard (state resets this in its dtor)

	// Drain the entire queue into one popup; steps are shown back-to-back and
	// the state pops itself when the batch is exhausted. Its dtor calls
	// notifyPopupClosed() so a fresh batch can push on the next frame.
	std::vector<const CalypsoTutorialStep*> batch(_queue.begin(), _queue.end());
	_queue.clear();
	_popupActive = true;
	game->pushState(new CalypsoTutorialState(std::move(batch)));
}

void CalypsoTutorial::anchor(const std::string& key, int x, int y, int w, int h)
{
	_anchors[key] = SDL_Rect{x, y, w, h};
}

void CalypsoTutorial::anchorAll(std::initializer_list<CalypsoAnchorSpec> specs)
{
	for (const auto& s : specs)
	{
		if (!s.a) continue;
		int x = s.a->getX(), y = s.a->getY();
		int w = s.a->getWidth(), h = s.a->getHeight();
		if (s.b)
		{
			int ax2 = s.a->getX() + s.a->getWidth();
			int ay2 = s.a->getY() + s.a->getHeight();
			int bx2 = s.b->getX() + s.b->getWidth();
			int by2 = s.b->getY() + s.b->getHeight();
			x = std::min(s.a->getX(), s.b->getX());
			y = std::min(s.a->getY(), s.b->getY());
			w = std::max(ax2, bx2) - x;
			h = std::max(ay2, by2) - y;
		}
		anchor(s.key, x, y, w, h);
	}
}

bool CalypsoTutorial::anchorRect(const std::string& key, SDL_Rect& out) const
{
	auto it = _anchors.find(key);
	if (it == _anchors.end()) return false;
	out = it->second;
	return true;
}

void CalypsoTutorial::markShown(const std::string& stepId)
{
	_shown.insert(stepId);
}

void CalypsoTutorial::deferSteps(const std::vector<const CalypsoTutorialStep*>& steps)
{
	// Disabling help while the popup is open is an explicit request to discard
	// the rest of that popup, not defer it until a later re-enable.
	if (!_campaignEnabled || !Options::calypsoTutorial) return;

	for (const auto* step : steps)
	{
		if (!step || _shown.count(step->id) != 0) continue;

		bool duplicate = false;
		for (const auto* deferred : _deferred)
		{
			if (deferred->id == step->id) { duplicate = true; break; }
		}
		for (const auto* queued : _queue)
		{
			if (queued->id == step->id) { duplicate = true; break; }
		}
		if (!duplicate) _deferred.push_back(step);
	}
}

void CalypsoTutorial::disableForCampaign()
{
	_campaignEnabled = false;
	_queue.clear();
	_deferred.clear();
}

void CalypsoTutorial::beginCampaign(bool enabled)
{
	resetCampaign();
	_configuredEnabled = enabled;
	_campaignEnabled = enabled;
}

void CalypsoTutorial::suspendForPrologue()
{
	// The scripted battle must not display the generic campaign tutorial, but
	// keep the user's new-game choice for finishPrologue() and autosave reload.
	_campaignEnabled = false;
	_queue.clear();
	_deferred.clear();
}

void CalypsoTutorial::toggleDisabled()
{
	if (_campaignEnabled)
	{
		_campaignEnabled = false;
		_queue.clear();
		_deferred.clear();
	}
	else
	{
		_campaignEnabled = true;
	}
}

void CalypsoTutorial::resetCampaign()
{
	_shown.clear();
	_campaignEnabled = true;
	_configuredEnabled = true;
	_queue.clear();
	_deferred.clear();
	_holdWhileDogfight = false;
	_popupActive = false;
	_checklistOpen = false;
	CalypsoAdvisor::get().resetCounters();
}

void CalypsoTutorial::save(YAML::YamlNodeWriter writer) const
{
	writer.setAsMap();                                  // ensure the node is a map
	writer.write("enabled", _campaignEnabled);
	writer.write("configuredEnabled", _configuredEnabled);
	// _shown is std::set<std::string>; serialize as a sequence of strings.
	// YamlNodeWriter::write(key, vec) works for std::vector<std::string>
	// (matches the SavedGame.cpp "mods" precedent); convert here.
	std::vector<std::string> shownVec(_shown.begin(), _shown.end());
	writer.write("shown", shownVec);
	writer.write("checklistOpen", _checklistOpen);
}

void CalypsoTutorial::load(const YAML::YamlNodeReader& reader, bool legacyPrologueSave)
{
	// Always reset _shown first to avoid cross-campaign bleed when the same
	// singleton is reused across loads.
	_shown.clear();
	_queue.clear();
	_deferred.clear();
	_campaignEnabled = reader["enabled"].readVal<bool>(true);
	const bool hasConfiguredValue = static_cast<bool>(reader["configuredEnabled"]);
	const bool serializedConfigured = reader["configuredEnabled"].readVal<bool>(_campaignEnabled);
	_configuredEnabled = Calypso::configuredTutorialFromSave(
		_campaignEnabled, hasConfiguredValue, serializedConfigured, legacyPrologueSave);
	if (reader["shown"])
	{
		auto shownVec = reader["shown"].readVal<std::vector<std::string>>(std::vector<std::string>{});
		for (const auto& id : shownVec) _shown.insert(id);
	}
	_checklistOpen = reader["checklistOpen"].readVal<bool>(false);
}

void CalypsoTutorial::dump() const
{
	// Private helper: full manager state to the engine log, used by the
	// calypso_tutorial_dump export below. Reads _shown directly (it is private
	// to this class) so we do not need to widen the public API.
	Game* g = getCurrentGame();
	size_t n = g ? g->getMod()->getCalypsoTutorialSteps().size() : 0;
	Log(LOG_INFO) << "[tutorial] " << n << " steps loaded";
	if (g)
	{
		for (const auto& step : g->getMod()->getCalypsoTutorialSteps())
		{
			Log(LOG_INFO) << "[tutorial] step id='" << step.id
			              << "' trigger='" << step.trigger << "'";
		}
	}
	Log(LOG_INFO) << "[tutorial] shown=" << _shown.size()
	              << " campaignEnabled=" << (_campaignEnabled ? "yes" : "no")
	              << " queue=" << _queue.size()
	              << " deferred=" << _deferred.size();
}

} // namespace OpenXcom

/* ---- Debug exports (extern "C", EMSCRIPTEN_KEEPALIVE) ----------------------
 * Same style as src/Calypso/EmscriptenHarness.cpp's calypso_menu_* knobs. These
 * let the JS console / Playwright drive the tutorial for QA without engine
 * rebuilds. OpenXcom::getCurrentGame() (declared in Engine/Game.h) returns the
 * live Game* or null before boot completes — every export null-checks it. */
extern "C" {

EMSCRIPTEN_KEEPALIVE
void calypso_tutorial_fire(const char* event, const char* arg)
{
	if (OpenXcom::Game* g = OpenXcom::getCurrentGame())
		OpenXcom::CalypsoTutorial::get().fire(g, event ? event : "", arg ? arg : "");
}

EMSCRIPTEN_KEEPALIVE
void calypso_tutorial_reset()
{
	OpenXcom::CalypsoTutorial::get().resetCampaign();
}

EMSCRIPTEN_KEEPALIVE
void calypso_tutorial_dump()
{
	OpenXcom::CalypsoTutorial::get().dump();
}

} /* extern "C" */

#endif // __EMSCRIPTEN__
