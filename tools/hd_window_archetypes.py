"""Template-driven builders for HD window archetypes beyond confirmations.

This module is an implementation detail of generate-hd-window-form.py. It is
not a second generator entry point. Semantic configs are validated here while
all geometry, density, limits, style, and motion remain template-owned.
"""

import copy
import hashlib
import json
import os
import re

ID_RE = re.compile(r"^[a-z][a-z0-9-]*$")
VERSION_RE = re.compile(r"^[A-Za-z0-9._-]+$")
COLOR_RE = re.compile(r"^[0-9A-Fa-f]{8}$")
COMMON_CONFIG_FIELDS = {
    "schema",
    "id",
    "familyId",
    "version",
    "archetype",
    "title",
    "actions",
}
COMMON_CONFIG_OPTIONAL_FIELDS = {
    "state", "labelKeys", "presentation", "contentRole",
}
ACTION_FIELDS = {"id", "label", "tone", "action"}
ACTION_OPTIONAL_FIELDS = {"labelKey"}
CONTROL_COMMON_FIELDS = {"id", "label", "kind", "action"}
CONTROL_OPTIONAL_FIELDS = {"labelKey"}
LABEL_KEY_RE = re.compile(r"^[A-Z][A-Z0-9_]*$")
OPS_WORKSPACE_PRESENTATIONS = {"list-inspector", "table-context"}
OPS_DETAIL_PRESENTATIONS = {"controls-summary", "requirements-summary", "dependency-list"}
OPS_COLUMN_ROLES = {
    "name", "category", "status", "count", "quantity",
    "money", "time", "date", "type",
}
OPERATIONS_TEMPLATE_VERSIONS = {
    "operations-workspace": 4,
    "operations-detail": 2,
}
OPERATIONS_PROFILE_ID = "operations-ac"
OPERATIONS_PROFILE_VERSION = "hd.2026-09-10.1"
STATE_RE = re.compile(r"^[A-Za-z][A-Za-z0-9_]*$")
TEMPLATE_FIELDS = {
    "schema",
    "id",
    "version",
    "generatorKind",
    "styleProfile",
    "supportedButtonTones",
    "buttonToneStyles",
    "buttonToneTokens",
    "limits",
    "style",
    "layouts",
    "motion",
    "sharedChrome",
}
ARCHETYPE_KINDS = {
    "scrollable-collection": "collection",
    "operations-workspace": "operations",
    "tabbed-management": "tabbed",
    "wide-detail": "detail",
    "operations-detail": "detail",
    "selection-list": "selection",
}
LAYOUT_FIELDS = {
    "collection": {
        "designWidth",
        "designHeight",
        "window",
        "title",
        "controlBar",
        "viewport",
        "footer",
        "controlWidth",
        "controlActionWidth",
        "controlGap",
        "controlInset",
        "headerHeight",
        "rowHeight",
        "visibleRows",
        "gridColumns",
        "gridRows",
        "gridGap",
        "gridLabelLineHeight",
        "gridLabelMaxLines",
        "scrollbarWidth",
        "scrollbarGap",
        "minThumbHeight",
        "textUnitWidth",
        "cellInlineInset",
        "actionWidth",
        "actionGap",
        "actionInset",
    },
    "operations": {
        "designWidth",
        "designHeight",
        "window",
        "screenHeader",
        "headerArt",
        "title",
        "summaryBar",
        "toolbarBar",
        "collectionViewport",
        "detailPanel",
        "footer",
        "controlWidth",
        "controlActionWidth",
        "controlGap",
        "controlInset",
        "toolbarWidth",
        "toolbarGap",
        "contextDockHeight",
        "detailInset",
        "detailLabelHeight",
        "detailIdentityHeight",
        "detailMetricColumns",
        "detailMetricRowHeight",
        "detailNoteLineHeight",
        "detailNoteVisible",
        "detailActionGap",
        "headerHeight",
        "rowHeight",
        "visibleRows",
        "gridColumns",
        "gridRows",
        "gridGap",
        "gridLabelLineHeight",
        "gridLabelMaxLines",
        "scrollbarWidth",
        "scrollbarGap",
        "minThumbHeight",
        "textUnitWidth",
        "cellInlineInset",
        "actionWidth",
        "actionGap",
        "actionInset",
    },
    "tabbed": {
        "designWidth",
        "designHeight",
        "window",
        "title",
        "summaryBar",
        "tabBar",
        "toolbarBar",
        "collectionViewport",
        "detailPanel",
        "footer",
        "controlWidth",
        "controlActionWidth",
        "controlGap",
        "controlInset",
        "tabGap",
        "toolbarWidth",
        "toolbarGap",
        "detailInset",
        "detailLabelHeight",
        "detailIdentityHeight",
        "detailMetricColumns",
        "detailMetricRowHeight",
        "detailNoteLineHeight",
        "detailNoteVisible",
        "detailActionGap",
        "headerHeight",
        "rowHeight",
        "visibleRows",
        "gridColumns",
        "gridRows",
        "gridGap",
        "gridLabelLineHeight",
        "gridLabelMaxLines",
        "scrollbarWidth",
        "scrollbarGap",
        "minThumbHeight",
        "textUnitWidth",
        "cellInlineInset",
        "actionWidth",
        "actionGap",
        "actionInset",
    },
    "detail": {
        "designWidth",
        "designHeight",
        "maxWindowWidthPx",
        "window",
        "status",
        "title",
        "controlBar",
        "footer",
        "regionSlots",
        "regionInset",
        "controlWidth",
        "controlActionWidth",
        "controlGap",
        "controlInset",
        "regionHeaderHeight",
        "fieldRowHeight",
        "fieldLabelPercent",
        "fieldColumnGap",
        "regionActionColumns",
        "regionActionVisibleRows",
        "regionActionGap",
        "headerHeight",
        "rowHeight",
        "visibleRows",
        "gridColumns",
        "gridRows",
        "gridGap",
        "gridLabelLineHeight",
        "gridLabelMaxLines",
        "scrollbarWidth",
        "scrollbarGap",
        "minThumbHeight",
        "textUnitWidth",
        "cellInlineInset",
        "actionWidth",
        "actionGap",
        "actionInset",
    },
    "selection": {
        "designWidth",
        "designHeight",
        "window",
        "status",
        "title",
        "list",
        "footer",
        "rowHeight",
        "visibleRows",
        "scrollbarWidth",
        "minThumbHeight",
        "textUnitWidth",
        "cellInlineInset",
        "actionWidth",
        "actionGap",
        "actionInset",
    },
}
LIMIT_FIELDS = {
    "collection": {
        "maxItems",
        "maxColumns",
        "maxCellCharacters",
        "maxControls",
        "maxControlOptions",
        "maxActions",
    },
    "operations": {
        "maxTabs",
        "maxToolbarActions",
        "maxSummaryFields",
        "maxControls",
        "maxControlOptions",
        "maxActions",
        "maxDetailMetrics",
        "maxDetailActions",
        "maxDetailNoteLines",
        "maxItems",
        "maxColumns",
        "maxCellCharacters",
    },
    "tabbed": {
        "maxTabs",
        "maxToolbarActions",
        "maxSummaryFields",
        "maxControls",
        "maxControlOptions",
        "maxActions",
        "maxDetailMetrics",
        "maxDetailActions",
        "maxDetailNoteLines",
        "maxItems",
        "maxColumns",
        "maxCellCharacters",
    },
    "detail": {
        "maxRegions",
        "maxControls",
        "maxControlOptions",
        "maxFieldsPerRegion",
        "maxRegionActions",
        "maxActions",
        "maxItems",
        "maxColumns",
        "maxCellCharacters",
    },
    "selection": {
        "maxItems",
        "maxColumns",
        "maxCellCharacters",
        "maxActions",
    },
}
ZERO_CAPABLE_LAYOUT_FIELDS = {
    "actionGap",
    "actionInset",
    "controlGap",
    "controlInset",
    "detailActionGap",
    "detailInset",
    "detailNoteVisible",
    "fieldColumnGap",
    "gridGap",
    "regionActionGap",
    "regionInset",
    "scrollbarGap",
    "tabGap",
    "toolbarGap",
}
DETAIL_ROLE_BY_KIND = {
    "collection": "primary",
    "preview": "secondary",
    "fields": "summary",
    "actions": "navigation",
}


class ArchetypeError(ValueError):
    pass


def _strict(obj, required, optional, label):
    if not isinstance(obj, dict):
        raise ArchetypeError(label + " must be an object")
    allowed = set(required) | set(optional)
    unknown = sorted(set(obj) - allowed)
    if unknown:
        raise ArchetypeError(label + " has unsupported fields: " + ", ".join(unknown))
    missing = sorted(set(required) - set(obj))
    if missing:
        raise ArchetypeError(label + " is missing fields: " + ", ".join(missing))

PROFILE_BASE = os.path.normpath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "Calypso")
)
TOKEN_RE = re.compile(
    r"inline\s+constexpr\s+Color8\s+([A-Za-z_][A-Za-z0-9_]*)\s*\{\s*"
    r"0x([0-9A-Fa-f]{2})\s*,\s*0x([0-9A-Fa-f]{2})\s*,\s*"
    r"0x([0-9A-Fa-f]{2})\s*,\s*0x([0-9A-Fa-f]{2})\s*\}"
)


def _load_operations_profile(template):
    profile_name = template.get("styleProfile")
    if not isinstance(profile_name, str) or not profile_name:
        raise ArchetypeError("operations template.styleProfile is required")
    profile_path = os.path.normpath(os.path.join(PROFILE_BASE, profile_name))
    if not profile_path.startswith(PROFILE_BASE + os.sep) or not os.path.isfile(profile_path):
        raise ArchetypeError("operations style profile is missing: " + profile_name)
    try:
        with open(profile_path, "r", encoding="utf-8") as handle:
            profile = json.load(handle)
    except (OSError, json.JSONDecodeError) as exc:
        raise ArchetypeError("operations style profile is not parseable: " + str(exc))
    _strict(profile, {"schema", "id", "version", "theme", "actionTones",
                      "columnRoles", "layouts", "art", "minimumHitTarget"}, set(),
            "operations style profile")
    if profile["schema"] != 1 or profile["id"] != OPERATIONS_PROFILE_ID:
        raise ArchetypeError("operations style profile schema/id is invalid")
    if profile["version"] != OPERATIONS_PROFILE_VERSION:
        raise ArchetypeError("operations style profile version is stale")
    theme = profile["theme"]
    _strict(theme, {"source", "sourceHash", "tokens"}, set(),
            "operations style profile.theme")
    theme_path = os.path.normpath(os.path.join(PROFILE_BASE, theme["source"]))
    if not theme_path.startswith(PROFILE_BASE + os.sep) or not os.path.isfile(theme_path):
        raise ArchetypeError("operations theme source is missing: " + theme["source"])
    with open(theme_path, "rb") as handle:
        source = handle.read()
    actual_hash = hashlib.sha256(source).hexdigest()
    if actual_hash != theme["sourceHash"]:
        raise ArchetypeError("operations theme source hash is stale")
    values = {
        name: "".join(ch.upper() for ch in rgba)
        for name, *rgba in TOKEN_RE.findall(source.decode("utf-8"))
    }
    if not values:
        raise ArchetypeError("operations theme source has no parseable Color8 tokens")
    resolved_tokens = {}
    for name, token in theme["tokens"].items():
        if not isinstance(token, str) or token not in values:
            raise ArchetypeError("operations profile token is missing: " + str(token))
        resolved_tokens[name] = values[token]
    tone_tokens = profile["actionTones"]
    if set(tone_tokens) != {"normal", "safe", "primary", "warning", "danger"}:
        raise ArchetypeError("operations profile action tones are incomplete")
    resolved_tones = {}
    for tone, refs in tone_tokens.items():
        _strict(refs, {"fill", "border", "text"}, set(),
                "operations profile.actionTones." + tone)
        resolved_tones[tone] = {}
        for field, token in refs.items():
            if not isinstance(token, str) or token not in values:
                raise ArchetypeError(
                    "operations profile action token is missing: " + str(token)
                )
            resolved_tones[tone][field] = values[token]
    resolved_roles = {}
    for role, policy in profile["columnRoles"].items():
        if role not in OPS_COLUMN_ROLES:
            raise ArchetypeError("operations profile column role is unsupported: " + str(role))
        _strict(policy, {"weight", "minWidth", "font", "align"}, set(),
                "operations profile.columnRoles." + role)
        _positive_int(policy["weight"], "operations profile.columnRoles." + role + ".weight")
        _positive_int(policy["minWidth"], "operations profile.columnRoles." + role + ".minWidth")
        if policy["font"] not in {"body", "data"} or policy["align"] not in {"left", "right"}:
            raise ArchetypeError("operations profile column role policy is invalid: " + role)
        resolved_roles[role] = copy.deepcopy(policy)
    if set(resolved_roles) != OPS_COLUMN_ROLES:
        raise ArchetypeError("operations profile column roles are incomplete")
    return {
        "path": profile_name,
        "data": profile,
        "resolvedTokens": resolved_tokens,
        "resolvedTones": resolved_tones,
        "resolvedRoles": resolved_roles,
        "themeValues": values,
    }


def _prepare_operations_template(template):
    if template.get("generatorKind") != "operations" and template.get("id") != "operations-detail":
        return template, None
    resolved = _load_operations_profile(template)
    prepared = copy.deepcopy(template)
    prepared["buttonToneStyles"] = copy.deepcopy(resolved["resolvedTones"])
    token_values = resolved["resolvedTokens"]
    prepared["style"] = copy.deepcopy(template.get("style") or {})
    prepared["style"].update({
        "panelFillBottom": token_values["background"],
        "frame": token_values["border"],
        "divider": token_values["border"],
        "text": token_values["text"],
        "mutedText": token_values["secondary"],
        "selection": token_values["selected"],
    })
    prepared["_operationsProfile"] = resolved
    if template["id"] == "operations-detail":
        prepared["style"].update({
            "regionFill": token_values["panel"],
            "scrollTrack": token_values["panel"],
            "scrollThumb": token_values["accent"],
        })
    return prepared, resolved


def _one_line(value, label, limit=64):
    if not isinstance(value, str) or not value.strip():
        raise ArchetypeError(label + " must be a non-empty string")
    if "\n" in value or "\r" in value:
        raise ArchetypeError(label + " must be one line")
    if len(value) > limit:
        raise ArchetypeError(label + " exceeds the " + str(limit) + " character limit")

def _label_key(value, label):
    if not isinstance(value, str) or not LABEL_KEY_RE.fullmatch(value):
        raise ArchetypeError(label + " must be an uppercase localization key")


def _validate_metadata(config):
    state = config.get("state")
    if state is not None and not isinstance(state, str):
        raise ArchetypeError("config.state must be a native state identifier")
    if state is not None and not STATE_RE.fullmatch(state):
        raise ArchetypeError("config.state must be a native state identifier")
    label_keys = config.get("labelKeys")
    if label_keys is not None:
        if not isinstance(label_keys, dict):
            raise ArchetypeError("config.labelKeys must be an object")
        for key, value in label_keys.items():
            if not isinstance(key, str) or not key:
                raise ArchetypeError("config.labelKeys keys must be non-empty strings")
            _label_key(value, "config.labelKeys." + key)


def _validate_optional_label_key(obj, label):
    if "labelKey" in obj:
        _label_key(obj["labelKey"], label + ".labelKey")

