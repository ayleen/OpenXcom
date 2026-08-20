#!/usr/bin/env python3
"""Validate every semantic HD screen recipe and all checked consumers."""

import argparse
import json
import os
import re
import subprocess
import sys


TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
CALYPSO_DIR = os.path.normpath(os.path.join(TOOLS_DIR, "..", "src", "Calypso"))
CONFIG_DIR = os.path.join(CALYPSO_DIR, "ScreenConfigs")
CONTRACT_DIR = os.path.join(CALYPSO_DIR, "Contracts")
REGISTRY_PATH = os.path.join(CALYPSO_DIR, "ContractRegistry", "hd-ui-contracts.json")
SCREEN_COMPILER = os.path.join(TOOLS_DIR, "generate-hd-screen.py")
CONSUMER_GENERATOR = os.path.join(TOOLS_DIR, "generate-hd-ui-contracts.py")
REPO_ROOT = os.path.normpath(os.path.join(TOOLS_DIR, "..", "..", ".."))
ENGINE_BIN_DIR = os.path.normpath(os.path.join(TOOLS_DIR, "..", "bin"))
HD_RULESET = os.path.join(REPO_ROOT, "mods", "calypso-hd-pack", "Ruleset",
                          "calypso-hd-pack.rul")
HARNESS_JS = os.path.join(REPO_ROOT, "web", "public", "hd-harness.js")
BROWSER_RENDERER = os.path.join(REPO_ROOT, "web", "public", "hd-screen-reference.js")
NATIVE_RENDERER = os.path.join(CALYPSO_DIR, "CalypsoHdScreenRenderer.cpp")
HOST_MODEL = os.path.join(CALYPSO_DIR, "CalypsoHdHarnessHostModel.h")


def fail(message):
    print("check-hd-ui-authoring: error: " + message, file=sys.stderr)
    raise SystemExit(1)


def load_json(path, label):
    try:
        with open(path, "r", encoding="utf-8") as stream:
            return json.load(stream)
    except (OSError, json.JSONDecodeError) as error:
        fail(label + " cannot be read: " + str(error))


def run_checked(command, label):
    completed = subprocess.run(command, text=True, capture_output=True, check=False)
    if completed.returncode != 0:
        detail = completed.stderr.strip() or completed.stdout.strip()
        fail(label + " failed: " + detail)


def read_text(path, label):
    try:
        with open(path, "r", encoding="utf-8") as stream:
            return stream.read()
    except OSError as error:
        fail(label + " cannot be read: " + str(error))


def locale_keys(locale):
    keys = set()
    for root, _, names in os.walk(ENGINE_BIN_DIR):
        locale_name = locale + ".yml"
        if locale_name not in names:
            continue
        source = read_text(os.path.join(root, locale_name), locale_name)
        keys.update(re.findall(r"^\s{2}(STR_[A-Z0-9_]+):", source, re.MULTILINE))
    if os.path.isfile(HD_RULESET):
        ruleset = read_text(HD_RULESET, "Calypso HD ruleset")
        match = re.search(r"^\s{2}- type: " + re.escape(locale)
                          + r"\s*$([\s\S]*?)(?=^\s{2}- type: |\Z)",
                          ruleset, re.MULTILINE)
        if match:
            keys.update(re.findall(r"^\s{6}(STR_[A-Z0-9_]+):", match.group(1),
                                   re.MULTILINE))
    return keys


