#!/usr/bin/env python3
"""Generate a titleless HD content block from plain text or a Markdown table.

The authored config contains identity and content only. The reviewed archetype
owns shell material, text-safe padding, typography metrics, internal dividers,
and Wide/Compact geometry. Output is deterministic JSON suitable for the
part-list C++/DOM consumer generator.
"""

import argparse
import copy
import json
import os
import re
import sys


TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
CALYPSO_DIR = os.path.normpath(os.path.join(TOOLS_DIR, "..", "src", "Calypso"))
DEFAULT_TEMPLATE = os.path.join(
    CALYPSO_DIR, "ContentBlockTemplates", "content-block.json")
ID_RE = re.compile(r"^[a-z][a-z0-9-]*$")
VERSION_RE = re.compile(r"^[A-Za-z0-9._-]+$")
COLOR_RE = re.compile(r"^[0-9A-Fa-f]{8}$")
SEPARATOR_RE = re.compile(r"^-{3,}$")
UNSUPPORTED_MARKDOWN_RE = re.compile(r"[\\*_`~<>\[\]!]")
HARD_BREAK_RE = re.compile(r"<br>", re.IGNORECASE)
CONFIG_FIELDS = {
    "schema", "id", "familyId", "version", "archetype", "content",
}
CONTENT_FIELDS = {"kind", "value"}
TEMPLATE_FIELDS = {
    "schema", "id", "supportedContentKinds", "limits", "style", "motion", "layouts",
}
LIMIT_FIELDS = {"maxRows", "maxColumns", "maxLinesPerCell", "maxTotalLines"}
STYLE_FIELDS = {
    "panelFillTop", "panelFillBottom", "frame", "divider", "text",
    "cutCornerPx", "borderWidthPx",
}
MOTION_FIELDS = {"durationMs", "scaleFrom", "captureModeDurationMs"}
LAYOUT_FIELDS = {
    "maxBlockWidth", "cellPaddingX", "cellPaddingY", "guardedLineHeight",
    "textUnitWidth", "dividerWidth",
}


class BlockError(ValueError):
    pass


def load_json(path):
    try:
        with open(path, "r", encoding="utf-8") as handle:
            return json.load(handle)
    except (OSError, json.JSONDecodeError) as exc:
        raise BlockError(str(exc)) from exc


def require_exact_fields(obj, allowed, label):
    if not isinstance(obj, dict):
        raise BlockError(label + " must be an object")
    unknown = sorted(set(obj) - allowed)
    if unknown:
        raise BlockError(label + " has unsupported fields: " + ", ".join(unknown))
    missing = sorted(allowed - set(obj))
    if missing:
        raise BlockError(label + " is missing fields: " + ", ".join(missing))


def positive_int(value, label, minimum=1):
    if not isinstance(value, int) or isinstance(value, bool) or value < minimum:
        raise BlockError(label + " must be an integer >= " + str(minimum))


def validate_template(template):
    require_exact_fields(template, TEMPLATE_FIELDS, "template")
    if template["schema"] != 1:
        raise BlockError("template.schema must be 1")
    if template["id"] != "content-block":
        raise BlockError("template.id must be content-block")
    if template["supportedContentKinds"] != ["text", "table"]:
        raise BlockError("template.supportedContentKinds must be text, table")

    limits = template["limits"]
    require_exact_fields(limits, LIMIT_FIELDS, "template.limits")
    for key in LIMIT_FIELDS:
        positive_int(limits[key], "template.limits." + key)
    if limits["maxColumns"] < 2:
        raise BlockError("template.limits.maxColumns must be >= 2")

    style = template["style"]
    require_exact_fields(style, STYLE_FIELDS, "template.style")
    for key in ("panelFillTop", "panelFillBottom", "frame", "divider", "text"):
        if not isinstance(style[key], str) or not COLOR_RE.fullmatch(style[key]):
            raise BlockError("template.style." + key + " must be RRGGBBAA")
    positive_int(style["cutCornerPx"], "template.style.cutCornerPx")
    positive_int(style["borderWidthPx"], "template.style.borderWidthPx")

    motion = template["motion"]
    require_exact_fields(motion, MOTION_FIELDS, "template.motion")
    positive_int(motion["durationMs"], "template.motion.durationMs")
    if motion["durationMs"] > 1000:
        raise BlockError("template.motion.durationMs must be <= 1000")
    if (not isinstance(motion["scaleFrom"], (int, float))
            or isinstance(motion["scaleFrom"], bool)
            or not 0 < motion["scaleFrom"] <= 1):
        raise BlockError("template.motion.scaleFrom must be in (0, 1]")
    if motion["captureModeDurationMs"] != 0:
        raise BlockError("template.motion.captureModeDurationMs must be 0")

    layouts = template["layouts"]
    require_exact_fields(layouts, {"wide", "compact"}, "template.layouts")
    for name in ("wide", "compact"):
        layout = layouts[name]
        require_exact_fields(layout, LAYOUT_FIELDS, "template.layouts." + name)
        for key in LAYOUT_FIELDS:
            positive_int(layout[key], "template.layouts." + name + "." + key)
        if layout["cellPaddingX"] < 8 or layout["cellPaddingY"] < 8:
            raise BlockError("template.layouts." + name + " cell padding must be >= 8")
        if layout["maxBlockWidth"] <= 2 * layout["cellPaddingX"]:
            raise BlockError("template.layouts." + name + " leaves no text width")


