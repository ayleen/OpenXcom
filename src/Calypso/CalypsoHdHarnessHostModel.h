#pragma once
/*
 * Phase 46.4-F33 (Calypso) -- portable lifecycle model of the opaque-black
 * engine harness host.
 *
 * F33-PARITY-002: the comparison harness pushed a _screen = false target over
 * the visible MainMenuState and put the black backdrop INSIDE the target's
 * atomic subgroup, so a text failure rejected the backdrop and exposed the
 * menu. The host must instead be STRUCTURAL: a full-canvas opaque-black state
 * (_screen = true, F33.3) pushed BELOW the target preview, independent of the
 * target adapter's readiness.
 *
 * This header owns the pure lifecycle + the stable-scenario registry; the
 * browser-only CalypsoHdHarnessHostState and the generic export (F33.3) drive
 * it. Rules pinned here:
 *   - a repeated open request never stacks a second host (hostUp latch);
 *   - the target can never be "up" while the host is down;
 *   - close resets the session (host pops itself, globals cleared);
 *   - an explicitly requested layout class survives resize (F33-PARITY-005):
 *     the harness overrides ordinary automatic classification until closed.
 *
 * Pure, dependency-free, natively unit tested (CalypsoHdHarnessHostModelTest).
 */
#include "CalypsoUiMetrics.h"
#include <cstdint>

