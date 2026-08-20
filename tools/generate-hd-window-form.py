#!/usr/bin/env python3
"""Generate a complete HD window visual object from semantic content JSON.

The authored config contains semantics and copy only. Geometry, material,
spacing, typography slots, button slots, and motion come from the reviewed
archetype template. Output is deterministic JSON suitable as the canonical
input for the existing C++/DOM consumer generator.
"""

import argparse
import copy
import json
import os
import re
import sys

from hd_window_archetypes import ArchetypeError, build_extended_contract


TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
CALYPSO_DIR = os.path.normpath(os.path.join(TOOLS_DIR, "..", "src", "Calypso"))
FORM_TEMPLATES_DIR = os.path.join(CALYPSO_DIR, "FormTemplates")
ID_RE = re.compile(r"^[a-z][a-z0-9-]*$")
VERSION_RE = re.compile(r"^[A-Za-z0-9._-]+$")
DATE_RE = re.compile(r"^\d{2}\.\d{2}\.\d{4}$")
COLOR_RE = re.compile(r"^[0-9A-Fa-f]{8}$")
CONFIG_FIELDS = {
    "schema", "id", "familyId", "version", "archetype", "protocol",
    "title", "body", "icon", "buttons",
}
OPTIONAL_CONFIG_FIELDS = {"table", "input", "visibleButtons"}
PROTOCOL_FIELDS = {"authority", "record", "code", "revision", "effectiveDate"}
ICON_FIELDS = {"kind", "glyph", "tone"}
BUTTON_FIELDS = {"id", "label", "tone", "action"}
ICON_KINDS = {"warning", "info", "question", "none"}
SEPARATOR_RE = re.compile(r"^-{3,}$")
UNSUPPORTED_MARKDOWN_RE = re.compile(r"[\\*_\x60~<>\\[\\]!]")
HARD_BREAK_RE = re.compile(r"<br>", re.IGNORECASE)
TABLE_LIMITS = {"maxRows": 8, "maxColumns": 4, "maxLinesPerCell": 6, "maxTotalLines": 48}
TABLE_PADDING = 8
TABLE_DIVIDER = 1
TABLE_TEXT_UNIT_WIDE = 8
TABLE_TEXT_UNIT_COMPACT = 7
TABLE_LINE_HEIGHT_WIDE = 22
TABLE_LINE_HEIGHT_COMPACT = 18
INPUT_HEIGHT_WIDE = 38
INPUT_HEIGHT_COMPACT = 32
HINT_HEIGHT_WIDE = 18
HINT_HEIGHT_COMPACT = 16
GAP_MESSAGE_INPUT = 14
GAP_INPUT_HINT = 8
GAP_HINT_FOOTER = 14
PROTOCOL_INLINE_SAFE_PX = 8
PROTOCOL_ADVANCE_TENTHS = 7


class FormError(ValueError):
    pass


def load_json(path):
    try:
        with open(path, "r", encoding="utf-8") as handle:
            return json.load(handle)
    except (OSError, json.JSONDecodeError) as exc:
        raise FormError(str(exc)) from exc


def require_exact_fields(obj, allowed, label):
    if not isinstance(obj, dict):
        raise FormError(label + " must be an object")
    unknown = sorted(set(obj) - allowed)
    if unknown:
        raise FormError(label + " has unsupported fields: " + ", ".join(unknown))
    missing = sorted(allowed - set(obj))
    if missing:
        raise FormError(label + " is missing fields: " + ", ".join(missing))


def one_line(value, label, max_chars):
    if not isinstance(value, str) or not value.strip():
        raise FormError(label + " must be a non-empty string")
    if "\n" in value or "\r" in value:
        raise FormError(label + " must be one line")
    if len(value) > max_chars:
        raise FormError(label + " exceeds the " + str(max_chars) + " character safety limit")
    return value


def right(rect):
    return rect["x"] + rect["width"]


def bottom(rect):
    return rect["y"] + rect["height"]


def contained(inner, outer):
    return (inner["x"] >= outer["x"] and inner["y"] >= outer["y"]
            and right(inner) <= right(outer) and bottom(inner) <= bottom(outer))


def validate_rect(rect, label):
    if not isinstance(rect, dict) or set(rect) != {"x", "y", "width", "height"}:
        raise FormError(label + " must be an x/y/width/height rectangle")
    if not all(isinstance(rect[key], int) for key in ("x", "y", "width", "height")):
        raise FormError(label + " values must be integers")
    if rect["width"] <= 0 or rect["height"] <= 0:
        raise FormError(label + " width/height must be positive")


