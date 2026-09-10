#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace OpenXcom { namespace Calypso {

enum class MarketSide { Buy, Sell };

struct MarketOffer
{
	std::string counterpartyId;
	std::string displayNameKey;
	StandingTier standing;
	int64_t unitPrice;
	int remaining;
	bool blackMarket;
};

struct MarketAllocation
{
	std::string counterpartyId;
	std::string displayNameKey;
	int quantity;
	int64_t unitPrice;
	int64_t subtotal;
};

/// Deterministically allocate a request across eligible quote entries.
inline std::vector<MarketAllocation> allocateMarketOrder(
	const std::vector<MarketOffer>& offers,
	MarketSide side,
	int requested,
	bool includeBlackMarket)
{
	std::vector<const MarketOffer*> eligible;
	if (requested <= 0) return {};
	for (const auto& offer : offers)
		if (offer.remaining > 0 && (includeBlackMarket || !offer.blackMarket))
			eligible.push_back(&offer);

	std::sort(eligible.begin(), eligible.end(), [side](const MarketOffer* a, const MarketOffer* b) {
		if (a->unitPrice != b->unitPrice)
			return side == MarketSide::Buy
				? a->unitPrice < b->unitPrice
				: a->unitPrice > b->unitPrice;
		if (a->blackMarket != b->blackMarket) return !a->blackMarket;
		if (a->counterpartyId != b->counterpartyId)
			return a->counterpartyId < b->counterpartyId;
		return a->displayNameKey < b->displayNameKey;
	});

	std::vector<MarketAllocation> result;
	int remaining = requested;
	for (const MarketOffer* offer : eligible)
	{
		if (remaining <= 0) break;
		const int quantity = std::min(remaining, offer->remaining);
		result.push_back({
			offer->counterpartyId,
			offer->displayNameKey,
			quantity,
			offer->unitPrice,
			offer->unitPrice * quantity
		});
		remaining -= quantity;
	}
	return result;
}

/// Unit price: base cost * counterparty multiplier * demand/price modifier, rounded to money.
inline int64_t marketPrice(int baseCost, double counterpartyMult, double priceMod)
{ return static_cast<int64_t>(std::llround(static_cast<double>(baseCost) * counterpartyMult * priceMod)); }

inline double clampPriceMod(double m, double floor, double ceil)
{ return m < floor ? floor : (m > ceil ? ceil : m); }

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
