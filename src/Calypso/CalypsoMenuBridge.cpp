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

#include "CalypsoMenuBridge.h"
#include "../Engine/Game.h"
#include "../Engine/Language.h"
#include "../Mod/Mod.h"
#include "../Menu/NewGameState.h"
#include "../Interface/ToggleTextButton.h"

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

} /* extern "C" */

#endif // __EMSCRIPTEN__
