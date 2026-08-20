#!/usr/bin/env python3
"""Compile semantic HD screen recipes through reviewed screen/component templates."""

import argparse
import json
import os
import re
import sys

from hd_ui_authoring_schema import ACTION_ID_RE, SCREEN_ID_RE, SLUG_RE, handler_matches_runtime


TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
CALYPSO_DIR = os.path.normpath(os.path.join(TOOLS_DIR, "..", "src", "Calypso"))
SCREEN_TEMPLATES_DIR = os.path.join(CALYPSO_DIR, "ScreenTemplates")
COMPONENT_TEMPLATES_DIR = os.path.join(CALYPSO_DIR, "ComponentTemplates")
THEME_PATH = os.path.join(CALYPSO_DIR, "Contracts", "hd-ui-theme.json")
MIN_TARGET = 44
VISUAL_KEYS = {
    "x", "y", "width", "height", "rect", "font", "color", "radius",
    "padding", "gap", "glow", "motion", "renderer", "browserBuilder", "shader",
}
TOP_LEVEL_KEYS = {
    "schema", "id", "version", "archetype", "runtime", "actions", "bindings",
    "entities", "context", "fixture",
}
ACTION_KEYS = {
    "id", "labelKey", "icon", "component", "slotRole", "tone", "handler",
    "inputs", "availability", "timePolicy", "focusOrder", "visibility", "hotkey",
    "rowIndex", "variants", "variantContext",
}


def fail(message):
    print("generate-hd-screen: error: " + message, file=sys.stderr)
    raise SystemExit(1)


def load_json(path, label):
    if not os.path.isfile(path):
        fail(label + " missing: " + path)
    try:
        with open(path, "r", encoding="utf-8") as stream:
            return json.load(stream)
    except (OSError, json.JSONDecodeError) as error:
        fail(label + " cannot be read: " + str(error))


def find_visual_escape(value, path="recipe"):
    if isinstance(value, list):
        for index, child in enumerate(value):
            find_visual_escape(child, path + "[" + str(index) + "]")
    elif isinstance(value, dict):
        for key, child in value.items():
            if key in VISUAL_KEYS:
                fail(path + "." + key + " is generator-owned; change the reviewed template")
            find_visual_escape(child, path + "." + key)


def validate_rect(value, where):
    if (not isinstance(value, list) or len(value) != 4
            or any(not isinstance(number, int) or isinstance(number, bool) for number in value)):
        fail(where + " must be [x, y, width, height] integers")
    if value[2] <= 0 or value[3] <= 0:
        fail(where + " width and height must be positive")