def validate_template(template):
    if template.get("schema") != 1 or template.get("id") != "small-confirmation":
        raise FormError("template must be schema 1 small-confirmation")
    if template.get("version") != 3:
        raise FormError("template must use content-sized small-confirmation version 3")
    tones = template.get("supportedButtonTones")
    tone_styles = template.get("buttonToneStyles")
    if not isinstance(tones, list) or set(tones) != {"normal", "safe", "primary", "warning", "danger"}:
        raise FormError("template supportedButtonTones drifted from the standard")
    if not isinstance(tone_styles, dict) or set(tone_styles) != set(tones):
        raise FormError("template buttonToneStyles must resolve every supported tone")
    for tone, style in tone_styles.items():
        if set(style) != {"fill", "border", "text"}:
            raise FormError("template button tone " + tone + " must define fill/border/text")
        for key, value in style.items():
            if not isinstance(value, str) or not COLOR_RE.fullmatch(value):
                raise FormError("template button tone " + tone + "." + key + " must be RRGGBBAA")

    style = template.get("style") or {}
    if style.get("cutCornerPx") != 14 or style.get("protocolTextInsetPx") != 26:
        raise FormError("template chamfer/protocol rail drifted from 14/26")
    if style["protocolTextInsetPx"] != style["cutCornerPx"] + 12:
        raise FormError("template protocol inset must be 12 after the chamfer")

    expected = {
        "wide": {
            "canvas": (1280, 720), "window": (580, 236), "status": 38,
            "content": 136, "footer": 62, "lastGap": 14,
            "button": (158, 44), "buttonGap": 10, "topPad": 9, "bottomPad": 9,
        },
        "compact": {
            "canvas": (740, 360), "window": (600, 221), "status": 34,
            "content": 121, "footer": 66, "lastGap": 17,
            "button": (148, 44), "buttonGap": 12, "topPad": 10, "bottomPad": 12,
        },
    }
    layouts = template.get("layouts") or {}
    sizing = template.get("contentSizing") or {}
    density_profiles = template.get("densityProfiles") or {}
    expected_brief = {
        "id": "brief-acknowledgement",
        "scaleNumerator": 2,
        "scaleDenominator": 3,
        "footerHeightPx": 48,
        "minimumActionHeightPx": 44,
        "minimumMessageHeightPx": 40,
    }
    if density_profiles != {"briefAcknowledgement": expected_brief}:
        raise FormError("template briefAcknowledgement density drifted from the reviewed policy")
    if set(sizing) != {"wide", "compact"}:
        raise FormError("template contentSizing requires exact wide/compact policies")
    if set(layouts) != {"wide", "compact"}:
        raise FormError("template requires exact wide/compact layouts")
    for name, want in expected.items():
        layout = layouts[name]
        policy = sizing[name]
        expected_policy = {
            "safeMarginPx": 32 if name == "wide" else 28,
            "textInlineSafePx": 8,
            "titleTextUnitPx": 20 if name == "wide" else 18,
            "bodyTextUnitPx": 9 if name == "wide" else 7,
            "maxWindowWidthPx": want["window"][0],
        }
        if policy != expected_policy:
            raise FormError(name + ".contentSizing drifted from the reviewed policy")
        for key in ("window", "status", "icon", "title", "titleWithoutIcon", "body", "footer"):
            validate_rect(layout.get(key), name + "." + key)
        slots = layout.get("buttonSlots")
        if not isinstance(slots, list) or len(slots) != 2:
            raise FormError(name + ".buttonSlots must contain exactly two slots")
        for index, rect in enumerate(slots):
            validate_rect(rect, name + ".buttonSlots[" + str(index) + "]")
        window = layout["window"]
        canvas = {"x": 0, "y": 0, "width": layout.get("designWidth"), "height": layout.get("designHeight")}
        validate_rect(canvas, name + ".canvas")
        if (layout["designWidth"], layout["designHeight"]) != want["canvas"]:
            raise FormError(name + " canvas drifted")
        if (window["width"], window["height"]) != want["window"] or not contained(window, canvas):
            raise FormError(name + " window drifted or escaped the canvas")
        for key in ("status", "icon", "title", "titleWithoutIcon", "body", "footer"):
            if not contained(layout[key], window):
                raise FormError(name + "." + key + " escaped the window")
        for rect in slots:
            if not contained(rect, layout["footer"]):
                raise FormError(name + " button slot escaped the footer")
        content_height = layout["footer"]["y"] - bottom(layout["status"])
        if (layout["status"]["height"] != want["status"]
                or content_height != want["content"]
                or layout["footer"]["height"] != want["footer"]
                or window["height"] != layout["status"]["height"] + content_height + layout["footer"]["height"]):
            raise FormError(name + " fixed shell equation drifted")
        if layout["footer"]["y"] - bottom(layout["body"]) != want["lastGap"]:
            raise FormError(name + " final content gap drifted")
        if any((slot["width"], slot["height"]) != want["button"] for slot in slots):
            raise FormError(name + " button size drifted")
        if slots[1]["x"] - right(slots[0]) != want["buttonGap"]:
            raise FormError(name + " button gap drifted")
        if slots[0]["y"] - layout["footer"]["y"] != want["topPad"]:
            raise FormError(name + " footer top padding drifted")
        if bottom(layout["footer"]) - bottom(slots[0]) != want["bottomPad"]:
            raise FormError(name + " footer bottom padding drifted")
        if right(slots[1]) != right(layout["body"]):
            raise FormError(name + " action group must end on the content rail")