def validate_recipe(config_name, recipe, entry, localized):
    archetype = recipe.get("archetype")
    ownership = entry.get("rendererOwnership")
    if ownership != archetype:
        fail(recipe.get("id", config_name) + ": renderer ownership " + str(ownership)
             + " differs from archetype " + str(archetype))

    runtime = recipe.get("runtime")
    actions = recipe.get("actions") or []
    bindings = recipe.get("bindings") or {}
    fixture = recipe.get("fixture") or {}
    fixture_labels = fixture.get("labels") or {}
    action_labels = fixture_labels.get("actions") or {}
    entity_labels = fixture_labels.get("entities") or {}

    for action in actions:
        action_id = action.get("id")
        if action_id not in action_labels or not str(action_labels[action_id]).strip():
            fail(config_name + ": fixture label missing for action " + str(action_id))
        key = action.get("labelKey")
        missing_locales = [locale for locale, keys in localized.items() if key not in keys]
        if missing_locales:
            fail(config_name + ": localization key " + str(key) + " missing from "
                 + " and ".join(missing_locales))
        handler = action.get("handler")
        if not isinstance(handler, str) or not handler.strip() or "todo" in handler.lower():
            fail(config_name + ": action " + str(action_id) + " has empty/TODO handler")
        if runtime == "proof-only":
            if not handler.startswith("proof."):
                fail(config_name + ": proof-only handler " + handler
                     + " must use proof.* namespace")
        elif handler.startswith("proof.") or not re.fullmatch(
                r"[a-z][a-z0-9-]*(?:\.[A-Za-z][A-Za-z0-9-]*)+", handler):
            fail(config_name + ": production handler " + handler
                 + " must use a non-proof runtime namespace")

    for entity in recipe.get("entities") or []:
        entity_id = entity.get("id")
        if entity_id not in entity_labels or not str(entity_labels[entity_id]).strip():
            fail(config_name + ": fixture label missing for entity " + str(entity_id))
        key = entity.get("nameKey")
        missing_locales = [locale for locale, keys in localized.items() if key not in keys]
        if missing_locales:
            fail(config_name + ": localization key " + str(key) + " missing from "
                 + " and ".join(missing_locales))

    for binding_name, binding in bindings.items():
        if not isinstance(binding, str) or not binding.strip() or "todo" in binding.lower():
            fail(config_name + ": binding " + str(binding_name) + " is empty/TODO")
        if runtime == "proof-only" and not binding.startswith("proof."):
            fail(config_name + ": proof-only binding " + binding
                 + " must use proof.* namespace")
        if runtime != "proof-only" and binding.startswith("proof."):
            fail(config_name + ": production binding " + binding
                 + " cannot use proof.* namespace")

    element = entry.get("harnessElement")
    scenarios = entry.get("harnessScenarioIds")
    if not isinstance(element, str) or not element:
        fail(recipe.get("id", config_name) + ": harnessElement missing")
    if not isinstance(scenarios, list):
        fail(recipe.get("id", config_name) + ": harnessScenarioIds missing")
    if runtime == "proof-only" and scenarios:
        fail(recipe.get("id", config_name) + ": proof-only screen cannot own a scenario")
    if runtime != "proof-only" and not scenarios:
        fail(recipe.get("id", config_name) + ": production screen requires a stable scenario")


def rects_overlap(first, second):
    return (first[0] < second[0] + second[2]
            and second[0] < first[0] + first[2]
            and first[1] < second[1] + second[3]
            and second[1] < first[1] + first[3])


def validate_contract_geometry(contract_name, contract):
    for layout_name, layout in (contract.get("layouts") or {}).items():
        canvas = layout.get("designSize")
        actions = layout.get("actions") or {}
        by_space = {}
        for action_id, action in actions.items():
            hit = action.get("hitRect")
            if (not isinstance(hit, list) or len(hit) != 4
                    or any(not isinstance(value, int) for value in hit)
                    or hit[2] <= 0 or hit[3] <= 0):
                fail(contract_name + ": " + layout_name + " invalid hit rectangle " + action_id)
            space = action.get("coordinateSpace")
            if space == "screen" and (hit[0] < 0 or hit[1] < 0
                    or hit[0] + hit[2] > canvas[0] or hit[1] + hit[3] > canvas[1]):
                fail(contract_name + ": " + layout_name + " screen hit escapes canvas " + action_id)
            for other_id, other_hit in by_space.setdefault(space, []):
                if rects_overlap(hit, other_hit):
                    fail(contract_name + ": " + layout_name + " " + str(space)
                         + " hit collision " + other_id + " <> " + action_id)
            by_space[space].append((action_id, hit))


def harness_entry_block(source, element):
    marker = re.search(r"\bid:\s*['\"]" + re.escape(element) + r"['\"]", source)
    if not marker:
        return None
    start = source.rfind("{", 0, marker.start())
    end = source.find("\n    },", marker.end())
    if start < 0 or end < 0:
        return None
    return source[start:end + 7]