def validate_recipe(recipe):
    if recipe.get("schema") != 1:
        fail("recipe.schema must be 1")
    unknown = sorted(set(recipe) - TOP_LEVEL_KEYS)
    if unknown:
        fail("recipe contains unknown fields: " + ", ".join(unknown))
    for key in ("id", "version", "archetype", "runtime"):
        if not isinstance(recipe.get(key), str) or not recipe[key]:
            fail("recipe." + key + " must be a non-empty string")
    if not SCREEN_ID_RE.fullmatch(recipe["id"]):
        fail("recipe.id must be a lower-kebab identifier")
    if recipe["runtime"] not in {"production", "proof-only"}:
        fail("recipe.runtime must be production or proof-only")
    find_visual_escape(recipe)
    actions = recipe.get("actions")
    if not isinstance(actions, list) or not actions:
        fail("recipe.actions must be a non-empty array")
    seen = set()
    for index, action in enumerate(actions):
        where = "recipe.actions[" + str(index) + "]"
        if not isinstance(action, dict):
            fail(where + " must be an object")
        unknown = sorted(set(action) - ACTION_KEYS)
        if unknown:
            fail(where + " contains unknown fields: " + ", ".join(unknown))
        for key in ("id", "component", "slotRole", "tone",
                    "availability", "timePolicy", "visibility"):
            if not isinstance(action.get(key), str) or not action[key]:
                fail(where + "." + key + " must be a non-empty string")
        if action["id"] in seen:
            fail(where + " duplicates action id " + action["id"])
        seen.add(action["id"])
        if not ACTION_ID_RE.fullmatch(action["id"]):
            fail(where + ".id must be a stable lower-case identifier")
        if not SLUG_RE.match(action["component"]):
            fail(where + ".component must be a lower-kebab identifier")
        if not SLUG_RE.match(action["slotRole"]):
            fail(where + ".slotRole must be a lower-kebab identifier")
        inputs = action.get("inputs")
        if not isinstance(inputs, list) or not inputs or any(not isinstance(item, str) for item in inputs):
            fail(where + ".inputs must contain input names")
        if not isinstance(action.get("focusOrder"), int) or isinstance(action.get("focusOrder"), bool):
            fail(where + ".focusOrder must be an integer")
        if ("rowIndex" in action
                and (not isinstance(action["rowIndex"], int) or isinstance(action["rowIndex"], bool)
                     or action["rowIndex"] < 0)):
            fail(where + ".rowIndex must be a non-negative integer")
        variants = action.get("variants", [])
        if not isinstance(variants, list):
            fail(where + ".variants must be an array")
        if not variants:
            for key in ("labelKey", "handler"):
                if not isinstance(action.get(key), str) or not action[key]:
                    fail(where + "." + key + " must be a non-empty string")
            if not handler_matches_runtime(action["handler"], recipe["runtime"]):
                fail(where + ".handler must match the "
                     + ("proof" if recipe["runtime"] == "proof-only" else "production")
                     + " runtime namespace grammar")
        else:
            context = action.get("variantContext")
            if not isinstance(context, str) or not context:
                fail(where + ".variantContext must name the boolean fixture context")
            whens = []
        for variant in variants:
            if (not isinstance(variant, dict)
                    or set(variant) != {"when", "labelKey", "handler"}
                    or not isinstance(variant.get("when"), bool)
                    or not all(isinstance(variant.get(key), str) and variant[key]
                               for key in ("labelKey", "handler"))):
                fail(where + ".variants entries must contain when, labelKey, handler")
            if not handler_matches_runtime(variant["handler"], recipe["runtime"]):
                fail(where + ".variants handler must match the "
                     + ("proof" if recipe["runtime"] == "proof-only" else "production")
                     + " runtime namespace grammar")
            whens.append(variant["when"])
        if variants and set(whens) != {False, True}:
            fail(where + ".variants must contain unique exhaustive boolean branches for " + context)
    if not isinstance(recipe.get("entities", []), list):
        fail("recipe.entities must be an array")


