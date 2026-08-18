#!/usr/bin/env python3
"""Generate a complete HD small-confirmation visual object from content JSON.

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


TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
CALYPSO_DIR = os.path.normpath(os.path.join(TOOLS_DIR, "..", "src", "Calypso"))
DEFAULT_TEMPLATE = os.path.join(CALYPSO_DIR, "FormTemplates", "small-confirmation.json")
ID_RE = re.compile(r"^[a-z][a-z0-9-]*$")
VERSION_RE = re.compile(r"^[A-Za-z0-9._-]+$")
DATE_RE = re.compile(r"^\d{2}\.\d{2}\.\d{4}$")
COLOR_RE = re.compile(r"^[0-9A-Fa-f]{8}$")
CONFIG_FIELDS = {
    "schema", "id", "familyId", "version", "archetype", "protocol",
    "title", "body", "icon", "buttons",
}
PROTOCOL_FIELDS = {"authority", "record", "code", "revision", "effectiveDate"}
ICON_FIELDS = {"kind", "glyph", "tone"}
BUTTON_FIELDS = {"id", "label", "tone", "action"}
ICON_KINDS = {"warning", "info", "question", "none"}


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
            "canvas": (1280, 720), "window": (580, 260), "status": 38,
            "content": 160, "footer": 62, "lastGap": 14,
            "button": (158, 44), "buttonGap": 10, "topPad": 9, "bottomPad": 9,
        },
        "compact": {
            "canvas": (740, 360), "window": (600, 240), "status": 34,
            "content": 140, "footer": 66, "lastGap": 17,
            "button": (148, 44), "buttonGap": 12, "topPad": 10, "bottomPad": 12,
        },
    }
    layouts = template.get("layouts") or {}
    if set(layouts) != {"wide", "compact"}:
        raise FormError("template requires exact wide/compact layouts")
    for name, want in expected.items():
        layout = layouts[name]
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
    require_exact_fields(config, CONFIG_FIELDS, "config")
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
    for key in ("authority", "record", "code", "revision"):
        one_line(protocol[key], "config.protocol." + key, 40)
    if not isinstance(protocol["effectiveDate"], str) or not DATE_RE.fullmatch(protocol["effectiveDate"]):
        raise FormError("config.protocol.effectiveDate must be DD.MM.YYYY lore copy")

    one_line(config["title"], "config.title", 48)
    body = config["body"]
    if not isinstance(body, list) or not 1 <= len(body) <= 2:
        raise FormError("config.body must contain one or two lines")
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
    if not isinstance(buttons, list) or not 1 <= len(buttons) <= 2:
        raise FormError("config.buttons must contain one or two buttons")
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


def protocol_text(protocol):
    return (protocol["authority"] + " · " + protocol["record"] + " " + protocol["code"]
            + " · REV. " + protocol["revision"] + " · EFFECTIVE " + protocol["effectiveDate"])


def build_contract(config, template, source_name):
    validate_template(template)
    validate_config(config, template)
    visible_icon = config["icon"]["kind"] != "none"
    buttons = []
    for button in config["buttons"]:
        generated = copy.deepcopy(button)
        generated["style"] = copy.deepcopy(template["buttonToneStyles"][button["tone"]])
        buttons.append(generated)

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
            "protocol": protocol_text(config["protocol"]),
            "title": config["title"],
            "message": copy.deepcopy(config["body"]),
        },
        "style": copy.deepcopy(template["style"]),
        "layouts": {},
        "motion": copy.deepcopy(template["motion"]),
    }

    button_count = len(buttons)
    for name in ("wide", "compact"):
        authored = template["layouts"][name]
        slots = authored["buttonSlots"][2 - button_count:]
        layout = {
            "designWidth": authored["designWidth"],
            "designHeight": authored["designHeight"],
            "window": copy.deepcopy(authored["window"]),
            "status": copy.deepcopy(authored["status"]),
            "warning": copy.deepcopy(authored["icon"]),
            "title": copy.deepcopy(authored["title"] if visible_icon else authored["titleWithoutIcon"]),
            "message": copy.deepcopy(authored["body"]),
            "footer": copy.deepcopy(authored["footer"]),
            "buttons": {},
        }
        for button, rect in zip(buttons, slots):
            layout["buttons"][button["id"]] = copy.deepcopy(rect)
        out["layouts"][name] = layout

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
    parser.add_argument("--template", default=DEFAULT_TEMPLATE, help="reviewed archetype template JSON")
    parser.add_argument("--output", help="expanded visual-object JSON; stdout when omitted")
    parser.add_argument("--check", action="store_true", help="compare output without writing")
    args = parser.parse_args(argv)
    if args.check and not args.output:
        parser.error("--check requires --output")
    try:
        config = load_json(args.config)
        template = load_json(args.template)
        source_name = "FormConfigs/" + os.path.basename(args.config)
        rendered = render_json(build_contract(config, template, source_name))
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
    except FormError as exc:
        print("generate-hd-window-form: error: " + str(exc), file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
