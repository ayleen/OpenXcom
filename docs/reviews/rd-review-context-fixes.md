# R&D cross-review context fixes

This correction pass addresses the context findings from the 7 November 2026
cross-review.

## R7 — Tech Tree context

The F15 HD adapter now publishes a generated summary region with the native
selected-topic/type text, progress text, and applicable research cost
indicator. The summary panel uses bottom-shifted left anchoring in wide mode
and fixed right anchoring in compact mode, while the dependency columns retain
their localized `STR_REQUIRES` and `STR_UNLOCKS` labels. The native
feature-disabled title remains authoritative when the viewer is disabled.
Native palette/size control bytes are removed from summary presentation; their
localized text remains unchanged. The summary heading uses the plain Progress
label, without an empty formatted percentage.

## R8 — Harness save ownership

Fixture preparation now fails closed when a live player `SavedGame` is present
without an explicit harness-owned replacement. This applies both before a
lease is created and after a live save has been recorded as the lease's
original. Fixture preview and profile-switch routes therefore cannot seed
diary, research, manufacture, funds, or base records into the player campaign.

The accepted isolated route remains a harness-created replacement save. No
full campaign or browser run is part of this correction pass.

The ownership rule is shared by production preparation and the focused probe:
an active lease, a non-null fixture save, and identity equality with the
current game save are all required. This keeps the check about actual
production ownership rather than an independently duplicated test predicate.

## R9 — Placement extraction

The remaining Emscripten placement and interaction bodies now live in the
Calypso placement translation units (`CalypsoPlacementHooks.cpp` and
`CalypsoTextList.cpp`). The native state files keep short call sites, and the
new units are registered in the engine CMake source list. This extraction is
structural; native behavior remains in the original `#else` paths.

## Focused checks

The source-level checks for this pass are:

```sh
python3 tools/calypso-ifdef-guard.py --base 0ececf20245d82f13f35a8a10e06806c60d2d602

# From the parent Calypso checkout, compile the persisted ownership/input probe:
c++ -std=c++17 tests/tools/rd_review_input_probe.cpp -o /tmp/rd-review-input-probe
/tmp/rd-review-input-probe
```

The guard is rerun after the placement extraction is complete. Full acceptance campaigns are intentionally deferred; WASM compilation is
performed on the trusted build host. The duplicate browser definition of
`TextList::rebaseNativeSize` was removed from Interface/TextList.cpp; the
Calypso translation unit is its sole owner.
