# Calypso Fork Divergence Note

This file documents the known divergences between the `oxce-plus` branch
(Calypso's engine fork) and the upstream `OpenXcom/OpenXcom-Extended` (`master`
or latest tagged release).

## Summary

Calypso's Phase 6–7 work introduced an **ARGB8888 rendering pipeline** that
replaces the 8 bpp palette-indexed internal framebuffer for all compositing
operations. This is the primary architectural divergence. The upstream codebase
continues to use 8 bpp palette indices internally.

## Divergence details

### D1 — `Surface` internal format (ARGB8888 by default)

- **oxce-plus:** `Surface::NewSdlSurface` creates ARGB8888 surfaces by default.
  `Surface::_buffer` is `Uint8*` but points to a 32bpp pixel array; pixel
  access via `getPixel32()` / `setPixel32()`. Shade tables (`ShadeTable.h`)
  replace palette-index lookups for blending.
- **upstream:** Surfaces are 8 bpp indexed; palette defines the colour mapping.

### D2 — `ShadeTable` and `Surface::rebuildShadeTable`

New files / significant additions:
- `engine/oxce/src/Engine/ShadeTable.h` — 32×128 ColorReplace table; built
  from the load-time palette once, used for all runtime shade operations.
- `Surface::setPalette` triggers `rebuildShadeTable`; runtime swaps are
  intentionally no-ops (gate: `Mod::isLoadInProgress()`).

These do not exist in upstream.

### D3 — `Globe::drawShadow` ARGB path

`CreateShadow32` / `CreateShadowWithoutCache32` shaders (Globe.cpp) replace the
8bpp `CreateShadow` dispatch. The cached `_earthData` path is cross-platform.

### D4 — `#ifdef __EMSCRIPTEN__` guard discipline

All rendering-path code (Surface, ShadeTable, Screen, UnitSprite, Globe shadow)
is cross-platform — no `#ifdef __EMSCRIPTEN__` in any of it. Emscripten guards
are reserved for genuine port quirks (canvas, IDBFS, LBM decoder, GPU sphere,
audio stubs).

### D5 — `EmscriptenCompat.cpp`, `EmscriptenHarness.cpp`, `GpuSmokeState.*`

Calypso-only files; linked only when `EMSCRIPTEN` is defined in CMake.

## Commit range

The divergence begins approximately at the Phase 6.0 SDL2 migration.
The relevant HEAD on `oxce-plus` is in the submodule pointer at
`engine/oxce` in the parent repo. Use `git log` in the submodule to find
the exact range since the last common ancestor with upstream.

## Upstream coordination status

As of 2026-04-30: no upstream issue has been filed. The OXCE project does not
actively maintain a public issue tracker for third-party forks. The divergence
is expected to be permanent — upstream is unlikely to adopt an ARGB-by-default
pipeline for desktop play.

**Maintenance plan:** Merge OXCE upstream monthly (or on OXCE release tags).
Resolve conflicts by-hand in the files listed below. Budget approximately
1 working day per upstream merge:

| File | Expected conflict area |
|---|---|
| `engine/oxce/src/Engine/Surface.cpp` | `NewSdlSurface`, `setPalette`, LBM decoder |
| `engine/oxce/src/Engine/Surface.h` | `SurfaceRaw<Uint32>` constructors, `rebuildShadeTable` |
| `engine/oxce/src/Engine/Screen.cpp` | `flip()` ARGB composite path |
| `engine/oxce/src/Geoscape/Globe.cpp` | `drawShadow`, `rebuildEarthData` |
| `engine/oxce/src/Battlescape/UnitSprite.cpp` | `blitBody`, `blitBodyHD` |
| `engine/oxce/src/CMakeLists.txt` | `if(EMSCRIPTEN)` source list, link flags |