def validate_config(config, template):
    if not isinstance(config, dict):
        raise FormError("config must be an object")
    unknown = sorted(set(config) - CONFIG_FIELDS - OPTIONAL_CONFIG_FIELDS)
    if unknown:
        raise FormError("config has unsupported fields: " + ", ".join(unknown))
    missing = sorted(CONFIG_FIELDS - set(config))
    if missing:
        raise FormError("config is missing fields: " + ", ".join(missing))
    if config["schema"] != 1:
        raise FormError("config.schema must be 1")
    if not isinstance(config["id"], str) or not ID_RE.fullmatch(config["id"]):
        raise FormError("config.id must match ^[a-z][a-z0-9-]*$")
    if not isinstance(config["familyId"], int) or config["familyId"] <= 0:
        raise FormError("config.familyId must be a positive integer")
    if not isinstance(config["version"], str) or not VERSION_RE.fullmatch(config["version"]):
        raise FormError("config.version must be a stable ASCII version")
    if config["archetype"] != template["id"]:
        raise FormError("config.archetype must be " + template["id"])

    protocol = config["protocol"]
    require_exact_fields(protocol, PROTOCOL_FIELDS, "config.protocol")
    for key in ("authority", "record", "revision"):
        one_line(protocol[key], "config.protocol." + key, 40)
    one_line(protocol["code"], "config.protocol.code", 16)
    if not isinstance(protocol["effectiveDate"], str) or not DATE_RE.fullmatch(protocol["effectiveDate"]):
        raise FormError("config.protocol.effectiveDate must be DD.MM.YYYY lore copy")

    one_line(config["title"], "config.title", 48)
    body = config["body"]
    if not isinstance(body, list) or len(body) > 2:
        raise FormError("config.body must contain zero, one, or two lines")
    for index, line in enumerate(body):
        one_line(line, "config.body[" + str(index) + "]", 96)

    icon = config["icon"]
    require_exact_fields(icon, ICON_FIELDS, "config.icon")
    if icon["kind"] not in ICON_KINDS:
        raise FormError("config.icon.kind must be warning, info, question, or none")
    if icon["tone"] not in template["supportedButtonTones"]:
        raise FormError("config.icon.tone is unsupported")
    if not isinstance(icon["glyph"], str) or len(icon["glyph"]) > 1:
        raise FormError("config.icon.glyph must contain zero or one character")
    if icon["kind"] == "none" and icon["glyph"]:
        raise FormError("config.icon.glyph must be empty when kind is none")
    if icon["kind"] != "none" and not icon["glyph"]:
        raise FormError("config.icon.glyph is required for a visible icon")

    buttons = config["buttons"]
    if not isinstance(buttons, list) or not 1 <= len(buttons) <= 3:
        raise FormError("config.buttons must contain between one and three buttons")
    ids = set()
    actions = set()
    for index, button in enumerate(buttons):
        label = "config.buttons[" + str(index) + "]"
        require_exact_fields(button, BUTTON_FIELDS, label)
        if not isinstance(button["id"], str) or not ID_RE.fullmatch(button["id"]):
            raise FormError(label + ".id must be a stable lowercase ASCII ID")
        if not isinstance(button["action"], str) or not ID_RE.fullmatch(button["action"]):
            raise FormError(label + ".action must be a stable lowercase ASCII action")
        one_line(button["label"], label + ".label", 24)
        if button["tone"] not in template["supportedButtonTones"]:
            raise FormError(label + ".tone is unsupported")
        if button["id"] in ids:
            raise FormError("button IDs must be unique")
        if button["action"] in actions:
            raise FormError("button actions must be unique")
        ids.add(button["id"])
        actions.add(button["action"])

    visible_buttons = config.get("visibleButtons")
    if visible_buttons is not None:
        if not isinstance(visible_buttons, list) or not visible_buttons:
            raise FormError("config.visibleButtons must contain one or more button IDs")
        if len(set(visible_buttons)) != len(visible_buttons):
            raise FormError("config.visibleButtons must not contain duplicates")
        unknown_visible = sorted(set(visible_buttons) - ids)
        if unknown_visible:
            raise FormError("config.visibleButtons contains unknown button IDs: " + ", ".join(unknown_visible))
    table = config.get("table")
    if table is not None:
        if not isinstance(table, str) or not table.strip():
            raise FormError("config.table must be a non-empty Markdown table string when present")
        try:
            parse_table(table, TABLE_LIMITS)
        except FormError as e:
            raise FormError("config.table: " + str(e))
    inp = config.get("input")
    if inp is not None:
        if not isinstance(inp, dict):
            raise FormError("config.input must be an object")
        if set(inp) != {"value", "hint"}:
            raise FormError("config.input must contain exactly value and hint")
        one_line(inp["value"], "config.input.value", 48)
        one_line(inp["hint"], "config.input.hint", 96)


