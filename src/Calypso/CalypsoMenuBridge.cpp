#ifdef __EMSCRIPTEN__
/*
 * Phase 41 (Calypso): HTML meta-menu bridge — implementation.
 * Whole file is Emscripten-only — the native desktop build never sees it.
 *
 * Pattern (plan §5.1): the JS overlay screens (web/public/screens/*.js) drive
 * the engine through small EMSCRIPTEN_KEEPALIVE C exports defined here. Helpers
 * live inside namespace OpenXcom; exports live in one extern "C" block. This
 * mirrors the structure of CalypsoTutorial.cpp (whole-file guard, namespace
 * helpers, trailing extern "C" exports) and the calypso_menu_* knobs in
 * Engine/EmscriptenHarness.cpp.
 *
 * String/JSON returns build into a function-local `static std::string s_buf`
 * so the returned const char* stays valid until the next call (single-threaded
 * JS↔WASM, no reentrancy). No JSON library in the engine — build strings by
 * hand and escape text through jsonEscape.
 *
 * Slice A0 ships only the liveness probe; later slices append the New Game /
 * New Battle / Load-Save / Mods / Options exports to the same extern "C" block.
 */

#include <emscripten.h>
#include <string>
#include <cstdio>
#include <algorithm>
#include <vector>

#include "CalypsoMenuBridge.h"
#include "../Engine/Game.h"
#include "../Engine/Language.h"
#include "../Engine/CrossPlatform.h"
#include "../Engine/Options.h"
#include "../Engine/OptionInfo.h"
#include "../Engine/Screen.h"
#include "../Mod/Mod.h"
#include "../Menu/NewGameState.h"
#include "../Menu/LoadGameState.h"
#include "../Menu/SaveGameState.h"
#include "../Menu/StartState.h"
#include "../Menu/OptionsBaseState.h"
#include "../Savegame/SavedGame.h"
#include "../Interface/ToggleTextButton.h"
#include <set>

namespace OpenXcom
{

/* JSON string escaper (plan §5.1) — the single JSON helper reused by every
 * export that returns JSON. File-local (static); declared here, not in the
 * header, because it is an implementation detail of this TU. */
static std::string jsonEscape(const std::string &in)
{
	std::string out; out.reserve(in.size() + 8);
	for (unsigned char c : in)
	{
		switch (c) {
		case '"':  out += "\\\""; break;
		case '\\': out += "\\\\"; break;
		case '\n': out += "\\n"; break;
		case '\r': out += "\\r"; break;
		case '\t': out += "\\t"; break;
		default:
			if (c < 0x20) { char b[8]; snprintf(b, 8, "\\u%04x", c); out += b; }
			else out += (char)c;
		}
	}
	return out;
}

/* Slice A1 — New Game overlay driver. Static helpers only; friended by
 * NewGameState (Menu/NewGameState.h) so the exports below can reach into its
 * private members without widening NewGameState's own public interface. */
struct CalypsoNewGameBridge
{
	/* The engine's own NewGameState if it's the current top state, else null. */
	static NewGameState *top(Game *g) { return dynamic_cast<NewGameState *>(g->getTopState()); }

	/* All private-member access lives HERE — this struct is the friend of
	 * NewGameState, the extern "C" exports are not, so they must delegate. */

	/* Build the New Game info JSON (reads the private ironman toggle state). */
	static std::string infoJson(Game *g, NewGameState *s)
	{
		Language *lang = g->getLanguage();
		static const char *ids[5] = {
			"STR_1_BEGINNER", "STR_2_EXPERIENCED", "STR_3_VETERAN", "STR_4_GENIUS", "STR_5_SUPERHUMAN"
		};
		std::string out = "{\"difficulties\":[";
		for (int i = 0; i < 5; ++i)
		{
			if (i > 0) out += ",";
			out += "\"" + jsonEscape(lang->getString(ids[i])) + "\"";
		}
		out += "],\"selected\":" + std::to_string((int)g->getMod()->getStartingDifficulty());
		out += ",\"ironman\":" + std::string(s->_btnIronman->getPressed() ? "1" : "0");
		out += "}";
		return out;
	}

