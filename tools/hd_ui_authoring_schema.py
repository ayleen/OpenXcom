"""Shared semantic identifier and runtime-handler grammar for HD UI tools."""

import re


SCREEN_ID_RE = re.compile(r"^[a-z0-9][a-z0-9-]*$")
ACTION_ID_RE = re.compile(r"^[a-z0-9][a-z0-9-]*(?:\.[a-z0-9][a-z0-9-]*)*$")
SLUG_RE = re.compile(r"^[a-z0-9][a-z0-9-]*$")
HANDLER_RE = re.compile(r"^[a-z][a-z0-9-]*(?:\.[A-Za-z][A-Za-z0-9-]*)+$")


def handler_matches_runtime(handler, runtime):
    """Return whether handler is syntactically valid for the selected runtime."""
    if not isinstance(handler, str) or not HANDLER_RE.fullmatch(handler):
        return False
    proof_handler = handler.startswith("proof.")
    if runtime == "proof-only":
        return proof_handler
    if runtime == "production":
        return not proof_handler
    return False
