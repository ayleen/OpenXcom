#!/usr/bin/env python3
"""
Calypso ifdef placement guard (policy R8).

The guard fails a changed file OUTSIDE `src/Calypso/` when this PR adds a NEW
in-place `#ifdef __EMSCRIPTEN__` block whose longest contiguous run of added
emscripten lines exceeds ``defaultMaxLines`` (20), or ``hookMaxLines`` (5) for
a graduated ``frozen`` file.

The check is per-PR (diff-based): pre-existing legacy blocks never trip it,
and pure extractions (which only add small `*Gl();` hooks) never trip it
either, because a one-line hook is a run of ~1.

CLI:
    python3 tools/calypso-ifdef-guard.py [--base <git-ref>]

Default base (when --base omitted):
    output of `git merge-base HEAD origin/oxce-plus`
"""

import argparse
import json
import os
import re
import subprocess
import sys


# --------------------------------------------------------------------------
# emscripten region detection — distinguishes positive (#ifdef /
# #if defined(__EMSCRIPTEN__) / #elif defined(__EMSCRIPTEN__)) regions from
# negated native ones (#ifndef __EMSCRIPTEN__ / !defined(__EMSCRIPTEN__)).
# A native #ifndef block is NOT emscripten footprint.
# --------------------------------------------------------------------------
def _em_negated(s):
    """True if directive negates __EMSCRIPTEN__ (native branch)."""
    t = s.replace(" ", "")
    return (
        t.startswith("#ifndef")
        or t.startswith("#elifndef")
        or "!defined(__EMSCRIPTEN__)" in t
    )


def _em_positive(s):
    """True if directive positively enables an __EMSCRIPTEN__ region."""
    return ("__EMSCRIPTEN__" in s) and not _em_negated(s)


def emscripten_member_lines(text):
    """Set of 0-based line indices that live inside an emscripten-positive
    #ifdef / #if defined(__EMSCRIPTEN__) region (conservative whole-block span;
    an #elif that introduces __EMSCRIPTEN__ marks its enclosing block; native
    #ifndef __EMSCRIPTEN__ / !defined(...) blocks are NOT counted)."""
    lines = text.split("\n")
    member = set()
    stack = []  # [start_index, is_emscripten]
    for i, line in enumerate(lines):
        s = line.strip()
        if re.match(r"#\s*if", s):
            stack.append([i, _em_positive(s)])
        elif re.match(r"#\s*elif", s):
            if stack and _em_positive(s):
                stack[-1][1] = True
        elif re.match(r"#\s*endif", s) and stack:
            start, is_em = stack.pop()
            if is_em:
                for j in range(start + 1, i):
                    member.add(j)
    return member


