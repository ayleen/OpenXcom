#ifdef __EMSCRIPTEN__
#ifndef OPENXCOM_CALYPSOMENUBRIDGE_H
#define OPENXCOM_CALYPSOMENUBRIDGE_H

/*
 * Phase 41 (Calypso): HTML meta-menu bridge — export declarations.
 * Whole file is Emscripten-only — the native desktop build never sees it.
 *
 * Every JS→engine export for the HTML overlay screens (New Game, New Battle,
 * Load/Save, Mods, Options) is defined EMSCRIPTEN_KEEPALIVE in
 * CalypsoMenuBridge.cpp. The KEEPALIVE attribute — NOT the EXPORTED_FUNCTIONS
 * list — is what survives wasm dead-stripping (plan §5.1; same trick the
 * calypso_menu_* knobs in Calypso/EmscriptenHarness.cpp rely on). JS reaches
 * them through Module.ccall / Module.cwrap.
 *
 * String/JSON returns build into a function-local `static std::string s_buf`
 * in the .cpp (valid until the next call — single-threaded JS↔WASM, no
 * reentrancy) and marshal back through UTF8ToString (added to
 * EXPORTED_RUNTIME_METHODS in slice A0). No JSON library in the engine; the
 * jsonEscape helper lives file-local in the .cpp.
 *
 * Slice A0 ships only the liveness probe `calypso_bridge_ping`; later slices
 * append the screen-specific exports to the same extern "C" block in the .cpp
 * and declare them here.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Bridge liveness probe (slice A0 checkpoint): returns the literal JSON
 * {"ok":1}. Pure constant — no Game dependency, works before callMain. */
const char *calypso_bridge_ping(void);

/* Slice A1 — New Game overlay. Drives the native NewGameState (pushed by
 * calypso_menu_new_game() in Calypso/EmscriptenHarness.cpp) through the
 * friend struct CalypsoNewGameBridge defined in CalypsoMenuBridge.cpp. */
int calypso_newgame_ready(void);
const char *calypso_newgame_info(void);
int calypso_newgame_start(int difficulty, int ironman, int tutorial);
int calypso_newgame_cancel(void);

/* Slice A2 — Load/Save/Delete overlay (pattern 1: pure data-bridge). No
 * native state is pushed to read the list; the action exports mirror the
 * small native handler logic themselves (ListLoadState/ListSaveState/
 * DeleteGameState) and push the *result* states (LoadGameState/
 * SaveGameState) directly. */
const char *calypso_saves_json(void);
/* Returns 1 = pushed LoadGameState, 2 = mods mismatch (retry with force=1),
 * 0 = no live Game. */
int calypso_save_load(const char *file, int origin, int force);
/* Returns 1 on success, 0 on failure (delete failed / no live Game). */
int calypso_save_delete(const char *file);
/* In-game only (needs a live SavedGame). Returns 1 on success, 0 otherwise. */
int calypso_save_write(const char *displayName, int origin);
/* Downloads a save as a browser file. Validates `file` against
 * SavedGame::getList (the path-traversal boundary — only exact matches are
 * accepted), then reads the MEMFS/IDBFS file via FS.readFile and triggers an
 * application/x-yaml download. Returns 1 on success, 0 on failure (no live
 * Game / unknown filename / JS download error). */
int calypso_save_download(const char *file);
/* Automation fallback (regression harness / fixture authoring / Copy YAML):
 * a binary-safe, exact-byte wasm32 export in two paired calls.
 *   int len  = calypso_save_export_prepare(file);  // 0 = failure
 *   const char *p = calypso_save_export_bytes();   // valid for `len` bytes
 * _prepare validates `file` against SavedGame::getList (the same
 * path-traversal boundary as calypso_save_download — only an exact match is
 * accepted), reads the full file via CrossPlatform::readFileRaw (no NUL
 * truncation or small fixed cap; files larger than INT_MAX are rejected by
 * the signed wasm32 length ABI), caches the bytes in a TU-local static buffer,
 * and returns the byte length (0 on any failure / empty file). _bytes then
 * returns a pointer into wasm linear memory for those `len` bytes.
 *
 * POINTER LIFETIME: the returned address is into the TU-local cache buffer,
 * which is overwritten by the NEXT calypso_save_export_prepare (the sole
 * writer). Single-threaded JS↔WASM, no reentrancy. The JS caller MUST copy
 * the bytes out of Module.HEAPU8 synchronously (e.g.
 * new Uint8Array(HEAPU8.subarray(ptr, ptr+len))) as the very first thing it
 * does — before any other engine export (which may grow the wasm heap and
 * invalidate the address, or call _prepare again and reassign the buffer).
 * Returns nullptr if no save has been prepared or the last prepare failed. */
