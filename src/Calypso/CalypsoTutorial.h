#ifdef __EMSCRIPTEN__
#ifndef OPENXCOM_CALYPSOTUTORIAL_H
#define OPENXCOM_CALYPSOTUTORIAL_H

/*
 * Phase 37 (Calypso): in-engine, data-driven, fully optional tutorial manager.
 * Whole file is Emscripten-only — the native desktop build never sees it.
 *
 * Hooks in upstream states call fire(...) on game events; the manager queues
 * matching steps and pump() (called once per frame from Game::run) pushes the
 * popup (CalypsoTutorialState, lands in 37.2). All anchors are value-copied
 * SDL_Rect so destroyed states can never dangle.
 *
 * Content (triggers, anchors, page text via extraStrings) lives in the
 * `calypso-tutorial` mod; the manager only stores the parsed step table on Mod.
 */

#include <string>
#include <vector>
#include <set>
#include <deque>
#include <map>
#include <initializer_list>
#include <SDL.h>

namespace OpenXcom
{

// Forward declarations to keep this header light.
namespace YAML { class YamlNodeReader; class YamlNodeWriter; }
class Game;
class Surface;
struct CalypsoTutorialStep {
	std::string id, trigger, triggerArg;
	std::vector<std::string> triggerArgs; // OR-match; overrides triggerArg if non-empty
	std::vector<std::string> pages;       // extraStrings ids
	std::vector<std::string> pageAnchors; // "" or anchor keys; empty vector ok
	std::string anchor;                   // default anchor for all pages
};

struct CalypsoAnchorSpec { std::string key; Surface* a; Surface* b = nullptr; };

class CalypsoTutorial {
public:
	static CalypsoTutorial& get();                       // lazy singleton
	void fire(Game* game, const std::string& event,
	          const std::string& arg = "");              // queue matching steps
	void pump(Game* game);                               // per-frame; pushes popup if queued
	void anchor(const std::string& key, int x, int y, int w, int h);
	void anchorAll(std::initializer_list<CalypsoAnchorSpec> specs);
	bool anchorRect(const std::string& key, SDL_Rect& out) const;
	void markShown(const std::string& stepId);
	void disableForCampaign();
	bool isActive(const Game* game) const;               // option && campaign flag
	void save(YAML::YamlNodeWriter writer) const;        // persistence (wired in 37.4); by value (matches AlienStrategy::save)
	void load(const YAML::YamlNodeReader& reader);
	void resetCampaign();                                // new game / no node
	// hold flag for dogfight (used in 37.5; declare now, default false)
	void setHoldWhileDogfight(bool v) { _holdWhileDogfight = v; }
	void dump() const; // logs step table + shown-set (called by calypso_tutorial_dump export)
	/// Called by the popup state on destruction so pump() can push the next batch.
	void notifyPopupClosed() { _popupActive = false; }
private:
	CalypsoTutorial() {}
	std::set<std::string> _shown;
	bool _campaignEnabled = true;
	bool _holdWhileDogfight = false;
	bool _popupActive = false; ///< a CalypsoTutorialState is on the stack; suppress re-push
	std::deque<const CalypsoTutorialStep*> _queue;
	std::map<std::string, SDL_Rect> _anchors;
};

} // namespace OpenXcom

#endif // OPENXCOM_CALYPSOTUTORIAL_H
#endif // __EMSCRIPTEN__
