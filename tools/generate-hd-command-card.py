#!/usr/bin/env python3
"""Generate an F21 command-card contract from authored copy and block data.

The existing command-card contracts keep family-specific geometry because the
native adapters consume named parts. This generator makes the authored part
reproducible without routing F21 through the incompatible small-confirmation
archetype. Geometry remains in the checked contract template; copy and
internal-block semantics live in FormConfigs/.
"""

import argparse
import copy
import json
import os
import re
import sys


ID_RE = re.compile(r"^[a-z][a-z0-9-]*$")
VERSION_RE = re.compile(r"^[A-Za-z0-9._-]+$")
UNSUPPORTED_MARKDOWN_RE = re.compile(r"[\\*_`~<>\[\]!]" )
HARD_BREAK_RE = re.compile(r"<br>", re.IGNORECASE)
MAX_ROWS = 8
MAX_COLUMNS = 4
MAX_LINES_PER_CELL = 6
MAX_TOTAL_LINES = 48


class CardError(ValueError):
    pass


def load_json(path):
    try:
        with open(path, "r", encoding="utf-8") as handle:
            return json.load(handle)
    except (OSError, json.JSONDecodeError) as exc:
        raise CardError(str(exc)) from exc


def require_exact_fields(value, allowed, label):
    if not isinstance(value, dict):
        raise CardError(label + " must be an object")
    unknown = sorted(set(value) - allowed)
    if unknown:
        raise CardError(label + " has unsupported fields: " + ", ".join(unknown))
    missing = sorted(allowed - set(value))
    if missing:
        raise CardError(label + " is missing fields: " + ", ".join(missing))


def non_empty_string(value, label):
    if not isinstance(value, str) or not value.strip():
        raise CardError(label + " must be a non-empty string")
    if "\r" in value:
        raise CardError(label + " must use LF line endings")
    return value


def explicit_lines(value, label):
    non_empty_string(value, label)
    if "\t" in value:
        raise CardError(label + " cannot contain tabs")
    flattened = HARD_BREAK_RE.sub("\n", value).split("\n")
    if any(not line.strip() for line in flattened):
        raise CardError(label + " cannot contain blank lines")
    if any(UNSUPPORTED_MARKDOWN_RE.search(line) for line in flattened):
        raise CardError(label + " contains unsupported Markdown")
    return flattened


def validate_rect(rect, label):
    if not isinstance(rect, dict) or set(rect) != {"x", "y", "width", "height"}:
        raise CardError(label + " must be an x/y/width/height rectangle")
    if any(not isinstance(rect[key], int) or isinstance(rect[key], bool)
           for key in rect):
        raise CardError(label + " values must be integers")
    if rect["width"] <= 0 or rect["height"] <= 0:
        raise CardError(label + " width/height must be positive")


def contained(inner, outer):
    return (inner["x"] >= outer["x"] and inner["y"] >= outer["y"]
            and inner["x"] + inner["width"] <= outer["x"] + outer["width"]
            and inner["y"] + inner["height"] <= outer["y"] + outer["height"])


