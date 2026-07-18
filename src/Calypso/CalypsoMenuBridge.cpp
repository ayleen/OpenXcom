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
 * Calypso/EmscriptenHarness.cpp.
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
#include <limits>
#include <vector>

#include "CalypsoMenuBridge.h"
#include "CalypsoDirector.h"
#include "../Engine/Game.h"
#include "../Engine/Language.h"
#include "../Engine/CrossPlatform.h"
#include "../Engine/Options.h"
#include "../Engine/OptionInfo.h"
#include "../Engine/Screen.h"
#include "../Mod/Mod.h"
#include "../Menu/NewGameState.h"
#include "../Menu/NewBattleState.h"
#include "../Menu/LoadGameState.h"
#include "../Menu/StartState.h"
#include "../Menu/PauseState.h"
#include "../Menu/OptionsBaseState.h"
#include "../Savegame/SavedGame.h"
#include "../Engine/Exception.h"
#include "../Engine/Logger.h"
#include "../Interface/ToggleTextButton.h"
#include "../Interface/ComboBox.h"
#include "../Interface/Slider.h"
#include "CalypsoResolutionFloor.h"
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
		// The global option remains authoritative.  Do not let a stale browser
		// screen advertise an enabled campaign when global help is disabled.
		out += ",\"tutorial\":" + std::string((Options::calypsoTutorial && s->_calypsoTutorial) ? "1" : "0");
		out += ",\"globalTutorial\":" + std::string(Options::calypsoTutorial ? "1" : "0");
		out += "}";
		return out;
	}

	/* Select the difficulty radio + ironman toggle, then fire the native OK
	 * handler (difficulty is pre-validated to 0..4 by the caller). */
	static void start(NewGameState *s, int difficulty, int ironman, int tutorial)
	{
		TextButton *btns[5] = { s->_btnBeginner, s->_btnExperienced, s->_btnVeteran, s->_btnGenius, s->_btnSuperhuman };
		s->_difficulty = btns[difficulty];
		s->_btnIronman->setPressed(ironman != 0);
		s->_calypsoTutorial = tutorial != 0;
		s->btnOkClick(nullptr);
	}
};

/* Slice A5 — New Battle overlay driver (pattern 2, the heaviest — plan §A5).
 * Static helpers only; friended by NewBattleState (Menu/NewBattleState.h) so
 * the exports below can reach into its private members. ComboBox has no
 * option-list getter, so the option strings are rebuilt from NewBattleState's
 * own backing vectors (_missionTypes/_crafts/_terrainTypes/_alienRaces),
 * translated exactly the way the ctor/handlers populate the comboboxes. */
struct CalypsoNewBattleBridge
{
	/* The engine's own NewBattleState if it's the current top state, else null. */
	static NewBattleState *top(Game *g) { return dynamic_cast<NewBattleState *>(g->getTopState()); }

	/* Emits {"options":[...],"selected":N,"visible":bool} for one combobox,
	 * translating each backing-vector entry through lang->getString (with an
	 * optional "MAP_" prefix for terrain, matching cbxMissionChange:780). */
	static std::string comboJson(Language *lang, const std::vector<std::string> &backing,
		const std::string &prefix, size_t selected, bool visible)
	{
		std::string out = "{\"options\":[";
		for (size_t i = 0; i < backing.size(); ++i)
		{
			if (i > 0) out += ",";
			out += "\"" + jsonEscape(lang->getString(prefix + backing[i])) + "\"";
		}
		out += "],\"selected\":" + std::to_string((int)selected);
		out += ",\"visible\":" + std::string(visible ? "true" : "false") + "}";
		return out;
	}

	/* Difficulty is NOT vector-backed -- mirror the 5 literal strings the ctor
	 * fills it with (NewBattleState.cpp:271-277) verbatim. */
	static std::string difficultyJson(Language *lang, size_t selected)
	{
		static const char *ids[5] = {
			"STR_1_BEGINNER", "STR_2_EXPERIENCED", "STR_3_VETERAN", "STR_4_GENIUS", "STR_5_SUPERHUMAN"
		};
		std::string out = "{\"options\":[";
		for (int i = 0; i < 5; ++i)
		{
			if (i > 0) out += ",";
			out += "\"" + jsonEscape(lang->getString(ids[i])) + "\"";
		}
		out += "],\"selected\":" + std::to_string((int)selected) + ",\"visible\":true}";
		return out;
	}