def _stable_id(value, label):
    if not isinstance(value, str) or not ID_RE.fullmatch(value):
        raise ArchetypeError(label + " must match ^[a-z][a-z0-9-]*$")


def _positive_int(value, label, minimum=1):
    if not isinstance(value, int) or isinstance(value, bool) or value < minimum:
        raise ArchetypeError(label + " must be an integer >= " + str(minimum))


def _rect(value, label):
    if not isinstance(value, dict) or set(value) != {"x", "y", "width", "height"}:
        raise ArchetypeError(label + " must be an x/y/width/height rectangle")
    for key in ("x", "y", "width", "height"):
        if not isinstance(value[key], int) or isinstance(value[key], bool):
            raise ArchetypeError(label + "." + key + " must be an integer")
    if value["width"] <= 0 or value["height"] <= 0:
        raise ArchetypeError(label + " width/height must be positive")


def _right(rect):
    return rect["x"] + rect["width"]
def _operations_presentation(config, template):
    if template["generatorKind"] == "operations":
        allowed = OPS_WORKSPACE_PRESENTATIONS
    elif template["id"] == "operations-detail":
        allowed = OPS_DETAIL_PRESENTATIONS
    else:
        return None
    presentation = config.get("presentation")
    if not isinstance(presentation, str) or presentation not in allowed:
        raise ArchetypeError(
            "config.presentation must be one of " + ", ".join(sorted(allowed))
        )
    return presentation


def _operations_column_roles(template):
    profile = template.get("_operationsProfile")
    if not isinstance(profile, dict):
        raise ArchetypeError("operations style profile was not resolved")
    return profile["resolvedRoles"]

def _bottom(rect):
    return rect["y"] + rect["height"]


def _contained(rect, width, height):
    return (
        rect["x"] >= 0
        and rect["y"] >= 0
        and _right(rect) <= width
        and _bottom(rect) <= height
    )


def _contained_by(rect, parent):
    return (
        rect["x"] >= parent["x"]
        and rect["y"] >= parent["y"]
        and _right(rect) <= _right(parent)
        and _bottom(rect) <= _bottom(parent)
    )


def _overlaps(first, second):
    return not (
        _right(first) <= second["x"]
        or _right(second) <= first["x"]
        or _bottom(first) <= second["y"]
        or _bottom(second) <= first["y"]
    )


def _make_rect(x, y, width, height, stable_id=None):
    out = {"x": x, "y": y, "width": width, "height": height}
    if stable_id is not None:
        out["id"] = stable_id
    return out


def _named_rect(rect):
    return {
        "id": rect["id"],
        "rect": {key: rect[key] for key in ("x", "y", "width", "height")},
    }


def _layout_rect_fields(kind):
    if kind == "collection":
        return ("window", "title", "controlBar", "viewport", "footer")
    if kind == "selection":
        return ("window", "status", "title", "list", "footer")
    if kind == "operations":
        return (
            "window",
            "screenHeader",
            "headerArt",
            "title",
            "summaryBar",
            "toolbarBar",
            "collectionViewport",
            "detailPanel",
            "footer",
        )
    if kind == "tabbed":
        return (
            "window",
            "title",
            "summaryBar",
            "tabBar",
            "toolbarBar",
            "collectionViewport",
            "detailPanel",
            "footer",
        )
    return ("window", "status", "title", "controlBar", "footer")

def _validate_template(template):
    _strict(
        template,
        TEMPLATE_FIELDS - {"sharedChrome"},
        {"sharedChrome", "styleProfile", "buttonToneTokens", "_operationsProfile"},
        "template",
    )
    if template["schema"] != 1:
        raise ArchetypeError("template.schema must be 1")
    archetype = template["id"]
    if not isinstance(archetype, str) or archetype not in ARCHETYPE_KINDS:
        raise ArchetypeError("unsupported extended template id: " + str(archetype))
    if template["generatorKind"] != ARCHETYPE_KINDS[archetype]:
        raise ArchetypeError("template.generatorKind does not match template.id")
    is_operations = template["generatorKind"] == "operations" or archetype == "operations-detail"
    if is_operations:
        expected_version = OPERATIONS_TEMPLATE_VERSIONS[archetype]
        if template["version"] != expected_version:
            raise ArchetypeError(
                "template " + archetype + " must use version " + str(expected_version)
            )
        profile = template.get("_operationsProfile")
        if not isinstance(profile, dict):
            raise ArchetypeError("operations template profile was not resolved")
        if template.get("buttonToneTokens") != profile["data"]["actionTones"]:
            raise ArchetypeError("operations template button tone token refs drifted")
        _strict(template.get("sharedChrome"), {"id", "version"}, set(),
                "template.sharedChrome")
        if template["sharedChrome"]["id"] != "base-command-shell":
            raise ArchetypeError("template.sharedChrome.id must be base-command-shell")
        _one_line(template["sharedChrome"]["version"],
                  "template.sharedChrome.version", 64)
    elif "sharedChrome" in template:
        raise ArchetypeError(
            "template.sharedChrome is only valid for Basescape child forms")
    tones = template["supportedButtonTones"]
    styles = template["buttonToneStyles"]
    expected_tones = {"normal", "safe", "primary", "warning", "danger"}
    if (
        not isinstance(tones, list)
        or not all(isinstance(t, str) for t in tones)
        or set(tones) != expected_tones
    ):
        raise ArchetypeError("template.supportedButtonTones drifted")
    if not isinstance(styles, dict) or set(styles) != expected_tones:
        raise ArchetypeError("template.buttonToneStyles must resolve every tone")
    for tone, style in styles.items():
        _strict(
            style,
            {"fill", "border", "text"},
            set(),
            "template.buttonToneStyles." + tone,
        )
        for field, value in style.items():
            if not isinstance(value, str) or not COLOR_RE.fullmatch(value):
                raise ArchetypeError(
                    "template.buttonToneStyles."
                    + tone
                    + "."
                    + field
                    + " must be RRGGBBAA"
                )
    if not isinstance(template["style"], dict) or not template["style"]:
        raise ArchetypeError("template.style must be a non-empty token object")
    for field, value in template["style"].items():
        if isinstance(value, str):
            if not COLOR_RE.fullmatch(value):
                raise ArchetypeError("template.style." + field + " must be RRGGBBAA")
        else:
            _positive_int(value, "template.style." + field)
    _strict(template["layouts"], {"wide", "compact"}, set(), "template.layouts")
    _strict(
        template["limits"],
        LIMIT_FIELDS[template["generatorKind"]],
        set(),
        "template.limits",
    )
    for field, value in template["limits"].items():
        _positive_int(value, "template.limits." + field)
    _strict(
        template["motion"],
        {"durationMs", "scaleFrom", "easing", "captureModeDurationMs"},
        set(),
        "template.motion",
    )
    _positive_int(template["motion"]["durationMs"], "template.motion.durationMs")
    scale_from = template["motion"]["scaleFrom"]
    if (
        not isinstance(scale_from, (int, float))
        or isinstance(scale_from, bool)
        or not 0 < scale_from <= 1
    ):
        raise ArchetypeError("template.motion.scaleFrom must be in (0, 1]")
    _one_line(template["motion"]["easing"], "template.motion.easing", 32)
    if template["motion"]["captureModeDurationMs"] != 0:
        raise ArchetypeError("template.motion.captureModeDurationMs must be 0")
    for name in ("wide", "compact"):
        layout = template["layouts"][name]
        layout_fields = set(LAYOUT_FIELDS[template["generatorKind"]])
        if archetype == "operations-detail":
            layout_fields.add("headerArt")
        _strict(
            layout,
            layout_fields,
            set(),
            "template.layouts." + name,
        )
        for field in ("designWidth", "designHeight"):
            _positive_int(layout.get(field), "template.layouts." + name + "." + field)
        non_rect_fields = (
            layout_fields
            - set(_layout_rect_fields(template["generatorKind"]))
            - {"designWidth", "designHeight", "regionSlots", "headerArt"}
        )
        for field in non_rect_fields:
            minimum = 0 if field in ZERO_CAPABLE_LAYOUT_FIELDS else 1
            _positive_int(
                layout[field],
                "template.layouts." + name + "." + field,
                minimum,
            )
        if template["generatorKind"] in {"tabbed", "operations"}:
            detail_minimums = {
                "detailLabelHeight": 12,
                "detailIdentityHeight": 20,
                "detailMetricColumns": 1,
                "detailMetricRowHeight": 16,
                "detailNoteLineHeight": 12,
            }
            for field, minimum in detail_minimums.items():
                _positive_int(
                    layout[field],
                    "template.layouts." + name + "." + field,
                    minimum,
                )
        layout_rect_fields = list(_layout_rect_fields(template["generatorKind"]))
        if archetype == "operations-detail":
            layout_rect_fields.append("headerArt")
        for field in layout_rect_fields:
            _rect(layout.get(field), "template.layouts." + name + "." + field)
            if not _contained(
                layout[field], layout["designWidth"], layout["designHeight"]
            ):
                raise ArchetypeError(
                    "template.layouts." + name + "." + field + " escaped the canvas"
                )
        window = layout["window"]
        owned_rects = []
        for field in layout_rect_fields:
            if field == "window":
                continue
            if not _contained_by(layout[field], window):
                raise ArchetypeError(
                    "template.layouts." + name + "." + field + " escaped the window"
                )
            owned_rects.append((field, layout[field]))
        if template["generatorKind"] == "detail":
            _strict(
                layout["regionSlots"],
                {"primary", "secondary", "summary", "navigation"},
                set(),
                "template.layouts." + name + ".regionSlots",
            )
            for role, rect in layout["regionSlots"].items():
                _rect(rect, "template.layouts." + name + ".regionSlots." + role)
                if not _contained(rect, layout["designWidth"], layout["designHeight"]):
                    raise ArchetypeError(
                        "template.layouts."
                        + name
                        + ".regionSlots."
                        + role
                        + " escaped the canvas"
                    )
                if not _contained_by(rect, window):
                    raise ArchetypeError(
                        "template.layouts."
                        + name
                        + ".regionSlots."
                        + role
                        + " escaped the window"
                    )
                owned_rects.append(("regionSlots." + role, rect))
            if not 1 <= layout["fieldLabelPercent"] <= 99:
                raise ArchetypeError(
                    "template.layouts."
                    + name
                    + ".fieldLabelPercent must be between 1 and 99"
                )
            summary = layout["regionSlots"]["summary"]
            summary_content_height = (
                summary["height"]
                - 2 * layout["regionInset"]
                - layout["regionHeaderHeight"]
            )
            field_capacity = summary_content_height // layout["fieldRowHeight"]
            if template["limits"]["maxFieldsPerRegion"] > field_capacity:
                raise ArchetypeError(
                    "template.limits.maxFieldsPerRegion is not achievable in "
                    + name
                    + " summary region"
                )
        for index, (first_name, first_rect) in enumerate(owned_rects):
            for second_name, second_rect in owned_rects[index + 1 :]:
                if _overlaps(first_rect, second_rect):
                    shell_pair = {first_name, second_name}
                    if (
                        template["generatorKind"] == "operations"
                        and "screenHeader" in shell_pair
                        and shell_pair & {"title", "summaryBar", "headerArt"}
                    ) or (
                        archetype == "operations-detail"
                        and "status" in shell_pair
                        and shell_pair & {"title", "controlBar", "headerArt"}
                    ):
                        continue
                    if (
                        archetype == "operations-detail"
                        and shell_pair
                        == {"regionSlots.secondary", "regionSlots.navigation"}
                    ):
                        continue
                    raise ArchetypeError(
                        "template.layouts."
                        + name
                        + "."
                        + first_name
                        + " and "
                        + second_name
                        + " overlap"
                    )
        target_fields = ["actionWidth", "rowHeight", "minThumbHeight"]
        if template["generatorKind"] in {"collection", "tabbed", "operations", "detail"}:
            target_fields.extend(("controlWidth", "controlActionWidth"))
        if (
            template["generatorKind"] in {"collection", "detail"}
            and layout["controlBar"]["height"] < 44
        ):
            raise ArchetypeError(
                "template.layouts." + name + ".controlBar violates the 44px minimum"
            )
        if template["generatorKind"] == "tabbed":
            target_fields.append("toolbarWidth")
            if layout["tabBar"]["height"] < 44:
                raise ArchetypeError(
                    "template.layouts." + name + ".tabBar violates the 44px minimum"
                )
            if layout["toolbarBar"]["height"] < 44:
                raise ArchetypeError(
                    "template.layouts." + name + ".toolbarBar violates the 44px minimum"
                )
            if layout["detailNoteVisible"] not in {0, 1}:
                raise ArchetypeError(
                    "template.layouts." + name + ".detailNoteVisible must be 0 or 1"
                )
            if _bottom(layout["tabBar"]) > layout["toolbarBar"]["y"]:
                raise ArchetypeError(
                    "template.layouts." + name + " tab bar must precede toolbar"
                )
            if (
                _bottom(layout["toolbarBar"]) > layout["collectionViewport"]["y"]
                or _bottom(layout["toolbarBar"]) > layout["detailPanel"]["y"]
            ):
                raise ArchetypeError(
                    "template.layouts." + name + " toolbar must precede workspace"
                )
            if (
                _bottom(layout["collectionViewport"]) > layout["footer"]["y"]
                or _bottom(layout["detailPanel"]) > layout["footer"]["y"]
            ):
                raise ArchetypeError(
                    "template.layouts." + name + " workspace must precede footer"
                )
        if template["generatorKind"] == "operations":
            target_fields.append("toolbarWidth")
            if layout["toolbarBar"]["height"] < 44:
                raise ArchetypeError(
                    "template.layouts." + name + ".toolbarBar violates the 44px minimum"
                )
            if layout["detailNoteVisible"] not in {0, 1}:
                raise ArchetypeError(
                    "template.layouts." + name + ".detailNoteVisible must be 0 or 1"
                )
            if _bottom(layout["toolbarBar"]) > layout["collectionViewport"]["y"]:
                raise ArchetypeError(
                    "template.layouts." + name + " toolbar must precede workspace"
                )
            if (
                _bottom(layout["collectionViewport"]) > layout["footer"]["y"]
                or _bottom(layout["detailPanel"]) > layout["footer"]["y"]
            ):
                raise ArchetypeError(
                    "template.layouts." + name + " workspace must precede footer"
                )
        for field in target_fields:
            if layout[field] < 44:
                raise ArchetypeError(
                    "template.layouts."
                    + name
                    + "."
                    + field
                    + " violates the 44px minimum"
                )
        maximum_actions = template["limits"]["maxActions"]
        required_action_width = (
            maximum_actions * layout["actionWidth"]
            + max(0, maximum_actions - 1) * layout["actionGap"]
            + layout["actionInset"]
        )
        if required_action_width > layout["footer"]["width"]:
            raise ArchetypeError(
                "template.layouts."
                + name
                + " maxActions is not achievable in the footer"
            )