def validate_harness(entries, recipe_by_contract, harness_source, host_source):
    elements = set()
    for entry in entries:
        contract_name = entry.get("contract")
        recipe = recipe_by_contract[contract_name]
        element = entry.get("harnessElement")
        if element in elements:
            fail(entry.get("id", contract_name) + ": duplicate harness element " + str(element))
        elements.add(element)
        block = harness_entry_block(harness_source, element)
        if block is None:
            fail(entry.get("id", contract_name) + ": harness element " + str(element)
                 + " is not registered")
        scenarios = entry.get("harnessScenarioIds") or []
        if recipe.get("runtime") == "proof-only":
            if not re.search(r"\breferenceOnly:\s*true\b", block):
                fail(entry.get("id", contract_name) + ": proof harness must be referenceOnly")
            if re.search(r"\bscenario:\s*\d+", block):
                fail(entry.get("id", contract_name) + ": proof harness cannot expose a scenario")
        else:
            for scenario in scenarios:
                if not re.search(r"\bscenario:\s*" + str(scenario) + r"\b", block):
                    fail(entry.get("id", contract_name) + ": harness scenario "
                         + str(scenario) + " is not wired")
                if not re.search(r"=\s*" + str(scenario) + r"\b", host_source):
                    fail(entry.get("id", contract_name) + ": host model scenario "
                         + str(scenario) + " is not registered")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--web-output", required=True,
                        help="checked browser-consumer directory")
    parser.add_argument("--registry", default=REGISTRY_PATH)
    parser.add_argument("--config-dir", default=CONFIG_DIR)
    parser.add_argument("--contract-dir", default=CONTRACT_DIR)
    parser.add_argument("--harness-js", default=HARNESS_JS)
    parser.add_argument("--host-model", default=HOST_MODEL)
    parser.add_argument("--browser-renderer", default=BROWSER_RENDERER)
    parser.add_argument("--native-renderer", default=NATIVE_RENDERER)
    args = parser.parse_args(argv)

    config_dir = os.path.abspath(args.config_dir)
    contract_dir = os.path.abspath(args.contract_dir)
    registry_path = os.path.abspath(args.registry)
    if not os.path.isdir(config_dir):
        fail("screen recipe directory missing: " + config_dir)
    config_names = sorted(name for name in os.listdir(config_dir) if name.endswith(".json"))
    if not config_names:
        fail("no semantic screen recipes found")

    registry = load_json(registry_path, "contract registry")
    entries = registry.get("entries")
    if not isinstance(entries, list):
        fail("contract registry entries missing")
    screen_entries = [entry for entry in entries if entry.get("emitter") == "screen"]
    entry_by_contract = {entry.get("contract"): entry for entry in screen_entries}
    if len(entry_by_contract) != len(screen_entries):
        fail("screen contract registry contains duplicate sources")

    expected_contracts = set()
    recipe_by_contract = {}
    localized = {locale: locale_keys(locale) for locale in ("en-US", "en-GB")}
    for config_name in config_names:
        config_path = os.path.join(config_dir, config_name)
        recipe = load_json(config_path, "screen recipe " + config_name)
        screen_id = recipe.get("id")
        if not isinstance(screen_id, str) or not screen_id:
            fail(config_name + ": recipe id missing")
        contract_name = screen_id + ".json"
        expected_contracts.add(contract_name)
        entry = entry_by_contract.get(contract_name)
        if entry is None:
            fail(config_name + ": no screen registry entry for " + contract_name)
        recipe_by_contract[contract_name] = recipe
        validate_recipe(config_name, recipe, entry, localized)
        contract_path = os.path.join(contract_dir, contract_name)
        contract = load_json(contract_path, "expanded contract " + contract_name)
        validate_contract_geometry(contract_name, contract)
        screen = contract.get("screen") or {}
        if screen.get("id") != screen_id:
            fail(contract_name + ": expanded screen id differs from recipe")
        if recipe.get("runtime") == "proof-only" and screen.get("productionHook") is not False:
            fail(contract_name + ": proof-only recipe exposed a production hook")
        run_checked([
            sys.executable,
            SCREEN_COMPILER,
            "--config", config_path,
            "--output", contract_path,
            "--check",
        ], config_name)

    registered_contracts = set(entry_by_contract)
    if registered_contracts != expected_contracts:
        extra = sorted(registered_contracts - expected_contracts)
        missing = sorted(expected_contracts - registered_contracts)
        fail("screen registry/config drift; extra=" + repr(extra) + ", missing=" + repr(missing))

    harness_source = read_text(os.path.abspath(args.harness_js), "HD harness")
    host_source = read_text(os.path.abspath(args.host_model), "HD harness host model")
    validate_harness(screen_entries, recipe_by_contract, harness_source, host_source)

    renderer_sources = [
        read_text(os.path.abspath(args.browser_renderer), "browser screen renderer"),
        read_text(os.path.abspath(args.native_renderer), "native screen renderer"),
    ]
    for contract_name, recipe in recipe_by_contract.items():
        screen_id = recipe.get("id")
        for source in renderer_sources:
            if screen_id in source:
                fail(contract_name + ": shared renderer contains screen identity " + screen_id)

    canonical_paths = [
        (config_dir, os.path.abspath(CONFIG_DIR)),
        (contract_dir, os.path.abspath(CONTRACT_DIR)),
        (registry_path, os.path.abspath(REGISTRY_PATH)),
    ]
    if any(actual != canonical for actual, canonical in canonical_paths):
        fail("fixture path overrides are negative-test seams only")

    web_output = os.path.abspath(args.web_output)
    run_checked([
        sys.executable,
        CONSUMER_GENERATOR,
        "--check",
        "--registry", registry_path,
        "--web-output", web_output,
    ], "consumer generation")

    print("OK: " + str(len(config_names)) + " screen recipes, "
          + str(len(expected_contracts)) + " expanded contracts, "
          + str(len(entries) * 2) + " consumers")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
