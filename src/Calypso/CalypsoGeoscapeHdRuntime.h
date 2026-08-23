#pragma once
/*
 * Phase 46.4 Stage 8a — Geoscape HD runtime mapping.
 *
 * Pure, engine-independent translation of the generated strategic-command-shell
 * contract into the shared HD screen render model, plus the live-state inputs
 * the Geoscape state owns (copy strings, selection, action availability).
 * No SDL, no OpenGL, no state pushes: the same code is unit-tested natively
 * and consumed by the Emscripten production adapter.
 */
#include <string>
#include <vector>

#include "CalypsoHdScreenModel.h"
#include "Generated/CalypsoGeoscapeCommandShell.generated.h"
#include "CalypsoUiMetrics.h"

namespace OpenXcom
{
namespace Calypso
{

struct CalypsoGeoscapeHdRuntimeInput
{
	std::vector<CalypsoHdScreenCopy> copy;
	std::vector<std::string> disabledActionIds;
	std::string selectedActionId;
};

struct CalypsoGeoscapeHdRuntimeModel : public CalypsoHdScreenRenderModel
{
	std::vector<std::string> disabledActionIds;
};

inline CalypsoGeoscapeHdRuntimeModel calypsoGeoscapeHdRuntimeModel(
	const CalypsoGeoscapeCommandShellGen::CalypsoGeoscapeCommandShellGenLayout& layout,
	const CalypsoGeoscapeHdRuntimeInput& input)
{
	CalypsoGeoscapeHdRuntimeModel result;
	result.archetype = CalypsoGeoscapeCommandShellGen::kArchetype;
	result.designWidth = layout.designWidth;
	result.designHeight = layout.designHeight;
	result.selectedActionId = input.selectedActionId;

	for (int i = 0; i < layout.actionCount; ++i)
	{
		const auto& source = layout.actions[i];
		bool disabled = false;
		for (const auto& id : input.disabledActionIds)
			if (id == source.id) { disabled = true; break; }
		if (disabled)
		{
			result.disabledActionIds.push_back(source.id);
			continue;
		}
		CalypsoHdScreenActionVisual action;
		action.id = source.id;
		action.label = source.label;
		action.component = source.component;
		action.slotRole = source.slotRole;
		action.coordinateSpace = source.coordinateSpace;
		action.visible = { source.visible.x, source.visible.y, source.visible.w, source.visible.h };
		action.hit = { source.hit.x, source.hit.y, source.hit.w, source.hit.h };
		action.focusOrder = source.focusOrder;
		action.zOrder = source.zOrder;
		result.actions.push_back(action);
	}

	for (int i = 0; i < layout.regionCount; ++i)
	{
		const auto& region = layout.regions[i];
		result.regions.push_back({ region.id,
			{ region.rect.x, region.rect.y, region.rect.w, region.rect.h } });
	}

	result.copy = input.copy;
	return result;
}

// --- Stage 8b: design-space projection -------------------------------------

/// One projection instance binds a generated canonical layout to the live
/// viewport metrics snapshot. The same object drives rendering and hit
/// testing so the two can never disagree; rebuild it whenever the viewport
/// runtime generation changes.
class CalypsoGeoscapeHdProjection
{
public:
	CalypsoGeoscapeHdProjection(const CalypsoGeoscapeCommandShellGen::CalypsoGeoscapeCommandShellGenLayout& layout,
	                           const CalypsoLayoutMetrics& metrics)
		: _layout(&layout), _metrics(metrics)
	{
		_wide = metrics.safeWidth >= CALYPSO_WIDE_WIDTH_THRESHOLD
		     && metrics.safeHeight >= CALYPSO_WIDE_HEIGHT_THRESHOLD;
	}

	CalypsoLayoutClass layoutClass() const { return _wide ? CalypsoLayoutClass::Wide : CalypsoLayoutClass::Compact; }
	int designWidth() const { return _layout->designWidth; }
	int designHeight() const { return _layout->designHeight; }

	/// Project a semantic action id into logical (engine base) pixels.
	CalypsoHdScreenRect project(const std::string& actionId) const
	{
		for (int i = 0; i < _layout->actionCount; ++i)
			if (actionId == _layout->actions[i].id)
				return projectRect({_layout->actions[i].visible.x, _layout->actions[i].visible.y,
				                    _layout->actions[i].visible.w, _layout->actions[i].visible.h});
		return {};
	}

	/// Project a design-space rectangle into logical pixels under the approved
	/// responsive rules: compact keeps controls at their canonical size and
	/// only grows gaps; wide anchors chrome to the safe-rectangle corners at
	/// canonical size while surplus space flows to the world region.
	CalypsoHdScreenRect projectRect(const CalypsoHdScreenRect& design) const
	{
		const int safeW = _metrics.safeWidth;
		const int safeH = _metrics.safeHeight;
		const int designW = _layout->designWidth;
		const int designH = _layout->designHeight;

		// Bottom-anchored rows (time control) ride with the safe bottom edge;
		// everything else keeps its canonical offset from its anchor corner.
		const bool bottomAnchored = isBottomAnchoredAction(design);

		int x = _metrics.safeX + design.x;
		int y = _metrics.safeY + design.y;
		if (_metrics.safeHeight > designH && bottomAnchored)
			y += safeH - designH;
		if (_metrics.safeWidth > designW)
		{
			// Right-anchored columns follow the safe right edge.
			if (isRightAnchoredAction(design))
				x += safeW - designW;
		}
		return { x, y, design.w, design.h };
	}

private:
	bool isBottomAnchoredAction(const CalypsoHdScreenRect& design) const
	{
		return design.y + design.h > _layout->designHeight - 80;
	}
	bool isRightAnchoredAction(const CalypsoHdScreenRect& design) const
	{
		return design.x + design.w > _layout->designWidth - 160;
	}

