#pragma once
/*
 * Phase 46.4-F33 (Calypso) -- read-only interaction visual state snapshot.
 *
 * F33-PARITY-008: the physical adapter submitted STATIC button styles and
 * never snapshotted hover, focus, or pressed state. This seam maps an
 * immutable (action tone, interaction state) pair to the SEMANTIC token keys
 * shared by engine and DOM through the generated contract -- the adapter reads
 * it and NEVER owns input events: the logical widgets keep focus, keyboard,
 * controller, pointer hit testing, and transitions.
 *
 * Token keys are contract identifiers (stable strings), not values: their
 * actual fill/border values come from the generated theme contract
 * (CalypsoHdTheme.generated.h for C++, hd-ui-theme.js for DOM). A state x tone
 * pair always maps to a DISTINCT pair of keys, so a screen never presents two
 * states as identical (the a11y contract: colour alone is never the only cue).
 *
 * Pure, dependency-free, natively unit tested (CalypsoHdInteractionStateTest).
 */
#include <cstddef>

namespace OpenXcom
{
namespace Calypso
{

/// Read-only interaction state of an action widget (no event ownership).
enum class CalypsoInteractionState
{
	Rest,
	Hover,
	Focus,
	Pressed,
	Disabled
};

/// Semantic action tone: the safe (e.g. NO / resume) vs the destructive
/// (e.g. YES / abandon) action.
enum class CalypsoActionTone
{
	Safe,
	Destructive
};

/// The generated-theme token keys for one (tone, state) presentation.
struct CalypsoInteractionTokenPair
{
	const char* fillToken;   // semantic colour token for the fill
	const char* borderToken; // semantic colour token for the border
};

/// Map (tone, state) to its semantic token keys. All ten (5 states x 2 tones)
/// pairs are pairwise distinct.
CalypsoInteractionTokenPair calypsoInteractionTokenPair(
	CalypsoActionTone tone, CalypsoInteractionState state);

/// The non-colour focus cue token key (focus ring / underline style), distinct
/// for safe and destructive actions.
const char* calypsoFocusRingToken(CalypsoActionTone tone);

/// Presentation opacity multiplier: Rest/Hover/Focus/Pressed are fully opaque;
/// Disabled is dimmed (strictly in (0, 1)).
float calypsoInteractionOpacity(CalypsoInteractionState state);

// --- Inline reference implementation ----------------------------------------
//
// Token KEYS are stable contract identifiers; the generated theme contract
// binds them to actual colour values on each consumer side.

inline CalypsoInteractionTokenPair calypsoInteractionTokenPair(
	CalypsoActionTone tone, CalypsoInteractionState state)
{
	// Semantic token keys, grouped per tone so the ten pairs stay distinct.
	static const char* const kSafe[5][2] = {
		{ "token.color.safeRestFill",       "token.color.safeRestBorder" },
		{ "token.color.safeHoverFill",      "token.color.safeHoverBorder" },
		{ "token.color.safeFocusFill",      "token.color.safeFocusBorder" },
		{ "token.color.safePressedFill",    "token.color.safePressedBorder" },
		{ "token.color.safeDisabledFill",   "token.color.safeDisabledBorder" },
	};
	static const char* const kDestructive[5][2] = {
		{ "token.color.destructiveRestFill",     "token.color.destructiveRestBorder" },
		{ "token.color.destructiveHoverFill",    "token.color.destructiveHoverBorder" },
		{ "token.color.destructiveFocusFill",    "token.color.destructiveFocusBorder" },
		{ "token.color.destructivePressedFill",  "token.color.destructivePressedBorder" },
		{ "token.color.destructiveDisabledFill", "token.color.destructiveDisabledBorder" },
	};
	int idx = static_cast<int>(state);
	if (idx < 0 || idx > 4) idx = 0;
	if (tone == CalypsoActionTone::Destructive)
	{
		return CalypsoInteractionTokenPair{ kDestructive[idx][0], kDestructive[idx][1] };
	}
	return CalypsoInteractionTokenPair{ kSafe[idx][0], kSafe[idx][1] };
}

inline const char* calypsoFocusRingToken(CalypsoActionTone tone)
{
	return tone == CalypsoActionTone::Destructive
		? "token.color.focusRingDanger"
		: "token.color.focusRingSafe";
}

inline float calypsoInteractionOpacity(CalypsoInteractionState state)
{
	return state == CalypsoInteractionState::Disabled ? 0.55f : 1.0f;
}

} // namespace Calypso
} // namespace OpenXcom
