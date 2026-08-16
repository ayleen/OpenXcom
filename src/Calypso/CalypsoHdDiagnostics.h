#pragma once
/*
 * Phase 46.4-F33 (Calypso) -- bounded renderer diagnostics gate.
 *
 * Raster/shader failures retry every frame (nothing caches a failure), so an
 * uncapped "tracked raster failed" line was emitted per frame (F33-PARITY-001).
 * This gate allows ONE diagnostic per (failure class, resource/contract
 * generation, state instance) tuple and stays silent for that tuple until the
 * resource recovers (noteRecovered) or the generation is explicitly
 * invalidated (invalidateGeneration/reset) -- matching the acceptance rule:
 *   "A forced repeated failure emits at most one error for each
 *    (failure class, resource/contract generation, state instance) tuple and
 *    emits no further copies until recovery or explicit invalidation."
 *
 * The stored set is bounded (default 32 tuples): a pathological many-text
 * failure cannot grow memory unboundedly; the oldest logged tuples are dropped.
 *
 * Pure, dependency-free, natively unit tested (CalypsoHdDiagnosticsTest.cpp).
 */
#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <unordered_set>
#include <vector>

namespace OpenXcom
{
namespace Calypso
{

/// One diagnostic tuple identity.
struct CalypsoDiagnosticKey
{
	std::string failClass;
	std::uint64_t generation = 0; // resource/contract generation
	std::uint64_t instance = 0;   // state/resource instance (e.g. text-key hash)

	bool operator==(const CalypsoDiagnosticKey& o) const
	{
		return failClass == o.failClass && generation == o.generation
			&& instance == o.instance;
	}
};

} // namespace Calypso
} // namespace OpenXcom

// --- Hash specialisation -----------------------------------------------------
// Declared in the REAL global ::std, between the key struct and its users, so
// unordered_set<CalypsoDiagnosticKey> instantiates it before first use.
namespace std
{
template <>
struct hash<OpenXcom::Calypso::CalypsoDiagnosticKey>
{
	std::size_t operator()(const OpenXcom::Calypso::CalypsoDiagnosticKey& k) const
	{
		std::size_t h = std::hash<std::string>()(k.failClass);
		h ^= std::hash<std::uint64_t>()(k.generation) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
		h ^= std::hash<std::uint64_t>()(k.instance) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
		return h;
	}
};
} // namespace std

namespace OpenXcom
{
namespace Calypso
{

class CalypsoDiagnosticGate
{
public:
	explicit CalypsoDiagnosticGate(std::size_t maxKeys = 32) : _maxKeys(maxKeys > 0 ? maxKeys : 1) {}

	/// True iff this tuple was NOT logged since its last recovery/invalidation:
	/// the caller emits exactly one diagnostic for it. Repeated failures return
	/// false (silent) until noteRecovered/invalidate/reset re-arms the tuple.
	bool shouldLog(const CalypsoDiagnosticKey& key)
	{
		if (_logged.count(key)) return false;
		logKey(key);
		return true;
	}

	/// A successful outcome for the same tuple re-arms it (one new diagnostic
	/// per future failure cycle). No-op when the tuple was never logged.
	void noteRecovered(const CalypsoDiagnosticKey& key)
	{
		if (!_logged.count(key)) return;
		_logged.erase(key);
		removeFromOrder(key);
	}

	/// Explicit invalidation: every logged tuple of this generation is
	/// forgotten, so the next failure of that generation may log again.
	void invalidateGeneration(std::uint64_t generation)
	{
		std::vector<CalypsoDiagnosticKey> victims;
		victims.reserve(_order.size());
		for (const CalypsoDiagnosticKey& k : _order)
		{
			if (k.generation == generation) victims.push_back(k);
		}
		for (const CalypsoDiagnosticKey& k : victims)
		{
			_logged.erase(k);
			removeFromOrder(k);
		}
	}

	void reset() { _logged.clear(); _order.clear(); }

	std::size_t size() const { return _logged.size(); }

private:
	void logKey(const CalypsoDiagnosticKey& key)
	{
		_logged.insert(key);
		_order.push_back(key);
		while (_order.size() > _maxKeys)
		{
			const CalypsoDiagnosticKey oldest = _order.front();
			_order.pop_front();
			_logged.erase(oldest);
		}
	}

	void removeFromOrder(const CalypsoDiagnosticKey& key)
	{
		for (auto it = _order.begin(); it != _order.end(); ++it)
		{
			if (*it == key) { _order.erase(it); return; }
		}
	}

	std::size_t _maxKeys;
	std::deque<CalypsoDiagnosticKey> _order;
	std::unordered_set<CalypsoDiagnosticKey> _logged;
};

} // namespace Calypso
} // namespace OpenXcom