	const CalypsoGeoscapeCommandShellGen::CalypsoGeoscapeCommandShellGenLayout* _layout;
	CalypsoLayoutMetrics _metrics;
	bool _wide = false;
};

/// Factory: bind a generated canonical layout to the live metrics snapshot.
inline CalypsoGeoscapeHdProjection calypsoGeoscapeHdProjection(
	const CalypsoGeoscapeCommandShellGen::CalypsoGeoscapeCommandShellGenLayout& layout,
	const CalypsoLayoutMetrics& metrics)
{
	return CalypsoGeoscapeHdProjection(layout, metrics);
};

// --- Stage 8c: feature gate decision ----------------------------------------

/// Stable disable reasons reported by the gate decision. Values are part of
/// the diagnostics contract; never rename without bumping the screen
/// template version.
struct CalypsoGeoscapeHdGateDecision
{
	bool enabled = false;
	const char* reason = "";
};

/// Fail-safe enablement decision for the Geoscape command shell (F16).
/// Enabled only when every precondition holds; the first failing one wins
/// and is reported with a stable reason. Pure: the live wiring supplies the
/// booleans from Mod::isHdUiFamilyEnabled and the renderer/asset checks.
inline CalypsoGeoscapeHdGateDecision calypsoGeoscapeHdGateDecision(
	bool hdPackActive, bool familyListed, bool rendererSupported, bool assetsPresent)
{
	if (!hdPackActive) return { false, "hd-pack-inactive" };
	if (!familyListed) return { false, "family-not-listed" };
	if (!rendererSupported) return { false, "renderer-unsupported" };
	if (!assetsPresent) return { false, "assets-missing" };
	return { true, "enabled" };
};

// --- Stage 9: widget binding table ------------------------------------------

/// One row per contract action: which live GeoscapeState affordance realizes
/// it. Roles are declarative so slices can implement them incrementally:
///   "widget:<member>"   reproject an existing InteractiveSurface/TextButton
///   "state:<behavior>"  state-level behavior without a legacy widget
///   "deferred"          owned by a later slice; must not receive input
struct CalypsoGeoscapeHdWidgetBinding
{
	const char* actionId;
	const char* handler;   // production runtime namespace name
	const char* role;
};

inline const std::vector<CalypsoGeoscapeHdWidgetBinding>& calypsoGeoscapeHdWidgetBindings()
{
	static const std::vector<CalypsoGeoscapeHdWidgetBinding> bindings = {
		{ "action.session", "geoscape.openSession", "state:session-drawer" },
		{ "action.bases", "geoscape.openBases", "widget:btnBases" },
		{ "action.graphs", "geoscape.openGraphs", "widget:btnGraphs" },
		{ "action.extended", "", "widget:btnFunding" },
		{ "action.intercept", "geoscape.openIntercept", "widget:btnIntercept" },
		{ "action.ufopaedia", "geoscape.openUfopaedia", "widget:btnUfopaedia" },
		{ "action.options", "geoscape.openOptions", "widget:btnOptions" },
		{ "drawer.funding", "geoscape.openFunding", "state:drawer-funding" },
		{ "drawer.tech-tree", "geoscape.openTechTree", "state:drawer-tech-tree" },
		{ "drawer.global-research", "geoscape.openGlobalResearch", "state:drawer-research" },
		{ "drawer.global-production", "geoscape.openGlobalProduction", "state:drawer-production" },
		{ "drawer.global-containment", "geoscape.openGlobalContainment", "state:drawer-containment" },
		{ "drawer.ufo-tracker", "geoscape.openUfoTracker", "state:drawer-ufo-tracker" },
		{ "drawer.pilot-experience", "geoscape.openPilotExperience", "state:drawer-pilot-xp" },
		{ "drawer.notes", "geoscape.openNotes", "state:drawer-notes" },
		{ "drawer.music", "geoscape.openMusic", "state:drawer-music" },
		{ "drawer.debug", "geoscape.openDebug", "state:drawer-debug" },
		{ "drawer.quick-save", "geoscape.quickSave", "state:quick-save" },
		{ "drawer.instant-save", "geoscape.instantSave", "state:instant-save" },
		{ "drawer.quick-load", "geoscape.quickLoad", "state:quick-load" },
		{ "world.zoom.in", "geoscape.zoomIn", "widget:btnZoomIn" },
		{ "world.recenter", "geoscape.recenter", "state:recenter" },
		{ "world.zoom.out", "geoscape.zoomOut", "widget:btnZoomOut" },
		{ "notification.contact.open", "geoscape.openNotification", "state:notification-open" },
		{ "time.pause", "geoscape.setSpeedPause", "state:pause-toggle" },
		{ "time.speed.5sec", "geoscape.setSpeed5Seconds", "widget:btn5Secs" },
		{ "time.speed.1min", "geoscape.setSpeed1Minute", "widget:btn1Min" },
		{ "time.speed.5min", "geoscape.setSpeed5Minutes", "widget:btn5Mins" },
		{ "time.speed.30min", "geoscape.setSpeed30Minutes", "widget:btn30Mins" },
		{ "time.speed.1hour", "geoscape.setSpeed1Hour", "widget:btn1Hour" },
		{ "time.speed.1day", "geoscape.setSpeed1Day", "widget:btn1Day" },
	};
	return bindings;
}

/// Legacy side-panel fillers the physical shell replaces when the gate is on.
inline const std::vector<const char*>& calypsoGeoscapeHdSuppressedFillers()
{
	static const std::vector<const char*> fillers = { "sidebar", "sideLine", "sideTop", "sideBottom" };
	return fillers;
};

} // namespace Calypso
} // namespace OpenXcom