def validate_template(template, archetype):
    if template.get("schema") != 1 or template.get("id") != archetype:
        fail("screen template identity does not match recipe.archetype " + archetype)
    if not isinstance(template.get("version"), str) or not template["version"]:
        fail("screen template version is required")
    coordinate_spaces = template.get("coordinateSpaces")
    if (not isinstance(coordinate_spaces, list) or not coordinate_spaces
            or any(not isinstance(space, str) or not space for space in coordinate_spaces)
            or len(set(coordinate_spaces)) != len(coordinate_spaces)):
        fail("screen template coordinateSpaces must be a non-empty unique string array")
    layouts = template.get("layouts")
    if not isinstance(layouts, dict) or set(layouts) != {"wide", "compact"}:
        fail("screen template must define exactly wide and compact layouts")
    for layout_name, layout in layouts.items():
        canvas = layout.get("canvas")
        validate_rect([0, 0] + canvas if isinstance(canvas, list) else canvas,
                      "template.layouts." + layout_name + ".canvas")
        slots = layout.get("slots")
        if not isinstance(slots, dict) or not slots:
            fail("template.layouts." + layout_name + ".slots must be non-empty")
        for slot_id, slot in slots.items():
            if not isinstance(slot, dict):
                fail("template slot " + slot_id + " must be an object")
            validate_rect(slot.get("visibleRect"), layout_name + ".slots." + slot_id + ".visibleRect")
            validate_rect(slot.get("hitRect", slot.get("visibleRect")),
                          layout_name + ".slots." + slot_id + ".hitRect")
            space = slot.get("coordinateSpace", "screen")
            if space not in coordinate_spaces:
                fail(layout_name + " unsupported coordinateSpace " + str(space)
                     + " for " + slot_id)
        for name, region in (layout.get("regions") or {}).items():
            validate_rect(region, layout_name + ".regions." + name)
        for collection_id, collection in (layout.get("collections") or {}).items():
            where = "template.layouts." + layout_name + ".collections." + collection_id
            for key in ("origin", "itemSize", "stride"):
                value = collection.get(key)
                if (not isinstance(value, list) or len(value) != 2
                        or any(not isinstance(number, int) or isinstance(number, bool)
                               for number in value)):
                    fail(where + "." + key + " must contain two integers")
            if any(value <= 0 for value in collection["itemSize"]):
                fail(where + ".itemSize must be positive")
            space = collection.get("coordinateSpace", "screen")
            if space not in coordinate_spaces:
                fail(layout_name + " unsupported coordinateSpace " + str(space)
                     + " for " + collection_id)
            maximum = collection.get("maxItems")
            if not isinstance(maximum, int) or isinstance(maximum, bool) or maximum <= 0:
                fail(where + ".maxItems must be a positive integer")
        for index, region in enumerate(layout.get("reservedRegions") or []):
            validate_rect(region.get("bounds"), layout_name + ".reservedRegions[" + str(index) + "].bounds")


def load_component(component_id):
    path = os.path.join(COMPONENT_TEMPLATES_DIR, component_id + ".json")
    component = load_json(path, "component template " + component_id)
    if component.get("schema") != 1 or component.get("id") != component_id:
        fail("component template identity mismatch for " + component_id)
    if not isinstance(component.get("version"), str) or not component["version"]:
        fail("component template " + component_id + " requires version")
    minimum = component.get("minimumTarget")
    if (not isinstance(minimum, list) or len(minimum) != 2
            or any(not isinstance(value, int) or value < MIN_TARGET for value in minimum)):
        fail("component template " + component_id + " minimumTarget must be at least 44x44")
    return component