def reject_unsupported_markdown(cell):
    if (UNSUPPORTED_MARKDOWN_RE.search(cell) or re.match(r"^#{1,6}(?:\s|$)", cell)
            or "\\|" in cell):
        raise FormError("table contains unsupported Markdown")


def parse_table(value, limits):
    if "\r" in value:
        raise FormError("table must use LF line endings")
    lines = value.strip().split("\n")
    if any(not line.strip() for line in lines):
        raise FormError("table cannot contain blank lines")
    if len(lines) < 3:
        raise FormError("table requires a header, separator, and at least one data row")
    parsed = []
    for line_number, line in enumerate(lines, 1):
        stripped = line.strip()
        if not stripped.startswith("|") or not stripped.endswith("|"):
            raise FormError("table line " + str(line_number) + " must start and end with |")
        cells = [cell.strip() for cell in stripped[1:-1].split("|")]
        if any(not cell for cell in cells):
            raise FormError("table cells must be non-empty")
        parsed.append(cells)
    columns = len(parsed[0])
    if columns < 2 or columns > limits["maxColumns"]:
        raise FormError("table must contain between 2 and " + str(limits["maxColumns"]) + " columns")
    if any(len(row) != columns for row in parsed):
        raise FormError("table rows must contain the same number of cells")
    if not all(SEPARATOR_RE.fullmatch(cell) for cell in parsed[1]):
        raise FormError("table second row must be a Markdown separator")
    rows = [[HARD_BREAK_RE.sub("\n", cell) for cell in row]
            for row in [parsed[0]] + parsed[2:]]
    if len(rows) > limits["maxRows"]:
        raise FormError("table exceeds the " + str(limits["maxRows"]) + " row limit")
    for row in rows:
        for cell in row:
            reject_unsupported_markdown(cell)
    return rows


def explicit_lines(value, label="value"):
    if "\t" in value:
        raise FormError(label + " cannot contain tabs")
    return value.split("\n")


def proportional_text_width(value, text_unit_width):
    quarter_units = sum(3 if character.islower() else 4 for character in value)
    return (quarter_units * text_unit_width + 3) // 4


def _rect(x, y, w, h):
    return {"x": x, "y": y, "width": w, "height": h}


def button_group_width(authored, button_count):
    button_width = authored["buttonSlots"][0]["width"]
    gap = authored["buttonSlots"][1]["x"] - right(authored["buttonSlots"][0])
    return button_count * button_width + max(0, button_count - 1) * gap


def table_required_content_width(rows, is_wide):
    # Reuse the table generator's real column metrics. The oversized scratch
    # rect removes shell width from the calculation without creating a second
    # table-measurement implementation.
    scratch = _rect(0, 0, 1000000, 1000000)
    _, metrics = build_table_inside_message(rows, scratch, is_wide)
    return metrics["tableWidth"] + 2 * TABLE_PADDING


def semantic_content_width(config, template, name, table_rows):
    """Measure the widest semantic requirement in deterministic design px."""
    authored = template["layouts"][name]
    policy = template["contentSizing"][name]
    inline_safe = 2 * policy["textInlineSafePx"]
    visible_icon = config["icon"]["kind"] != "none"

    title_width = proportional_text_width(config["title"], policy["titleTextUnitPx"])
    title_width += inline_safe
    if visible_icon:
        icon_gap = authored["title"]["x"] - right(authored["icon"])
        title_width += authored["icon"]["width"] + icon_gap

    body_width = max(
        [proportional_text_width(line, policy["bodyTextUnitPx"]) + inline_safe
         for line in config["body"]]
        or [0]
    )
    action_width = button_group_width(authored, len(config["buttons"]))
    optional_width = 0
    if table_rows is not None:
        optional_width = max(
            optional_width,
            table_required_content_width(table_rows, name == "wide"))
    if config.get("input") is not None:
        optional_width = max(
            optional_width,
            proportional_text_width(config["input"]["value"], policy["bodyTextUnitPx"])
                + inline_safe,
            proportional_text_width(config["input"]["hint"], policy["bodyTextUnitPx"])
                + inline_safe,
        )
    return max(title_width, body_width, action_width, optional_width)


