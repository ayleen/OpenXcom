#!/usr/bin/env python3
"""Create semantic-only HD UI authoring bundles from approved archetypes."""

import argparse
import json
import os
import re
import sys


TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
CALYPSO_DIR = os.path.normpath(os.path.join(TOOLS_DIR, "..", "src", "Calypso"))
SCREEN_TEMPLATES_DIR = os.path.join(CALYPSO_DIR, "ScreenTemplates")
COMPONENT_TEMPLATES_DIR = os.path.join(CALYPSO_DIR, "ComponentTemplates")
ID_RE = re.compile(r"^[a-z0-9][a-z0-9.-]*$")
SLUG_RE = re.compile(r"^[a-z0-9][a-z0-9-]*$")
LABEL_RE = re.compile(r"^STR_[A-Z0-9_]+$")
HANDLER_RE = re.compile(r"^[A-Za-z][A-Za-z0-9]*(?:\.[A-Za-z][A-Za-z0-9]*)+$")


def fail(message):
    print("scaffold-hd-ui: error: " + message, file=sys.stderr)
    raise SystemExit(1)


def load_json(path, label):
    try:
        with open(path, "r", encoding="utf-8") as stream:
            return json.load(stream)
    except (OSError, json.JSONDecodeError) as error:
        fail(label + " cannot be read: " + str(error))