def validate_config(config, template):
    require_exact_fields(config, CONFIG_FIELDS, "config")
    if config["schema"] != 1:
        raise BlockError("config.schema must be 1")
    if not isinstance(config["id"], str) or not ID_RE.fullmatch(config["id"]):
        raise BlockError("config.id must match ^[a-z][a-z0-9-]*$")
    if not isinstance(config["familyId"], int) or isinstance(config["familyId"], bool) \
            or config["familyId"] <= 0:
        raise BlockError("config.familyId must be a positive integer")
    if not isinstance(config["version"], str) or not VERSION_RE.fullmatch(config["version"]):
        raise BlockError("config.version must be a stable ASCII version")
    if config["archetype"] != template["id"]:
        raise BlockError("config.archetype must be " + template["id"])
    require_exact_fields(config["content"], CONTENT_FIELDS, "config.content")
    kind = config["content"]["kind"]
    if kind not in template["supportedContentKinds"]:
        raise BlockError("config.content.kind must be text or table")
    value = config["content"]["value"]
    if not isinstance(value, str) or not value.strip():
        raise BlockError("config.content.value must be a non-empty string")


def reject_unsupported_markdown(cell):
    if (UNSUPPORTED_MARKDOWN_RE.search(cell) or re.match(r"^#{1,6}(?:\s|$)", cell)
            or "\\|" in cell):
        raise BlockError("table contains unsupported Markdown")


def parse_table(value, limits):
    if "\r" in value:
        raise BlockError("table must use LF line endings")
    lines = value.strip().split("\n")
    if any(not line.strip() for line in lines):
        raise BlockError("table cannot contain blank lines")
    if len(lines) < 3:
        raise BlockError("table requires a header, separator, and at least one data row")

    parsed = []
    for line_number, line in enumerate(lines, 1):
        stripped = line.strip()
        if not stripped.startswith("|") or not stripped.endswith("|"):
            raise BlockError("table line " + str(line_number) + " must start and end with |")
        cells = [cell.strip() for cell in stripped[1:-1].split("|")]
        if any(not cell for cell in cells):
            raise BlockError("table cells must be non-empty")
        parsed.append(cells)

    columns = len(parsed[0])
    if columns < 2 or columns > limits["maxColumns"]:
        raise BlockError("table must contain between 2 and " + str(limits["maxColumns"]) + " columns")
    if any(len(row) != columns for row in parsed):
        raise BlockError("table rows must contain the same number of cells")
    if not all(SEPARATOR_RE.fullmatch(cell) for cell in parsed[1]):
        raise BlockError("table second row must be a Markdown separator")

    rows = [[HARD_BREAK_RE.sub("\n", cell) for cell in row]
            for row in [parsed[0]] + parsed[2:]]
    if len(rows) > limits["maxRows"]:
        raise BlockError("table exceeds the " + str(limits["maxRows"]) + " row limit")
    for row in rows:
        for cell in row:
            reject_unsupported_markdown(cell)
    return rows


def explicit_lines(value, label):
    if "\t" in value:
        raise BlockError(label + " cannot contain tabs")
    return value.split("\n")


def proportional_text_width(value, text_unit_width):
    """Return a deterministic conservative width without raw character count.

    Lowercase glyphs in the shared proportional body face average roughly
    three quarters of the full uppercase/mono cell. Keep every other codepoint
    at the full unit so mono readouts, digits, punctuation, and unknown scripts
    never become narrower than the previous safe estimate.
    """
    quarter_units = sum(3 if character.islower() else 4 for character in value)
    return (quarter_units * text_unit_width + 3) // 4


