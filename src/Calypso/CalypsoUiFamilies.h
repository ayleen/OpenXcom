#pragma once
/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenXcom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenXcom.  If not, see <http://www.gnu.org/licenses/>.
 */
/*
 * Phase 43.1.3 (Calypso) -- ruleset-owned HD UI family rollout gate.
 *
 * Pure, dependency-free validation + fail-safe lookup + deduplicating parse
 * helper for the `hdUiFamilies:` ruleset key. The approved program partitions
 * the 149 in-scope native state classes into families F01..F38
 * (docs/hd-ui-family-tracker.md); this gate lets each family's HD layout be
 * turned on or off per-mod without 38 player options, compile-time forks, or
 * campaign-save writes (plan §43.1.3 + implementer contract #6).
 *
 * Deliberately NOT wrapped in #ifdef __EMSCRIPTEN__: it is a pure helper with
 * no Emscripten-specific behavior, matching the established Calypso pure-helper
 * convention (CalypsoUiMetrics.h, CalypsoEconomyMath.h, ...) so the real
 * formulas are exercised by the native doctest suite. The Emscripten glue
 * (parsed set lives on Mod; YAML parsed in Calypso/ModHd.cpp::loadFileCalypso)
 * carries the whole-file guard, as the per-family adapters will in later
 * slices. This slice performs no gameplay mutation, performs no per-call
 * allocation, has no callers, and therefore leaves native OXCE behavior
 * byte-for-byte unchanged.
 *
 * Fail-safe contract (plan §43.1.3): missing or inactive HD pack, missing
 * config key, native build, malformed id, or unknown family all return false
 * (legacy layout). Only an Emscripten build with an active HD pack that parsed
 * a valid, listed F01..F38 id returns true. The native path and the
 * missing-HD-pack path are both exercised directly by the unit suite:
 * isHdUiFamilyEnabled(false, ...) and isHdUiFamilyEnabled(true, nullptr, ...)
 * both return false.
 */
#include <algorithm>
#include <set>
#include <string>
#include <vector>

namespace OpenXcom
{
namespace Calypso
{

// --- Approved family id range ----------------------------------------------

/// Lowest approved family number (1-based; family ids are zero-padded to two
/// digits, so this is "F01"). Immutable part of the approved F01-F38 baseline.
constexpr int CALYPSO_HD_UI_FAMILY_MIN = 1;

/// Highest approved family number ("F38"). Families are added to the active
/// list only in the commit that passes their implementation checkpoint, so the
/// shipped ruleset list stays empty until then.
constexpr int CALYPSO_HD_UI_FAMILY_MAX = 38;

// --- Validation -------------------------------------------------------------

/// True iff `id` is an exact approved family id: the letter 'F' (uppercase)
/// followed by exactly two ASCII digits whose numeric value is in
/// [CALYPSO_HD_UI_FAMILY_MIN, CALYPSO_HD_UI_FAMILY_MAX]. Pure; no allocation.
///
/// Accepted: "F01" .. "F38".
/// Rejected: "F00" (below min), "F39" (above max), "F1" / "F001" (wrong
/// width), "f01" (lowercase), "X01" (wrong prefix), "F3A" / "F--" / "" /
/// "F 1" (non-digits or stray characters), and anything containing whitespace
/// or surrounding text. The strict width + digit + range rules mean the gate
/// never silently matches a typo or a partially-typed id.
inline bool isValidHdUiFamilyId(const std::string& id)
{
	// Exactly three characters: 'F' + two digits. Anything else is malformed.
	if (id.size() != 3) return false;
	if (id[0] != 'F') return false;
	if (id[1] < '0' || id[1] > '9') return false;
	if (id[2] < '0' || id[2] > '9') return false;
	const int value = (id[1] - '0') * 10 + (id[2] - '0');
	return value >= CALYPSO_HD_UI_FAMILY_MIN
	    && value <= CALYPSO_HD_UI_FAMILY_MAX;
}

// --- Fail-safe enabled lookup ----------------------------------------------

/// True iff `familyId` is a valid, listed HD UI family id AND the HD pack is
/// active. The first parameter is the hard fail-safe: a missing or inactive
/// HD pack forces false for every family, even one that is valid and listed,
/// so a mod that ships `hdUiFamilies:` without the supporting HD pack
/// infrastructure never flips a family on.
///
///   - hdPackActive == false (no/missing HD pack) -> false          [fail-safe]
///   - sortedEnabled == nullptr (native build / no parsed config) -> false
///   - malformed or out-of-range familyId -> false (isValidHdUiFamilyId gate)
///   - valid id absent from the list -> false
///   - hdPackActive && valid id present in the list -> true          [Emscripten]
///
/// The lookup is allocation-free on every path (`std::binary_search` on a
/// sorted range). The pointed-to sequence is never modified. Callers keep it
/// sorted + deduplicated; parseHdUiFamilies() below is the canonical producer.
inline bool isHdUiFamilyEnabled(bool hdPackActive,
                                const std::vector<std::string>* sortedEnabled,
                                const std::string& familyId)
{
	if (!hdPackActive) return false;                        // missing / inactive HD pack
	if (!sortedEnabled) return false;                       // native / no parsed config
	if (!isValidHdUiFamilyId(familyId)) return false;       // malformed / unknown
	return std::binary_search(sortedEnabled->begin(),
	                          sortedEnabled->end(),
	                          familyId);                    // O(log n), no allocation
}

// --- Pure parse helper ------------------------------------------------------

/// Result of parsing a raw `hdUiFamilies:` sequence. `families` is the sorted,
/// deduplicated set of valid ids to keep (empty when nothing valid was listed);
/// `rejected` lists each DISTINCT invalid entry once, in first-occurrence
/// order, so the caller can log each malformed id exactly once without
/// re-scanning the YAML or deduplicating itself. Pure; no logging side effects.
struct ParsedHdUiFamilies
{
	std::vector<std::string> families;   ///< valid ids, sorted + deduplicated
	std::vector<std::string> rejected;   ///< distinct invalid ids, first-occurrence order
};

/// Parse a raw sequence of family-id strings into a sorted, deduplicated set
/// of valid ids plus the rejected tail. Duplicates are harmless: a repeated
/// valid id is kept once, and a repeated invalid id is reported once (only its
/// first occurrence is appended to `rejected`). Pure and allocation-bounded by
/// the input size; the engine wrapper in ModHd.cpp is the only caller and runs
/// once per ruleset file that carries the `hdUiFamilies:` key.
inline ParsedHdUiFamilies parseHdUiFamilies(const std::vector<std::string>& raw)
{
	ParsedHdUiFamilies out;
	// Track distinct invalid ids so each malformed string is reported once, in
	// the order first seen, without a second pass over `raw` or `rejected`.
	std::set<std::string> seenRejected;
	// Build `families` via a sorted insert position so it stays sorted + unique
	// in a single pass (no separate sort/unique step, no mutation of `raw`).
	for (const std::string& entry : raw)
	{
		if (!isValidHdUiFamilyId(entry))
		{
			if (seenRejected.insert(entry).second)   // first occurrence of this invalid id
				out.rejected.push_back(entry);
			continue;
		}
		// keep sorted + deduplicated: lower_bound + identity check
		auto pos = std::lower_bound(out.families.begin(), out.families.end(), entry);
		if (pos == out.families.end() || *pos != entry)
			out.families.insert(pos, entry);
	}
	return out;
}

} // namespace Calypso
} // namespace OpenXcom