# --------------------------------------------------------------------------
# git helpers
# --------------------------------------------------------------------------
def git(args, check=True):
    """Run a git subprocess, returning stdout as text."""
    proc = subprocess.run(
        ["git"] + args,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if check and proc.returncode != 0:
        sys.stderr.write(proc.stderr)
        raise SystemExit(proc.returncode)
    return proc


def git_show_path(base, path):
    """Return the textual contents of `<base>:<path>` or "" if missing."""
    proc = git(["show", "{}:{}".format(base, path)], check=False)
    if proc.returncode != 0:
        # Missing path at base (or ambiguous ref) — treat as empty text.
        return ""
    return proc.stdout


def default_base():
    """Default base ref: `git merge-base HEAD origin/oxce-plus`."""
    proc = git(["merge-base", "HEAD", "origin/oxce-plus"], check=False)
    if proc.returncode != 0:
        sys.stderr.write(
            "calypso-ifdef-guard: could not resolve "
            "`git merge-base HEAD origin/oxce-plus` "
            "(pass --base explicitly)\n"
        )
        raise SystemExit(proc.returncode)
    return proc.stdout.strip()


def changed_files(base):
    """`git diff --name-only <base>...HEAD` (three-dot), as a list of str."""
    proc = git(["diff", "--name-only", "{}...HEAD".format(base)])
    out = proc.stdout
    return [line.strip() for line in out.splitlines() if line.strip()]


def added_head_lines(base, path):
    """Set of 0-based HEAD line indices ADDED by this PR (base...HEAD), parsed
    from `git diff --unified=0` hunk headers (`@@ -a,b +c,d @@` -> head lines
    c .. c+d-1)."""
    proc = git(["diff", "--unified=0", "{}...HEAD".format(base), "--", path])
    added = set()
    for line in proc.stdout.splitlines():
        m = re.match(r"^@@ -\d+(?:,\d+)? \+(\d+)(?:,(\d+))? @@", line)
        if m:
            start = int(m.group(1))
            count = int(m.group(2)) if m.group(2) is not None else 1
            for k in range(start, start + count):
                added.add(k - 1)  # to 0-based
    return added


def longest_run(indices):
    """Longest run of consecutive integers in the given set (0 if empty)."""
    if not indices:
        return 0
    ordered = sorted(indices)
    best = run = 1
    for prev, cur in zip(ordered, ordered[1:]):
        run = run + 1 if cur == prev + 1 else 1
        if run > best:
            best = run
    return best


# --------------------------------------------------------------------------
# path filtering
# --------------------------------------------------------------------------
def is_source(path):
    """True if path is under src/ and ends in .cpp or .h."""
    if not path.startswith("src/"):
        return False
    return path.endswith(".cpp") or path.endswith(".h")


def in_calypso(path):
    """True if path is under src/Calypso/ (unlimited by policy)."""
    return path.startswith("src/Calypso/")


def normalize(path):
    """Normalise to forward-slash repo-relative paths."""
    return path.replace("\\", "/")


# --------------------------------------------------------------------------
# main
# --------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(
        description="Calypso #ifdef __EMSCRIPTEN__ placement guard (policy R8)."
    )
    parser.add_argument(
        "--base",
        default=None,
        help="git ref to diff against (default: git merge-base HEAD origin/oxce-plus)",
    )
    args = parser.parse_args()

    base = args.base if args.base else default_base()

    here = os.path.dirname(os.path.abspath(__file__))
    config_path = os.path.join(here, "calypso-ifdef-guard.json")
    with open(config_path, "r", encoding="utf-8") as fh:
        config = json.load(fh)

    default_max = int(config.get("defaultMaxLines", 20))
    hook_max = int(config.get("hookMaxLines", 5))
    frozen = {normalize(p) for p in config.get("frozen", [])}

    failures = []

    for path in changed_files(base):
        path = normalize(path)

        if not is_source(path):
            continue
        if in_calypso(path):
            continue

        head_text = ""
        if os.path.exists(path):
            with open(path, "r", encoding="utf-8", errors="replace") as fh:
                head_text = fh.read()

        member = emscripten_member_lines(head_text)
        added = added_head_lines(base, path)
        added_em = added & member
        run = longest_run(added_em)

        if run <= 0:
            continue

        limit = hook_max if path in frozen else default_max
        if run > limit:
            if path in frozen:
                failures.append(
                    (
                        path,
                        run,
                        "{} has a src/Calypso/ counterpart — move the code there "
                        "and leave only a <=5-line hook (policy R2/R8); largest new "
                        "in-place #ifdef __EMSCRIPTEN__ run is {} lines".format(path, run),
                    )
                )
            else:
                failures.append(
                    (
                        path,
                        run,
                        "{}: a new {}-line in-place #ifdef __EMSCRIPTEN__ block — "
                        "extract to src/Calypso/ and hook it in (policy R3)".format(path, run),
                    )
                )

    if failures:
        for path, delta, message in failures:
            sys.stderr.write(message + "\n")
        sys.stderr.write(
            "\ncalypso-ifdef-guard: {} file(s) violated the placement policy.\n".format(
                len(failures)
            )
        )
        sys.exit(1)

    print("calypso-ifdef-guard: OK — no #ifdef __EMSCRIPTEN__ placement violations.")
    sys.exit(0)


if __name__ == "__main__":
    main()