int calypso_save_export_prepare(const char *file);
const char *calypso_save_export_bytes(void);

/* Slice A3 — Mods overlay (pattern 1: pure data-bridge, mirrors
 * ModListState). No native ModListState is pushed; the exports read/write
 * Options::mods directly and mirror ModListState's small handler bodies. */
const char *calypso_mods_json(void);
/* Returns 1 if the mod was found and toggled, 0 otherwise. */
int calypso_mod_set(const char *id, int active);
/* Moves a mod by one position (delta<0 = up/earlier, delta>0 = down/later).
 * Returns 1 if moved, 0 if not found or already at the end. */
int calypso_mod_move(const char *id, int delta);
/* Mirrors ModListState::btnOkClick. Returns 1 (no live Game returns 0). */
int calypso_mods_apply(void);
/* Mirrors the state-restoring half of ModListState::btnCancelClick (no
 * popState — the overlay has no native state pushed). Returns 1. */
int calypso_mods_revert(void);

/* Slice A4 — Options overlay (pattern 1: generic registry bridge). No
 * native Options*State is pushed; the exports read/write the OptionInfo
 * registry (Engine/OptionInfo.h) directly and mirror OptionsBaseState's
 * btnOkClick/btnCancelClick bodies. */
/* Mirrors MainMenuState.cpp:352-356. Returns 1. */
int calypso_options_open(void);
/* Enumerates Options::getOptionInfo(), skipping category()=="HIDDEN" and
 * duplicate ids (first occurrence wins). */
const char *calypso_options_json(void);
/* Generic typed setters. Verify type() matches, refuse fixed-by-mod options
 * and the four new*-backed display/scale ids (see calypso_video_set_scale).
 * Return 1 on success, 0 otherwise. No Options::save() here — apply does it. */
int calypso_option_set_bool(const char *id, int value);
int calypso_option_set_int(const char *id, int value);
int calypso_option_set_string(const char *id, const char *value);
/* Writes Options::newBattlescapeScale/newGeoscapeScale (battlescape!=0
 * selects the former). Mirrors OptionsVideoState::updateBattlescapeScale/
 * updateGeoscapeScale. calypso_options_apply's switchDisplay() commits it. */
int calypso_video_set_scale(int battlescape, int value);
/* Curated video-tab data: proportional display fractions (the same ladder
 * OptionsVideoState builds under __EMSCRIPTEN__), current scales, and the
 * language list/selection. */
const char *calypso_video_json(void);
/* Mirrors the __EMSCRIPTEN__ branch of OptionsBaseState::btnOkClick. Returns
 * 1 (no live Game returns 0). */
int calypso_options_apply(int origin);
/* Mirrors OptionsBaseState::btnCancelClick, minus popState (no native state
 * was pushed for this overlay). Returns 1 (no live Game returns 0). */
int calypso_options_cancel(int origin);

/* Slice A5 — New Battle overlay (pattern 2: friend bridge, mirrors A1). The
 * native NewBattleState is pushed by calypso_menu_new_battle() (Engine/
 * Calypso/EmscriptenHarness.cpp); the exports below drive it through the friend
 * struct CalypsoNewBattleBridge defined in CalypsoMenuBridge.cpp. */
int calypso_newbattle_ready(void);
const char *calypso_newbattle_json(void);
/* field in {"mission","craft","terrain","difficulty","race","darkness",
 * "alienTech","depth","ufoLanded"}. Returns 1 on success, 0 on unknown field
 * or no live NewBattleState. */
int calypso_newbattle_set(const char *field, int value);
int calypso_newbattle_random(void);
/* Pushes the native CraftInfoState; the HTML overlay closes fully afterward
 * (known UX seam — see docs/html-menu-screens.md). */
int calypso_newbattle_equip(void);
int calypso_newbattle_start(void);
int calypso_newbattle_cancel(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* OPENXCOM_CALYPSOMENUBRIDGE_H */
#endif /* __EMSCRIPTEN__ */
