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
#include <cmath>

#include "CalypsoHdScreenModel.h"
#include "Generated/CalypsoGeoscapeCommandShell.generated.h"
#include "CalypsoUiMetrics.h"

namespace OpenXcom
{
namespace Calypso
{

inline bool calypsoGeoscapeHdPreviewFamilyEnabled(bool familyListed, bool previewEnabled)
{
	return familyListed || previewEnabled;
}

struct CalypsoGeoscapeHdRuntimeInput
{
	std::vector<CalypsoHdScreenCopy> copy;
	std::vector<std::string> disabledActionIds;
	std::string selectedActionId;
};

/// Runtime metadata for rows that are materialized by the live Geoscape shell.
/// These are semantic facts shared by native tests and the Emscripten adapter;
/// handlers remain owned by GeoscapeState.
struct CalypsoGeoscapeHdLiveDrawerRow
{
	const char* actionId;
	const char* handler;
	const char* labelKey;
	const char* availability;
	bool stateOwned;
};

inline const std::vector<CalypsoGeoscapeHdLiveDrawerRow>& calypsoGeoscapeHdLiveDrawerRows()
{
	static const std::vector<CalypsoGeoscapeHdLiveDrawerRow> rows = {
		{ "drawer.funding", "geoscape.openFunding", "STR_FUNDING", "extended-links", true },
		{ "drawer.tech-tree", "geoscape.openTechTree", "STR_TECH_TREE_VIEWER", "extended-links", true },
		{ "drawer.global-research", "geoscape.openGlobalResearch", "STR_GLOBAL_RESEARCH", "extended-links", true },
		{ "drawer.global-production", "geoscape.openGlobalProduction", "STR_GLOBAL_MANUFACTURE", "extended-links", true },
		{ "drawer.global-containment", "geoscape.openGlobalContainment", "STR_GLOBAL_ALIEN_CONTAINMENT", "extended-links", true },
		{ "drawer.ufo-tracker", "geoscape.openUfoTracker", "STR_UFO_TRACKER", "extended-links", true },
		{ "drawer.pilot-experience", "geoscape.openPilotExperience", "STR_DAILY_PILOT_EXPERIENCE", "extended-links", true },
		{ "drawer.notes", "geoscape.openNotes", "STR_NOTES", "extended-links", true },
		{ "drawer.music", "geoscape.openMusic", "STR_SELECT_MUSIC_TRACK", "extended-links", true },
		{ "drawer.debug", "geoscape.openDebug", "STR_DEBUG", "extended-links", true },
		{ "drawer.quick-save", "geoscape.quickSave", "STR_QUICK_SAVE", "non-ironman", true },
		{ "drawer.instant-save", "geoscape.instantSave", "STR_INSTANT_SAVE", "non-ironman", true },
		{ "drawer.quick-load", "geoscape.quickLoad", "STR_QUICK_LOAD", "non-ironman", true },
	};
	return rows;
}

/// Per-Geoscape state for the session chip/drawer lifecycle. Surface pointers
/// are held by the owning GeoscapeState and never in process-global storage.
struct CalypsoGeoscapeHdDrawerState
{
	bool open = false;
	bool pauseBeforeOpen = false;

	void toggle(bool currentlyPaused)
	{
		if (!open)
		{
			pauseBeforeOpen = currentlyPaused;
			open = true;
		}
		else
		{
			open = false;
		}
	}