	/* Select the difficulty radio + ironman toggle, then fire the native OK
	 * handler (difficulty is pre-validated to 0..4 by the caller). */
	static void start(NewGameState *s, int difficulty, int ironman)
	{
		TextButton *btns[5] = { s->_btnBeginner, s->_btnExperienced, s->_btnVeteran, s->_btnGenius, s->_btnSuperhuman };
		s->_difficulty = btns[difficulty];
		s->_btnIronman->setPressed(ironman != 0);
		s->btnOkClick(nullptr);
	}
};

} // namespace OpenXcom

using namespace OpenXcom;   // exports below reach Game/NewGameState/etc. unqualified

extern "C" {

/* Bridge liveness probe (plan §A0 checkpoint). Returns the literal JSON
 * {"ok":1} so the JS console can verify three things at once:
 *   (a) the CalypsoMenuBridge TU compiled and linked into the wasm,
 *   (b) the EMSCRIPTEN_KEEPALIVE attribute defeated wasm dead-stripping, and
 *   (c) UTF8ToString marshals a const char* return through Module.ccall.
 * Pure constant — no Game dependency, so it works before callMain too.
 *
 * jsonEscape (defined above) gets its first real caller in
 * calypso_newgame_info() below (slice A1). */
EMSCRIPTEN_KEEPALIVE
const char *calypso_bridge_ping()
{
	static std::string s_buf;
	s_buf = "{\"ok\":1}";
	return s_buf.c_str();
}

/* Slice A1 — New Game overlay exports. Every export starts by resolving the
 * live Game and, where relevant, the live NewGameState — a null either means
 * the engine isn't booted yet or the state has already been popped/replaced
 * (e.g. the user hit Start/Cancel through the native fallback while the HTML
 * overlay still thought it was open). */

EMSCRIPTEN_KEEPALIVE
int calypso_newgame_ready()
{
	Game *g = getCurrentGame();
	if (!g) return 0;
	return CalypsoNewGameBridge::top(g) != nullptr ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
const char *calypso_newgame_info()
{
	static std::string s_buf;
	Game *g = getCurrentGame();
	if (!g) { s_buf = ""; return s_buf.c_str(); }
	NewGameState *s = CalypsoNewGameBridge::top(g);
	if (!s) { s_buf = ""; return s_buf.c_str(); }
	s_buf = CalypsoNewGameBridge::infoJson(g, s);
	return s_buf.c_str();
}

EMSCRIPTEN_KEEPALIVE
int calypso_newgame_start(int difficulty, int ironman)
{
	Game *g = getCurrentGame();
	if (!g) return 0;
	NewGameState *s = CalypsoNewGameBridge::top(g);
	if (!s) return 0;
	if (difficulty < 0 || difficulty > 4) return 0;
	CalypsoNewGameBridge::start(s, difficulty, ironman);
	return 1;
}

EMSCRIPTEN_KEEPALIVE
int calypso_newgame_cancel()
{
	Game *g = getCurrentGame();
	if (!g) return 0;
	NewGameState *s = CalypsoNewGameBridge::top(g);
	if (!s) return 0;
	s->btnCancelClick(nullptr);
	return 1;
}

/* Slice A2 — Load/Save/Delete overlay exports (pattern 1: pure data-bridge,
 * plan §A2). No native state is pushed to read the list; each export mirrors
 * a small piece of native handler logic and pushes the *result* state
 * (LoadGameState/SaveGameState) directly, exactly like ListLoadState /
 * ListSaveState / DeleteGameState do from their own button handlers. */

/* Save list, sorted by timestamp descending (mirrors what the native list UI
 * shows, newest first) — SavedGame::getList (Savegame/SavedGame.h:201-202;
 * SaveInfo struct at SavedGame.h:90-99). */
EMSCRIPTEN_KEEPALIVE
const char *calypso_saves_json()
{
	static std::string s_buf;
	Game *g = getCurrentGame();
	if (!g) { s_buf = ""; return s_buf.c_str(); }

	std::vector<SaveInfo> saves = SavedGame::getList(g->getLanguage(), true);
	std::sort(saves.begin(), saves.end(), [](const SaveInfo &a, const SaveInfo &b) { return a.timestamp > b.timestamp; });

	std::string out = "[";
	for (size_t i = 0; i < saves.size(); ++i)
	{
		const SaveInfo &s = saves[i];
		if (i > 0) out += ",";
		out += "{\"file\":\"" + jsonEscape(s.fileName) + "\"";
		out += ",\"name\":\"" + jsonEscape(s.displayName) + "\"";
		out += ",\"date\":\"" + jsonEscape(s.isoDate) + "\"";
		out += ",\"time\":\"" + jsonEscape(s.isoTime) + "\"";
		out += ",\"details\":\"" + jsonEscape(s.details) + "\"";
		out += ",\"mods\":[";
		for (size_t m = 0; m < s.mods.size(); ++m)
		{
			if (m > 0) out += ",";
			out += "\"" + jsonEscape(s.mods[m]) + "\"";
		}
		out += "]";
		out += ",\"reserved\":" + std::string(s.reserved ? "true" : "false") + "}";
	}
	out += "]";
	s_buf = out;
	return s_buf.c_str();
}

/* Loads a save. Mirrors ListLoadState::loadSave (Menu/ListLoadState.cpp:87-99):
 * when force==0, confirm before loading a save whose mod list doesn't match
 * the currently active mods (both sides normalised through
 * SavedGame::sanitizeModName, as the native check does). Returns 2 for JS to
 * show its own confirm and retry with force=1; 1 once LoadGameState is
 * pushed; 0 if there's no live Game. */
EMSCRIPTEN_KEEPALIVE
int calypso_save_load(const char *file, int origin, int force)
{
	Game *g = getCurrentGame();
	if (!g || !file || !*file) return 0;
	std::string fileName(file);

	if (!force)
	{
		std::vector<SaveInfo> saves = SavedGame::getList(g->getLanguage(), true);
		auto it = std::find_if(saves.begin(), saves.end(), [&](const SaveInfo &s) { return s.fileName == fileName; });
		if (it != saves.end())
		{
			for (const auto &modName : it->mods)
			{
				std::string name = SavedGame::sanitizeModName(modName);
				if (std::find(Options::mods.begin(), Options::mods.end(), std::make_pair(name, true)) == Options::mods.end())
				{
					return 2;
				}
			}
		}
	}

	g->pushState(new LoadGameState((OptionsOrigin)origin, fileName, g->getScreen()->getPalette()));
	return 1;
}

/* Deletes exactly one save file. Mirrors DeleteGameState::btnYesClick
 * (Menu/DeleteGameState.cpp:97-107): delete via CrossPlatform::deleteFile,
 * then flush IDBFS with the same EM_ASM syncfs snippet SavedGame::save() uses
 * (Savegame/SavedGame.cpp:951-953) so the removal survives a reload. Never
 * touches any other file. */
EMSCRIPTEN_KEEPALIVE
int calypso_save_delete(const char *file)
{
	Game *g = getCurrentGame();
	if (!g || !file || !*file) return 0;

	bool ok = CrossPlatform::deleteFile(Options::getMasterUserFolder() + file);
	if (ok)
	{
		EM_ASM(({ FS.syncfs(false, function(err) { if (err) console.error('[calypso] syncfs error', err); }); }));
	}
	return ok ? 1 : 0;
}

/* Writes a new save under a fresh, deduplicated filename. Mirrors the
 * new-slot branch of ListSaveState::saveGame (Menu/ListSaveState.cpp:168-190):
 * set the SavedGame's display name, sanitize it to a filename
 * (CrossPlatform::sanitizeFilename, as ListSaveState.cpp:172), and append "_"
 * until the ".sav" name is free instead of silently overwriting. In-game
 * only — the JS overlay never reaches this screen without a live SavedGame.
 * SavedGame::save() (called from SaveGameState::think) already flushes IDBFS
 * itself, so no extra JS is needed here. */
EMSCRIPTEN_KEEPALIVE
int calypso_save_write(const char *displayName, int origin)
{
	Game *g = getCurrentGame();
	if (!g || !g->getSavedGame() || !displayName) return 0;

	g->getSavedGame()->setName(displayName);
	std::string fileName = CrossPlatform::sanitizeFilename(displayName);
	while (CrossPlatform::fileExists(Options::getMasterUserFolder() + fileName + ".sav"))
	{
		fileName += "_";
	}
	fileName += ".sav";

	g->pushState(new SaveGameState((OptionsOrigin)origin, fileName, g->getScreen()->getPalette()));
	return 1;
}

/* Slice A3 — Mods overlay exports (pattern 1: pure data-bridge, plan §A3).
 * No native ModListState is pushed; each export mirrors a small piece of
 * ModListState's own handler logic (Menu/ModListState.cpp) directly against
 * Options::mods. */

/* Mods list, in Options::mods priority order (low->high), with the active
 * master's id. Mirrors the master-scan in ModListState::ModListState
 * (Menu/ModListState.cpp:92-118) to find the current master id, then the
 * masters-excluded filter in ModListState::lstModsRefresh (cpp:222-238) to
 * build the list. Unlike lstModsRefresh, this does NOT filter out mods that
 * fail canActivate(curMasterId) -- it reports canActivate per-row instead, so
 * JS can grey out the toggle rather than hide the mod. */
EMSCRIPTEN_KEEPALIVE
const char *calypso_mods_json()
{
	static std::string s_buf;
	Game *g = getCurrentGame();
	if (!g) { s_buf = ""; return s_buf.c_str(); }

	const std::map<std::string, ModInfo> &modInfos = Options::getModInfos();

	std::string curMasterId;
	for (const auto &pair : Options::mods)
	{
		auto search = modInfos.find(pair.first);
		if (search == modInfos.end() || !search->second.isMaster()) continue;
		if (pair.second) { curMasterId = pair.first; break; }
	}

	std::string out = "{\"master\":\"" + jsonEscape(curMasterId) + "\",\"list\":[";
	bool first = true;
	for (const auto &pair : Options::mods)
	{
		auto search = modInfos.find(pair.first);
		if (search == modInfos.end()) continue;
		const ModInfo &modInfo = search->second;
		if (modInfo.isMaster()) continue;

		if (!first) out += ",";
		first = false;
		out += "{\"id\":\"" + jsonEscape(pair.first) + "\"";
		out += ",\"name\":\"" + jsonEscape(modInfo.getName()) + "\"";
		out += ",\"version\":\"" + jsonEscape(modInfo.getVersion()) + "\"";
		out += ",\"active\":" + std::string(pair.second ? "true" : "false");
		out += ",\"isMaster\":false";
		out += ",\"canActivate\":" + std::string(modInfo.canActivate(curMasterId) ? "true" : "false");
		out += "}";
	}
	out += "]}";
	s_buf = out;
	return s_buf.c_str();
}

/* Mirrors ModListState::toggleMod (Menu/ModListState.cpp:280-298), minus the
 * ListView row update. Returns 1 if the mod was found and flipped. */
EMSCRIPTEN_KEEPALIVE
int calypso_mod_set(const char *id, int active)
{
	if (!id || !*id) return 0;
	std::string modId(id);
	for (auto &pair : Options::mods)
	{
		if (pair.first != modId) continue;
		pair.second = (active != 0);
		Options::reload = true;
		return 1;
	}
	return 0;
}

/* Reorders a mod within Options::mods by one position. Mirrors the
 * erase/insert swap ModListState::moveModUp/moveModDown perform via
 * _moveAbove/_moveBelow (Menu/ModListState.cpp:318-339,397-418), simplified
 * to move-by-one against the immediate neighbour instead of the row-based
 * scroll math (there's no ListView here). delta<0 = up/earlier,
 * delta>0 = down/later. Returns 1 if moved, 0 if not found / already at the
 * end in that direction. */
EMSCRIPTEN_KEEPALIVE
int calypso_mod_move(const char *id, int delta)
{
	if (!id || !*id || delta == 0) return 0;
	std::string modId(id);

	auto it = std::find_if(Options::mods.begin(), Options::mods.end(),
		[&](const std::pair<std::string, bool> &p) { return p.first == modId; });
	if (it == Options::mods.end()) return 0;

	// Reorder only within the non-master rows, exactly like the native ListView
	// does: masters live in the same Options::mods vector, so swap against the
	// NEAREST non-master neighbour and step over any master entry — never drag a
	// mod across the master boundary (that would silently drop its override
	// priority below the active master).
	const std::map<std::string, ModInfo> &modInfos = Options::getModInfos();
	auto isMasterEntry = [&](const std::pair<std::string, bool> &p) {
		auto s = modInfos.find(p.first);
		return s != modInfos.end() && s->second.isMaster();
	};

	if (delta < 0)
	{
		for (auto j = it; j != Options::mods.begin(); )
		{
			--j;
			if (!isMasterEntry(*j)) { std::iter_swap(it, j); Options::reload = true; return 1; }
		}
		return 0;
	}
	else
	{
		for (auto j = it + 1; j != Options::mods.end(); ++j)
		{
			if (!isMasterEntry(*j)) { std::iter_swap(it, j); Options::reload = true; return 1; }
		}
		return 0;
	}
}

/* Mirrors ModListState::btnOkClick (Menu/ModListState.cpp:492-503). The
 * overlay has no native state pushed (MainMenuState stays underneath), so
 * unlike the native handler this never calls popState -- StartState is
 * pushed directly, or nothing happens if nothing changed. */
EMSCRIPTEN_KEEPALIVE
int calypso_mods_apply()
{
	Game *g = getCurrentGame();
	if (!g) return 0;
	Options::save();
	if (Options::reload)
	{
		g->setState(new StartState);
	}
	return 1;
}

/* Mirrors the state-restoring half of ModListState::btnCancelClick
 * (Menu/ModListState.cpp:519-524) -- no popState, since no native state was
 * pushed for this overlay. */
EMSCRIPTEN_KEEPALIVE
int calypso_mods_revert()
{
	Options::reload = false;
	Options::load();
	return 1;
}

/* Slice A4 — Options overlay exports (pattern 1: generic registry bridge,
 * plan §A4). No native Options*State is pushed; the exports read/write the
 * OptionInfo registry (Engine/OptionInfo.h) directly and mirror
 * OptionsBaseState's btnOkClick/btnCancelClick bodies
 * (Menu/OptionsBaseState.cpp:220-290). */

/* The 5-entry proportional display-fraction ladder Calypso's video tab
 * offers, combobox display order Full/3/4/1/2/1/3/1/4 — mirrors the
 * __EMSCRIPTEN__ branch of OptionsVideoState's ctor (Menu/OptionsVideoState.cpp:
 * 322-327) so the bridge and the native menu never drift out of sync. */
static const int CALYPSO_VIDEO_SCALE_LADDER[5] = {
	SCALE_SCREEN, SCALE_SCREEN_3_4, SCALE_SCREEN_DIV_2, SCALE_SCREEN_DIV_3, SCALE_SCREEN_DIV_4
};

/* Forward map: internal ScaleType (Engine/Options.h's 17-entry enum) to a
 * ladder index above, for legacy/off-ladder values already sitting in an old
 * options.cfg. Mirrors OptionsVideoState's _scales table verbatim
 * (Menu/OptionsVideoState.cpp:333-349). */
static const int CALYPSO_VIDEO_SCALE_FORWARD[17] = {
	2, 2, 2, 3, 2, 0, 4, 4, 4, 4, 4, 2, 2, 2, 2, 2, 1
};

static int calypsoVideoScaleToLadder(int scaleType)
{
	if (scaleType < 0 || scaleType > 16) return 2; // SCALE_SCREEN_DIV_2 default
	return CALYPSO_VIDEO_SCALE_FORWARD[scaleType];
}

/* Linear lookup by id in the OptionInfo registry. Returns a pointer into the
 * live (persistent, never reallocated after Options::create()) registry
 * vector, or null. */
static const OptionInfo *calypsoFindOption(const std::string &id)
{
	for (const OptionInfo &info : Options::getOptionInfo())
	{
		if (info.id() == id) return &info;
	}
	return nullptr;
}

/* Scale trap guard (plan §A4 grounding): battlescapeScale/geoscapeScale/
 * displayWidth/displayHeight are new*-backed display fields. Writing them
 * through the generic setter and then letting calypso_options_apply call
 * switchDisplay() would revert the change — they MUST go through
 * calypso_video_set_scale instead. */
static bool calypsoOptionIsScaleGuarded(const std::string &id)
{
	return id == "battlescapeScale" || id == "geoscapeScale" || id == "displayWidth" || id == "displayHeight";
}

/* Mirrors MainMenuState.cpp:352-356. */
EMSCRIPTEN_KEEPALIVE
int calypso_options_open()
{
	Options::backupDisplay();
	return 1;
}

/* Enumerates Options::getOptionInfo(), skipping category()=="HIDDEN" entries
 * and duplicate ids (some base options are pushed under more than one ifdef
 * branch of Options::create() — first occurrence wins, per plan pitfall #12). */
EMSCRIPTEN_KEEPALIVE
const char *calypso_options_json()
{
	static std::string s_buf;
	Game *g = getCurrentGame();
	if (!g) { s_buf = ""; return s_buf.c_str(); }

	Language *lang = g->getLanguage();
	const std::map<std::string, std::string> &fixed = g->getMod()->getFixedUserOptions();

	std::string out = "[";
	bool first = true;
	std::set<std::string> seen;
	for (const OptionInfo &info : Options::getOptionInfo())
	{
		if (info.category() == "HIDDEN") continue;
		if (!seen.insert(info.id()).second) continue;

		std::string typeStr, valueJson;
		switch (info.type())
		{
		case OPTION_BOOL:
			typeStr = "bool";
			valueJson = *info.asBool() ? "true" : "false";
			break;
		case OPTION_INT:
			typeStr = "int";
			valueJson = std::to_string(*info.asInt());
			break;
		case OPTION_STRING:
			typeStr = "string";
			valueJson = "\"" + jsonEscape(*info.asString()) + "\"";
			break;
		default:
			// OPTION_KEY: the key ctor is disabled under Emscripten
			// (OptionInfo.h:50-55) -- keybindings register as OPTION_INT
			// instead, so this branch should never see live data. Skip
			// defensively rather than call the wrong as*() accessor
			// (pitfall #3: throws on type mismatch).
			continue;
		}

		if (!first) out += ",";
		first = false;
		out += "{\"id\":\"" + jsonEscape(info.id()) + "\"";
		out += ",\"type\":\"" + typeStr + "\"";
		out += ",\"value\":" + valueJson;
		out += ",\"cat\":\"" + (info.category().empty() ? std::string() : jsonEscape(lang->getString(info.category()))) + "\"";
		out += ",\"desc\":\"" + (info.description().empty() ? std::string() : jsonEscape(lang->getString(info.description()))) + "\"";
		out += ",\"owner\":" + std::to_string((int)info.owner());
		out += ",\"fixed\":" + std::string(fixed.count(info.id()) ? "true" : "false");
		out += ",\"isKey\":" + std::string(info.id().rfind("key", 0) == 0 ? "true" : "false");
		out += "}";
	}
	out += "]";
	s_buf = out;
	return s_buf.c_str();
}

/* Generic typed setters. Verify type() first (pitfall #3), refuse
 * mod-fixed options and the scale-trap ids (above), then write through the
 * matching as*() accessor. No Options::save() here -- calypso_options_apply
 * does that, same as the native OK button. */
EMSCRIPTEN_KEEPALIVE
int calypso_option_set_bool(const char *id, int value)
{
	Game *g = getCurrentGame();
	if (!g || !id || !*id) return 0;
	std::string optId(id);
	if (calypsoOptionIsScaleGuarded(optId)) return 0;
	if (g->getMod()->getFixedUserOptions().count(optId)) return 0;
	const OptionInfo *info = calypsoFindOption(optId);
	if (!info || info->type() != OPTION_BOOL) return 0;
	*info->asBool() = (value != 0);
	return 1;
}

EMSCRIPTEN_KEEPALIVE
int calypso_option_set_int(const char *id, int value)
{
	Game *g = getCurrentGame();
	if (!g || !id || !*id) return 0;
	std::string optId(id);
	if (calypsoOptionIsScaleGuarded(optId)) return 0;
	if (g->getMod()->getFixedUserOptions().count(optId)) return 0;
	const OptionInfo *info = calypsoFindOption(optId);
	if (!info || info->type() != OPTION_INT) return 0;
	*info->asInt() = value;
	return 1;
}

EMSCRIPTEN_KEEPALIVE
int calypso_option_set_string(const char *id, const char *value)
{
	Game *g = getCurrentGame();
	if (!g || !id || !*id || !value) return 0;
	std::string optId(id);
	if (calypsoOptionIsScaleGuarded(optId)) return 0;
	if (g->getMod()->getFixedUserOptions().count(optId)) return 0;
	const OptionInfo *info = calypsoFindOption(optId);
	if (!info || info->type() != OPTION_STRING) return 0;
	*info->asString() = value;
	return 1;
}

/* Writes the new*-backed scale twin. Mirrors OptionsVideoState::
 * updateBattlescapeScale/updateGeoscapeScale (Menu/OptionsVideoState.cpp:
 * 701-717) -- those write `_reverseScales[selected]`, i.e. the ScaleType for
 * a ladder index, never the raw index. calypso_options_apply's
 * switchDisplay() call is what makes the value live (scale trap, plan §A4
 * grounding). `value` is a ladder index (0..4), not a ScaleType. */
EMSCRIPTEN_KEEPALIVE
int calypso_video_set_scale(int battlescape, int value)
{
	Game *g = getCurrentGame();
	if (!g) return 0;
	if (value < 0 || value > 4) return 0;
	int scaleType = CALYPSO_VIDEO_SCALE_LADDER[value];
	if (battlescape) Options::newBattlescapeScale = scaleType;
	else Options::newGeoscapeScale = scaleType;
	return 1;
}

/* Curated video-tab data. The fraction labels and pixelRatioY math mirror
 * OptionsVideoState's __EMSCRIPTEN__ ctor branch verbatim
 * (Menu/OptionsVideoState.cpp:302-327); geoscapeScale/battlescapeScale are
 * reported as ladder indices (0..4), matching what calypso_video_set_scale
 * expects back. */
EMSCRIPTEN_KEEPALIVE
const char *calypso_video_json()
{
	static std::string s_buf;
	Game *g = getCurrentGame();
	if (!g) { s_buf = ""; return s_buf.c_str(); }

	double pixelRatioY = 1.0;
	if (Options::nonSquarePixelRatio && !Options::allowResize) pixelRatioY = 1.2;

	std::string out = "{\"fractions\":[";
	for (int i = 0; i < 5; ++i)
	{
		int num = 1, den = 1;
		Screen::getScreenScaleFraction(CALYPSO_VIDEO_SCALE_LADDER[i], num, den);
		int w = Options::displayWidth * num / den;
		int h = (int)(Options::displayHeight / pixelRatioY * num / den);
		if (i > 0) out += ",";
		out += "\"" + std::to_string(w) + "x" + std::to_string(h) + "\"";
	}
	out += "]";
	out += ",\"geoscapeScale\":" + std::to_string(calypsoVideoScaleToLadder(Options::geoscapeScale));
	out += ",\"battlescapeScale\":" + std::to_string(calypsoVideoScaleToLadder(Options::battlescapeScale));

	std::vector<std::string> langCodes, langNames;
	Language::getList(langCodes, langNames);
	out += ",\"languages\":[";
	for (size_t i = 0; i < langCodes.size(); ++i)
	{
		if (i > 0) out += ",";
		out += "{\"id\":\"" + jsonEscape(langCodes[i]) + "\",\"name\":\"" + jsonEscape(langNames[i]) + "\"}";
	}
	out += "]";
	out += ",\"language\":\"" + jsonEscape(Options::language) + "\"";
	out += "}";
	s_buf = out;
	return s_buf.c_str();
}

/* Mirrors the __EMSCRIPTEN__ branch of OptionsBaseState::btnOkClick
 * (Menu/OptionsBaseState.cpp:220-275), transcribed step by step. Two verified
 * deviations from the native body: recenter(dX, dY) is skipped -- it
 * recenters the options state itself, which does not exist in the HTML flow
 * (restart(origin) rebuilds the origin state at the new resolution anyway);
 * and the OptionsConfirmState branch can never fire here -- display width/
 * height are pinned to the canvas and scale fields are not part of its
 * comparison -- so this always goes straight to the reload check / restart. */
EMSCRIPTEN_KEEPALIVE
int calypso_options_apply(int origin)
{
	Game *g = getCurrentGame();
	if (!g) return 0;
	OptionsOrigin o = (OptionsOrigin)origin;

	Options::newDisplayWidth = Options::displayWidth;
	Options::newDisplayHeight = Options::displayHeight;
	Options::switchDisplay();
	Screen::updateScale(Options::battlescapeScale, Options::baseXBattlescape, Options::baseYBattlescape, o == OPT_BATTLESCAPE);
	Screen::updateScale(Options::geoscapeScale, Options::baseXGeoscape, Options::baseYGeoscape, o != OPT_BATTLESCAPE);
	Options::save();
	g->loadLanguages();
	// NEVER the no-arg resetDisplay() -- it tears down the one WebGL context (pitfall #4).
	g->getScreen()->resetDisplay(false);
	SDL_WM_GrabInput(Options::captureMouse);
	g->setVolume(Options::soundVolume, Options::musicVolume, Options::uiVolume);

	if (Options::reload && o == OPT_MENU)
	{
		g->setState(new StartState);
		return 1;
	}
	OptionsBaseState::restart(o);
	return 1;
}

/* Mirrors OptionsBaseState::btnCancelClick (Menu/OptionsBaseState.cpp:
 * 281-290), minus popState -- no native state was pushed for this overlay.
 * Takes origin (unlike the native no-arg handler) because updateScale's
 * change flag needs it -- an intentional, noted deviation from the plan's
 * no-arg signature. */
EMSCRIPTEN_KEEPALIVE
int calypso_options_cancel(int origin)
{
	Game *g = getCurrentGame();
	if (!g) return 0;
	OptionsOrigin o = (OptionsOrigin)origin;

	Options::reload = false;
	Options::load();
	SDL_WM_GrabInput(Options::captureMouse);
	Screen::updateScale(Options::battlescapeScale, Options::baseXBattlescape, Options::baseYBattlescape, o == OPT_BATTLESCAPE);
	Screen::updateScale(Options::geoscapeScale, Options::baseXGeoscape, Options::baseYGeoscape, o != OPT_BATTLESCAPE);
	g->setVolume(Options::soundVolume, Options::musicVolume, Options::uiVolume);
	return 1;
}

} /* extern "C" */

#endif // __EMSCRIPTEN__