def _validate_common(config, template, required_fields, optional_fields=None):

    _strict(
        config,
        COMMON_CONFIG_FIELDS | set(required_fields),
        COMMON_CONFIG_OPTIONAL_FIELDS | set(optional_fields or ()),
        "config",
    )
    if config["schema"] != 1:
        raise ArchetypeError("config.schema must be 1")
    _stable_id(config["id"], "config.id")
    if (
        not isinstance(config["familyId"], int)
        or isinstance(config["familyId"], bool)
        or config["familyId"] <= 0
    ):
        raise ArchetypeError("config.familyId must be a positive integer")
    if not isinstance(config["version"], str) or not VERSION_RE.fullmatch(
        config["version"]
    ):
        raise ArchetypeError("config.version must be a stable ASCII version")
    if config["archetype"] != template["id"]:
        raise ArchetypeError("config.archetype must match template.id")
    _one_line(config["title"], "config.title", 64)
    _validate_metadata(config)
    _operations_presentation(config, template)
    return _validate_actions(config["actions"], template, "config.actions")


def _validate_actions(actions, template, label):
    return _validate_action_items(
        actions, template, label, template["limits"]["maxActions"]
    )

def _validate_action_items(actions, template, label, maximum):
    if not isinstance(actions, list) or not 1 <= len(actions) <= maximum:
        raise ArchetypeError(
            label + " must contain between one and " + str(maximum) + " actions"
        )
    ids = set()
    roles = set()
    result = []
    for index, action in enumerate(actions):
        item_label = label + "[" + str(index) + "]"
        _strict(action, ACTION_FIELDS, ACTION_OPTIONAL_FIELDS, item_label)
        _validate_optional_label_key(action, item_label)
        _stable_id(action["id"], item_label + ".id")
        _stable_id(action["action"], item_label + ".action")
        _one_line(action["label"], item_label + ".label", 24)
        tone = action.get("tone")
        if not isinstance(tone, str) or tone not in template["supportedButtonTones"]:
            raise ArchetypeError(item_label + ".tone is unsupported")
        if action["id"] in ids or action["action"] in roles:
            raise ArchetypeError(label + " IDs and behavior actions must be unique")
        ids.add(action["id"])
        roles.add(action["action"])
        generated = copy.deepcopy(action)
        generated["style"] = copy.deepcopy(template["buttonToneStyles"][action["tone"]])
        result.append(generated)
    return result


def _validate_controls(controls, template, label):
    limits = template["limits"]
    if not isinstance(controls, list) or len(controls) > limits["maxControls"]:
        raise ArchetypeError(label + " exceeds the template limit")
    ids = set()
    result = []
    actions = set()
    for index, control in enumerate(controls):
        item_label = label + "[" + str(index) + "]"
        if not isinstance(control, dict):
            raise ArchetypeError(item_label + " must be an object")
        kind = control.get("kind")
        if kind == "stepper":
            fields = CONTROL_COMMON_FIELDS | {"value", "minimum", "maximum"}
        elif kind == "select":
            fields = CONTROL_COMMON_FIELDS | {"value", "options"}
        elif kind == "toggle":
            fields = CONTROL_COMMON_FIELDS | {"checked"}
        elif kind == "text-input":
            fields = CONTROL_COMMON_FIELDS | {"value", "placeholder"}
        elif kind == "action":
            fields = CONTROL_COMMON_FIELDS | {"tone"}
        else:
            raise ArchetypeError(
                item_label + ".kind must be stepper, select, toggle, text-input, or action"
            )
        _strict(control, fields, CONTROL_OPTIONAL_FIELDS, item_label)
        _validate_optional_label_key(control, item_label)
        _stable_id(control["id"], item_label + ".id")
        _stable_id(control["action"], item_label + ".action")
        _one_line(control["label"], item_label + ".label", 24)
        if control["id"] in ids or control["action"] in actions:
            raise ArchetypeError(label + " IDs and behavior actions must be unique")
        ids.add(control["id"])
        actions.add(control["action"])
        if kind == "stepper":
            value = control["value"]
            minimum = control["minimum"]
            maximum = control["maximum"]
            if (
                not all(isinstance(item, int) and not isinstance(item, bool)
                        for item in (value, minimum, maximum))
                or minimum > value
                or value > maximum
            ):
                raise ArchetypeError(item_label + ".stepper values are invalid")
        elif kind == "select":
            options = control["options"]
            if (
                not isinstance(options, list)
                or not 1 <= len(options) <= limits["maxControlOptions"]
            ):
                raise ArchetypeError(
                    item_label + ".options is outside the template limit"
                )
            option_ids = set()
            for option_index, option in enumerate(options):
                option_label = item_label + ".options[" + str(option_index) + "]"
                _strict(option, {"id", "label"}, {"labelKey"}, option_label)
                _validate_optional_label_key(option, option_label)
                _stable_id(option["id"], option_label + ".id")
                _one_line(option["label"], option_label + ".label", 32)
                if option["id"] in option_ids:
                    raise ArchetypeError(item_label + " option IDs must be unique")
                option_ids.add(option["id"])
            value = control.get("value")
            if not isinstance(value, str) or value not in option_ids:
                raise ArchetypeError(
                    item_label + ".value must name a configured option"
                )
        elif kind == "toggle":
            if not isinstance(control["checked"], bool):
                raise ArchetypeError(item_label + ".checked must be a boolean")
        elif kind == "text-input":
            value = control["value"]
            if (
                not isinstance(value, str)
                or "\n" in value
                or "\r" in value
                or len(value) > 64
            ):
                raise ArchetypeError(
                    item_label + ".value must be a single line of at most 64 characters"
                )
            _one_line(control["placeholder"], item_label + ".placeholder", 32)
        elif (
            not isinstance(control.get("tone"), str)
            or control["tone"] not in template["supportedButtonTones"]
        ):
            raise ArchetypeError(item_label + ".tone is unsupported")
        generated = copy.deepcopy(control)
        if kind == "action":
            generated["style"] = copy.deepcopy(
                template["buttonToneStyles"][control["tone"]]
            )
        result.append(generated)
    return result


def _ensure_unique_interactions(groups):
    ids = set()
    actions = set()
    for label, items in groups:
        for item in items:
            if item["id"] in ids or item["action"] in actions:
                raise ArchetypeError(
                    "interaction IDs and behavior actions must be globally unique; "
                    + label
                    + " duplicates an earlier binding"
                )
            ids.add(item["id"])
            actions.add(item["action"])


def _validate_collection_value(
    collection, limits, label, allow_bands=False, column_roles=None
):
    if not isinstance(collection, dict):
        raise ArchetypeError(label + " must be an object")
    mode = collection.get("mode")
    if not isinstance(mode, str) or mode not in {"list", "table", "grid"}:
        raise ArchetypeError(label + ".mode must be list, table, or grid")
    fields = {"mode", "selectionRole", "items"}
    if mode != "grid":
        fields.add("columns")
    optional = {"heading", "meta"} if allow_bands else set()
    _strict(collection, fields, optional, label)
    if "heading" in collection:
        _one_line(collection["heading"], label + ".heading", limits["maxCellCharacters"])
    if "meta" in collection:
        if (not isinstance(collection["meta"], str)
                or "\n" in collection["meta"] or "\r" in collection["meta"]
                or len(collection["meta"]) > limits["maxCellCharacters"]):
            raise ArchetypeError(label + ".meta must be one line within the cell character limit")
    _stable_id(collection["selectionRole"], label + ".selectionRole")
    items = collection["items"]
    if not isinstance(items, list) or not 1 <= len(items) <= limits["maxItems"]:
        raise ArchetypeError(label + ".items exceeds the template item limit")
    item_ids = set()
    if mode == "grid":
        for index, item in enumerate(items):
            item_label = label + ".items[" + str(index) + "]"
            _strict(item, {"id", "label"}, set(), item_label)
            _stable_id(item["id"], item_label + ".id")
            _one_line(item["label"], item_label + ".label", limits["maxCellCharacters"])
            if item["id"] in item_ids:
                raise ArchetypeError(label + " item IDs must be unique")
            item_ids.add(item["id"])
        return

    columns = collection["columns"]
    minimum = 2 if mode == "table" else 1
    if (
        not isinstance(columns, list)
        or not minimum <= len(columns) <= limits["maxColumns"]
    ):
        raise ArchetypeError(label + ".columns is outside the template limit")
    column_ids = set()
    for index, column in enumerate(columns):
        column_label = label + ".columns[" + str(index) + "]"
        column_fields = {"id", "label"}
        if column_roles is not None:
            column_fields.add("contentRole")
        _strict(column, column_fields, set(), column_label)
        if column_roles is not None:
            role = column["contentRole"]
            if role not in OPS_COLUMN_ROLES or role not in column_roles:
                raise ArchetypeError(column_label + ".contentRole is unsupported")
        _stable_id(column["id"], column_label + ".id")
        _one_line(
            column["label"],
            column_label + ".label",
            limits["maxCellCharacters"],
        )
        if column["id"] in column_ids:
            raise ArchetypeError(label + " column IDs must be unique")
        column_ids.add(column["id"])
    for index, item in enumerate(items):
        item_label = label + ".items[" + str(index) + "]"
        _strict(item, {"id", "values"}, set(), item_label)
        _stable_id(item["id"], item_label + ".id")
        if item["id"] in item_ids:
            raise ArchetypeError(label + " item IDs must be unique")
        item_ids.add(item["id"])
        if not isinstance(item["values"], list) or len(item["values"]) != len(columns):
            raise ArchetypeError(item_label + ".values must match the column count")
        for value_index, value in enumerate(item["values"]):
            _one_line(
                value,
                item_label + ".values[" + str(value_index) + "]",
                limits["maxCellCharacters"],
            )


def _validate_collection(config, template):
    actions = _validate_common(config, template, {"collection", "controls"})
    roles = _operations_column_roles(template) if template["generatorKind"] == "operations" else None
    _validate_collection_value(
        config["collection"], template["limits"], "config.collection",
        column_roles=roles,
    )
    controls = _validate_controls(config["controls"], template, "config.controls")
    _ensure_unique_interactions(
        [("config.actions", actions), ("config.controls", controls)]
    )
    return actions, controls


def _validate_tabbed_detail(detail, template):
    _strict(
        detail,
        {"id", "label", "title", "subtitle", "metrics", "actions"},
        {"note"},
        "config.detail",
    )
    _stable_id(detail["id"], "config.detail.id")
    _one_line(detail["label"], "config.detail.label", 32)
    _one_line(detail["title"], "config.detail.title", 48)
    subtitle = detail["subtitle"]
    if not isinstance(subtitle, str) or "\n" in subtitle or "\r" in subtitle or len(subtitle) > 48:
        raise ArchetypeError("config.detail.subtitle must be a single line of at most 48 characters")
    if not subtitle and template["id"] not in {"operations-workspace", "wide-detail"}:
        raise ArchetypeError("config.detail.subtitle must be non-empty")

    metrics = detail["metrics"]
    if (
        not isinstance(metrics, list)
        or not 1 <= len(metrics) <= template["limits"]["maxDetailMetrics"]
    ):
        raise ArchetypeError("config.detail.metrics is outside the template limit")
    metric_ids = set()
    for index, metric in enumerate(metrics):
        label = "config.detail.metrics[" + str(index) + "]"
        _strict(metric, {"id", "label", "value"}, set(), label)
        _stable_id(metric["id"], label + ".id")
        _one_line(metric["label"], label + ".label", 24)
        _one_line(metric["value"], label + ".value", 32)
        if metric["id"] in metric_ids:
            raise ArchetypeError("config.detail metric IDs must be unique")
        metric_ids.add(metric["id"])

    if "note" in detail:
        note = detail["note"]
        _strict(note, {"label", "title", "body"}, set(), "config.detail.note")
        _one_line(note["label"], "config.detail.note.label", 24)
        _one_line(note["title"], "config.detail.note.title", 32)
        body = note["body"]
        if (
            not isinstance(body, list)
            or not 1 <= len(body) <= template["limits"]["maxDetailNoteLines"]
        ):
            raise ArchetypeError(
                "config.detail.note.body is outside the template limit"
            )
        for index, line in enumerate(body):
            _one_line(line, "config.detail.note.body[" + str(index) + "]", 64)

    generated = copy.deepcopy(detail)
    generated["actions"] = _validate_action_items(
        detail["actions"],
        template,
        "config.detail.actions",
        template["limits"]["maxDetailActions"],
    )
    return generated


def _validate_tabbed(config, template):
    actions = _validate_common(
        config,
        template,
        {
            "tabs",
            "selectedTab",
            "summary",
            "collection",
            "controls",
            "toolbar",
        },
        {"detail", "visual"},
    )
    if template["generatorKind"] == "operations":
        visual = config.get("visual")
        presentation = _operations_presentation(config, template)
        if visual is not None:
            _strict(visual, {"shell", "headerArt"}, set(), "config.visual")
            if visual["shell"] != "base-operations":
                raise ArchetypeError("config.visual.shell must be base-operations")
            if visual["headerArt"] not in {"base-research", "base-manufacture"}:
                raise ArchetypeError("config.visual.headerArt is not an audited department asset")
        expected_mode = "list" if presentation == "list-inspector" else "table"
        if config["collection"]["mode"] != expected_mode:
            raise ArchetypeError(
                "config.collection.mode must be " + expected_mode
                + " for " + presentation
            )
        if presentation == "list-inspector" and "detail" not in config:
            raise ArchetypeError("list-inspector requires semantic detail identity/actions")
    collection_roles = (
        _operations_column_roles(template)
        if template["generatorKind"] == "operations" else None
    )
    _validate_collection_value(
        config["collection"], template["limits"], "config.collection",
        template["generatorKind"] == "operations",
        collection_roles,
    )
    summary = config["summary"]
    if (
        not isinstance(summary, list)
        or not (0 if template["generatorKind"] == "operations" else 1)
        <= len(summary) <= template["limits"]["maxSummaryFields"]
    ):
        raise ArchetypeError("config.summary is outside the template limit")
    summary_ids = set()
    for index, field in enumerate(summary):
        label = "config.summary[" + str(index) + "]"
        _strict(field, {"id", "label", "value"}, set(), label)
        _stable_id(field["id"], label + ".id")
        _one_line(field["label"], label + ".label", 24)
        _one_line(field["value"], label + ".value", 32)
        if field["id"] in summary_ids:
            raise ArchetypeError("config.summary IDs must be unique")
        summary_ids.add(field["id"])
    tabs = config["tabs"]
    if (
        not isinstance(tabs, list)
        or not 2 <= len(tabs) <= template["limits"]["maxTabs"]
    ):
        raise ArchetypeError("config.tabs is outside the template limit")
    tab_ids = set()
    for index, tab in enumerate(tabs):
        label = "config.tabs[" + str(index) + "]"
        _strict(tab, {"id", "label"}, set(), label)
        _stable_id(tab["id"], label + ".id")
        _one_line(tab["label"], label + ".label", 24)
        if tab["id"] in tab_ids:
            raise ArchetypeError("config.tabs IDs must be unique")
        tab_ids.add(tab["id"])
    selected = config.get("selectedTab")
    if not isinstance(selected, str):
        raise ArchetypeError("config.selectedTab must name a configured tab")
    _stable_id(selected, "config.selectedTab")
    if selected not in tab_ids:
        raise ArchetypeError("config.selectedTab must name a configured tab")
    toolbar = config["toolbar"]
    if (
        not isinstance(toolbar, list)
        or len(toolbar) > template["limits"]["maxToolbarActions"]
    ):
        raise ArchetypeError("config.toolbar exceeds the template limit")
    toolbar_actions = []
    toolbar_ids = set()
    toolbar_roles = set()
    for index, action in enumerate(toolbar):
        label = "config.toolbar[" + str(index) + "]"
        _strict(action, ACTION_FIELDS, ACTION_OPTIONAL_FIELDS, label)
        _validate_optional_label_key(action, label)
        _stable_id(action["id"], label + ".id")
        _stable_id(action["action"], label + ".action")
        _one_line(action["label"], label + ".label", 24)
        tone = action.get("tone")
        if not isinstance(tone, str) or tone not in template["supportedButtonTones"]:
            raise ArchetypeError(label + ".tone is unsupported")
        if action["id"] in toolbar_ids or action["action"] in toolbar_roles:
            raise ArchetypeError(
                "config.toolbar IDs and behavior actions must be unique"
            )
        toolbar_ids.add(action["id"])
        toolbar_roles.add(action["action"])
        generated = copy.deepcopy(action)
        generated["style"] = copy.deepcopy(template["buttonToneStyles"][action["tone"]])
        toolbar_actions.append(generated)
    footer_ids = {action["id"] for action in actions}
    footer_roles = {action["action"] for action in actions}
    if footer_ids & toolbar_ids or footer_roles & toolbar_roles:
        raise ArchetypeError(
            "config toolbar and footer actions must use unique IDs and behavior actions"
        )
    controls = _validate_controls(config["controls"], template, "config.controls")
    detail = (
        _validate_tabbed_detail(config["detail"], template)
        if "detail" in config
        else None
    )
    interaction_groups = [
        ("config.actions", actions),
        ("config.toolbar", toolbar_actions),
        ("config.controls", controls),
    ]
    if detail is not None:
        interaction_groups.append(("config.detail.actions", detail["actions"]))
    _ensure_unique_interactions(interaction_groups)
    return actions, toolbar_actions, controls, detail