def validate_template(template, config):
    if template.get("schema") != 1:
        raise CardError("template.schema must be 1")
    if template.get("visualProfile") != "command-card-v1":
        raise CardError("template.visualProfile must be command-card-v1")
    if template.get("version") != config["version"]:
        raise CardError("template and config versions must match")
    if not isinstance(template.get("parts"), list) or not template["parts"]:
        raise CardError("template.parts must be a non-empty list")
    if template["parts"][0] != "window" or len(set(template["parts"])) != len(template["parts"]):
        raise CardError("template.parts must be unique and start with window")
    if not isinstance(template.get("actions"), list):
        raise CardError("template.actions must be a list")
    for action in template["actions"]:
        if action not in template["parts"]:
            raise CardError("template action " + repr(action) + " is not a part")
    layouts = template.get("layouts")
    if not isinstance(layouts, dict) or set(layouts) != {"wide", "compact"}:
        raise CardError("template.layouts must contain Wide and Compact")
    for name, layout in layouts.items():
        canvas = {"x": 0, "y": 0, "width": layout.get("designWidth", 0),
                  "height": layout.get("designHeight", 0)}
        validate_rect(canvas, name + ".canvas")
        validate_rect(layout.get("window"), name + ".window")
        if not contained(layout["window"], canvas):
            raise CardError(name + ".window must fit the design canvas")
        for part in template["parts"]:
            if part == "window":
                continue
            validate_rect(layout.get(part), name + "." + part)
            if not contained(layout[part], layout["window"]):
                raise CardError(name + "." + part + " must fit the window")
        for action in template["actions"]:
            rect = layout[action]
            if rect["width"] < 44 or rect["height"] < 44:
                raise CardError(name + "." + action + " is below the 44px action floor")


def validate_table(block, label, parts):
    rows = block["rows"]
    if not isinstance(rows, list) or not 1 <= len(rows) <= MAX_ROWS:
        raise CardError(label + ".rows must contain one to eight rows")
    if not all(isinstance(row, list) and 2 <= len(row) <= MAX_COLUMNS for row in rows):
        raise CardError(label + ".rows must contain two to four columns")
    columns = len(rows[0])
    if any(len(row) != columns for row in rows):
        raise CardError(label + ".rows must have equal column counts")
    line_total = 0
    for ri, row in enumerate(rows, 1):
        for ci, cell in enumerate(row, 1):
            lines = explicit_lines(cell, label + ".rows[%d][%d]" % (ri, ci))
            if len(lines) > MAX_LINES_PER_CELL:
                raise CardError(label + ".rows[%d][%d] exceeds six lines" % (ri, ci))
            line_total += len(lines)
    if line_total > MAX_TOTAL_LINES:
        raise CardError(label + ".rows exceeds the total line limit")
    for part in block["parts"]:
        if part not in parts:
            raise CardError(label + " references missing part " + repr(part))
    for part in block.get("append", []):
        if part not in parts:
            raise CardError(label + " references missing append part " + repr(part))


def validate_blocks(blocks, template):
    if blocks is None:
        return []
    if not isinstance(blocks, list):
        raise CardError("config.blocks must be a list")
    parts = template["parts"]
    seen = set()
    normalized = []
    for index, block in enumerate(blocks):
        label = "config.blocks[" + str(index) + "]"
        if not isinstance(block, dict):
            raise CardError(label + " must be an object")
        kind = block.get("kind")
        if kind == "table":
            allowed = {"id", "kind", "parts", "rows", "append"}
        elif kind == "input":
            allowed = {"id", "kind", "parts", "value", "hint"}
        elif kind == "list":
            allowed = {"id", "kind", "parts", "items"}
        else:
            raise CardError(label + ".kind must be table, input, or list")
        require_exact_fields(block, allowed, label)
        block_id = block["id"]
        if not isinstance(block_id, str) or not ID_RE.fullmatch(block_id) or block_id in seen:
            raise CardError(label + ".id must be a unique stable ID")
        seen.add(block_id)
        if not isinstance(block["parts"], list) or not block["parts"]:
            raise CardError(label + ".parts must be non-empty")
        if len(set(block["parts"])) != len(block["parts"]):
            raise CardError(label + ".parts must be unique")
        if kind == "table":
            if not isinstance(block.get("append", []), list):
                raise CardError(label + ".append must be a list")
            validate_table(block, label, parts)
        elif kind == "input":
            for key in ("value", "hint"):
                value = non_empty_string(block[key], label + "." + key)
                if "\n" in value:
                    raise CardError(label + "." + key + " must be one line")
                if len(value) > (48 if key == "value" else 96):
                    raise CardError(label + "." + key + " exceeds its safety limit")
            for part in block["parts"]:
                if part not in parts:
                    raise CardError(label + " references missing part " + repr(part))
        else:
            items = block["items"]
            if not isinstance(items, list) or not 1 <= len(items) <= MAX_ROWS:
                raise CardError(label + ".items must contain one to eight entries")
            for item in items:
                explicit_lines(item, label + ".items")
            for part in block["parts"]:
                if part not in parts:
                    raise CardError(label + " references missing part " + repr(part))
        normalized_block = copy.deepcopy(block)
        if kind == "table":
            normalized_block["rows"] = [
                [HARD_BREAK_RE.sub("\n", cell) for cell in row]
                for row in normalized_block["rows"]
            ]
        elif kind == "list":
            normalized_block["items"] = [HARD_BREAK_RE.sub("\n", item)
                                           for item in normalized_block["items"]]
        normalized.append(normalized_block)
    return normalized