namespace OpenXcom
{
namespace Calypso
{

/// Stable harness scenario ids (generic registry keys).
enum class CalypsoHarnessScenario
{
	F33Abandon = 33,
	F21Site = 51,
	F21Transaction = 52,
	F21Name = 53,
	F21Defense = 54,
	F21Destruction = 55,
	F21SiteError = 56,
	F03Dismantle = 60,
	F04SackSoldier = 61,
	F18CraftError = 62,
	F18LowFuel = 63,
	F18NotEnoughPilots = 64,
	F19DogfightError = 65,
	F20ConfirmLanding = 66,
	F20ConfirmCydonia = 67,
	F24ResearchRequired = 68,
	F24ResearchComplete = 69,
	F28AbortMission = 70,
	F28ConfirmEnd = 71,
	F17UfoLost = 72,
	F17UfoDetected = 73,
	F17MissionDetected = 74,
	F22TrainingFinished = 75,
	F30NoExperience = 76,
	F24ProductionComplete = 77,
	F05SoldierTransform = 78,
	F06SoldierDiary = 79,
	F12TransferConfirm = 80,
	F10ManufactureCheck = 81,
	F13Containment = 82,
	F24ItemsArriving = 83
};

/// True iff `id` names a known scenario (the generic export never guesses).
inline bool calypsoHarnessScenarioValid(int id)
{
	return id == static_cast<int>(CalypsoHarnessScenario::F33Abandon)
		|| id == static_cast<int>(CalypsoHarnessScenario::F21Site)
		|| id == static_cast<int>(CalypsoHarnessScenario::F21Transaction)
		|| id == static_cast<int>(CalypsoHarnessScenario::F21Name)
		|| id == static_cast<int>(CalypsoHarnessScenario::F21Defense)
		|| id == static_cast<int>(CalypsoHarnessScenario::F21Destruction)
		|| id == static_cast<int>(CalypsoHarnessScenario::F21SiteError)
		|| id == static_cast<int>(CalypsoHarnessScenario::F03Dismantle)
		|| id == static_cast<int>(CalypsoHarnessScenario::F04SackSoldier)
		|| id == static_cast<int>(CalypsoHarnessScenario::F18CraftError)
		|| id == static_cast<int>(CalypsoHarnessScenario::F18LowFuel)
		|| id == static_cast<int>(CalypsoHarnessScenario::F18NotEnoughPilots)
		|| id == static_cast<int>(CalypsoHarnessScenario::F19DogfightError)
		|| id == static_cast<int>(CalypsoHarnessScenario::F20ConfirmLanding)
		|| id == static_cast<int>(CalypsoHarnessScenario::F20ConfirmCydonia)
		|| id == static_cast<int>(CalypsoHarnessScenario::F24ResearchRequired)
		|| id == static_cast<int>(CalypsoHarnessScenario::F24ResearchComplete)
		|| id == static_cast<int>(CalypsoHarnessScenario::F28AbortMission)
		|| id == static_cast<int>(CalypsoHarnessScenario::F28ConfirmEnd)
		|| id == static_cast<int>(CalypsoHarnessScenario::F17UfoLost)
		|| id == static_cast<int>(CalypsoHarnessScenario::F17UfoDetected)
		|| id == static_cast<int>(CalypsoHarnessScenario::F17MissionDetected)
		|| id == static_cast<int>(CalypsoHarnessScenario::F22TrainingFinished)
		|| id == static_cast<int>(CalypsoHarnessScenario::F30NoExperience)
		|| id == static_cast<int>(CalypsoHarnessScenario::F24ProductionComplete)
		|| id == static_cast<int>(CalypsoHarnessScenario::F05SoldierTransform)
		|| id == static_cast<int>(CalypsoHarnessScenario::F06SoldierDiary)
		|| id == static_cast<int>(CalypsoHarnessScenario::F12TransferConfirm)
		|| id == static_cast<int>(CalypsoHarnessScenario::F10ManufactureCheck)
		|| id == static_cast<int>(CalypsoHarnessScenario::F13Containment)
		|| id == static_cast<int>(CalypsoHarnessScenario::F24ItemsArriving);
}

/// Mutable session state of one harness run.
struct CalypsoHarnessSession
{
	bool hostUp = false;
	bool targetUp = false;
	const void* activeTarget = nullptr;
	std::uint64_t generation = 0;
	bool layoutExplicit = false;
	CalypsoLayoutClass requestedLayout = CalypsoLayoutClass::Compact;
	bool motionDisabled = false; // deterministic capture mode (motion=0)
	/// Presentation-clock freeze for deterministic motion capture: -1 = live
	/// ramp; 0..100 = freeze the opening-motion progress at that percent so a
	/// capture can screenshot a stable mid-ramp frame (F33.5 motion evidence).
	int motionHoldPct = -1;
	/// Side-by-side comparison mode (Phase 46.F21): the target dialog shifts
	/// into the left half of the Wide canvas so the DOM reference card fits on
	/// the right. Session-level since F21; the F33 adapter keeps its own
	/// mirrored global for continuity.
	bool sideBySide = false;
};

inline bool calypsoHarnessHostUp(const CalypsoHarnessSession& s) { return s.hostUp; }
inline bool calypsoHarnessTargetUp(const CalypsoHarnessSession& s) { return s.targetUp; }

/// Request to open the harness: returns true only when this call brings the
/// host UP (first request); later requests are no-ops (never stack).
inline bool calypsoHarnessRequestOpen(CalypsoHarnessSession& s)
{
	if (s.hostUp) return false;
	s.hostUp = true;
	return true;
}

/// The target preview appeared (host pushed it once). Returns true and marks
/// the target up; a second call while already up is a no-op.
inline bool calypsoHarnessTargetUp(CalypsoHarnessSession& s, const void* target = nullptr)
{
	if (!s.hostUp || s.targetUp) return false;
	s.targetUp = true;
	s.activeTarget = target;
	++s.generation;
	return true;
}

/// Close: the target popped (or the host was torn down). Resets the session so
/// a later fresh open works; the host pops itself and clears harness globals.
inline void calypsoHarnessClose(CalypsoHarnessSession& s)
{
	s.hostUp = false;
	s.targetUp = false;
	s.activeTarget = nullptr;
	s.layoutExplicit = false;
	s.requestedLayout = CalypsoLayoutClass::Compact;
	s.motionDisabled = false;
	s.motionHoldPct = -1;
	s.sideBySide = false;
}

inline bool calypsoHarnessCloseForTarget(CalypsoHarnessSession& s, const void* target, std::uint64_t gen)
{
	if (s.activeTarget != target || s.generation != gen) return false;
	calypsoHarnessClose(s);
	return true;
}

/// Deterministic capture mode: presentation motion is disabled for the active
/// preview (F33 capture mode, motion=0 from the DOM controller).
inline void calypsoHarnessSetMotionDisabled(CalypsoHarnessSession& s, bool disabled)
{
	s.motionDisabled = disabled;
}

/// Freeze (0..100) or release (-1) the opening-motion presentation clock.
/// Used ONLY by the capture harness: while frozen, the adapter renders the
/// ramp at exactly that progress every frame (deterministic screenshots);
/// -1 restores the live clock.
inline void calypsoHarnessSetMotionHold(CalypsoHarnessSession& s, int pct)
{
	s.motionHoldPct = (pct >= 0 && pct <= 100) ? pct : -1;
}

/// Explicitly request a layout class for the active preview; preserved across
/// resize until close (F33-PARITY-005).
inline void calypsoHarnessSetRequestedLayout(CalypsoHarnessSession& s, CalypsoLayoutClass cls)
{
	s.requestedLayout = cls;
	s.layoutExplicit = true;
}

/// Reconfigure the live preview without pushing another host or target.
/// Returns false unless both lifecycle layers are active.
inline bool calypsoHarnessReconfigure(CalypsoHarnessSession& s, CalypsoLayoutClass cls)
{
	if (!s.hostUp || !s.targetUp) return false;
	calypsoHarnessSetRequestedLayout(s, cls);
	return true;
}

/// The layout class the preview must use: the explicit request when set,
/// otherwise the ordinary safe-area classification.
inline CalypsoLayoutClass calypsoHarnessEffectiveLayout(
	const CalypsoHarnessSession& s, const CalypsoBaseSafeRect& safe)
{
	if (s.layoutExplicit) return s.requestedLayout;
	return calypsoClassifySafeArea(safe.width, safe.height);
}

} // namespace Calypso
} // namespace OpenXcom
