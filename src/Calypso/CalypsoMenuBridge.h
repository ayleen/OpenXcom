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
 * calypso_menu_* knobs in Engine/EmscriptenHarness.cpp rely on). JS reaches
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
 * calypso_menu_new_game() in Engine/EmscriptenHarness.cpp) through the
 * friend struct CalypsoNewGameBridge defined in CalypsoMenuBridge.cpp. */
int calypso_newgame_ready(void);
const char *calypso_newgame_info(void);
int calypso_newgame_start(int difficulty, int ironman);
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

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* OPENXCOM_CALYPSOMENUBRIDGE_H */
#endif /* __EMSCRIPTEN__ */