def content_sized_shell(config, template, name, table_rows):
    """Derive every horizontal rect from semantic content and safe margins."""
    authored = template["layouts"][name]
    policy = template["contentSizing"][name]
    safe_margin = policy["safeMarginPx"]
    content_width = semantic_content_width(config, template, name, table_rows)
    window_width = content_width + 2 * safe_margin
    if window_width > policy["maxWindowWidthPx"]:
        raise FormError(
            name + " semantic content requires a " + str(window_width)
            + "px window, exceeding the " + str(policy["maxWindowWidthPx"])
            + "px small-confirmation maximum")

    window_x = (authored["designWidth"] - window_width) // 2
    window = _rect(window_x, authored["window"]["y"],
                   window_width, authored["window"]["height"])
    content_left = window_x + safe_margin
    content_right = window_x + window_width - safe_margin

    status = _rect(window_x, authored["status"]["y"],
                   window_width, authored["status"]["height"])
    footer = _rect(window_x, authored["footer"]["y"],
                   window_width, authored["footer"]["height"])
    warning = _rect(content_left, authored["icon"]["y"],
                    authored["icon"]["width"], authored["icon"]["height"])
    if config["icon"]["kind"] == "none":
        title_x = content_left
        title_source = authored["titleWithoutIcon"]
    else:
        title_x = right(warning) + authored["title"]["x"] - right(authored["icon"])
        title_source = authored["title"]
    title = _rect(title_x, title_source["y"],
                  content_right - title_x, title_source["height"])
    message = _rect(content_left, authored["body"]["y"],
                    content_width, authored["body"]["height"])

    button_width = authored["buttonSlots"][0]["width"]
    button_height = authored["buttonSlots"][0]["height"]
    button_y = authored["buttonSlots"][0]["y"]
    gap = authored["buttonSlots"][1]["x"] - right(authored["buttonSlots"][0])
    group_width = button_group_width(authored, len(config["buttons"]))
    first_button_x = content_right - group_width
    slots = [
        _rect(first_button_x + index * (button_width + gap), button_y,
              button_width, button_height)
        for index in range(len(config["buttons"]))
    ]
    return window, status, warning, title, message, footer, slots


def resolved_presentation(config, template):
    """Choose a reviewed density from semantic structure, never caller geometry."""
    is_brief_acknowledgement = (
        len(config["buttons"]) == 1
        and 1 <= len(config["body"]) <= 2
        and config.get("table") is None
        and config.get("input") is None
    )
    if is_brief_acknowledgement:
        profile = template["densityProfiles"]["briefAcknowledgement"]
        return {
            "density": profile["id"],
            "scaleNumerator": profile["scaleNumerator"],
            "scaleDenominator": profile["scaleDenominator"],
        }
    return {"density": "standard", "scaleNumerator": 1, "scaleDenominator": 1}


def nearest_center_preserving_size(value, numerator, denominator):
    """Scale a size while preserving its integer-center parity."""
    scaled = value * numerator
    floor_value = scaled // denominator
    candidates = [candidate for candidate in range(max(1, floor_value - 2), floor_value + 4)
                  if candidate % 2 == value % 2]
    return min(candidates, key=lambda candidate: (abs(candidate * denominator - scaled), candidate))


