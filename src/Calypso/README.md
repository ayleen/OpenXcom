# `src/Calypso/` — Emscripten-only engine code

This directory holds Calypso's browser-port (WebAssembly / Emscripten) engine
code, extracted out of the upstream OXCE source files so those files stay close
to their original shape and merge cleanly against upstream.

Every file here is wrapped in a single whole-file `#ifdef __EMSCRIPTEN__ …
#endif` guard, so it compiles to an empty translation unit in a native build.

## Placement policy (rules R1–R6)

- **R1 — New Calypso-only code goes here.** New functions, classes, GL passes,
  shaders-adjacent CPU code, HD pipelines: create/extend a file under
  `src/Calypso/`, wrapped in a single whole-file `#ifdef __EMSCRIPTEN__ …
  #endif` guard. No other `#ifdef __EMSCRIPTEN__` inside such files — they are
  100 % Calypso by definition.

- **R2 — Small edits to upstream functions (≤ ~20 lines) stay in-place** as
  `#ifdef __EMSCRIPTEN__` blocks. Do not extract them; the in-place block is the
  merge-conflict tripwire we want.
  **Exception — "graduated" files:** once a file has a `src/Calypso/`
  counterpart (listed in `tools/calypso-ifdef-guard.json` → `frozen`), R2 no
  longer applies to it. In a graduated file the ONLY allowed in-place addition
  is an R3 hook (≤ 5 lines); even a 10-line block must go into the file's
  Calypso counterpart instead. Rationale: the counterpart already exists, so the
  convenience argument for in-place code is gone.

- **R3 — Large edits to upstream functions (> ~20 lines) are extracted**: put
  the body in a member/free function defined in a `src/Calypso/` file, and leave
  a ≤ 5-line hook at the original site:
  ```cpp
  #ifdef __EMSCRIPTEN__
  	if (calypsoLoadTileAtlasNode(node, this)) return;   // src/Calypso/ModHd.cpp
  #endif
  ```

- **R4 — Never fork/duplicate an upstream file** (no `Map_ecms.cpp` parallel
  copies, no CMake-level file substitution). Duplicates rot silently on upstream
  merges; hooks conflict loudly, which is the point.

- **R5 — Header declarations**: Calypso members of upstream classes are declared
  in the upstream header inside the existing grouped `#ifdef __EMSCRIPTEN__`
  block of that class (this is already the pattern in `Map.h`, `Mod.h`,
  `BattlescapeState.h`). Do not create parallel headers for member declarations.
  Free functions/structs used only by Calypso code get headers under
  `src/Calypso/`.

- **R6 — Extraction commits are relocation-only.** Verify with
  `git diff --color-moved=dimmed-zebra` — moved lines must show as moved, not as
  add/delete of differing text. Behavior changes go in separate commits.

Full policy (incl. R7 monthly upstream sync and R8 mechanical enforcement via
`tools/calypso-ifdef-guard.py`) lives in the parent repo's
`docs/phases/phase-36-calypso-extraction.md` §2 and `CLAUDE.md` § Critical rules.