	void reset()
	{
		open = false;
		pauseBeforeOpen = false;
	}
};

struct CalypsoGeoscapeHdRuntimeModel : public CalypsoHdScreenRenderModel
{
	std::vector<std::string> disabledActionIds;
	std::vector<std::string> expectedActionIds;
};

inline bool calypsoGeoscapeHdActionExpected(const std::string& actionId,
	const std::string& role, bool available)
{
	if (actionId == "world.recenter" || actionId == "notification.contact.open") return false;
	if (actionId == "action.session" || actionId == "time.pause"
		|| actionId.rfind("time.speed.", 0) == 0) return true;
	if (role.rfind("widget:", 0) == 0) return true;
	return available;
}

inline const char* calypsoGeoscapeHdSelectedTimeAction(bool paused, const char* runningSpeed)
{
	return paused ? "time.pause" : runningSpeed;
}

inline const std::vector<std::string>& calypsoGeoscapeHdRequiredCopyKeys(bool fundsVisible)
{
	static const std::vector<std::string> withoutFunds = { "time", "date" };
	static const std::vector<std::string> withFunds = { "time", "date", "funds" };
	return fundsVisible ? withFunds : withoutFunds;
}

inline bool calypsoGeoscapeHdModelReady(const CalypsoHdScreenRenderModel& model,
	const std::vector<std::string>& expectedActionIds,
	const std::vector<std::string>& requiredCopyKeys)
{
	for (const auto& key : requiredCopyKeys)
	{
		bool present = false;
		for (const auto& copy : model.copy)
			if (copy.key == key && !copy.value.empty()) { present = true; break; }
		if (!present) return false;
	}
	for (const auto& expected : expectedActionIds)
	{
		bool ready = false;
		for (const auto& action : model.actions)
			if (action.id == expected && action.widget != nullptr && !action.label.empty())
			{
				ready = true;
				break;
			}
		if (!ready) return false;
	}
	return true;
}

inline bool calypsoGeoscapeHdFrameReady(const CalypsoHdScreenRenderModel& model,
	const std::vector<std::string>& expectedActionIds,
	const std::vector<std::string>& requiredCopyKeys, bool resourcesReady)
{
	return resourcesReady && calypsoGeoscapeHdModelReady(model, expectedActionIds,
		requiredCopyKeys);
}

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
	                           const CalypsoLayoutMetrics& metrics,
	                           int baseWidth = 0, int baseHeight = 0)
		: _layout(&layout), _metrics(metrics), _baseWidth(baseWidth), _baseHeight(baseHeight)
	{
		_wide = metrics.safeWidth >= CALYPSO_WIDE_WIDTH_THRESHOLD
		     && metrics.safeHeight >= CALYPSO_WIDE_HEIGHT_THRESHOLD;
		if (_baseWidth > 0 && _baseHeight > 0)
		{
			_baseSafe = calypsoProjectSafeRect(metrics, _baseWidth, _baseHeight);
			_scale = calypsoFitUiScale(_baseSafe, layout.designWidth, layout.designHeight);
		}
	}

	CalypsoLayoutClass layoutClass() const { return _wide ? CalypsoLayoutClass::Wide : CalypsoLayoutClass::Compact; }
	int designWidth() const { return _layout->designWidth; }
	int designHeight() const { return _layout->designHeight; }
	double uiScale() const { return _scale > 0.0 ? _scale : 1.0; }
	int canvasX() const { return canvasOriginX(); }
	int canvasY() const { return canvasOriginY(); }
	int canvasWidth() const { return _baseWidth > 0 ? static_cast<int>(std::lround(_layout->designWidth * uiScale())) : _metrics.safeWidth; }
	int canvasHeight() const { return _baseHeight > 0 ? static_cast<int>(std::lround(_layout->designHeight * uiScale())) : _metrics.safeHeight; }

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
		if (_baseWidth <= 0 || _baseHeight <= 0)
		{
			int x = _metrics.safeX + design.x;
			int y = _metrics.safeY + design.y;
			if (_metrics.safeHeight > _layout->designHeight
				&& design.y + design.h > _layout->designHeight - 80)
				y += _metrics.safeHeight - _layout->designHeight;
			if (_metrics.safeWidth > _layout->designWidth
				&& design.x + design.w > _layout->designWidth - 160)
				x += _metrics.safeWidth - _layout->designWidth;
			return { x, y, design.w, design.h };
		}
		const double scale = uiScale();
		return { canvasOriginX() + static_cast<int>(std::lround(design.x * scale)),
			canvasOriginY() + static_cast<int>(std::lround(design.y * scale)),
			static_cast<int>(std::lround(design.w * scale)),
			static_cast<int>(std::lround(design.h * scale)) };
	}

private:
	int canvasOriginX() const
	{
		return _baseWidth > 0 ? _baseSafe.x + (_baseSafe.width - canvasWidth()) / 2 : _metrics.safeX;
	}
	int canvasOriginY() const
	{
		return _baseHeight > 0 ? _baseSafe.y + (_baseSafe.height - canvasHeight()) / 2 : _metrics.safeY;
	}

	const CalypsoGeoscapeCommandShellGen::CalypsoGeoscapeCommandShellGenLayout* _layout;
	CalypsoLayoutMetrics _metrics;
	int _baseWidth = 0;
	int _baseHeight = 0;
	CalypsoBaseSafeRect _baseSafe;
	double _scale = 0.0;
	bool _wide = false;
};