	static std::string sliderJson(Slider *s)
	{
		std::string out = "{\"min\":" + std::to_string(s->getMin());
		out += ",\"max\":" + std::to_string(s->getMax());
		out += ",\"value\":" + std::to_string(s->getValue());
		out += ",\"visible\":" + std::string(s->getVisible() ? "true" : "false") + "}";
		return out;
	}

	/* Build the full New Battle UI model. All private access lives HERE. */
	static std::string json(Game *g, NewBattleState *s)
	{
		Language *lang = g->getLanguage();
		std::string out = "{";
		out += "\"mission\":" + comboJson(lang, s->_missionTypes, "", s->_cbxMission->getSelected(), s->_cbxMission->getVisible());
		out += ",\"craft\":" + comboJson(lang, s->_crafts, "", s->_cbxCraft->getSelected(), s->_cbxCraft->getVisible());
		out += ",\"terrain\":" + comboJson(lang, s->_terrainTypes, "MAP_", s->_cbxTerrain->getSelected(), s->_cbxTerrain->getVisible());
		out += ",\"difficulty\":" + difficultyJson(lang, s->_cbxDifficulty->getSelected());
		out += ",\"race\":" + comboJson(lang, s->_alienRaces, "", s->_cbxAlienRace->getSelected(), s->_cbxAlienRace->getVisible());
		out += ",\"darkness\":" + sliderJson(s->_slrDarkness);
		out += ",\"alienTech\":" + sliderJson(s->_slrAlienTech);
		out += ",\"depth\":" + sliderJson(s->_slrDepth);
		out += ",\"ufoLanded\":{\"pressed\":" + std::string(s->_btnUfoLanded->getPressed() ? "true" : "false");
		out += ",\"visible\":" + std::string(s->_btnUfoLanded->getVisible() ? "true" : "false") + "}";
		out += "}";
		return out;
	}

	/* Field->widget->handler table (plan §A5, verified against
	 * NewBattleState.cpp: only three onChange handlers exist and
	 * btnAlienRaceChange derefs its Action* so it must never be called here). */
	static bool set(NewBattleState *s, const std::string &field, int value)
	{
		if (field == "mission") { s->_cbxMission->setSelected(value); s->cbxMissionChange(nullptr); }
		else if (field == "craft") { s->_cbxCraft->setSelected(value); s->cbxCraftChange(nullptr); }
		else if (field == "terrain") { s->_cbxTerrain->setSelected(value); s->cbxTerrainChange(nullptr); }
		else if (field == "difficulty") { s->_cbxDifficulty->setSelected(value); }
		else if (field == "race") { s->_cbxAlienRace->setSelected(value); }
		else if (field == "darkness") { s->_slrDarkness->setValue(value); }
		else if (field == "alienTech") { s->_slrAlienTech->setValue(value); }
		else if (field == "depth") { s->_slrDepth->setValue(value); }
		else if (field == "ufoLanded") { s->_btnUfoLanded->setPressed(value != 0); }
		else return false;
		return true;
	}