def _validate_detail(config, template):
    is_operations_detail = template["id"] == "operations-detail"
    required = {"controls", "regions", "visual"} if is_operations_detail else {
        "controls", "regions"}
    actions = _validate_common(config, template, required)
    if is_operations_detail:
        visual = config["visual"]
        _strict(visual, {"shell", "headerArt"}, set(), "config.visual")
        if visual["shell"] != "base-operations":
            raise ArchetypeError("config.visual.shell must be base-operations")
        if visual["headerArt"] not in {"base-research", "base-manufacture"}:
            raise ArchetypeError("config.visual.headerArt is not an audited department asset")
    controls = _validate_controls(config["controls"], template, "config.controls")
    regions = config["regions"]
    limits = template["limits"]
    presentation = _operations_presentation(config, template) if is_operations_detail else None
    operation_roles = _operations_column_roles(template) if is_operations_detail else None
    if not isinstance(regions, list) or not 1 <= len(regions) <= limits["maxRegions"]:
        raise ArchetypeError("config.regions is outside the template limit")
    region_ids = set()
    roles = set()
    generated_region_actions = {}
    interaction_groups = [
        ("config.actions", actions),
        ("config.controls", controls),
    ]
    allowed_roles = {"primary", "secondary", "summary", "navigation"}
    for region_index, region in enumerate(regions):
        region_label = "config.regions[" + str(region_index) + "]"
        if not isinstance(region, dict):
            raise ArchetypeError(region_label + " must be an object")
        kind = region.get("kind")
        fields_by_kind = {
            "collection": {"id", "role", "kind", "label", "collection"},
            "preview": {"id", "role", "kind", "label", "contentId"},
            "fields": {"id", "role", "kind", "label", "fields"},
            "actions": {"id", "role", "kind", "label", "actions"},
        }
        if not isinstance(kind, str) or kind not in fields_by_kind:
            raise ArchetypeError(
                region_label + ".kind must be collection, preview, fields, or actions"
            )
        _strict(region, fields_by_kind[kind], set(), region_label)
        _stable_id(region["id"], region_label + ".id")
        _one_line(region["label"], region_label + ".label", 32)
        role = region.get("role")
        if not isinstance(role, str) or role not in allowed_roles:
            raise ArchetypeError(region_label + ".role is unsupported")
        expected_role = DETAIL_ROLE_BY_KIND[kind]
        role_ok = region["role"] == expected_role
        if kind == "fields":
            # Secondary fields host typed resource summaries (e.g. production
            # controls free engineers / workspace / unit facts).
            role_ok = region["role"] in {"primary", "summary", "secondary"}
        elif kind == "preview":
            role_ok = region["role"] in {"primary", "secondary"}
        if not role_ok:
            raise ArchetypeError(
                region_label + ".kind " + kind + " has an unsupported role"
            )
        if region["id"] in region_ids:
            raise ArchetypeError("config.regions IDs must be unique")
        if region["role"] in roles:
            raise ArchetypeError("config.regions roles must be unique")
        region_ids.add(region["id"])
        roles.add(region["role"])
        if kind == "collection":
            _validate_collection_value(
                region["collection"], limits, region_label + ".collection",
                column_roles=operation_roles,
            )
        elif kind == "preview":
            _stable_id(region["contentId"], region_label + ".contentId")
        elif kind == "fields":
            fields = region["fields"]
            if (
                not isinstance(fields, list)
                or not 1 <= len(fields) <= limits["maxFieldsPerRegion"]
            ):
                raise ArchetypeError(
                    region_label + ".fields is outside the template limit"
                )
            field_ids = set()
            for field_index, field in enumerate(fields):
                field_label = region_label + ".fields[" + str(field_index) + "]"
                _strict(field, {"id", "label", "value"}, set(), field_label)
                _stable_id(field["id"], field_label + ".id")
                _one_line(field["label"], field_label + ".label", 32)
                _one_line(field["value"], field_label + ".value", 48)
                if field["id"] in field_ids:
                    raise ArchetypeError(region_label + " field IDs must be unique")
                field_ids.add(field["id"])
        else:
            generated_region_actions[region["id"]] = _validate_action_items(
                region["actions"],
                template,
                region_label + ".actions",
                limits["maxRegionActions"],
            )
            interaction_groups.append(
                (region_label + ".actions", generated_region_actions[region["id"]])
            )
    if is_operations_detail:
        required_regions = {
            "controls-summary": {
                "primary": "preview", "summary": "fields", "navigation": "actions",
            },
            "requirements-summary": {
                "primary": "collection", "summary": "fields", "navigation": "actions",
            },
            "dependency-list": {"primary": "collection"},
        }[presentation]
        kinds_by_role = {region["role"]: region["kind"] for region in regions}
        for role, expected_kind in required_regions.items():
            if (
                role == "navigation"
                and expected_kind == "actions"
                and role not in kinds_by_role
                and actions
            ):
                continue
            if kinds_by_role.get(role) != expected_kind:
                raise ArchetypeError(
                    "config.presentation " + presentation + " requires "
                    + expected_kind + " region with role " + role
                )
    _ensure_unique_interactions(interaction_groups)
    return actions, generated_region_actions, controls


def _action_rects(actions, layout, split_last=False):
    footer = layout["footer"]
    width = layout["actionWidth"]
    gap = layout["actionGap"]
    inset = layout["actionInset"]
    height = 44
    y = footer["y"] + (footer["height"] - height) // 2
    if split_last and len(actions) > 1:
        left = actions[:-1]
        left_total = len(left) * width + max(0, len(left) - 1) * gap
        left_x = footer["x"] + inset
        primary_x = _right(footer) - inset - width
        if left_x + left_total > primary_x:
            raise ArchetypeError(
                "template footer cannot separate utility and primary actions"
            )
        rectangles = {
            action["id"]: _make_rect(left_x + index * (width + gap), y, width, height)
            for index, action in enumerate(left)
        }
        rectangles[actions[-1]["id"]] = _make_rect(primary_x, y, width, height)
    else:
        total = len(actions) * width + max(0, len(actions) - 1) * gap
        x = _right(footer) - inset - total
        if x < footer["x"]:
            raise ArchetypeError(
                "template footer cannot contain the configured action group"
            )
        rectangles = {
            action["id"]: _make_rect(x + index * (width + gap), y, width, height)
            for index, action in enumerate(actions)
        }
    if any(
        not _contained(rect, _right(footer), _bottom(footer))
        or rect["x"] < footer["x"]
        or rect["y"] < footer["y"]
        for rect in rectangles.values()
    ):
        raise ArchetypeError("template footer action escaped its footer")
    return rectangles


def _ensure_text_fits(text, rect, authored, label):
    available = rect["width"] - 2 * authored["cellInlineInset"]
    required = len(text) * authored["textUnitWidth"]
    if available <= 0 or required > available:
        raise ArchetypeError(label + " cannot fit its semantic copy")


def _grid_label_line_count(text, available_width, text_unit_width):
    line_count = 1
    used_width = 0
    for word in text.split(" "):
        word_width = len(word) * text_unit_width
        if word_width > available_width:
            return None
        if used_width == 0:
            used_width = word_width
        elif used_width + text_unit_width + word_width <= available_width:
            used_width += text_unit_width + word_width
        else:
            line_count += 1
            used_width = word_width
    return line_count


