#include <cstdint>
#include <cmath>

namespace OpenXcom { namespace Calypso {

inline double clampPriceMod(double m, double floor, double ceil)
{ return m < floor ? floor : (m > ceil ? ceil : m); }

/// Unit price: base cost * counterparty multiplier * demand/price modifier, rounded to money.
inline int64_t marketPrice(int baseCost, double counterpartyMult, double priceMod)
{ return static_cast<int64_t>(std::llround(static_cast<double>(baseCost) * counterpartyMult * priceMod)); }

/// Monthly stock/demand cap: base * catalog multiplier * difficulty multiplier, floored at 0.
inline int stockCap(int base, double catalogMult, double difficultyMult)
{ int v = static_cast<int>(static_cast<double>(base) * catalogMult * difficultyMult); return v < 0 ? 0 : v; }

/// New price modifier after the player sells `qty` units (downward pressure), clamped.
inline double applySellPressure(double mod, int qty, double perUnit, double floor, double ceil)
{ return clampPriceMod(mod - perUnit * qty, floor, ceil); }

/// Monthly relaxation of a price modifier back toward 1.0 by `fraction` (0..1).
inline double decayPriceMod(double mod, double fraction)
{ return mod + (1.0 - mod) * fraction; }

/// Demand/price multiplier while a terror boost is active for the item's category.
inline double terrorMultiplier(bool active, double boost)
{ return active ? 1.0 + boost : 1.0; }

} }