	static void random(NewBattleState *s) { s->btnRandomClick(nullptr); }
	static void equip(NewBattleState *s) { s->btnEquipClick(nullptr); }
	static void start(NewBattleState *s) { s->btnOkClick(nullptr); }
	static void cancel(NewBattleState *s) { s->btnCancelClick(nullptr); }
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
int calypso_newgame_start(int difficulty, int ironman, int tutorial)
{
	Game *g = getCurrentGame();
	if (!g) return 0;
	NewGameState *s = CalypsoNewGameBridge::top(g);
	if (!s) return 0;
	if (difficulty < 0 || difficulty > 4) return 0;
	CalypsoNewGameBridge::start(s, difficulty, ironman, tutorial);
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
	if (CalypsoDirector::get().activeSceneBlocksSaveLoad()) return 0;
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
 *
 * The save is performed inline rather than by pushing a SaveGameState. The
 * native SaveGameState::think (Menu/SaveGameState.cpp:149) pops three states
 * for a non-Ironman SAVE_DEFAULT — itself, the save-list (ListSaveState), and
 * the pause screen. The HTML overlay flow has no native SaveGameState or
 * ListSaveState on the stack (PauseState::btnSaveClick routes to the HTML
 * overlay and returns without pushing ListSaveState), so blindly replaying
 * three popStates would tear down the live Geoscape/Battlescape underneath the
 * pause screen. Instead we mirror the atomic backup->save->moveFile body
 * (SaveGameState.cpp:186-195) inline, then reproduce only the *last* of the
 * native pops: for a non-Ironman game pop the PauseState left on top so the
 * player lands back in the Geoscape/Battlescape (SaveGameState.cpp:166-170);
 * for Ironman leave it (native never exposes Save from an Ironman PauseState —
 * PauseState.cpp:139-144). SavedGame::save() flushes the temporary backup name
 * (Savegame/SavedGame.cpp:951-953); after moving it to the final .sav name we
 * flush once more so IDBFS persists the rename. The JS overlay owns its close.
 * `origin` is unused (the inline save needs no OptionsOrigin) but kept for the
 * JS ABI. */
EMSCRIPTEN_KEEPALIVE
int calypso_save_write(const char *displayName, int origin)
{
	(void)origin;
	Game *g = getCurrentGame();
	if (!g || !g->getSavedGame() || !displayName) return 0;
	if (CalypsoDirector::get().activeSceneBlocksSaveLoad()) return 0;

	g->getSavedGame()->setName(displayName);
	std::string fileName = CrossPlatform::sanitizeFilename(displayName);
	while (CrossPlatform::fileExists(Options::getMasterUserFolder() + fileName + ".sav"))
	{
		fileName += "_";
	}
	fileName += ".sav";

	try
	{
		std::string backup = fileName + ".bak";
		g->getSavedGame()->save(backup, g->getMod());
		std::string fullPath = Options::getMasterUserFolder() + fileName;
		std::string bakPath = Options::getMasterUserFolder() + backup;
		if (!CrossPlatform::moveFile(bakPath, fullPath))
		{
			throw Exception("Save backed up in " + backup);
		}
		// SavedGame::save() flushes while the file still has its temporary
		// .bak name. Persist the post-rename filesystem state as well; otherwise
		// a reload restores only the ignored .bak entry and loses the visible
		// .sav slot that this bridge just reported as successfully written.
		EM_ASM({
			setTimeout(function flushRenamedSave(attempt) {
				if (FS.syncFSRequests > 0) {
					var delay = Math.min(1000, 50 * (attempt + 1));
					setTimeout(function() { flushRenamedSave(attempt + 1); }, delay);
					return;
				}
				FS.syncfs(false, function(err) {
					if (err) console.error('[calypso] syncfs error', err);
				});
			}, 0, 0);
		});
	}
	catch (std::exception &e)
	{
		Log(LOG_ERROR) << "calypso_save_write: " << e.what();
		return 0;
	}

	// Non-Ironman: pop the PauseState the overlay left on top so the player
	// returns to the live Geoscape/Battlescape (mirrors the pause pop in
	// SaveGameState::think). Ironman keeps the pause screen up. The dynamic_cast
	// is a guard: it ensures we only ever pop the pause screen, never a live
	// Geoscape/Battlescape, if this export is somehow reached with something
	// other than PauseState on top (a stray second call, a future JS regression).
	if (!g->getSavedGame()->isIronman() && dynamic_cast<PauseState*>(g->getTopState()))
	{
		g->popState();
	}
	return 1;
}

/* Downloads one validated save as a browser file. The security boundary is
 * SavedGame::getList(g->getLanguage(), true): only a filename that is exactly
 * present in that list is accepted. This rejects null/empty input and any path
 * traversal or out-of-tree name — "../foo", absolute paths, and names the
 * engine didn't enumerate never match a SaveInfo::fileName, so the constructed
 * read path can only be Options::getMasterUserFolder() + a known-good basename.
 * The EM_ASM block then reads that exact MEMFS/IDBFS path via FS.readFile,
 * wraps the bytes in an application/x-yaml Blob, triggers a download with the
 * validated basename as the filename, and revokes the object URL
 * asynchronously. JS-side errors are caught/logged; a C++ try guard covers the
 * rest. Returns 1 on success, 0 on any failure. */
EMSCRIPTEN_KEEPALIVE
int calypso_save_download(const char *file)
{
	Game *g = getCurrentGame();
	if (!g || !file || !*file) return 0;
	std::string fileName(file);

	std::vector<SaveInfo> saves = SavedGame::getList(g->getLanguage(), true);
	auto it = std::find_if(saves.begin(), saves.end(), [&](const SaveInfo &s) { return s.fileName == fileName; });
	if (it == saves.end()) return 0;

	std::string fullPath = Options::getMasterUserFolder() + fileName;
	try
	{
		int ok = EM_ASM_INT({
			try {
				var path = UTF8ToString($0);
				var name = UTF8ToString($1);
				var data = FS.readFile(path);
				var blob = new Blob([data], { type: 'application/x-yaml' });
				var url = URL.createObjectURL(blob);
				var a = document.createElement('a');
				a.href = url;
				a.download = name;
				document.body.appendChild(a);
				a.click();
				setTimeout(function() {
					if (a.parentNode) a.parentNode.removeChild(a);
					URL.revokeObjectURL(url);
				}, 1000);
				return 1;
			} catch (e) {
				console.error('[calypso] save download failed', e);
				return 0;
			}
		}, fullPath.c_str(), fileName.c_str());
		return ok;
	}
	catch (std::exception &e)
	{
		Log(LOG_ERROR) << "calypso_save_download: " << e.what();
		return 0;
	}
}

/* TU-local cache shared by calypso_save_export_prepare / _bytes. File-scope
 * `static` = internal-linkage, one instance for the whole TU. The prepare call
 * is the sole writer (it clears, then assigns); _bytes is read-only. Lifetime
 * contract: the bytes stay valid until the NEXT calypso_save_export_prepare,
 * which overwrites them — JS must copy out of HEAPU8 before any further engine
 * call. Single-threaded JS↔WASM, no reentrancy, so this is the only alias.
 * Distinct from every other export's function-local s_buf on purpose: this one
 * must outlive a single function call so a second export can read it back. */
static std::string s_saveExportBuf;

/* Prepares the raw bytes of one validated save for binary-safe export. Same
 * path-traversal boundary as calypso_save_download — only a filename exactly
 * present in SavedGame::getList(g->getLanguage(), true) is accepted (rejects
 * null/empty input and any "../foo", absolute, or out-of-tree name, since none
 * of those ever match a SaveInfo::fileName). The file is read via
 * CrossPlatform::readFileRaw (full bytes — no NUL truncation or small fixed
 * cap) and cached in the TU-local s_saveExportBuf. Files larger than INT_MAX
 * are rejected because the paired wasm32 length ABI is signed. Returns the byte length on success;
 * 0 on any failure (no live Game / unknown filename / read error / empty file).
 * After a successful call the JS side reads the cached bytes through
 * calypso_save_export_bytes() — see that export for the pointer lifetime. */
EMSCRIPTEN_KEEPALIVE
int calypso_save_export_prepare(const char *file)
{
	s_saveExportBuf.clear();
	Game *g = getCurrentGame();
	if (!g || !file || !*file) return 0;
	std::string fileName(file);

	std::vector<SaveInfo> saves = SavedGame::getList(g->getLanguage(), true);
	auto it = std::find_if(saves.begin(), saves.end(), [&](const SaveInfo &s) { return s.fileName == fileName; });
	if (it == saves.end()) return 0;

	std::string fullPath = Options::getMasterUserFolder() + fileName;
	try
	{
		RawData raw = CrossPlatform::readFileRaw(fullPath);
		if (raw.size() == 0) return 0;
		if (raw.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
		{
			Log(LOG_ERROR) << "calypso_save_export_prepare: save is too large for the wasm32 length ABI";
			return 0;
		}
		s_saveExportBuf.assign(static_cast<const char*>(raw.data()), raw.size());
	}
	catch (std::exception &e)
	{
		Log(LOG_ERROR) << "calypso_save_export_prepare: " << e.what();
		s_saveExportBuf.clear();
		return 0;
	}
	return (int)s_saveExportBuf.size();
}

/* Returns a pointer into wasm linear memory to the bytes cached by the most
 * recent successful calypso_save_export_prepare call. Pair the two calls:
 *   int len  = calypso_save_export_prepare(file);   // 0 = failure
 *   const char *p = calypso_save_export_bytes();    // valid for `len` bytes
 *
 * POINTER LIFETIME: the returned address is into the TU-local s_saveExportBuf,
 * which is overwritten by the NEXT calypso_save_export_prepare (the sole
 * writer). Single-threaded JS↔WASM, no reentrancy. The JS caller MUST copy the
 * bytes out of Module.HEAPU8 synchronously (e.g.
 * new Uint8Array(HEAPU8.subarray(ptr, ptr+len))) as the very first thing it
 * does, before any other engine export is called — growing the wasm heap
 * (emscripten_resize_heap) between prepare and copy would invalidate the
 * address, and the next _prepare call reassigns the buffer. Returns nullptr if
 * no save has been prepared or the last prepare failed. */
EMSCRIPTEN_KEEPALIVE
const char *calypso_save_export_bytes(void)
{
	return s_saveExportBuf.empty() ? nullptr : s_saveExportBuf.data();
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
 * pushed directly, or nothing happens if nothing changed.
 *
 * Return distinguishes the two success paths so the JS overlay hides the
 * HTML main menu only when a restart actually began: 2 = restart started
 * (StartState will reload and re-fire calypsoOnMainMenu, which re-shows the
 * overlay); 1 = no-op (reload == false, MainMenuState stays live, so the
 * overlay must stay visible or it would never come back); 0 = no game. */
EMSCRIPTEN_KEEPALIVE
int calypso_mods_apply()
{
	Game *g = getCurrentGame();
	if (!g) return 0;
	Options::save();
	if (Options::reload)
	{
		g->setState(new StartState);
		return 2;
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
	int scaleType = Calypso::calypsoScaleAtLadderIndex(value);
	if (!Calypso::calypsoEvaluateScale(Options::displayWidth, Options::displayHeight,
	                                  Options::nonSquarePixelRatio, scaleType).eligible)
		return 0;
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

	std::string out = "{\"fractions\":[";
	for (int i = 0; i < 5; ++i)
	{
		const Calypso::CalypsoScaleResult result = Calypso::calypsoEvaluateScale(
			Options::displayWidth, Options::displayHeight,
			Options::nonSquarePixelRatio, Calypso::calypsoScaleAtLadderIndex(i));
		if (i > 0) out += ",";
		out += "{\"id\":" + std::to_string(i)
		     + ",\"label\":\"" + std::to_string(result.width) + "x" + std::to_string(result.height) + "\""
		     + ",\"enabled\":" + std::string(result.eligible ? "true" : "false") + "}";
	}
	out += "]";
	out += ",\"minimumScaleReason\":\"" + jsonEscape(g->getLanguage()->getString("STR_CAL_HD_MINIMUM_SCALE")) + "\"";
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

/* Slice A5 — New Battle overlay exports (pattern 2, friend bridge, plan §A5).
 * Every export resolves the live Game and the live NewBattleState (a null
 * either means the engine isn't booted or the state was already popped/
 * replaced) then delegates to CalypsoNewBattleBridge, which alone has the
 * friend access into NewBattleState's private members. */

EMSCRIPTEN_KEEPALIVE
int calypso_newbattle_ready()
{
	Game *g = getCurrentGame();
	if (!g) return 0;
	return CalypsoNewBattleBridge::top(g) != nullptr ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
const char *calypso_newbattle_json()
{
	static std::string s_buf;
	Game *g = getCurrentGame();
	if (!g) { s_buf = ""; return s_buf.c_str(); }
	NewBattleState *s = CalypsoNewBattleBridge::top(g);
	if (!s) { s_buf = ""; return s_buf.c_str(); }
	s_buf = CalypsoNewBattleBridge::json(g, s);
	return s_buf.c_str();
}

EMSCRIPTEN_KEEPALIVE
int calypso_newbattle_set(const char *field, int value)
{
	Game *g = getCurrentGame();
	if (!g || !field) return 0;
	NewBattleState *s = CalypsoNewBattleBridge::top(g);
	if (!s) return 0;
	return CalypsoNewBattleBridge::set(s, field, value) ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
int calypso_newbattle_random()
{
	Game *g = getCurrentGame();
	if (!g) return 0;
	NewBattleState *s = CalypsoNewBattleBridge::top(g);
	if (!s) return 0;
	CalypsoNewBattleBridge::random(s);
	return 1;
}

EMSCRIPTEN_KEEPALIVE
int calypso_newbattle_equip()
{
	Game *g = getCurrentGame();
	if (!g) return 0;
	NewBattleState *s = CalypsoNewBattleBridge::top(g);
	if (!s) return 0;
	CalypsoNewBattleBridge::equip(s);
	return 1;
}

EMSCRIPTEN_KEEPALIVE
int calypso_newbattle_start()
{
	Game *g = getCurrentGame();
	if (!g) return 0;
	NewBattleState *s = CalypsoNewBattleBridge::top(g);
	if (!s) return 0;
	CalypsoNewBattleBridge::start(s);
	return 1;
}

EMSCRIPTEN_KEEPALIVE
int calypso_newbattle_cancel()
{
	Game *g = getCurrentGame();
	if (!g) return 0;
	NewBattleState *s = CalypsoNewBattleBridge::top(g);
	if (!s) return 0;
	CalypsoNewBattleBridge::cancel(s);
	return 1;
}

} /* extern "C" */

#endif // __EMSCRIPTEN__