def scaled_edge(value, numerator, denominator):
    return (value * numerator + denominator // 2) // denominator


def scale_rect_from_window(rect, old_window, new_window, numerator, denominator):
    left = scaled_edge(rect["x"] - old_window["x"], numerator, denominator)
    top = scaled_edge(rect["y"] - old_window["y"], numerator, denominator)
    right_edge = scaled_edge(right(rect) - old_window["x"], numerator, denominator)
    bottom_edge = scaled_edge(bottom(rect) - old_window["y"], numerator, denominator)
    return _rect(new_window["x"] + left, new_window["y"] + top,
                 right_edge - left, bottom_edge - top)


def apply_brief_acknowledgement_density(layout, template, name):
    """Scale the complete visible composition; runtime retains the hit floor."""
    profile = template["densityProfiles"]["briefAcknowledgement"]
    numerator = profile["scaleNumerator"]
    denominator = profile["scaleDenominator"]
    old_window = copy.deepcopy(layout["window"])
    new_width = nearest_center_preserving_size(old_window["width"], numerator, denominator)
    new_height = nearest_center_preserving_size(old_window["height"], numerator, denominator)
    new_window = _rect(
        (layout["designWidth"] - new_width) // 2,
        (2 * old_window["y"] + old_window["height"] - new_height) // 2,
        new_width,
        new_height,
    )

    for key in ("status", "warning", "title", "message", "footer"):
        layout[key] = scale_rect_from_window(
            layout[key], old_window, new_window, numerator, denominator)
    for button_id in list(layout["buttons"]):
        layout["buttons"][button_id] = scale_rect_from_window(
            layout["buttons"][button_id], old_window, new_window, numerator, denominator)
    layout["window"] = new_window
    layout["status"]["x"] = new_window["x"]
    layout["status"]["y"] = new_window["y"]
    layout["status"]["width"] = new_window["width"]
    # Independent edge rounding can otherwise leave the right content margin
    # one pixel wider. The canonical rail is exactly symmetric by contract.
    message_margin = layout["message"]["x"] - new_window["x"]
    layout["message"]["width"] = new_window["width"] - 2 * message_margin

    # The complete visible composition is density-scaled. The native adapter
    # expands only the invisible input widget to the canonical touch floor;
    # Engine and Reference paint these generated visual rectangles unchanged.
    footer_height = profile["footerHeightPx"]
    layout["footer"] = _rect(
        new_window["x"], bottom(new_window) - footer_height,
        new_window["width"], footer_height)
    action_right = right(layout["message"])
    for button_id in reversed(layout["buttons"]):
        button = layout["buttons"][button_id]
        button["x"] = action_right - button["width"]
        button["y"] = layout["footer"]["y"] + (footer_height - button["height"]) // 2
        action_right = button["x"]
    if action_right < layout["message"]["x"]:
        raise FormError(name + " brief acknowledgement action group exceeds its content rail")

    # The native wrapped TTF surface includes transparent cap/baseline guard
    # rows. Keep those rows inside the atomic message rectangle without
    # changing the reviewed shell or moving the visible text baseline.
    message_height = profile["minimumMessageHeightPx"]
    if layout["message"]["height"] < message_height:
        message_center_twice = layout["message"]["y"] * 2 + layout["message"]["height"]
        layout["message"]["y"] = (message_center_twice - message_height) // 2
        layout["message"]["height"] = message_height
    if layout["message"]["y"] < bottom(layout["title"]):
        raise FormError(name + " brief acknowledgement message collides with its title")

    # Shared-edge scaling preserves the rail. Keep the invariant explicit so
    # a future template edit cannot turn rounding into a one-pixel gutter.
    rightmost = layout["buttons"][next(reversed(layout["buttons"]))]
    rightmost["width"] += right(layout["message"]) - right(rightmost)
    if bottom(layout["message"]) >= layout["footer"]["y"]:
        raise FormError(name + " brief acknowledgement message collides with its footer")


def build_table_inside_message(rows, message_rect, is_wide):
    text_unit = TABLE_TEXT_UNIT_WIDE if is_wide else TABLE_TEXT_UNIT_COMPACT
    line_h = TABLE_LINE_HEIGHT_WIDE if is_wide else TABLE_LINE_HEIGHT_COMPACT
    pad = TABLE_PADDING
    div = TABLE_DIVIDER
    columns = len(rows[0])
    col_text_widths = []
    col_widths = []
    for ci in range(columns):
        longest = max(
            proportional_text_width(line, text_unit)
            for r in rows
            for line in explicit_lines(r[ci], "table cell")
        )
        col_text_widths.append(longest)
        col_widths.append(longest + 2 * pad)
    total_w = sum(col_widths) + div * (columns - 1)
    avail_w = message_rect["width"] - 2 * pad
    if total_w > avail_w:
        raise FormError(f"table width {total_w} exceeds message width {avail_w}; shorten cells or add explicit breaks")
    counts = {}
    row_heights = []
    for ri, row in enumerate(rows):
        row_counts = []
        for ci, cell in enumerate(row):
            key = f"cellR{ri+1}C{ci+1}"
            cnt = len(explicit_lines(cell, key))
            if cnt > TABLE_LIMITS["maxLinesPerCell"]:
                raise FormError(f"{key} exceeds the {TABLE_LIMITS['maxLinesPerCell']} line limit")
            counts[key] = cnt
            row_counts.append(cnt)
        row_heights.append(max(row_counts) * line_h + 2 * pad)
    if sum(counts.values()) > TABLE_LIMITS["maxTotalLines"]:
        raise FormError("table exceeds total line limit")
    total_h = sum(row_heights) + div * (len(rows)-1)
    # Build rects positioned inside message_rect with outer padding
    rects = {}
    col_lefts = []
    left = message_rect["x"] + pad
    for ci, cw in enumerate(col_widths):
        col_lefts.append(left)
        left += cw + div
    row_top = message_rect["y"] + pad
    for ri, rh in enumerate(row_heights):
        if ri:
            rects[f"rowDivider{ri}"] = _rect(
                message_rect["x"] + pad,
                row_top - div,
                total_w,
                div)
        for ci in range(columns):
            key = f"cellR{ri+1}C{ci+1}"
            rects[key] = _rect(
                col_lefts[ci] + pad,
                row_top + pad,
                col_widths[ci] - 2*pad,
                rh - 2*pad)
        row_top += rh + div
    for ci in range(1, columns):
        rects[f"columnDivider{ci}"] = _rect(
            col_lefts[ci] - div,
            message_rect["y"] + pad,
            div,
            total_h)
    metrics = {
        "columns": columns,
        "rows": len(rows),
        "columnTextWidths": col_text_widths,
        "columnWidths": col_widths,
        "rowHeights": row_heights,
        "lineCounts": counts,
        "tableWidth": total_w,
        "tableHeight": total_h,
    }
    return rects, metrics


def protocol_text_width(value, font_px):
    """Conservative Share Tech Mono width including the canonical tracking."""
    return (len(value) * font_px * PROTOCOL_ADVANCE_TENTHS + 9) // 10


def protocol_text(protocol, layouts, template, presentation):
    """Choose the longest canonical status title that fits every layout."""
    candidates = [
        protocol["authority"] + " · " + protocol["record"] + " " + protocol["code"],
        protocol["record"] + " " + protocol["code"],
        protocol["authority"] + " · " + protocol["code"],
        protocol["code"],
    ]
    candidates = list(dict.fromkeys(candidates))
    numerator = presentation["scaleNumerator"]
    denominator = presentation["scaleDenominator"]
    inset = scaled_edge(
        template["style"]["protocolTextInsetPx"], numerator, denominator)
    limits = []
    for name, base_font in (("wide", 10), ("compact", 9)):
        font_px = max(8, scaled_edge(base_font, numerator, denominator))
        available = (layouts[name]["status"]["width"] - 2 * inset
                     - 2 * PROTOCOL_INLINE_SAFE_PX)
        limits.append((available, font_px))
    for candidate in candidates:
        if all(protocol_text_width(candidate, font_px) <= available
               for available, font_px in limits):
            return candidate
    raise FormError("config.protocol.code cannot fit the generated status rail")


def build_contract(config, template, source_name):
    validate_template(template)
    validate_config(config, template)
    visible_icon = config["icon"]["kind"] != "none"
    buttons = []
    for button in config["buttons"]:
        generated = copy.deepcopy(button)
        generated["style"] = copy.deepcopy(template["buttonToneStyles"][button["tone"]])
        buttons.append(generated)

    presentation = resolved_presentation(config, template)
    out = {
        "schema": 1,
        "version": config["version"],
        "form": {
            "id": config["id"],
            "familyId": config["familyId"],
            "archetype": config["archetype"],
            "source": source_name,
            "icon": copy.deepcopy(config["icon"]),
            "buttons": buttons,
        },
        "copy": {
            "protocol": "",
            "title": config["title"],
            "message": copy.deepcopy(config["body"]),
        },
        "presentation": presentation,
        "style": copy.deepcopy(template["style"]),
        "layouts": {},
        "motion": copy.deepcopy(template["motion"]),
    }
    if config.get("visibleButtons") is not None:
        out["form"]["visibleButtons"] = copy.deepcopy(config["visibleButtons"])

    # Optional table inside the fixed message body
    table_rows = None
    if config.get("table") is not None:
        table_rows = parse_table(config["table"], TABLE_LIMITS)

    button_count = len(buttons)
    for name in ("wide", "compact"):
        authored = template["layouts"][name]
        is_wide = name == "wide"
        window, status, warning, title, message, footer, slots = content_sized_shell(
            config, template, name, table_rows)

        layout = {
            "designWidth": authored["designWidth"],
            "designHeight": authored["designHeight"],
            "window": window,
            "status": status,
            "warning": warning,
            "title": title,
            "message": message,
            "footer": footer,
            "buttons": {},
        }
        for button, rect in zip(buttons, slots):
            layout["buttons"][button["id"]] = copy.deepcopy(rect)
        # Table + body lines share the same message band: body on top, table below with gap
        has_table = table_rows is not None
        has_input = config.get("input") is not None
        t_rects = None
        t_metrics = None
        if has_table:
            t_rects, t_metrics = build_table_inside_message(table_rows, layout["message"], is_wide)
            line_h = TABLE_LINE_HEIGHT_WIDE if is_wide else TABLE_LINE_HEIGHT_COMPACT
            body_h = len(config["body"]) * line_h + (2 * TABLE_PADDING if config["body"] else 0)
            gap = 4 if config["body"] else 0
            needed_body_h = body_h + gap + t_metrics["tableHeight"]
            cur_body_h = layout["message"]["height"]
            if needed_body_h > cur_body_h:
                delta = needed_body_h - cur_body_h
                layout["message"]["height"] = needed_body_h
                layout["footer"]["y"] += delta
                for bid in list(layout["buttons"].keys()):
                    layout["buttons"][bid]["y"] += delta
                layout["window"]["height"] += delta
                if layout["window"]["y"] + layout["window"]["height"] > layout["designHeight"]:
                    layout["designHeight"] = layout["window"]["y"] + layout["window"]["height"] + 10
            # Shift table rects down by body_h + gap so body occupies top of message
            body_shift = body_h + gap
            for k in list(t_rects.keys()):
                if "cell" in k or "Divider" in k:
                    t_rects[k]["y"] += body_shift - TABLE_PADDING
            for k,v in t_rects.items():
                layout[k] = v
            out.setdefault("_tableMetrics", {})[name] = t_metrics
        # Input field: stacked below body/table inside the same message band.
        # Expand the message (and thus window/footer) to make room so input never
        # overlaps the table. Order inside message top-to-bottom: body, [table],
        # input, hint.
        if has_input:
            input_h = INPUT_HEIGHT_WIDE if is_wide else INPUT_HEIGHT_COMPACT
            hint_h = HINT_HEIGHT_WIDE if is_wide else HINT_HEIGHT_COMPACT
            # Compute total height needed when input is present.
            line_h = TABLE_LINE_HEIGHT_WIDE if is_wide else TABLE_LINE_HEIGHT_COMPACT
            body_h_for_input = len(config["body"]) * line_h + (2 * TABLE_PADDING if config["body"] else 0)
            if has_table:
                # Message already includes body + gap(8) + tableHeight.
                needed_with_input = layout["message"]["height"] + GAP_MESSAGE_INPUT + input_h + GAP_INPUT_HINT + hint_h
            else:
                # No table: body + gap(14) + input + gap(8) + hint
                needed_with_input = body_h_for_input + GAP_MESSAGE_INPUT + input_h + GAP_INPUT_HINT + hint_h
            cur_h = layout["message"]["height"]
            if needed_with_input > cur_h:
                delta = needed_with_input - cur_h
                layout["message"]["height"] = needed_with_input
                layout["footer"]["y"] += delta
                for bid in list(layout["buttons"].keys()):
                    layout["buttons"][bid]["y"] += delta
                layout["window"]["height"] += delta
                if layout["window"]["y"] + layout["window"]["height"] > layout["designHeight"]:
                    layout["designHeight"] = layout["window"]["y"] + layout["window"]["height"] + 10
            # Place input/hint at the very bottom of the (now expanded) message.
            layout["inputHint"] = {"x": layout["message"]["x"], "y": layout["message"]["y"] + layout["message"]["height"] - hint_h, "width": layout["message"]["width"], "height": hint_h}
            layout["inputFrame"] = {"x": layout["message"]["x"], "y": layout["inputHint"]["y"] - GAP_INPUT_HINT - input_h, "width": layout["message"]["width"], "height": input_h}
        if presentation["density"] == "brief-acknowledgement":
            apply_brief_acknowledgement_density(layout, template, name)
        out["layouts"][name] = layout

    out["copy"]["protocol"] = protocol_text(
        config["protocol"], out["layouts"], template, presentation)

    if table_rows is not None:
        out["form"]["tableRows"] = copy.deepcopy(table_rows)
        out["copy"]["table"] = config["table"]
        out["tableMetrics"] = out.pop("_tableMetrics", {})
    if config.get("input") is not None:
        out["form"]["input"] = copy.deepcopy(config["input"])
        out["copy"]["inputValue"] = config["input"]["value"]
        out["copy"]["inputHint"] = config["input"]["hint"]

    # Compatibility aliases keep the shipped F33 adapter and DOM consumer on
    # the generated object while the generic form presenter is introduced.
    if len(buttons) == 2 and buttons[0]["tone"] == "safe" and buttons[1]["tone"] == "danger":
        out["copy"]["safeAction"] = buttons[0]["label"]
        out["copy"]["destructiveAction"] = buttons[1]["label"]
        for layout in out["layouts"].values():
            layout["no"] = copy.deepcopy(layout["buttons"][buttons[0]["id"]])
            layout["yes"] = copy.deepcopy(layout["buttons"][buttons[1]["id"]])
    return out


def render_json(value):
    return json.dumps(value, ensure_ascii=False, indent=2) + "\n"


def main(argv=None):
    parser = argparse.ArgumentParser(description="generate one HD window form object from JSON")
    parser.add_argument("--config", required=True, help="high-level form config JSON")
    parser.add_argument(
        "--template",
        help="reviewed archetype template JSON; selected from config.archetype when omitted")
    parser.add_argument("--output", help="expanded visual-object JSON; stdout when omitted")
    parser.add_argument("--check", action="store_true", help="compare output without writing")
    args = parser.parse_args(argv)
    if args.check and not args.output:
        parser.error("--check requires --output")
    try:
        config = load_json(args.config)
        archetype = config.get("archetype") if isinstance(config, dict) else None
        if not isinstance(archetype, str) or not ID_RE.fullmatch(archetype):
            raise FormError("config.archetype must be a stable lowercase ASCII ID")
        template_path = args.template or os.path.join(
            FORM_TEMPLATES_DIR, archetype + ".json")
        template = load_json(template_path)
        if template.get("id") != archetype:
            raise FormError("template.id must match config.archetype")
        source_name = "FormConfigs/" + os.path.basename(args.config)
        if template.get("generatorKind"):
            template_name = "FormTemplates/" + os.path.basename(template_path)
            contract = build_extended_contract(
                config, template, source_name, template_name)
        else:
            contract = build_contract(config, template, source_name)
        rendered = render_json(contract)
        if args.check:
            try:
                with open(args.output, "r", encoding="utf-8") as handle:
                    current = handle.read()
            except OSError:
                current = None
            if current != rendered:
                print("generate-hd-window-form: STALE: " + args.output, file=sys.stderr)
                return 1
            print("generate-hd-window-form: current: " + args.output)
            return 0
        if not args.output:
            sys.stdout.write(rendered)
            return 0
        os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)
        temp = args.output + ".tmp"
        with open(temp, "w", encoding="utf-8") as handle:
            handle.write(rendered)
        os.replace(temp, args.output)
        print("generate-hd-window-form: wrote " + args.output)
        return 0
    except (FormError, ArchetypeError) as exc:
        print("generate-hd-window-form: error: " + str(exc), file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
