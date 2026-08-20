#!/usr/bin/env python3
"""Validate every semantic HD screen recipe and all checked consumers."""

import argparse
import json
import os
import subprocess
import sys


TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
CALYPSO_DIR = os.path.normpath(os.path.join(TOOLS_DIR, "..", "src", "Calypso"))
CONFIG_DIR = os.path.join(CALYPSO_DIR, "ScreenConfigs")
CONTRACT_DIR = os.path.join(CALYPSO_DIR, "Contracts")
REGISTRY_PATH = os.path.join(CALYPSO_DIR, "ContractRegistry", "hd-ui-contracts.json")
SCREEN_COMPILER = os.path.join(TOOLS_DIR, "generate-hd-screen.py")
CONSUMER_GENERATOR = os.path.join(TOOLS_DIR, "generate-hd-ui-contracts.py")


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


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--web-output", required=True,
                        help="checked browser-consumer directory")
    args = parser.parse_args(argv)

    if not os.path.isdir(CONFIG_DIR):
        fail("screen recipe directory missing: " + CONFIG_DIR)
    config_names = sorted(name for name in os.listdir(CONFIG_DIR) if name.endswith(".json"))
    if not config_names:
        fail("no semantic screen recipes found")

    registry = load_json(REGISTRY_PATH, "contract registry")
    entries = registry.get("entries")
    if not isinstance(entries, list):
        fail("contract registry entries missing")
    screen_entries = [entry for entry in entries if entry.get("emitter") == "screen"]
    entry_by_contract = {entry.get("contract"): entry for entry in screen_entries}
    if len(entry_by_contract) != len(screen_entries):
        fail("screen contract registry contains duplicate sources")

    expected_contracts = set()
    for config_name in config_names:
        config_path = os.path.join(CONFIG_DIR, config_name)
        recipe = load_json(config_path, "screen recipe " + config_name)
        screen_id = recipe.get("id")
        if not isinstance(screen_id, str) or not screen_id:
            fail(config_name + ": recipe id missing")
        contract_name = screen_id + ".json"
        expected_contracts.add(contract_name)
        entry = entry_by_contract.get(contract_name)
        if entry is None:
            fail(config_name + ": no screen registry entry for " + contract_name)
        contract_path = os.path.join(CONTRACT_DIR, contract_name)
        run_checked([
            sys.executable,
            SCREEN_COMPILER,
            "--config", config_path,
            "--output", contract_path,
            "--check",
        ], config_name)
        contract = load_json(contract_path, "expanded contract " + contract_name)
        screen = contract.get("screen") or {}
        if screen.get("id") != screen_id:
            fail(contract_name + ": expanded screen id differs from recipe")
        if recipe.get("runtime") == "proof-only" and screen.get("productionHook") is not False:
            fail(contract_name + ": proof-only recipe exposed a production hook")

    registered_contracts = set(entry_by_contract)
    if registered_contracts != expected_contracts:
        extra = sorted(registered_contracts - expected_contracts)
        missing = sorted(expected_contracts - registered_contracts)
        fail("screen registry/config drift; extra=" + repr(extra) + ", missing=" + repr(missing))

    web_output = os.path.abspath(args.web_output)
    run_checked([
        sys.executable,
        CONSUMER_GENERATOR,
        "--check",
        "--web-output", web_output,
    ], "consumer generation")

    print("OK: " + str(len(config_names)) + " screen recipes, "
          + str(len(expected_contracts)) + " expanded contracts, "
          + str(len(entries) * 2) + " consumers")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