/// Factory: bind a generated canonical layout to the live metrics snapshot.
inline CalypsoGeoscapeHdProjection calypsoGeoscapeHdProjection(
	const CalypsoGeoscapeCommandShellGen::CalypsoGeoscapeCommandShellGenLayout& layout,
	const CalypsoLayoutMetrics& metrics, int baseWidth = 0, int baseHeight = 0)
{
	return CalypsoGeoscapeHdProjection(layout, metrics, baseWidth, baseHeight);
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
	const char* labelKey;   // localization key from the semantic recipe
	const char* role;
};

inline const std::vector<CalypsoGeoscapeHdWidgetBinding>& calypsoGeoscapeHdWidgetBindings()
{
	static const std::vector<CalypsoGeoscapeHdWidgetBinding> bindings = {
		{ "action.session", "geoscape.openSession", "STR_SESSION", "state:session-drawer" },
		{ "action.bases", "geoscape.openBases", "STR_BASES", "widget:btnBases" },
		{ "action.graphs", "geoscape.openGraphs", "STR_GRAPHS", "widget:btnGraphs" },
		{ "action.extended", "", "", "widget:btnFunding" },
		{ "action.intercept", "geoscape.openIntercept", "STR_INTERCEPT", "widget:btnIntercept" },
		{ "action.ufopaedia", "geoscape.openUfopaedia", "STR_UFOPAEDIA", "widget:btnUfopaedia" },
		{ "action.options", "geoscape.openOptions", "STR_OPTIONS", "widget:btnOptions" },
		{ "drawer.funding", "geoscape.openFunding", "STR_FUNDING", "state:drawer-funding" },
		{ "drawer.tech-tree", "geoscape.openTechTree", "STR_TECH_TREE_VIEWER", "state:drawer-tech-tree" },
		{ "drawer.global-research", "geoscape.openGlobalResearch", "STR_GLOBAL_RESEARCH", "state:drawer-research" },
		{ "drawer.global-production", "geoscape.openGlobalProduction", "STR_GLOBAL_MANUFACTURE", "state:drawer-production" },
		{ "drawer.global-containment", "geoscape.openGlobalContainment", "STR_GLOBAL_ALIEN_CONTAINMENT", "state:drawer-containment" },
		{ "drawer.ufo-tracker", "geoscape.openUfoTracker", "STR_UFO_TRACKER", "state:drawer-ufo-tracker" },
		{ "drawer.pilot-experience", "geoscape.openPilotExperience", "STR_DAILY_PILOT_EXPERIENCE", "state:drawer-pilot-xp" },
		{ "drawer.notes", "geoscape.openNotes", "STR_NOTES", "state:drawer-notes" },
		{ "drawer.music", "geoscape.openMusic", "STR_SELECT_MUSIC_TRACK", "state:drawer-music" },
		{ "drawer.debug", "geoscape.openDebug", "STR_DEBUG", "state:drawer-debug" },
		{ "drawer.quick-save", "geoscape.quickSave", "STR_QUICK_SAVE", "state:quick-save" },
		{ "drawer.instant-save", "geoscape.instantSave", "STR_INSTANT_SAVE", "state:instant-save" },
		{ "drawer.quick-load", "geoscape.quickLoad", "STR_QUICK_LOAD", "state:quick-load" },
		{ "world.zoom.in", "geoscape.zoomIn", "STR_ZOOM_IN", "widget:btnZoomIn" },
		{ "world.recenter", "geoscape.recenter", "STR_CENTER", "state:recenter" },
		{ "world.zoom.out", "geoscape.zoomOut", "STR_ZOOM_OUT", "widget:btnZoomOut" },
		{ "notification.contact.open", "geoscape.openNotification", "STR_OPEN", "state:notification-open" },
		{ "time.pause", "geoscape.setSpeedPause", "STR_PAUSE", "state:pause-toggle" },
		{ "time.speed.5sec", "geoscape.setSpeed5Seconds", "STR_5_SECONDS", "widget:btn5Secs" },
		{ "time.speed.1min", "geoscape.setSpeed1Minute", "STR_1_MINUTE", "widget:btn1Min" },
		{ "time.speed.5min", "geoscape.setSpeed5Minutes", "STR_5_MINUTES", "widget:btn5Mins" },
		{ "time.speed.30min", "geoscape.setSpeed30Minutes", "STR_30_MINUTES", "widget:btn30Mins" },
		{ "time.speed.1hour", "geoscape.setSpeed1Hour", "STR_1_HOUR", "widget:btn1Hour" },
		{ "time.speed.1day", "geoscape.setSpeed1Day", "STR_1_DAY", "widget:btn1Day" },
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