def validate_config(config, template):
    require_exact_fields(config, {"schema", "id", "familyId", "version", "archetype", "copy", "blocks"}, "config")
    if config["schema"] != 1:
        raise CardError("config.schema must be 1")
    if not isinstance(config["id"], str) or not ID_RE.fullmatch(config["id"]):
        raise CardError("config.id must be a stable lowercase ID")
    if not isinstance(config["familyId"], int) or isinstance(config["familyId"], bool) or config["familyId"] <= 0:
        raise CardError("config.familyId must be a positive integer")
    if not isinstance(config["version"], str) or not VERSION_RE.fullmatch(config["version"]):
        raise CardError("config.version must be a stable ASCII version")
    if config["archetype"] != "command-card-v1":
        raise CardError("config.archetype must be command-card-v1")
    # The legacy command-card contract predates explicit top-level identity
    # fields. Its geometry remains the template; identity is authored in the
    # config and checked by the output path/consumer registry.
    copy_values = config["copy"]
    if not isinstance(copy_values, dict) or not copy_values:
        raise CardError("config.copy must be a non-empty object")
    for key, value in copy_values.items():
        non_empty_string(value, "config.copy." + key)
        if "{STRING}" in value or "{ALT}" in value:
            raise CardError("config.copy." + key + " contains legacy placeholder/control syntax")
    validate_template(template, config)
    return validate_blocks(config["blocks"], template)


def build_contract(config, template, source_name):
    blocks = validate_config(config, template)
    output = copy.deepcopy(template)
    output["copy"] = copy.deepcopy(config["copy"])
    if blocks:
        output["blocks"] = blocks
    else:
        output.pop("blocks", None)
    output["generation"] = {
        "tool": "generate-hd-command-card.py",
        "source": source_name,
    }
    return output


def render(value):
    return json.dumps(value, ensure_ascii=False, indent=2) + "\n"


def main(argv=None):
    parser = argparse.ArgumentParser(description="generate one F21 command-card contract")
    parser.add_argument("--config", required=True)
    parser.add_argument("--template", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args(argv)
    try:
        config = load_json(args.config)
        template = load_json(args.template)
        template_id = os.path.splitext(os.path.basename(args.template))[0]
        if config.get("id") != template_id:
            raise CardError("config.id must match the template filename")
        generated = render(build_contract(config, template, os.path.basename(args.config)))
        if args.check:
            try:
                with open(args.output, "r", encoding="utf-8") as handle:
                    current = handle.read()
            except OSError:
                current = None
            if current != generated:
                print("generate-hd-command-card: STALE: " + args.output, file=sys.stderr)
                return 1
            print("generate-hd-command-card: current: " + args.output)
            return 0
        temporary = args.output + ".tmp"
        with open(temporary, "w", encoding="utf-8") as handle:
            handle.write(generated)
        os.replace(temporary, args.output)
        print("generate-hd-command-card: wrote " + args.output)
        return 0
    except CardError as exc:
        print("generate-hd-command-card: error: " + str(exc), file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