def _ensure_grid_label_fits(text, rect, authored, label):
    line_height = authored["gridLabelLineHeight"]
    available_lines = min(authored["gridLabelMaxLines"], rect["height"] // line_height)
    line_count = _grid_label_line_count(text, rect["width"], authored["textUnitWidth"])
    if available_lines < 1 or line_count is None or line_count > available_lines:
        raise ArchetypeError(label + " grid label cannot fit its semantic copy")


def _ensure_action_copy_fits(actions, rectangles, authored, label):
    for action in actions:
        _ensure_text_fits(
            action["label"],
            rectangles[action["id"]],
            authored,
            label + "." + action["id"],
        )


def _control_rects(controls, authored, bar, name, right_limit=None):
    if not controls:
        return {}
    gap = authored["controlGap"]
    inset = authored["controlInset"]
    widths = [
        authored["controlActionWidth"]
        if control["kind"] == "action"
        else authored["controlWidth"]
        for control in controls
    ]
    total = sum(widths) + max(0, len(controls) - 1) * gap
    x = bar["x"] + inset
    limit = _right(bar) - inset if right_limit is None else right_limit
    if x + total > limit:
        raise ArchetypeError(name + " controls exceed their template bar")
    y = bar["y"] + (bar["height"] - 44) // 2
    rectangles = {}
    cursor = x
    for control, width in zip(controls, widths):
        rectangles[control["id"]] = _make_rect(cursor, y, width, 44)
        cursor += width + gap
    if any(not _contained_by(rect, bar) for rect in rectangles.values()):
        raise ArchetypeError(name + " control escaped its template bar")
    for control in controls:
        rect = rectangles[control["id"]]
        if control["kind"] == "select":
            for option in control["options"]:
                _ensure_text_fits(
                    control["label"] + ": " + option["label"],
                    rect,
                    authored,
                    name + ".control." + control["id"] + "." + option["id"],
                )
        elif control["kind"] == "text-input":
            visible_value = control["value"] or control["placeholder"]
            _ensure_text_fits(
                control["label"] + ": " + visible_value,
                rect,
                authored,
                name + ".control." + control["id"],
            )
        else:
            _ensure_text_fits(
                control["label"],
                rect,
                authored,
                name + ".control." + control["id"],
            )
    return rectangles

def _control_parts(controls, rectangles, stepper_labels=False):
    parts = {}
    for control in controls:
        if control["kind"] != "stepper":
            continue
        control_id = control["id"]
        rect = rectangles[control_id]
        if rect["width"] < 88:
            raise ArchetypeError("stepper control is too narrow for +/- targets")
        parts[control_id] = {
            "decrement": _make_rect(rect["x"], rect["y"], 44, 44),
            "value": _make_rect(
                rect["x"] + 44,
                rect["y"],
                rect["width"] - 88,
                44,
            ),
            "increment": _make_rect(
                _right(rect) - 44, rect["y"], 44, 44
            ),
        }
        if stepper_labels:
            # Visible localized caption strip directly below the stepper so
            # the value cell stays numeric-only (external review R07). Only
            # the R&D operations kinds reserve this strip; the bar layouts of
            # other archetypes have no room below the controls.
            parts[control_id]["label"] = _make_rect(
                rect["x"],
                rect["y"] + rect["height"] + 2,
                rect["width"],
                14,
            )
    return parts


def _control_part_paths(controls, stepper_labels=False):
    paths = ["control." + control["id"] for control in controls]
    for control in controls:
        if control["kind"] == "stepper":
            base = "control." + control["id"]
            paths.extend((base + ".decrement", base + ".value", base + ".increment"))
            if stepper_labels:
                paths.append(base + ".label")
    return paths
def _weighted_rects(parent, columns, gap, roles):
    if len(columns) < 1:
        raise ArchetypeError("weighted rectangle group requires columns")
    available = parent["width"] - gap * (len(columns) - 1)
    minimum = sum(roles[column["contentRole"]]["minWidth"] for column in columns)
    if available < minimum:
        raise ArchetypeError("operations collection columns cannot satisfy profile minimum widths")
    weights = [roles[column["contentRole"]]["weight"] for column in columns]
    extra = available - minimum
    weight_total = sum(weights)
    widths = [
        roles[column["contentRole"]]["minWidth"] + extra * weight // weight_total
        for column, weight in zip(columns, weights)
    ]
    remainder = available - sum(widths)
    for index in range(remainder):
        widths[index % len(widths)] += 1
    out = []
    x = parent["x"]
    for column, width in zip(columns, widths):
        out.append(_make_rect(x, parent["y"], width, parent["height"], column["id"]))
        x += width + gap
    return out
def _collection_column_rects(parent, columns, gap, roles):
    if roles is None:
        return _equal_rects(parent, len(columns), gap, [column["id"] for column in columns])
    return _weighted_rects(parent, columns, gap, roles)


def _collection_column_cell(rect, column, roles):
    cell = _named_rect(rect)
    if roles is not None:
        policy = roles[column["contentRole"]]
        cell.update({
            "contentRole": column["contentRole"],
            "font": policy["font"],
            "align": policy["align"],
            "minWidth": policy["minWidth"],
            "weight": policy["weight"],
        })
    return cell






def _equal_rects(parent, count, gap, ids):
    if count <= 0 or len(ids) != count:
        raise ArchetypeError("equal rectangle group requires matching stable IDs")
    available = parent["width"] - gap * (count - 1)
    width = available // count
    remainder = available - width * count
    out = []
    x = parent["x"]
    for index in range(count):
        item_width = width + (remainder if index == count - 1 else 0)
        out.append(_make_rect(x, parent["y"], item_width, parent["height"], ids[index]))
        x += item_width + gap
    return out


def _scroll_metrics(
    total_items,
    visible_capacity,
    total_units,
    visible_units,
    items_per_unit,
    track,
    minimum,
):
    overflow = total_units > visible_units
    if overflow:
        thumb_height = max(minimum, track["height"] * visible_units // total_units)
        thumb_height = min(track["height"], thumb_height)
        if thumb_height >= track["height"]:
            # Degenerate one-row viewports (compact docked layouts) still need
            # working proportional scrolling: yield half the track as thumb.
            thumb_height = max(1, track["height"] // 2)
        if thumb_height >= track["height"]:
            raise ArchetypeError(
                "overflowing collection requires positive scrollbar thumb travel"
            )
    else:
        thumb_height = track["height"]
    thumb = _make_rect(track["x"], track["y"], track["width"], thumb_height)
    hit_width = max(44, track["width"])
    track_hit_target = _make_rect(
        _right(track) - hit_width,
        track["y"],
        hit_width,
        track["height"],
    )
    thumb_hit_target = _make_rect(
        _right(thumb) - hit_width,
        thumb["y"],
        hit_width,
        max(44, thumb["height"]),
    )
    return {
        "totalItems": total_items,
        "visibleCapacity": visible_capacity,
        "totalUnits": total_units,
        "visibleUnits": visible_units,
        "itemsPerUnit": items_per_unit,
        "scrollUnit": "row" if items_per_unit > 1 else "record",
        "overflow": overflow,
        "scrollSteps": max(0, total_units - visible_units),
        "track": copy.deepcopy(track),
        "thumb": thumb,
        "trackHitTarget": track_hit_target,
        "thumbHitTarget": thumb_hit_target,
        "thumbTravelPx": track["height"] - thumb_height,
    }


def _collection_parts(collection, template, prefix):
    """Return shared collection geometry parts for a given prefix.

    prefix is the dotted prefix for the collection, e.g. "" for standalone,
    "collection" for tabbed, or "region.<id>.collection" for detail.
    Includes scroll track/thumb plus column/row or tile parts.
    """
    base = (prefix + ".") if prefix else ""
    parts = [base + "scroll.track", base + "scroll.thumb"]
    if collection["mode"] in {"list", "table"}:
        parts.extend(
            base + "column." + column["id"] for column in collection["columns"]
        )
        visible_rows = (
            min(
                template["layouts"]["wide"]["visibleRows"],
                template["layouts"]["compact"]["visibleRows"],
            )
            if template.get("generatorKind") == "operations"
            or template.get("id") == "operations-detail"
            else max(
                template["layouts"]["wide"]["visibleRows"],
                template["layouts"]["compact"]["visibleRows"],
            )
        )
        parts.extend(base + "row-slot." + str(index + 1) for index in range(visible_rows))
    else:
        max_slots = max(
            template["layouts"]["wide"]["gridColumns"]
            * template["layouts"]["wide"]["gridRows"],
            template["layouts"]["compact"]["gridColumns"]
            * template["layouts"]["compact"]["gridRows"],
        )
        parts.extend(base + "tile-slot." + str(index + 1) for index in range(max_slots))
        parts.extend(
            base + "tile-slot." + str(index + 1) + ".label"
            for index in range(max_slots)
        )
    return parts


def _base_contract(config, template, source_name, template_name, actions):
    form = {
        "id": config["id"],
        "familyId": config["familyId"],
        "archetype": config["archetype"],
        "source": source_name,
        "actions": actions,
    }
    if "presentation" in config:
        form["presentation"] = config["presentation"]
    if "contentRole" in config:
        form["contentRole"] = config["contentRole"]
    if "state" in config:
        form["state"] = config["state"]
    if "labelKeys" in config:
        form["labelKeys"] = copy.deepcopy(config["labelKeys"])
    out = {
        "schema": 1,
        "version": config["version"],
        "form": form,
        "copy": {"title": config["title"]},
        "style": copy.deepcopy(template["style"]),
        "layouts": {},
        "motion": copy.deepcopy(template["motion"]),
        "provenance": {
            "template": template_name,
            "templateVersion": template["version"],
            "generatorKind": template["generatorKind"],
        },
    }
    profile = template.get("_operationsProfile")
    if profile is not None:
        profile_data = copy.deepcopy(profile["data"])
        profile_data["theme"]["resolvedTokens"] = copy.deepcopy(profile["resolvedTokens"])
        profile_data["resolvedActionTones"] = copy.deepcopy(profile["resolvedTones"])
        profile_data["resolvedColumnRoles"] = copy.deepcopy(profile["resolvedRoles"])
        out["profile"] = profile_data
        out["provenance"]["profile"] = profile["path"]
        out["provenance"]["profileVersion"] = profile_data["version"]
        out["provenance"]["themeSource"] = profile_data["theme"]["source"]
        out["provenance"]["themeSourceHash"] = profile_data["theme"]["sourceHash"]
    return out


def build_extended_contract(config, template, source_name, template_name):
    """Validate and build one non-confirmation contract."""
    # The strict per-kind template gate (declared layout fields, window
    # containment, 44px floors) runs on the prepared operations template
    # before any synthesis; without it rogue authored fields (an invented
    # topBar, private chrome geometry) silently pass through to the contract.
    template, profile = _prepare_operations_template(template)
    if template.get("generatorKind") == "operations" or template.get("id") == "operations-detail":
        _validate_template(template)
    kind = template["generatorKind"]
    if kind == "operations":
        return _build_operations(config, template, source_name, template_name)
    if kind == "collection":
        return _build_collection(config, template, source_name, template_name)
    if kind == "selection":
        return _build_selection(config, template, source_name, template_name)
    if kind == "tabbed":
        return _build_tabbed(config, template, source_name, template_name)
    if kind == "detail":
        return _build_detail_regions(config, template, source_name, template_name)
    raise ArchetypeError("unsupported template.generatorKind: " + str(kind))

def _cpp_part_name(path):
    name = re.sub(r"[^A-Za-z0-9_]", "_", path)
    if name and name[0].isdigit():
        name = "_" + name
    return name


def _resolve_part_rect(node, segments):
    if not segments:
        if not isinstance(node, dict):
            return None
        if "rect" in node:
            return _resolve_part_rect(node["rect"], [])
        if "panel" in node:
            return _resolve_part_rect(node["panel"], [])
        if "row" in node:
            return _resolve_part_rect(node["row"], [])
        if set(("x", "y", "width", "height")) <= set(node):
            return {key: node[key] for key in ("x", "y", "width", "height")}
        return None
    if isinstance(node, dict):
        segment = segments[0]
        if segment == "collection" and len(segments) == 1 and "content" in node:
            return _resolve_part_rect(node["content"], segments[1:])
        if segment == "control" and len(segments) > 1:
            wanted = segments[1]
            if len(segments) == 2 and wanted in node.get("controls", {}):
                return _resolve_part_rect(node["controls"][wanted], [])
            if wanted in node.get("controlParts", {}):
                return _resolve_part_rect(
                    node["controlParts"][wanted], segments[2:]
                )
        if (segment == "note" and isinstance(node.get("note"), dict)
                and not node["note"].get("visible", False) and "panel" in node):
            return _resolve_part_rect(node["panel"], [])
        if segment in node:
            return _resolve_part_rect(node[segment], segments[1:])
        if "collection" in node and segment != "collection":
            resolved = _resolve_part_rect(node["collection"], segments)
            if resolved is not None:
                return resolved
        if "content" in node and isinstance(node["content"], dict):
            resolved = _resolve_part_rect(node["content"], segments)
            if resolved is not None:
                return resolved
        if segment == "detail" and "detail" in node:
            return _resolve_part_rect(node["detail"], segments[1:])
        for slot_key, slot_prefix in (
            ("actionSlots", "action-slot-"),
            ("rowSlots", "row-slot-"),
            ("tileSlots", "tile-slot-"),
        ):
            if segment.startswith(slot_prefix) and slot_key in node:
                for slot in node[slot_key]:
                    if isinstance(slot, dict) and slot.get("id") == segment:
                        return _resolve_part_rect(slot, segments[1:])
                if node[slot_key] and not node.get("_strictSlots"):
                    return _resolve_part_rect(node[slot_key][-1], [])
        if segment.startswith("line-") and isinstance(node.get("lines"), list):
            try:
                index = int(segment[5:]) - 1
            except ValueError:
                index = -1
            if 0 <= index < len(node["lines"]):
                return _resolve_part_rect(node["lines"][index], segments[1:])
        maps = {
            "action": "actions",
            "tab": "tabs",
            "toolbar": "toolbar",
            "control": "controls",
            "summary": "summary",
            "metric": "metrics",
            "region": "regions",
            "field": "fields",
            "column": "columnHeaders",
            "row-slot": "rowSlots",
            "tile-slot": "tileSlots",
            "action-slot": "actionSlots",
        }
        collection_key = maps.get(segment)
        if collection_key in node:
            values = node[collection_key]
            wanted = segments[1] if len(segments) > 1 else None
            if isinstance(values, dict) and wanted in values:
                return _resolve_part_rect(values[wanted], segments[2:])
            if isinstance(values, list):
                candidates = {wanted, segment + "-" + str(wanted)}
                for value in values:
                    if isinstance(value, dict) and value.get("id") in candidates:
                        return _resolve_part_rect(value, segments[2:])
                if (
                    values
                    and segment in {"row-slot", "tile-slot", "action-slot"}
                    and str(wanted).isdigit()
                    and not node.get("_strictSlots")
                ):
                    return _resolve_part_rect(values[-1], segments[2:])
        if node.get("id") == segment:
            return _resolve_part_rect(node, segments[1:])
    return None


def _strip_internal_markers(node):
    if isinstance(node, dict):
        node.pop("_strictSlots", None)
        for value in node.values():
            _strip_internal_markers(value)
    elif isinstance(node, list):
        for value in node:
            _strip_internal_markers(value)

def _finalize_parts(contract):
    semantic_parts = contract.pop("_partPaths", contract.get("parts", []))
    names = [_cpp_part_name(path) for path in semantic_parts]
    if len(set(names)) != len(names):
        raise ArchetypeError("parts must map to unique C++ identifiers")
    contract["parts"] = semantic_parts
    for layout in contract["layouts"].values():
        part_rects = {}
        for semantic in semantic_parts:
            rect = _resolve_part_rect(layout, semantic.split("."))
            if rect is None:
                raise ArchetypeError("layout is missing part geometry: " + semantic)
            part_rects[semantic] = rect
        layout["partRects"] = part_rects
        _strip_internal_markers(layout)
    return contract

def _build_collection_fragment(collection, authored, viewport, name, column_roles=None):
    fragment = {}
    mode = collection["mode"]
    total = len(collection["items"])
    track_y = viewport["y"]
    track_height = viewport["height"]
    if mode in {"list", "table"}:
        track_y += authored["headerHeight"]
        track_height = authored["visibleRows"] * authored["rowHeight"]
        if authored["headerHeight"] + track_height > viewport["height"]:
            raise ArchetypeError(name + " collection rows exceed the template viewport")
    track = _make_rect(
        _right(viewport) - authored["scrollbarWidth"],
        track_y,
        authored["scrollbarWidth"],
        track_height,
    )
    hit_rail_width = max(44, authored["scrollbarWidth"])
    data_width = viewport["width"] - authored["scrollbarGap"] - hit_rail_width
    if data_width <= 0:
        raise ArchetypeError(name + " scrollbar rail leaves no collection width")
    if mode in {"list", "table"}:
        columns = collection["columns"]
        layout_roles = column_roles
        if column_roles is not None:
            layout_roles = copy.deepcopy(column_roles)
            for index, column in enumerate(columns):
                candidates = [column["label"]]
                candidates.extend(item["values"][index] for item in collection["items"])
                semantic_minimum = (
                    max(len(value) for value in candidates)
                    * authored["textUnitWidth"]
                    + 2 * authored["cellInlineInset"]
                )
                role = column["contentRole"]
                layout_roles[role]["minWidth"] = max(
                    layout_roles[role]["minWidth"], semantic_minimum
                )
        header = _make_rect(
            viewport["x"], viewport["y"], data_width, authored["headerHeight"]
        )
        header_rects = _collection_column_rects(header, columns, 1, layout_roles)
        header_cells = [
            _collection_column_cell(rect, column, layout_roles)
            for rect, column in zip(header_rects, columns)
        ]
        for column_index, column in enumerate(columns):
            available_text_width = (
                header_cells[column_index]["rect"]["width"]
                - 2 * authored["cellInlineInset"]
            )
            candidates = [column["label"]]
            candidates.extend(
                item["values"][column_index] for item in collection["items"]
            )
            required_text_width = max(
                len(value) * authored["textUnitWidth"] for value in candidates
            )
            if required_text_width > available_text_width:
                raise ArchetypeError(
                    name
                    + " collection column "
                    + column["id"]
                    + " cannot fit its semantic copy"
                )
        fragment["columnHeaders"] = header_cells
        fragment["rowSlots"] = []
        for row_index in range(authored["visibleRows"]):
            row = _make_rect(
                viewport["x"],
                _bottom(header) + row_index * authored["rowHeight"],
                data_width,
                authored["rowHeight"],
            )
            row_rects = _collection_column_rects(row, columns, 1, layout_roles)
            cells = [
                _collection_column_cell(rect, column, layout_roles)
                for rect, column in zip(row_rects, columns)
            ]
            fragment["rowSlots"].append(
                {
                    "id": "row-slot-" + str(row_index + 1),
                    "rect": row,
                    "cells": cells,
                }
            )
        capacity = authored["visibleRows"]
        total_units = total
        visible_units = authored["visibleRows"]
        items_per_unit = 1
    else:
        columns = authored["gridColumns"]
        rows = authored["gridRows"]
        gap = authored["gridGap"]
        tile_width = (data_width - gap * (columns - 1)) // columns
        tile_height = (viewport["height"] - gap * (rows - 1)) // rows
        if tile_width < 44 or tile_height < 44:
            raise ArchetypeError(
                name + " grid template violates the 44px target minimum"
            )
        label_height = authored["gridLabelLineHeight"] * authored["gridLabelMaxLines"]
        if label_height > tile_height:
            raise ArchetypeError(name + " grid label band exceeds its tile")
        fragment["tileSlots"] = []
        for row_index in range(rows):
            for column_index in range(columns):
                slot_index = row_index * columns + column_index + 1
                tile = _make_rect(
                    viewport["x"] + column_index * (tile_width + gap),
                    viewport["y"] + row_index * (tile_height + gap),
                    tile_width,
                    tile_height,
                )
                label_rect = _make_rect(
                    tile["x"] + authored["cellInlineInset"],
                    _bottom(tile) - label_height,
                    tile["width"] - 2 * authored["cellInlineInset"],
                    label_height,
                )
                if label_rect["width"] <= 0:
                    raise ArchetypeError(name + " grid label has no inline width")
                fragment["tileSlots"].append(
                    {
                        "id": "tile-slot-" + str(slot_index),
                        "rect": tile,
                        "label": label_rect,
                    }
                )
        fixture_label_rect = fragment["tileSlots"][0]["label"]
        for item in collection["items"]:
            _ensure_grid_label_fits(
                item["label"],
                fixture_label_rect,
                authored,
                name + ".collection.item." + item["id"],
            )
        fragment["gridLabelPolicy"] = {
            "wrap": "word",
            "lineHeight": authored["gridLabelLineHeight"],
            "maxLines": authored["gridLabelMaxLines"],
        }
        capacity = columns * rows
        total_units = (total + columns - 1) // columns
        visible_units = rows
        items_per_unit = columns
    metrics = _scroll_metrics(
        total,
        capacity,
        total_units,
        visible_units,
        items_per_unit,
        track,
        authored["minThumbHeight"],
    )
    if column_roles is not None:
        fragment["_strictSlots"] = True
        fragment["count"] = min(total, capacity)
    fragment["scroll"] = {
        "track": metrics["track"],
        "thumb": metrics["thumb"],
    }
    return fragment, metrics


def _build_collection(config, template, source_name, template_name):
    actions, controls = _validate_collection(config, template)
    out = _base_contract(config, template, source_name, template_name, actions)
    collection = copy.deepcopy(config["collection"])
    out["form"]["collection"] = collection
    out["form"]["controls"] = controls
    out["copy"]["collection"] = copy.deepcopy(config["collection"])
    out["copy"]["controls"] = copy.deepcopy(config["controls"])
    out["collectionMetrics"] = {}
    for name in ("wide", "compact"):
        authored = template["layouts"][name]
        action_rects = _action_rects(actions, authored)
        _ensure_action_copy_fits(actions, action_rects, authored, name + ".action")
        _ensure_text_fits(config["title"], authored["title"], authored, name + ".title")
        control_rects = _control_rects(controls, authored, authored["controlBar"], name)
        layout = {
            "designWidth": authored["designWidth"],
            "designHeight": authored["designHeight"],
            "window": copy.deepcopy(authored["window"]),
            "title": copy.deepcopy(authored["title"]),
            "controlBar": copy.deepcopy(authored["controlBar"]),
            "controls": control_rects,
            "controlParts": _control_parts(controls, control_rects),
            "viewport": copy.deepcopy(authored["viewport"]),
            "footer": copy.deepcopy(authored["footer"]),
            "actions": action_rects,
        }
        collection_fragment, metrics = _build_collection_fragment(
            collection, authored, authored["viewport"], name
        )
        layout.update(collection_fragment)
        out["collectionMetrics"][name] = metrics
        out["layouts"][name] = layout
    out["_partPaths"] = [
        "window",
        "title",
        "controlBar",
        "viewport",
        "scroll.track",
        "scroll.thumb",
        "footer",
    ]
    out["_partPaths"].extend(_control_part_paths(controls))
    out["actions"] = ["action." + action["id"] for action in actions]
    out["_partPaths"].extend("action." + action["id"] for action in actions)
    # reuse shared collection helper (empty prefix -> standalone naming)
    # helper already includes scroll.track/thumb, so extend only column/slot parts
    # to avoid duplicating scroll entries, slice helper output
    collection_parts = _collection_parts(collection, template, "")
    # collection_parts[0:2] are scroll.track/thumb already in base list; skip them
    out["_partPaths"].extend(collection_parts[2:])
    return _finalize_parts(out)


def _validate_selection_rows(collection, limits, label):
    if not isinstance(collection, dict):
        raise ArchetypeError(label + " must be an object")
    _strict(collection, {"mode", "selectionRole", "columns", "items"}, set(), label)
    if collection.get("mode") != "list":
        raise ArchetypeError(label + ".mode must be list for a selection list")
    _stable_id(collection["selectionRole"], label + ".selectionRole")
    columns = collection["columns"]
    if (
        not isinstance(columns, list)
        or len(columns) != 1
        or not isinstance(columns[0], dict)
    ):
        raise ArchetypeError(label + ".columns must hold exactly one label column")
    _strict(columns[0], {"id", "label"}, set(), label + ".columns[0]")
    _stable_id(columns[0]["id"], label + ".columns[0].id")
    _one_line(columns[0]["label"], label + ".columns[0].label", 48)
    items = collection["items"]
    if not isinstance(items, list) or not 1 <= len(items) <= limits["maxItems"]:
        raise ArchetypeError(label + ".items exceeds the template item limit")
    seen = set()
    for index, item in enumerate(items):
        item_label = label + ".items[" + str(index) + "]"
        _strict(item, {"id", "label", "enabled"}, set(), item_label)
        _stable_id(item["id"], item_label + ".id")
        if item["id"] in seen:
            raise ArchetypeError(label + " item ids must be unique")
        seen.add(item["id"])
        _one_line(item["label"], item_label + ".label", limits["maxCellCharacters"])
        if not isinstance(item["enabled"], bool):
            raise ArchetypeError(item_label + ".enabled must be a boolean")


def _validate_selection(config, template):
    actions = _validate_common(config, template, {"collection"}, {"protocol"})
    _validate_selection_rows(config["collection"], template["limits"], "config.collection")
    for action in actions:
        if action["action"] not in {"cancel", "close", "back"}:
            raise ArchetypeError(
                "config.actions behavior must be a close action for a selection list"
            )
    protocol = config.get("protocol")
    if protocol is not None:
        _strict(
            protocol,
            {"authority", "record", "code", "revision", "effectiveDate"},
            set(),
            "config.protocol",
        )
        _one_line(protocol["authority"], "config.protocol.authority", 64)
        _one_line(protocol["record"], "config.protocol.record", 64)
        _one_line(protocol["code"], "config.protocol.code", 16)
        _one_line(protocol["revision"], "config.protocol.revision", 16)
        _one_line(protocol["effectiveDate"], "config.protocol.effectiveDate", 16)
    return actions


def _build_selection_fragment(collection, authored, viewport, name):
    total = len(collection["items"])
    track = _make_rect(
        _right(viewport) - authored["scrollbarWidth"],
        viewport["y"],
        authored["scrollbarWidth"],
        viewport["height"],
    )
    row_slots = []
    for row_index in range(authored["visibleRows"]):
        row_slots.append(
            _named_rect(
                _make_rect(
                    viewport["x"],
                    viewport["y"] + row_index * authored["rowHeight"],
                    viewport["width"],
                    authored["rowHeight"],
                    stable_id="row-slot-" + str(row_index + 1),
                )
            )
        )
    metrics = _scroll_metrics(
        total,
        authored["visibleRows"],
        total,
        authored["visibleRows"],
        1,
        track,
        authored["minThumbHeight"],
    )
    return {"rowSlots": row_slots}, metrics


def _build_selection(config, template, source_name, template_name):
    actions = _validate_selection(config, template)
    out = _base_contract(config, template, source_name, template_name, actions)
    out["presentation"] = {"density": "standard", "scaleNumerator": 1, "scaleDenominator": 1}
    collection = copy.deepcopy(config["collection"])
    out["form"]["collection"] = collection
    out["form"]["buttons"] = [
        {
            "id": action["id"],
            "label": action["label"],
            "tone": action["tone"],
            "action": action["action"],
            "style": copy.deepcopy(action["style"]),
        }
        for action in actions
    ]
    out["form"]["visibleButtons"] = [action["id"] for action in actions]
    protocol = config.get("protocol")
    if protocol is not None:
        out["copy"]["protocol"] = (
            protocol["authority"] + " · " + protocol["record"] + " " + protocol["code"]
        )
        for layout_name in ("wide", "compact"):
            authored_layout = template["layouts"][layout_name]
            _ensure_text_fits(
                out["copy"]["protocol"],
                authored_layout["status"],
                authored_layout,
                layout_name + ".protocol",
            )
    out["copy"]["rows"] = [
        {"label": item["label"], "enabled": item["enabled"]}
        for item in collection["items"]
    ]
    out["scrollMetrics"] = {}
    for name in ("wide", "compact"):
        authored = template["layouts"][name]
        if authored["list"]["height"] != authored["visibleRows"] * authored["rowHeight"]:
            raise ArchetypeError(
                name + " list height must equal visibleRows * rowHeight"
            )
        action_rects = _action_rects(actions, authored)
        _ensure_action_copy_fits(actions, action_rects, authored, name + ".action")
        _ensure_text_fits(config["title"], authored["title"], authored, name + ".title")
        for item in collection["items"]:
            _ensure_text_fits(
                item["label"],
                authored["list"],
                authored,
                name + ".collection.item." + item["id"],
            )
        fragment, metrics = _build_selection_fragment(
            collection, authored, authored["list"], name
        )
        layout = {
            "designWidth": authored["designWidth"],
            "designHeight": authored["designHeight"],
            "window": copy.deepcopy(authored["window"]),
            "status": copy.deepcopy(authored["status"]),
            "title": copy.deepcopy(authored["title"]),
            "list": copy.deepcopy(authored["list"]),
            "footer": copy.deepcopy(authored["footer"]),
            "actionCancel": copy.deepcopy(action_rects[actions[0]["id"]]),
            "rowHeight": authored["rowHeight"],
            "visibleRows": authored["visibleRows"],
            "rowSlots": fragment["rowSlots"],
            "buttons": {
                action["id"]: copy.deepcopy(action_rects[action["id"]])
                for action in actions
            },
            "scrollbarWidth": authored["scrollbarWidth"],
            "minThumbHeight": authored["minThumbHeight"],
        }
        if layout["buttons"][actions[0]["id"]] != layout["actionCancel"]:
            raise ArchetypeError(name + " cancel action left the button slot")
        out["scrollMetrics"][name] = metrics
        out["layouts"][name] = layout
    out["_partPaths"] = [
        "window",
        "status",
        "title",
        "list",
        "footer",
        "actionCancel",
    ]
    out["actions"] = ["actionCancel"]
    return _finalize_parts(out)


def _build_context_dock_fragment(detail, authored, dock):
    """A+C docked context composition: identity line, compact metrics and
    trailing 44px action slots inside a full-width dock above the footer.
    Emits the same fragment shape as the inspector builder so painters and
    native owners keep one contract; the inspector-only note is absent."""
    inner = _make_rect(
        dock["x"] + 12,
        dock["y"] + 4,
        dock["width"] - 24,
        dock["height"] - 8,
    )
    name = "contextDock"
    action_gap = 8
    action_width = 140 if dock["width"] > 900 else 112
    action_height = min(44, inner["height"])
    actions_total = (
        len(detail["actions"]) * action_width
        + max(0, len(detail["actions"]) - 1) * action_gap
    )
    if actions_total > inner["width"]:
        raise ArchetypeError(name + " cannot host its actions in one row")
    actions_x = _right(inner) - actions_total
    action_y = dock["y"] + (dock["height"] - 44) // 2
    identity_width = 260 if dock["width"] > 900 else 160
    metrics_x = inner["x"] + identity_width + 12
    metrics_avail = max(0, actions_x - 12 - metrics_x)
    count = len(detail["metrics"])
    metric_w = ((metrics_avail - 12 * (count - 1)) // count) if count else 0
    if count and metric_w < 48:
        raise ArchetypeError(name + " cannot host its metrics in one row")
    metrics = {}
    for index, metric in enumerate(detail["metrics"]):
        rect = _make_rect(
            metrics_x + index * (metric_w + 12),
            inner["y"],
            metric_w,
            inner["height"],
        )
        label_height = rect["height"] // 2
        metrics[metric["id"]] = {
            "rect": rect,
            "label": _make_rect(
                rect["x"], rect["y"], rect["width"], label_height
            ),
            "value": _make_rect(
                rect["x"],
                rect["y"] + label_height,
                rect["width"],
                rect["height"] - label_height,
            ),
        }
    top_height = max(12, inner["height"] * 3 // 10)
    label_rect = _make_rect(
        inner["x"], inner["y"], identity_width, top_height
    )
    subtitle_rect = _make_rect(
        inner["x"] + identity_width // 2,
        inner["y"],
        identity_width - identity_width // 2,
        top_height,
    )
    identity_rect = _make_rect(
        inner["x"],
        inner["y"] + top_height,
        identity_width,
        inner["height"] - top_height,
    )
    identity_title_height = identity_rect["height"] // 2
    if identity_title_height <= 0 or identity_title_height >= identity_rect["height"]:
        raise ArchetypeError(name + " identity cannot contain two text lines")
    identity_title = _make_rect(
        identity_rect["x"],
        identity_rect["y"],
        identity_rect["width"],
        identity_title_height,
    )
    identity_subtitle = _make_rect(
        identity_rect["x"],
        _bottom(identity_title),
        identity_rect["width"],
        identity_rect["height"] - identity_title_height,
    )
    return {
        "id": detail["id"],
        "panel": {
            "x": dock["x"],
            "y": dock["y"],
            "width": dock["width"],
            "height": dock["height"],
        },
        "label": label_rect,
        "identity": {
            "rect": identity_rect,
            "title": identity_title,
            "subtitle": identity_subtitle,
        },
        "metrics": metrics,
        "note": {"visible": False},
        "actions": [
            {
                "id": action["id"],
                "rect": _make_rect(
                    actions_x + index * (action_width + action_gap),
                    action_y,
                    action_width,
                    action_height,
                ),
            }
            for index, action in enumerate(detail["actions"])
        ],
    }


def _build_tabbed_detail_fragment(detail, authored, name):
    panel = copy.deepcopy(authored["detailPanel"])
    inset = authored["detailInset"]
    inner = _make_rect(
        panel["x"] + inset,
        panel["y"] + inset,
        panel["width"] - 2 * inset,
        panel["height"] - 2 * inset,
    )
    if inner["width"] <= 0 or inner["height"] <= 0:
        raise ArchetypeError(name + " detail inset leaves no content")

    cursor_y = inner["y"]
    detail_label_rect = _make_rect(
        inner["x"], cursor_y, inner["width"], authored["detailLabelHeight"]
    )
    cursor_y = _bottom(detail_label_rect)
    identity_rect = _make_rect(
        inner["x"], cursor_y, inner["width"], authored["detailIdentityHeight"]
    )
    identity_title_height = identity_rect["height"] // 2
    if identity_title_height <= 0 or identity_title_height >= identity_rect["height"]:
        raise ArchetypeError(name + " detail identity cannot contain two text lines")
    identity_title = _make_rect(
        identity_rect["x"],
        identity_rect["y"],
        identity_rect["width"],
        identity_title_height,
    )
    identity_subtitle = _make_rect(
        identity_rect["x"],
        _bottom(identity_title),
        identity_rect["width"],
        identity_rect["height"] - identity_title_height,
    )
    cursor_y = _bottom(identity_rect)
    _ensure_text_fits(
        detail["label"], detail_label_rect, authored, name + ".detail.label"
    )
    _ensure_text_fits(detail["title"], identity_title, authored, name + ".detail.title")
    _ensure_text_fits(
        detail["subtitle"], identity_subtitle, authored, name + ".detail.subtitle"
    )

    columns = authored["detailMetricColumns"]
    metric_rows = (len(detail["metrics"]) + columns - 1) // columns
    metrics = {}
    for row_index in range(metric_rows):
        row_metrics = detail["metrics"][
            row_index * columns : min((row_index + 1) * columns, len(detail["metrics"]))
        ]
        row = _make_rect(
            inner["x"],
            cursor_y,
            inner["width"],
            authored["detailMetricRowHeight"],
        )
        cells = _equal_rects(
            row,
            len(row_metrics),
            1,
            [metric["id"] for metric in row_metrics],
        )
        for metric, cell in zip(row_metrics, cells):
            rect = {key: value for key, value in cell.items() if key != "id"}
            label_height = rect["height"] // 2
            if label_height <= 0 or label_height >= rect["height"]:
                raise ArchetypeError(name + " detail metric cannot contain two lines")
            metric_label_rect = _make_rect(
                rect["x"], rect["y"], rect["width"], label_height
            )
            value_rect = _make_rect(
                rect["x"],
                _bottom(metric_label_rect),
                rect["width"],
                rect["height"] - label_height,
            )
            _ensure_text_fits(
                metric["label"],
                metric_label_rect,
                authored,
                name + ".detail.metric." + metric["id"] + ".label",
            )
            _ensure_text_fits(
                metric["value"],
                value_rect,
                authored,
                name + ".detail.metric." + metric["id"] + ".value",
            )
            metrics[metric["id"]] = {
                "rect": rect,
                "label": metric_label_rect,
                "value": value_rect,
            }
        cursor_y = _bottom(row)

    action_bar = _make_rect(inner["x"], _bottom(inner) - 44, inner["width"], 44)
    action_rect_list = _equal_rects(
        action_bar,
        len(detail["actions"]),
        authored["detailActionGap"],
        [action["id"] for action in detail["actions"]],
    )
    action_rects = {
        action["id"]: {key: value for key, value in rect.items() if key != "id"}
        for action, rect in zip(detail["actions"], action_rect_list)
    }
    if any(rect["width"] < 44 or rect["height"] < 44 for rect in action_rects.values()):
        raise ArchetypeError(name + " detail action violates the 44px minimum")
    _ensure_action_copy_fits(
        detail["actions"], action_rects, authored, name + ".detail.action"
    )

    note = {"visible": "note" in detail and bool(authored["detailNoteVisible"])}
    if note["visible"]:
        note_label = _make_rect(
            inner["x"],
            cursor_y,
            inner["width"],
            authored["detailNoteLineHeight"],
        )
        cursor_y = _bottom(note_label)
        note_title = _make_rect(
            inner["x"],
            cursor_y,
            inner["width"],
            authored["detailNoteLineHeight"],
        )
        cursor_y = _bottom(note_title)
        note_lines = []
        for index, line in enumerate(detail["note"]["body"]):
            line_rect = _make_rect(
                inner["x"],
                cursor_y,
                inner["width"],
                authored["detailNoteLineHeight"],
            )
            cursor_y = _bottom(line_rect)
            _ensure_text_fits(
                line, line_rect, authored, name + ".detail.note.line-" + str(index + 1)
            )
            note_lines.append(line_rect)
        _ensure_text_fits(
            detail["note"]["label"],
            note_label,
            authored,
            name + ".detail.note.label",
        )
        _ensure_text_fits(
            detail["note"]["title"],
            note_title,
            authored,
            name + ".detail.note.title",
        )
        note.update({"label": note_label, "title": note_title, "lines": note_lines})
    if cursor_y > action_bar["y"]:
        raise ArchetypeError(name + " detail content overlaps its actions")

    return {
        "id": detail["id"],
        "panel": panel,
        "label": detail_label_rect,
        "identity": {
            "rect": identity_rect,
            "title": identity_title,
            "subtitle": identity_subtitle,
        },
        "metrics": metrics,
        "note": note,
        "actions": [
            {"id": action["id"], "rect": action_rects[action["id"]]}
            for action in detail["actions"]
        ],
    }


def _build_tabbed(config, template, source_name, template_name):
    actions, toolbar_actions, controls, detail = _validate_tabbed(config, template)
    out = _base_contract(config, template, source_name, template_name, actions)
    out["form"]["tabs"] = copy.deepcopy(config["tabs"])
    out["form"]["selectedTab"] = config["selectedTab"]
    out["form"]["summary"] = copy.deepcopy(config["summary"])
    out["form"]["collection"] = copy.deepcopy(config["collection"])
    out["form"]["controls"] = controls
    out["form"]["toolbar"] = toolbar_actions
    if "visual" in config:
        out["form"]["visual"] = copy.deepcopy(config["visual"])
    if "sharedChrome" in template:
        out["form"]["sharedChrome"] = copy.deepcopy(template["sharedChrome"])
    out["copy"]["tabs"] = {tab["id"]: tab["label"] for tab in config["tabs"]}
    out["copy"]["summary"] = copy.deepcopy(config["summary"])
    out["copy"]["collection"] = copy.deepcopy(config["collection"])
    out["copy"]["controls"] = copy.deepcopy(config["controls"])
    out["copy"]["toolbar"] = {item["id"]: item["label"] for item in config["toolbar"]}
    if detail is not None:
        out["form"]["detail"] = detail
        out["copy"]["detail"] = copy.deepcopy(config["detail"])
    out["collectionMetrics"] = {}
    tab_ids = [tab["id"] for tab in config["tabs"]]
    for name in ("wide", "compact"):
        authored = template["layouts"][name]
        tabs = _equal_rects(
            authored["tabBar"], len(tab_ids), authored["tabGap"], tab_ids
        )
        if any(tab["width"] < 44 or tab["height"] < 44 for tab in tabs):
            raise ArchetypeError(name + " tab target violates the 44px minimum")
        for configured_tab, tab_rect in zip(config["tabs"], tabs):
            _ensure_text_fits(
                configured_tab["label"],
                tab_rect,
                authored,
                name + ".tab." + configured_tab["id"],
            )
        toolbar_parent = authored["toolbarBar"]
        toolbar_width = authored["toolbarWidth"]
        toolbar_gap = authored["toolbarGap"]
        toolbar_total = (
            len(toolbar_actions) * toolbar_width
            + max(0, len(toolbar_actions) - 1) * toolbar_gap
        )
        if toolbar_total > toolbar_parent["width"]:
            raise ArchetypeError(name + " toolbar actions exceed the template bar")
        toolbar_x = _right(toolbar_parent) - toolbar_total
        toolbar_y = toolbar_parent["y"] + (toolbar_parent["height"] - 44) // 2
        toolbar_rects = {
            item["id"]: _make_rect(
                toolbar_x + index * (toolbar_width + toolbar_gap),
                toolbar_y,
                toolbar_width,
                44,
            )
            for index, item in enumerate(toolbar_actions)
        }
        if any(
            not _contained_by(rect, toolbar_parent) for rect in toolbar_rects.values()
        ):
            raise ArchetypeError(name + " toolbar action escaped its template bar")
        _ensure_action_copy_fits(
            toolbar_actions, toolbar_rects, authored, name + ".toolbar"
        )
        control_right_limit = toolbar_x
        if controls and toolbar_actions:
            control_right_limit -= authored["controlGap"]
        control_rects = _control_rects(
            controls,
            authored,
            toolbar_parent,
            name,
            control_right_limit,
        )
        summary_rects = (
            _equal_rects(
                authored["summaryBar"],
                len(config["summary"]),
                1,
                [field["id"] for field in config["summary"]],
            )
            if config["summary"]
            else []
        )
        for field, rect in zip(config["summary"], summary_rects):
            _ensure_text_fits(
                field["label"] + " " + field["value"],
                rect,
                authored,
                name + ".summary." + field["id"],
            )
        action_rects = _action_rects(
            actions, authored, template["generatorKind"] == "operations"
        )
        _ensure_action_copy_fits(actions, action_rects, authored, name + ".action")
        _ensure_text_fits(config["title"], authored["title"], authored, name + ".title")
        collection_viewport = copy.deepcopy(authored["collectionViewport"])
        presentation = (
            _operations_presentation(config, template)
            if template["generatorKind"] == "operations" else None
        )
        docked_detail = (
            template["generatorKind"] == "operations"
            and detail is not None
            and (
                presentation == "table-context"
                or (presentation == "list-inspector" and name == "compact")
            )
        )
        context_dock = None
        if detail is None:
            collection_viewport["width"] = (
                _right(authored["detailPanel"]) - collection_viewport["x"]
            )
            collection_viewport["height"] = (
                max(
                    _bottom(authored["collectionViewport"]),
                    _bottom(authored["detailPanel"]),
                )
                - collection_viewport["y"]
            )
        elif docked_detail:
            # A+C: a table-context data area spans the full body width and the
            # selected-row context actions dock above the footer; a compact
            # list-inspector has no inspector column and docks the same way.
            dock_height = authored["contextDockHeight"]
            body_x = collection_viewport["x"]
            body_right = _right(authored["detailPanel"])
            context_dock = _make_rect(
                body_x,
                authored["footer"]["y"] - 8 - dock_height,
                body_right - body_x,
                dock_height,
            )
            collection_viewport["width"] = body_right - body_x
            collection_viewport["height"] = (
                context_dock["y"] - 8 - collection_viewport["y"]
            )
            if collection_viewport["height"] < authored["headerHeight"] + authored["rowHeight"]:
                raise ArchetypeError(
                    name + " context dock leaves no full row slot below the header"
                )
        fragment_authored = authored
        if docked_detail and context_dock is not None:
            # Only whole row slots fit between the header and the dock; the
            # emitted slot capacity follows the docked data viewport.
            fit_rows = max(1, (
                collection_viewport["height"] - authored["headerHeight"]
            ) // authored["rowHeight"])
            if fit_rows < authored["visibleRows"]:
                fragment_authored = dict(authored)
                fragment_authored["visibleRows"] = fit_rows
        collection_fragment, collection_metrics = _build_collection_fragment(
            config["collection"], fragment_authored, collection_viewport, name,
            _operations_column_roles(template) if template["generatorKind"] == "operations" else None,
        )
        if docked_detail and detail is not None:
            detail_fragment = _build_context_dock_fragment(
                detail, authored, context_dock
            )
        elif detail is not None:
            detail_fragment = _build_tabbed_detail_fragment(detail, authored, name)
        else:
            detail_fragment = None
        if context_dock is None and detail_fragment is not None:
            # Undocked inspector: the dock reports the inspector action band so
            # every consumer shares one context-dock concept.
            band = [a["rect"] for a in detail_fragment["actions"]]
            context_dock = _make_rect(
                min(r["x"] for r in band) - 12,
                min(r["y"] for r in band) - 4,
                max(r["x"] + r["width"] for r in band)
                    - min(r["x"] for r in band) + 24,
                max(r["y"] + r["height"] for r in band)
                    - min(r["y"] for r in band) + 8,
            )
        generated_layout = {
            "designWidth": authored["designWidth"],
            "designHeight": authored["designHeight"],
            "window": copy.deepcopy(authored["window"]),
            "title": copy.deepcopy(authored["title"]),
            "summaryBar": copy.deepcopy(authored["summaryBar"]),
            "summary": {
                field["id"]: {key: value for key, value in rect.items() if key != "id"}
                for field, rect in zip(config["summary"], summary_rects)
            },
            "tabBar": copy.deepcopy(authored["tabBar"]),
            "collectionViewport": collection_viewport,
            "detailPanel": copy.deepcopy(
                context_dock if docked_detail else authored["detailPanel"]
            ),
            "contextDock": copy.deepcopy(context_dock),
            "collection": collection_fragment,
            "toolbarBar": copy.deepcopy(authored["toolbarBar"]),
            "controls": control_rects,
            "controlParts": _control_parts(controls, control_rects,
                stepper_labels=template["generatorKind"] == "operations"),
            "footer": copy.deepcopy(authored["footer"]),
            "tabs": {
                tab["id"]: {key: value for key, value in tab.items() if key != "id"}
                for tab in tabs
            },
            "toolbar": toolbar_rects,
            "actions": action_rects,
        }
        if template["generatorKind"] == "operations":
            for shell_part in ("screenHeader", "headerArt"):
                generated_layout[shell_part] = copy.deepcopy(authored[shell_part])
        if detail is not None:
            generated_layout["detail"] = detail_fragment
        out["layouts"][name] = generated_layout
        out["collectionMetrics"][name] = collection_metrics
    out["_partPaths"] = [
        "window",
        "title",
        "summaryBar",
        "tabBar",
        "toolbarBar",
        "collectionViewport",
        "footer",
    ]
    if template["generatorKind"] == "operations":
        out["_partPaths"][1:1] = [
            "screenHeader",
            "headerArt",
        ]
    out["_partPaths"].extend("summary." + field["id"] for field in config["summary"])
    out["_partPaths"].extend("tab." + tab_id for tab_id in tab_ids)
    out["_partPaths"].extend(_control_part_paths(controls,
        template["generatorKind"] == "operations"))
    out["_partPaths"].extend("toolbar." + item["id"] for item in toolbar_actions)
    # collection geometry - shared helper ensures list/table/grid parity with standalone and detail
    parts_template = template
    if template["generatorKind"] == "operations":
        # Docked layouts may legitimately carry fewer whole row slots than the
        # authored template; the shared part list follows the fitted capacity.
        presentation = _operations_presentation(config, template)
        fits = {}
        for cls in ("wide", "compact"):
            lay = template["layouts"][cls]
            docked = detail is not None and (
                presentation == "table-context"
                or (presentation == "list-inspector" and cls == "compact")
            )
            if docked:
                docked_viewport_h = (
                    lay["footer"]["y"] - 8 - lay["contextDockHeight"]
                    - lay["collectionViewport"]["y"]
                )
                fits[cls] = max(
                    1,
                    (docked_viewport_h - lay["headerHeight"]) // lay["rowHeight"],
                )
            else:
                fits[cls] = lay["visibleRows"]
        if (
            fits["wide"] < template["layouts"]["wide"]["visibleRows"]
            or fits["compact"] < template["layouts"]["compact"]["visibleRows"]
        ):
            parts_template = copy.deepcopy(template)
            parts_template["layouts"]["wide"]["visibleRows"] = min(
                fits["wide"], template["layouts"]["wide"]["visibleRows"]
            )
            parts_template["layouts"]["compact"]["visibleRows"] = min(
                fits["compact"], template["layouts"]["compact"]["visibleRows"]
            )
    out["_partPaths"].extend(_collection_parts(config["collection"], parts_template, "collection"))
    if detail is not None:
        out["_partPaths"].append("detailPanel")
        out["_partPaths"].append("contextDock")
        out["_partPaths"].append("detail." + detail["id"])
        out["_partPaths"].extend(
            (
                "detail." + detail["id"] + ".label",
                "detail." + detail["id"] + ".identity.title",
                "detail." + detail["id"] + ".identity.subtitle",
            )
        )
        out["_partPaths"].extend(
            "detail." + detail["id"] + ".metric." + item["id"]
            for item in detail["metrics"]
        )
        if "note" in detail:
            out["_partPaths"].extend(
                (
                    "detail." + detail["id"] + ".note.label",
                    "detail." + detail["id"] + ".note.title",
                )
            )
            out["_partPaths"].extend(
                "detail." + detail["id"] + ".note.line-" + str(index + 1)
                for index in range(len(detail["note"]["body"]))
            )
        out["_partPaths"].extend(
            "detail." + detail["id"] + ".action." + item["id"]
            for item in detail["actions"]
        )
    out["_partPaths"].extend("action." + action["id"] for action in actions)
    out["actions"] = ["action." + action["id"] for action in actions]
    return _finalize_parts(out)

def _build_operations(config, template, source_name, template_name):
    synthetic = copy.deepcopy(config)
    synthetic["tabs"] = [
        {"id": "workspace-a", "label": "WORKSPACE"},
        {"id": "workspace-b", "label": "WORKSPACE"},
    ]
    synthetic["selectedTab"] = "workspace-a"
    builder_template = copy.deepcopy(template)
    builder_template["limits"]["maxTabs"] = 2
    builder_template["layouts"]["wide"]["tabBar"] = {
        "x": 92, "y": 158, "width": 836, "height": 44
    }
    builder_template["layouts"]["compact"]["tabBar"] = {
        "x": 52, "y": 102, "width": 466, "height": 44
    }
    builder_template["layouts"]["wide"]["tabGap"] = 8
    builder_template["layouts"]["compact"]["tabGap"] = 6
    out = _build_tabbed(synthetic, builder_template, source_name, template_name)
    out["form"].pop("tabs", None)
    out["form"].pop("selectedTab", None)
    removed = {"tabBar"}
    for layout in out["layouts"].values():
        layout.pop("tabBar", None)
        layout.pop("tabs", None)
    out["parts"] = [
        part for part in out["parts"]
        if not part.startswith("tab_workspace_") and part != "tabBar"
    ]
    for layout in out["layouts"].values():
        rects = layout.get("partRects", {})
        for part in list(rects):
            if part.startswith("tab_workspace_") or part == "tabBar":
                rects.pop(part, None)
    out["spacingRules"] = [
        rule for rule in out.get("spacingRules", [])
        if not any(rule.get(key) in removed for key in ("first", "second", "container", "child"))
    ]
    return out


def _region_shell(panel, authored):
    inset = authored["regionInset"]
    label = _make_rect(
        panel["x"] + inset,
        panel["y"] + inset,
        panel["width"] - 2 * inset,
        authored["regionHeaderHeight"],
    )
    content = _make_rect(
        label["x"],
        _bottom(label),
        label["width"],
        _bottom(panel) - inset - _bottom(label),
    )
    if content["height"] <= 0:
        raise ArchetypeError("detail region template leaves no content height")
    return label, content


def _build_region_action_fragment(actions, authored, content, name):
    columns = authored["regionActionColumns"]
    visible_rows = authored["regionActionVisibleRows"]
    gap = authored["regionActionGap"]
    track = _make_rect(
        _right(content) - authored["scrollbarWidth"],
        content["y"],
        authored["scrollbarWidth"],
        content["height"],
    )
    hit_rail_width = max(44, authored["scrollbarWidth"])
    data_width = content["width"] - authored["scrollbarGap"] - hit_rail_width
    if data_width <= 0:
        raise ArchetypeError(name + " scrollbar rail leaves no action width")
    slot_width = (data_width - gap * (columns - 1)) // columns
    required_height = visible_rows * 44 + max(0, visible_rows - 1) * gap
    if slot_width < 44 or required_height > content["height"]:
        raise ArchetypeError(name + " detail action policy cannot satisfy 44px targets")
    slots = []
    for row_index in range(visible_rows):
        for column_index in range(columns):
            slot_index = row_index * columns + column_index + 1
            slots.append(
                {
                    "id": "action-slot-" + str(slot_index),
                    "rect": _make_rect(
                        content["x"] + column_index * (slot_width + gap),
                        content["y"] + row_index * (44 + gap),
                        slot_width,
                        44,
                    ),
                }
            )
    total = len(actions)
    total_rows = (total + columns - 1) // columns
    metrics = _scroll_metrics(
        total,
        columns * visible_rows,
        total_rows,
        visible_rows,
        columns,
        track,
        authored["minThumbHeight"],
    )
    return slots, metrics


def _detail_region_parts(region, template):
    prefix = "region." + region["id"]
    parts = [prefix, prefix + ".label"]
    if region["kind"] == "preview":
        parts.append(prefix + ".content")
    elif region["kind"] == "collection":
        collection = region["collection"]
        collection_prefix = prefix + ".collection"
        parts.append(prefix + ".content")
        parts.append(collection_prefix)
        parts_template = template
        if template.get("id") == "operations-detail" and collection["mode"] in {"list", "table"}:
            # Region slots clip to whole rows; the shared part list follows the
            # smallest fitted per-class capacity.
            fits = {}
            for cls in ("wide", "compact"):
                lay = template["layouts"][cls]
                panel = lay["regionSlots"].get(
                    "primary" if region["role"] == "primary" else region["role"]
                )
                content_h = (
                    panel["height"] - 2 * lay["regionInset"] - lay["regionHeaderHeight"]
                )
                fits[cls] = max(
                    1, (content_h - lay["headerHeight"]) // lay["rowHeight"]
                )
            if (
                fits["wide"] < template["layouts"]["wide"]["visibleRows"]
                or fits["compact"] < template["layouts"]["compact"]["visibleRows"]
            ):
                parts_template = copy.deepcopy(template)
                parts_template["layouts"]["wide"]["visibleRows"] = min(
                    fits["wide"], template["layouts"]["wide"]["visibleRows"]
                )
                parts_template["layouts"]["compact"]["visibleRows"] = min(
                    fits["compact"], template["layouts"]["compact"]["visibleRows"]
                )
        parts.extend(_collection_parts(collection, parts_template, collection_prefix))
    elif region["kind"] == "fields":
        for field in region["fields"]:
            field_prefix = prefix + ".field." + field["id"]
            parts.extend(
                (field_prefix, field_prefix + ".label", field_prefix + ".value")
            )
    else:
        # scrollable action region exposes physical slots and scroll geometry,
        # not semantic actions (which have no rect and multiplex via scrolling)
        parts.append(prefix + ".content")
        parts.extend((prefix + ".scroll.track", prefix + ".scroll.thumb"))
        wide_slots = (
            template["layouts"]["wide"]["regionActionColumns"]
            * template["layouts"]["wide"]["regionActionVisibleRows"]
        )
        compact_slots = (
            template["layouts"]["compact"]["regionActionColumns"]
            * template["layouts"]["compact"]["regionActionVisibleRows"]
        )
        slots = (
            min(wide_slots, compact_slots)
            if template.get("id") == "operations-detail"
            else max(wide_slots, compact_slots)
        )
        parts.extend(f"{prefix}.action-slot-{index + 1}" for index in range(slots))
    return parts


def _ensure_spacing_rules_resolve(parts, rules):
    declared = set(parts)
    for rule in rules:
        for endpoint in ("first", "second", "container", "child"):
            if endpoint in rule and rule[endpoint] not in declared:
                raise ArchetypeError(
                    "spacing rule "
                    + rule["id"]
                    + "."
                    + endpoint
                    + " references undeclared part "
                    + rule[endpoint]
                )


def _build_detail_regions(config, template, source_name, template_name):
    actions, generated_region_actions, controls = _validate_detail(config, template)
    out = _base_contract(config, template, source_name, template_name, actions)
    form_regions = copy.deepcopy(config["regions"])
    for region in form_regions:
        if region["kind"] == "actions":
            region["actions"] = generated_region_actions[region["id"]]
    out["form"]["regions"] = form_regions
    out["form"]["controls"] = controls
    if template["id"] == "operations-detail":
        out["form"]["visual"] = copy.deepcopy(config["visual"])
        out["form"]["sharedChrome"] = copy.deepcopy(template["sharedChrome"])
    out["copy"]["regions"] = copy.deepcopy(config["regions"])
    out["copy"]["controls"] = copy.deepcopy(config["controls"])
    out["regionMetrics"] = {}
    role_to_region = {region["role"]: region for region in config["regions"]}
    for name in ("wide", "compact"):
        authored = template["layouts"][name]
        if authored["window"]["width"] > authored["maxWindowWidthPx"]:
            raise ArchetypeError(name + " detail window exceeds its template maximum")
        action_rects = _action_rects(actions, authored)
        _ensure_action_copy_fits(actions, action_rects, authored, name + ".action")
        _ensure_text_fits(config["title"], authored["title"], authored, name + ".title")
        control_rects = _control_rects(controls, authored, authored["controlBar"], name)
        regions = {}
        out["regionMetrics"][name] = {}
        for region in config["regions"]:
            panel = copy.deepcopy(authored["regionSlots"][region["role"]])
            label, content = _region_shell(panel, authored)
            _ensure_text_fits(
                region["label"], label, authored, name + ".region." + region["id"]
            )
            generated = {"panel": panel, "label": label}
            if region["kind"] == "preview":
                generated["content"] = content
            elif region["kind"] == "collection":
                region_authored = authored
                if region["collection"]["mode"] in {"list", "table"}:
                    # Only whole row slots fit inside the region content box;
                    # overflow scrolls instead of clipping.
                    fit_rows = max(1, (
                        content["height"] - authored["headerHeight"]
                    ) // authored["rowHeight"])
                    if fit_rows < authored["visibleRows"]:
                        region_authored = dict(authored)
                        region_authored["visibleRows"] = fit_rows
                fragment, metrics = _build_collection_fragment(
                    region["collection"], region_authored, content, name + "." + region["id"],
                    _operations_column_roles(template) if template["id"] == "operations-detail" else None,
                )
                generated["content"] = content
                generated["collection"] = fragment
                out["regionMetrics"][name][region["id"]] = metrics
            elif region["kind"] == "fields":
                fields_height = len(region["fields"]) * authored["fieldRowHeight"]
                if fields_height > content["height"]:
                    raise ArchetypeError(
                        name
                        + " detail field region "
                        + region["id"]
                        + " exceeds capacity"
                    )
                generated["fields"] = {}
                label_width = content["width"] * authored["fieldLabelPercent"] // 100
                value_x = content["x"] + label_width + authored["fieldColumnGap"]
                value_width = _right(content) - value_x
                if label_width <= 0 or value_width <= 0:
                    raise ArchetypeError(name + " detail field columns are invalid")
                for field_index, field in enumerate(region["fields"]):
                    row_y = content["y"] + field_index * authored["fieldRowHeight"]
                    generated["fields"][field["id"]] = {
                        "row": _make_rect(
                            content["x"],
                            row_y,
                            content["width"],
                            authored["fieldRowHeight"],
                        ),
                        "label": _make_rect(
                            content["x"],
                            row_y,
                            label_width,
                            authored["fieldRowHeight"],
                        ),
                        "value": _make_rect(
                            value_x,
                            row_y,
                            value_width,
                            authored["fieldRowHeight"],
                        ),
                    }
                    _ensure_text_fits(
                        field["label"],
                        generated["fields"][field["id"]]["label"],
                        authored,
                        name + ".field." + field["id"] + ".label",
                    )
                    _ensure_text_fits(
                        field["value"],
                        generated["fields"][field["id"]]["value"],
                        authored,
                        name + ".field." + field["id"] + ".value",
                    )
            else:
                action_slots, metrics = _build_region_action_fragment(
                    generated_region_actions[region["id"]],
                    authored,
                    content,
                    name + "." + region["id"],
                )
                generated["content"] = content
                generated["actionSlots"] = action_slots
                generated["scroll"] = {
                    "track": metrics["track"],
                    "thumb": metrics["thumb"],
                }
                if template["id"] == "operations-detail":
                    generated["_strictSlots"] = True
                for region_action in generated_region_actions[region["id"]]:
                    _ensure_text_fits(
                        region_action["label"],
                        action_slots[0]["rect"],
                        authored,
                        name + ".region-action." + region_action["id"],
                    )
                out["regionMetrics"][name][region["id"]] = metrics
            regions[region["id"]] = generated
        generated_layout = {
            "designWidth": authored["designWidth"],
            "designHeight": authored["designHeight"],
            "window": copy.deepcopy(authored["window"]),
            "status": copy.deepcopy(authored["status"]),
            "title": copy.deepcopy(authored["title"]),
            "controlBar": copy.deepcopy(authored["controlBar"]),
            "controls": control_rects,
            "controlParts": _control_parts(controls, control_rects,
                stepper_labels=template.get("id") == "operations-detail"),
            "footer": copy.deepcopy(authored["footer"]),
            "regions": regions,
            "actions": action_rects,
        }
        if template["id"] == "operations-detail":
            generated_layout["headerArt"] = copy.deepcopy(authored["headerArt"])
        out["layouts"][name] = generated_layout
    first_region = config["regions"][0]
    if template["id"] == "operations-detail":
        status_title_rule = {
            "id": "status-title",
            "kind": "inset",
            "container": "status",
            "child": "title",
            "edges": ["left", "top"],
            "wide": 18,
            "compact": 10,
        }
    else:
        status_title_rule = {
            "id": "status-title",
            "kind": "gap",
            "first": "status",
            "second": "title",
            "axis": "vertical",
            "wide": template["layouts"]["wide"]["title"]["y"]
            - _bottom(template["layouts"]["wide"]["status"]),
            "compact": template["layouts"]["compact"]["title"]["y"]
            - _bottom(template["layouts"]["compact"]["status"]),
        }
    rules = [
        status_title_rule,
        {
            "id": "title-first-region",
            "kind": "gap",
            "first": "title",
            "second": "region." + first_region["id"],
            "axis": "vertical",
            "wide": template["layouts"]["wide"]["regionSlots"][first_region["role"]][
                "y"
            ]
            - _bottom(template["layouts"]["wide"]["title"]),
            "compact": template["layouts"]["compact"]["regionSlots"][
                first_region["role"]
            ]["y"]
            - _bottom(template["layouts"]["compact"]["title"]),
        },
        {
            "id": "title-control",
            "kind": "gap",
            "first": "title",
            "second": "controlBar",
            "axis": "horizontal",
            "wide": template["layouts"]["wide"]["controlBar"]["x"]
            - _right(template["layouts"]["wide"]["title"]),
            "compact": template["layouts"]["compact"]["controlBar"]["x"]
            - _right(template["layouts"]["compact"]["title"]),
        },
        {
            "id": "control-first-region",
            "kind": "gap",
            "first": "controlBar",
            "second": "region." + first_region["id"],
            "axis": "vertical",
            "wide": template["layouts"]["wide"]["regionSlots"][first_region["role"]][
                "y"
            ]
            - _bottom(template["layouts"]["wide"]["controlBar"]),
            "compact": template["layouts"]["compact"]["regionSlots"][
                first_region["role"]
            ]["y"]
            - _bottom(template["layouts"]["compact"]["controlBar"]),
        },
    ]
    for region in config["regions"]:
        rules.append(
            {
                "id": "region-inset-" + region["id"],
                "kind": "inset",
                "container": "region." + region["id"],
                "child": "region." + region["id"] + ".label",
                "edges": ["left", "top", "right"],
                "wide": template["layouts"]["wide"]["regionInset"],
                "compact": template["layouts"]["compact"]["regionInset"],
            }
        )
    for rule_id, first_role, second_role, axis in (
        ("primary-secondary", "primary", "secondary", "horizontal"),
        ("primary-summary", "primary", "summary", "vertical"),
        ("secondary-navigation", "secondary", "navigation", "vertical"),
    ):
        if first_role not in role_to_region or second_role not in role_to_region:
            continue
        first = role_to_region[first_role]
        second = role_to_region[second_role]
        values = {}
        for layout_name in ("wide", "compact"):
            first_rect = template["layouts"][layout_name]["regionSlots"][first_role]
            second_rect = template["layouts"][layout_name]["regionSlots"][second_role]
            values[layout_name] = (
                second_rect["x"] - _right(first_rect)
                if axis == "horizontal"
                else second_rect["y"] - _bottom(first_rect)
            )
        rules.append(
            {
                "id": rule_id,
                "kind": "gap",
                "first": "region." + first["id"],
                "second": "region." + second["id"],
                "axis": axis,
                "wide": values["wide"],
                "compact": values["compact"],
            }
        )
    rules.append(
        {
            "id": "footer-action-inset",
            "kind": "inset",
            "container": "footer",
            "child": "action." + actions[-1]["id"],
            "edges": ["right"],
            "wide": template["layouts"]["wide"]["actionInset"],
            "compact": template["layouts"]["compact"]["actionInset"],
        }
    )
    out["spacingRules"] = rules
    out["_partPaths"] = ["window", "status", "title", "controlBar", "footer"]
    if template["id"] == "operations-detail":
        out["_partPaths"].insert(2, "headerArt")
    out["_partPaths"].extend(_control_part_paths(controls,
        template.get("id") == "operations-detail"))
    out["actions"] = ["action." + action["id"] for action in actions]
    for region in config["regions"]:
        out["_partPaths"].extend(_detail_region_parts(region, template))
    out["_partPaths"].extend("action." + action["id"] for action in actions)
    _ensure_spacing_rules_resolve(out["_partPaths"], rules)
    return _finalize_parts(out)