def write_json(path, value):
    with open(path, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(value, stream, indent=2, ensure_ascii=False)
        stream.write("\n")


def validate_identifier(value, pattern, label):
    if not isinstance(value, str) or not pattern.match(value):
        fail(label + " has an invalid identifier: " + repr(value))


def scaffold_action(args):
    config_path = os.path.abspath(args.config)
    recipe = load_json(config_path, "semantic screen recipe")
    archetype = recipe.get("archetype")
    validate_identifier(archetype, SLUG_RE, "recipe.archetype")
    if recipe.get("runtime") not in ("production", "proof-only"):
        fail("recipe.runtime must be production or proof-only")
    actions = recipe.get("actions")
    if not isinstance(actions, list):
        fail("recipe.actions must be an array")

    validate_identifier(args.action_id, ID_RE, "--id")
    validate_identifier(args.label_key, LABEL_RE, "--label-key")
    validate_identifier(args.component, SLUG_RE, "--component")
    validate_identifier(args.slot_role, SLUG_RE, "--slot-role")
    validate_identifier(args.handler, HANDLER_RE, "--handler")
    if any(action.get("id") == args.action_id for action in actions):
        fail("action id is already present in the selected recipe: " + args.action_id)
    if recipe["runtime"] == "proof-only" and not args.handler.startswith("proof."):
        fail("proof-only screen handlers must use the proof.* namespace")

    component_path = os.path.join(COMPONENT_TEMPLATES_DIR, args.component + ".json")
    if not os.path.isfile(component_path):
        fail("approved component template missing: " + args.component)
    component = load_json(component_path, "component template")
    if component.get("id") != args.component:
        fail("component template identity mismatch: " + args.component)
    if args.slot_role not in (component.get("allowedSlotRoles") or []):
        fail("component " + args.component + " does not allow slotRole " + args.slot_role)
    if args.tone not in (component.get("allowedTones") or []):
        fail("component " + args.component + " does not allow tone " + args.tone)

    template_path = os.path.join(SCREEN_TEMPLATES_DIR, archetype + ".json")
    template = load_json(template_path, "screen template")
    layouts = template.get("layouts") or {}
    if set(layouts) != {"wide", "compact"}:
        fail("screen template must define wide and compact layouts")

    collection_layouts = [
        (layout.get("collections") or {}).get(args.slot_role)
        for layout in layouts.values()
    ]
    fixed_layouts = [
        (layout.get("slots") or {}).get(args.slot_role)
        for layout in layouts.values()
    ]
    row_index = None
    if all(collection is not None for collection in collection_layouts):
        used = [
            action.get("rowIndex")
            for action in actions
            if action.get("slotRole") == args.slot_role
        ]
        if any(not isinstance(index, int) or index < 0 for index in used):
            fail("existing collection rows require non-negative rowIndex values")
        row_index = max(used, default=-1) + 1
        if any(row_index >= collection.get("maxItems", 0) for collection in collection_layouts):
            fail("collection " + args.slot_role + " has no remaining capacity")
    elif all(slot is not None for slot in fixed_layouts):
        if any(action.get("slotRole") == args.slot_role for action in actions):
            fail("fixed slot " + args.slot_role + " is already used")
    else:
        fail("slotRole " + args.slot_role + " is not available in both layout classes")

    focus_orders = [action.get("focusOrder") for action in actions]
    if any(not isinstance(value, int) for value in focus_orders):
        fail("existing actions require integer focusOrder values")
    action = {
        "id": args.action_id,
        "labelKey": args.label_key,
        "icon": args.icon,
        "component": args.component,
        "slotRole": args.slot_role,
    }
    if row_index is not None:
        action["rowIndex"] = row_index
    action.update({
        "tone": args.tone,
        "handler": args.handler,
        "inputs": ["pointer", "keyboard", "touch", "controller"],
        "availability": args.availability,
        "timePolicy": args.time_policy,
        "focusOrder": max(focus_orders, default=0) + 1,
        "visibility": args.visibility,
    })

    output_dir = os.path.abspath(args.output_dir)
    if os.path.exists(output_dir):
        fail("output directory already exists; refusing to overwrite: " + output_dir)
    parent = os.path.dirname(output_dir)
    if not os.path.isdir(parent):
        fail("output parent directory missing: " + parent)
    os.mkdir(output_dir)
    write_json(os.path.join(output_dir, "semantic-action.json"), action)
    write_json(os.path.join(output_dir, "localization-placeholder.json"), {
        "key": args.label_key,
        "value": "TODO",
    })
    write_json(os.path.join(output_dir, "binding-stub.json"), {
        "actionId": args.action_id,
        "handler": args.handler,
        "status": "TODO",
    })
    write_json(os.path.join(output_dir, "harness-fixture.json"), {
        "actionId": args.action_id,
        "scenarioId": None,
        "status": "TODO",
    })
    checklist = """# HD UI action implementation checklist

- Review the semantic fragment against the action audit.
- Add the localization key in every required locale.
- Bind the existing native behavior owner; do not duplicate gameplay logic.
- Add the fragment to the selected semantic recipe.
- Do not add coordinates, visual tokens, or a renderer.
- Run generate-hd-screen.py for the selected recipe.
- Run generate-hd-ui-contracts.py for native and browser consumers.
- Register deterministic harness evidence.
- Run check-hd-ui-authoring.py and focused native/browser tests.
- Remove this bundle after its contents are integrated and verified.
"""
    with open(os.path.join(output_dir, "CHECKLIST.md"), "w",
              encoding="utf-8", newline="\n") as stream:
        stream.write(checklist)
    print("scaffold-hd-ui: created semantic action bundle: " + output_dir)
    return 0


def build_parser():
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    action = subparsers.add_parser("action", help="scaffold one catalog-supported action")
    action.add_argument("--config", required=True)
    action.add_argument("--id", dest="action_id", required=True)
    action.add_argument("--label-key", required=True)
    action.add_argument("--icon", required=True)
    action.add_argument("--component", required=True)
    action.add_argument("--slot-role", required=True)
    action.add_argument("--handler", required=True)
    action.add_argument("--tone", default="normal")
    action.add_argument("--availability", required=True)
    action.add_argument("--time-policy", required=True)
    action.add_argument("--visibility", required=True)
    action.add_argument("--output-dir", required=True)
    action.set_defaults(func=scaffold_action)
    return parser


def main(argv=None):
    args = build_parser().parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
