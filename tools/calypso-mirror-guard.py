#!/usr/bin/env python3
"""
Calypso upstream-drift mirror guard (Phase 41).

Several Calypso HTML-overlay bridge exports in
``src/Calypso/CalypsoMenuBridge.cpp`` deliberately MIRROR the body of a small
upstream handler (e.g. ``OptionsBaseState::btnOkClick``) instead of pushing the
native state — pattern 1 in ``docs/phases/phase-41-hd-menus.md`` §5. That is
safe only as long as the mirrored copy stays faithful to the upstream body. The
monthly upstream OXCE sync can silently change one of those bodies; nothing in
the diff would point at the now-stale bridge copy.

This guard is that tripwire. For every entry in
``tools/calypso-mirror-guard.json`` it extracts the current upstream function
body, normalizes it (strips comments, collapses whitespace), sha256s the
result, and compares against the recorded hash. On any mismatch it fails CI
with a message naming the bridge export to re-verify. It is NOT diff-based —
it validates the recorded baseline on every run, so a drift is caught the first
time CI runs after the sync merges.

Workflow when it fires (``MIRROR DRIFT: ...``):
  1. Read the new upstream body of the named symbol.
  2. Re-verify the mirroring bridge export (``mirroredBy``) still matches it;
     update the export if the upstream logic changed.
  3. Re-seed the hash: ``python3 tools/calypso-mirror-guard.py --update``.

CLI:
    python3 tools/calypso-mirror-guard.py            # verify (CI); exit 1 on drift
    python3 tools/calypso-mirror-guard.py --update   # rewrite all hashes in place

Body extraction is deliberately simple (find the definition line, brace-count
the body). It fails LOUD (a symbol that can't be located is an error), never
silent — a guard that quietly passes when it can't find its target is worse
than useless.
"""

import argparse
import hashlib
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
CONFIG = os.path.join(HERE, "calypso-mirror-guard.json")


def _strip_comments(text):
    """Remove // line comments and /* */ block comments (not string-aware —
    the mirrored bodies contain no ``//`` or ``/*`` inside string literals;
    if that ever changes, the hash simply shifts and is re-seeded)."""
    # Block comments first, then line comments.
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.DOTALL)
    text = re.sub(r"//[^\n]*", " ", text)
    return text


def _normalize(body):
    """Comment-free, whitespace-insensitive canonical form for hashing."""
    body = _strip_comments(body)
    body = re.sub(r"\s+", " ", body)
    return body.strip()


def extract_body(path, symbol):
    """Return the normalized body ``{ ... }`` of the definition of ``symbol``
    in ``path``. ``symbol`` may be qualified (``Class::method``) or a free
    function (``switchDisplay``). Raises ValueError if not found."""
    with open(path, "r", encoding="utf-8") as fh:
        src = fh.read()

    # Match a definition line: the symbol followed by an argument list, whose
    # first non-whitespace character AFTER the balanced ')' is '{' (a call or
    # declaration is followed by ';', so it is skipped). \b before the symbol
    # keeps ``btnOkClick`` from matching ``FoobtnOkClick``; a leading '::' /
    # identifier char is fine because member definitions read ``Class::method``.
    pattern = re.compile(r"\b" + re.escape(symbol) + r"\s*\(")

    for m in pattern.finditer(src):
        # Balance the parentheses of the argument list starting at the '(' .
        i = src.index("(", m.end() - 1)
        depth = 0
        j = i
        while j < len(src):
            if src[j] == "(":
                depth += 1
            elif src[j] == ")":
                depth -= 1
                if depth == 0:
                    break
            j += 1
        if j >= len(src):
            continue
        # Skip whitespace / trailing specifiers (const, override, noexcept)
        # between ')' and the body opener; a ';' means this was a call or a
        # declaration, not a definition.
        k = j + 1
        while k < len(src) and src[k] not in "{;":
            k += 1
        if k >= len(src) or src[k] == ";":
            continue
        # Brace-count the body from this '{'.
        depth = 0
        b = k
        while b < len(src):
            if src[b] == "{":
                depth += 1
            elif src[b] == "}":
                depth -= 1
                if depth == 0:
                    return _normalize(src[k:b + 1])
            b += 1
        raise ValueError(f"unbalanced braces in body of {symbol} in {path}")

    raise ValueError(f"definition of {symbol} not found in {path}")


def load_config():
    with open(CONFIG, "r", encoding="utf-8") as fh:
        return json.load(fh)


def main():
    ap = argparse.ArgumentParser(description="Calypso upstream-drift mirror guard")
    ap.add_argument("--update", action="store_true",
                    help="rewrite all recorded hashes in place (re-seed)")
    args = ap.parse_args()

    cfg = load_config()
    mirrors = cfg.get("mirrors", [])
    if not mirrors:
        print("mirror-guard: no mirrors configured", file=sys.stderr)
        return 1

    drift = []
    for entry in mirrors:
        path = os.path.join(REPO, entry["file"])
        symbol = entry["symbol"]
        try:
            body = extract_body(path, symbol)
        except (OSError, ValueError) as exc:
            print(f"mirror-guard: ERROR locating {symbol}: {exc}", file=sys.stderr)
            return 2
        digest = hashlib.sha256(body.encode("utf-8")).hexdigest()
        if args.update:
            entry["sha256"] = digest
        elif entry.get("sha256") != digest:
            drift.append((entry, digest))

    if args.update:
        with open(CONFIG, "w", encoding="utf-8") as fh:
            json.dump(cfg, fh, indent=2, ensure_ascii=False)
            fh.write("\n")
        print(f"mirror-guard: re-seeded {len(mirrors)} hashes")
        return 0

    if drift:
        for entry, digest in drift:
            print(
                f"MIRROR DRIFT: {entry['symbol']} changed upstream — re-verify "
                f"{entry['mirroredBy']} against the new body, then update sha256 "
                f"in tools/calypso-mirror-guard.json "
                f"(recorded {entry.get('sha256', '<none>')[:12]}…, now {digest[:12]}…).",
                file=sys.stderr,
            )
        return 1

    print(f"mirror-guard: all {len(mirrors)} mirrored bodies match")
    return 0


if __name__ == "__main__":
    sys.exit(main())
