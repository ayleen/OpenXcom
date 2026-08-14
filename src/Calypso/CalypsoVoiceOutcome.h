#pragma once
/*
 * Phase 44 (Calypso) -- pure voice-outcome classifier, dependency-free so the
 * native doctest suite can exercise the real decision table. The browser
 * voice pilot (CalypsoVoiceG05 / CalypsoVoiceManager) aggregates per-shot
 * observations and delegates here to settle on exactly one bark outcome per
 * finalised attack.
 *
 * Hard rule: no engine, SDL, YAML, or OXCE includes may ever be added to this
 * header. Inputs are modelled by value; the function is state-free and total.
 */

namespace OpenXcom
{

/// Categorical outcome of one aggregated attack. Values are listed in
/// declaration order, not precedence order -- use `calypsoDecideVoiceOutcome`
/// to map an observation onto this enum.
enum class CalypsoVoiceOutcome
{
	Silence,      ///< Armor-blocked contact: a unit was touched but nothing
	              ///< was damaged. No boast, no apology (Known Pitfall #12).
	Miss,         ///< No unit was contacted at all -- the shot flew wide.
	HostileHit,   ///< At least one hostile was damaged (and none killed).
	HostileKill,  ///< At least one hostile was killed.
	FriendlyHit,  ///< A friendly (X-Com) unit was damaged or killed.
	CivilianHit   ///< A civilian was damaged or killed.
};

/// Aggregated observation of one attack's effects. Every field defaults to
/// false so callers (and tests) can flip only the ones that matter. The
/// classifier compares these in a fixed precedence order; original-faction
/// semantics must be preserved upstream so a mind-controlled friendly or
/// civilian casualty is never celebrated as a hostile kill (Known Pitfall
/// #11) -- by the time the observation reaches this header, the faction
/// labels are already correct.
struct CalypsoVoiceOutcomeObservation
{
	/// True when the shot so much as touched a unit, even for zero damage
	/// (e.g. absorbed by armor). Distinguishes a true miss from an
	/// armor-blocked contact.
	bool contactedUnit = false;

	/// True when any hostile was damaged. Independent of `hostileKilled`:
	/// a single attack may damage several hostiles and kill none.
	bool hostileDamaged = false;

	/// True when any hostile was killed.
	bool hostileKilled = false;

	/// True when any friendly (X-Com soldier) was damaged or killed.
	bool friendlyDamagedOrKilled = false;

	/// True when any civilian was damaged or killed.
	bool civilianDamagedOrKilled = false;
};

/// Deterministic, state-free classification of one aggregated attack into a
/// single bark outcome. Lower-precedence flags are ignored once a higher
/// category matches, so callers may set every flag that applied without
/// having to deduplicate themselves.
///
/// Precedence (highest first):
///   1. Civilian hit      -- civilianDamagedOrKilled
///   2. Friendly hit      -- friendlyDamagedOrKilled
///   3. Hostile kill      -- hostileKilled
///   4. Hostile damage    -- hostileDamaged
///   5. Miss              -- only when no unit was contacted at all
///   6. Silence           -- a unit was contacted but nothing was damaged
///                          (armor blocked the shot; Known Pitfall #12).
inline CalypsoVoiceOutcome calypsoDecideVoiceOutcome(
	const CalypsoVoiceOutcomeObservation &o)
{
	if (o.civilianDamagedOrKilled)
	{
		return CalypsoVoiceOutcome::CivilianHit;
	}
	if (o.friendlyDamagedOrKilled)
	{
		return CalypsoVoiceOutcome::FriendlyHit;
	}
	if (o.hostileKilled)
	{
		return CalypsoVoiceOutcome::HostileKill;
	}
	if (o.hostileDamaged)
	{
		return CalypsoVoiceOutcome::HostileHit;
	}
	if (!o.contactedUnit)
	{
		return CalypsoVoiceOutcome::Miss;
	}
	return CalypsoVoiceOutcome::Silence;
}

}