def rect(x, y, width, height):
    return {"x": x, "y": y, "width": width, "height": height}


def table_parts(rows, columns):
    parts = ["window"]
    parts.extend("columnDivider" + str(index) for index in range(1, columns))
    parts.extend("rowDivider" + str(index) for index in range(1, len(rows)))
    parts.extend(
        "cellR" + str(row + 1) + "C" + str(column + 1)
        for row in range(len(rows)) for column in range(columns))
    return parts


def build_table_layout(rows, authored, limits, layout_name):
    columns = len(rows[0])
    max_width = authored["maxBlockWidth"]
    padding_x = authored["cellPaddingX"]
    padding_y = authored["cellPaddingY"]
    guarded_line_height = authored["guardedLineHeight"]
    divider_width = authored["dividerWidth"]
    text_unit_width = authored["textUnitWidth"]

    column_text_widths = []
    column_widths = []
    for column_index in range(columns):
        longest = max(
            proportional_text_width(line, text_unit_width)
            for row_index, row in enumerate(rows)
            for line in explicit_lines(
                row[column_index],
                "config.content.cellR" + str(row_index + 1) + "C" + str(column_index + 1))
        )
        column_text_widths.append(longest)
        column_widths.append(longest + 2 * padding_x)
    width = sum(column_widths) + divider_width * (columns - 1)
    if width > max_width:
        raise BlockError(
            layout_name + " table exceeds the " + str(max_width) + " maximum block width; "
            "author explicit line breaks")

    counts = {}
    row_heights = []
    for row_index, row in enumerate(rows):
        row_counts = []
        for column_index, cell in enumerate(row):
            key = "cellR" + str(row_index + 1) + "C" + str(column_index + 1)
            count = len(explicit_lines(cell, "config.content." + key))
            if count > limits["maxLinesPerCell"]:
                raise BlockError(key + " exceeds the " + str(limits["maxLinesPerCell"]) + " line limit")
            counts[key] = count
            row_counts.append(count)
        row_heights.append(max(row_counts) * guarded_line_height + 2 * padding_y)
    if sum(counts.values()) > limits["maxTotalLines"]:
        raise BlockError("table exceeds the " + str(limits["maxTotalLines"]) + " total-line limit")

    height = sum(row_heights) + divider_width * (len(rows) - 1)
    layout = {
        "designWidth": width,
        "designHeight": height,
        "window": rect(0, 0, width, height),
    }
    column_lefts = []
    column_left = 0
    for column_index, column_width in enumerate(column_widths):
        column_lefts.append(column_left)
        column_left += column_width
        if column_index < columns - 1:
            column_left += divider_width
    for column in range(1, columns):
        layout["columnDivider" + str(column)] = rect(
            column_lefts[column] - divider_width,
            padding_y,
            divider_width,
            height - 2 * padding_y)

    row_top = 0
    for row_index, row_height in enumerate(row_heights):
        if row_index:
            layout["rowDivider" + str(row_index)] = rect(
                padding_x,
                row_top - divider_width,
                width - 2 * padding_x,
                divider_width)
        for column_index in range(columns):
            key = "cellR" + str(row_index + 1) + "C" + str(column_index + 1)
            layout[key] = rect(
                column_lefts[column_index] + padding_x,
                row_top + padding_y,
                column_widths[column_index] - 2 * padding_x,
                row_height - 2 * padding_y,
            )
        row_top += row_height + divider_width

    metrics = {
        "maxBlockWidth": max_width,
        "textUnitWidth": text_unit_width,
        "columnTextWidths": column_text_widths,
        "columnWidths": column_widths,
        "cellPaddingX": padding_x,
        "cellPaddingY": padding_y,
        "guardedLineHeight": guarded_line_height,
        "rowHeights": row_heights,
        "lineCounts": counts,
        "alignment": {key: "top-left" for key in counts},
    }
    return layout, metrics