def compile_contract(recipe, template, source_name):
    components = {}
    for action in recipe["actions"]:
        component_id = action["component"]
        if component_id not in components:
            components[component_id] = load_component(component_id)
        allowed_roles = components[component_id].get("allowedSlotRoles") or []
        if action["slotRole"] not in allowed_roles:
            fail(action["id"] + " uses unsupported " + component_id
                 + " slotRole " + action["slotRole"])
        allowed_tones = components[component_id].get("allowedTones") or []
        if action["tone"] not in allowed_tones:
            fail(action["id"] + " uses unsupported " + component_id
                 + " tone " + action["tone"])

    required = template.get("requiredActionIds") or []
    action_ids = [action["id"] for action in recipe["actions"]]
    missing = [action_id for action_id in required if action_id not in action_ids]
    if missing:
        fail("recipe is missing required actions: " + ", ".join(missing))
    persistent = [action["id"] for action in recipe["actions"] if action["visibility"] == "persistent"]
    for pattern in template.get("forbiddenPersistentPatterns") or []:
        match = next((action_id for action_id in persistent if re.search(pattern, action_id)), None)
        if match:
            fail("persistent action " + match + " is forbidden by " + template["id"])
    collection_positions = set()
    for action in recipe["actions"]:
        if "rowIndex" not in action:
            continue
        key = (action["slotRole"], action["rowIndex"])
        if key in collection_positions:
            fail("collection slot " + action["slotRole"] + " rowIndex "
                 + str(action["rowIndex"]) + " is used more than once")
        collection_positions.add(key)

    compiled_layouts = {}
    for layout_name in ("wide", "compact"):
        layout = template["layouts"][layout_name]
        compiled_actions = {}
        for action in recipe["actions"]:
            slot_id = action["slotRole"]
            slot = layout["slots"].get(slot_id)
            if slot is None:
                collection = (layout.get("collections") or {}).get(slot_id)
                if collection is None:
                    fail(template["id"] + " has no " + layout_name + " slot or collection "
                         + slot_id + " required by " + action["id"])
                row_index = action.get("rowIndex")
                if not isinstance(row_index, int):
                    fail(action["id"] + " requires rowIndex for collection " + slot_id)
                if row_index >= collection["maxItems"]:
                    fail(action["id"] + " rowIndex exceeds " + slot_id + " capacity")
                origin = collection["origin"]
                stride = collection["stride"]
                size = collection["itemSize"]
                slot = {
                    "visibleRect": [
                        origin[0] + stride[0] * row_index,
                        origin[1] + stride[1] * row_index,
                        size[0],
                        size[1],
                    ],
                    "coordinateSpace": collection.get("coordinateSpace", "screen"),
                    "zOrder": collection.get("zOrder", 1),
                }
            hit = slot.get("hitRect", slot["visibleRect"])
            minimum = components[action["component"]]["minimumTarget"]
            if hit[2] < minimum[0] or hit[3] < minimum[1]:
                fail(action["id"] + " " + layout_name + " hit target is below component minimum")
            compiled_actions[action["id"]] = {
                "id": action["id"],
                "component": action["component"],
                "slotRole": slot_id,
                "coordinateSpace": slot.get("coordinateSpace", "screen"),
                "visibleRect": slot["visibleRect"],
                "hitRect": hit,
                "focusOrder": action.get("focusOrder"),
                "zOrder": slot.get("zOrder", 1),
            }
        compiled_layouts[layout_name] = {
            "designSize": layout["canvas"],
            "regions": layout.get("regions", {}),
            "reservedRegions": layout.get("reservedRegions", []),
            "actions": compiled_actions,
        }

    theme = load_json(THEME_PATH, "HD theme")
    return {
        "schema": 1,
        "version": recipe["version"],
        "provenance": {
            "generator": "tools/generate-hd-screen.py",
            "source": "ScreenConfigs/" + source_name,
            "screenTemplate": "ScreenTemplates/" + template["id"] + ".json",
        },
        "screen": {
            "id": recipe["id"],
            "archetype": recipe["archetype"],
            "runtime": recipe["runtime"],
            "productionHook": recipe["runtime"] == "production",
        },
        "themeVersion": theme.get("version"),
        "archetypeVersion": template["version"],
        "componentVersions": {key: components[key]["version"] for key in sorted(components)},
        "coordinateSpaces": template["coordinateSpaces"],
        "actions": recipe["actions"],
        "bindings": recipe.get("bindings", {}),
        "entities": recipe.get("entities", []),
        "context": recipe.get("context", {}),
        "fixture": recipe.get("fixture", {}),
        "layouts": compiled_layouts,
    }


def render(contract):
    return json.dumps(contract, indent=2, ensure_ascii=False) + "\n"


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--screen-template-dir", default=SCREEN_TEMPLATES_DIR)
    args = parser.parse_args(argv)

    config_path = os.path.abspath(args.config)
    output_path = os.path.abspath(args.output)
    recipe = load_json(config_path, "semantic screen recipe")
    validate_recipe(recipe)
    template_path = os.path.join(os.path.abspath(args.screen_template_dir), recipe["archetype"] + ".json")
    template = load_json(template_path, "screen template " + recipe["archetype"])
    validate_template(template, recipe["archetype"])
    generated = render(compile_contract(recipe, template, os.path.basename(config_path)))

    if args.check:
        if not os.path.isfile(output_path):
            fail("expanded contract missing: " + output_path)
        with open(output_path, "r", encoding="utf-8") as stream:
            current = stream.read()
        if current != generated:
            fail("expanded contract is stale: " + output_path)
        print("generate-hd-screen: contract is current: " + output_path)
        return 0

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, "w", encoding="utf-8", newline="\n") as stream:
        stream.write(generated)
    print("generate-hd-screen: wrote " + output_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
