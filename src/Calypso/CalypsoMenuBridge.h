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

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* OPENXCOM_CALYPSOMENUBRIDGE_H */
#endif /* __EMSCRIPTEN__ */