def build_text_layout(value, authored, limits, layout_name):
    max_width = authored["maxBlockWidth"]
    padding_x = authored["cellPaddingX"]
    padding_y = authored["cellPaddingY"]
    guarded_line_height = authored["guardedLineHeight"]
    text_unit_width = authored["textUnitWidth"]
    lines = explicit_lines(value, "config.content.value")
    if len(lines) > limits["maxLinesPerCell"]:
        raise BlockError(
            layout_name + " text exceeds the " + str(limits["maxLinesPerCell"]) + " line limit")
    text_width = max(proportional_text_width(line, text_unit_width) for line in lines)
    width = text_width + 2 * padding_x
    if width > max_width:
        raise BlockError(
            layout_name + " text exceeds the " + str(max_width) + " maximum block width; "
            "author explicit line breaks")
    height = len(lines) * guarded_line_height + 2 * padding_y
    layout = {
        "designWidth": width,
        "designHeight": height,
        "window": rect(0, 0, width, height),
        "text": rect(padding_x, padding_y, text_width, len(lines) * guarded_line_height),
    }
    metrics = {
        "maxBlockWidth": max_width,
        "textUnitWidth": text_unit_width,
        "textWidth": text_width,
        "cellPaddingX": padding_x,
        "cellPaddingY": padding_y,
        "guardedLineHeight": guarded_line_height,
        "lineCount": len(lines),
        "alignment": "top-left",
    }
    return layout, metrics


def source_name(path):
    absolute = os.path.abspath(path)
    try:
        relative = os.path.relpath(absolute, CALYPSO_DIR)
    except ValueError:
        return os.path.basename(path)
    if relative == ".." or relative.startswith(".." + os.sep):
        return os.path.basename(path)
    return relative.replace(os.sep, "/")


def build_contract(config, template, config_path):
    validate_template(template)
    validate_config(config, template)
    kind = config["content"]["kind"]
    value = config["content"]["value"].strip()
    limits = template["limits"]
    rows = parse_table(value, limits) if kind == "table" else None

    if kind == "table":
        columns = len(rows[0])
        parts = table_parts(rows, columns)
        copy_values = {
            "cellR" + str(row + 1) + "C" + str(column + 1): rows[row][column]
            for row in range(len(rows)) for column in range(columns)
        }
        content = {"kind": "table", "columns": columns, "rows": copy.deepcopy(rows)}
    else:
        parts = ["window", "text"]
        copy_values = {"text": value}
        content = {"kind": "text", "value": value}

    output = {
        "schema": 1,
        "version": config["version"],
        "visualProfile": "content-block-v1",
        "block": {
            "id": config["id"],
            "familyId": config["familyId"],
            "archetype": config["archetype"],
            "source": source_name(config_path),
            "contentKind": kind,
        },
        "presentation": {
            "fitFailure": "exception",
            "legacyFallback": False,
        },
        "content": content,
        "copy": copy_values,
        "parts": parts,
        "actions": [],
        "style": copy.deepcopy(template["style"]),
        "metrics": {},
        "layouts": {},
        "motion": copy.deepcopy(template["motion"]),
    }

    for name in ("wide", "compact"):
        if kind == "table":
            layout, metrics = build_table_layout(rows, template["layouts"][name], limits, name)
        else:
            layout, metrics = build_text_layout(value, template["layouts"][name], limits, name)
        output["layouts"][name] = layout
        output["metrics"][name] = metrics
    return output


def render_json(value):
    return json.dumps(value, ensure_ascii=False, indent=2) + "\n"


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="generate one titleless HD content-block object from JSON")
    parser.add_argument("--config", required=True, help="high-level content-block config JSON")
    parser.add_argument("--template", default=DEFAULT_TEMPLATE,
                        help="reviewed content-block archetype template JSON")
    parser.add_argument("--output", help="expanded block JSON; stdout when omitted")
    parser.add_argument("--check", action="store_true", help="compare output without writing")
    args = parser.parse_args(argv)
    if args.check and not args.output:
        parser.error("--check requires --output")

    try:
        config = load_json(args.config)
        template = load_json(args.template)
        rendered = render_json(build_contract(config, template, args.config))
        if args.check:
            try:
                with open(args.output, "r", encoding="utf-8") as handle:
                    current = handle.read()
            except OSError:
                current = None
            if current != rendered:
                print("generate-hd-content-block: STALE: " + args.output, file=sys.stderr)
                return 1
            print("generate-hd-content-block: content block is current")
            return 0
        if args.output:
            directory = os.path.dirname(os.path.abspath(args.output))
            os.makedirs(directory, exist_ok=True)
            temporary = args.output + ".tmp"
            with open(temporary, "w", encoding="utf-8") as handle:
                handle.write(rendered)
            os.replace(temporary, args.output)
        else:
            sys.stdout.write(rendered)
        return 0
    except BlockError as exc:
        print("generate-hd-content-block: error: " + str(exc), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
