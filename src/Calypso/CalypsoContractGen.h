#pragma once
/*
 * Copyright 2010-2016 OpenXcom Developers. (GPLv3)
 *
 * Phase 38 (Calypso) -- pure, deterministic procurement-contract generator.
 * Dependency-free so the native doctest suite exercises the real algorithm.
 * The engine adapter (CalypsoEconomy.cpp) filters producible/catalog-matching
 * items into ContractCandidate[] and supplies RNG functors wrapping RNG::generate.
 */
#include <vector>
#include <string>
#include <cstdint>
#include "CalypsoEconomyMath.h"   // contractQty, contractReward

namespace OpenXcom
{
namespace Calypso
{

/// One producible, catalog-matching item eligible to be ordered.
struct ContractCandidate
{
	std::string itemId;
	int sellCost = 0;         // RuleItem::getSellCost()
	int manufactureTime = 0;  // RuleManufacture::getManufactureTime() (engineer-hours)
};

/// A generated contract offer (pre-id; the engine assigns ids).
struct GeneratedContract
{
	std::string itemId;
	int qty = 0;
	int64_t reward = 0;
	bool operator==(const GeneratedContract& o) const
	{ return itemId == o.itemId && qty == o.qty && reward == o.reward; }
};

/**
 * Generate `offers` contract offers from `candidates`.
 * Deterministic given the RNG functors: for each offer, in order, it picks a
 * candidate index then a price multiplier -- so the caller's global RNG advances
 * in a fixed order (Phase 38 pitfall P4).
 *   pickIndex(n)          -> int in [0, n-1]
 *   priceMult(lo, hi)     -> double in [lo, hi]
 * qty comes from contractQty (bounded to ~monthly capacity, so contracts stay
 * fulfillable, P3); reward from contractReward. Returns [] if no candidates / offers<=0.
 */
template <class PickIndex, class PriceMult>
std::vector<GeneratedContract> generateContracts(
	const std::vector<ContractCandidate>& candidates,
	int offers, double qtyFactor, int totalEngineers,
	double priceMultMin, double priceMultMax,
	PickIndex pickIndex, PriceMult priceMult)
{
	std::vector<GeneratedContract> out;
	if (candidates.empty() || offers <= 0) return out;
	for (int i = 0; i < offers; ++i)
	{
		int idx = pickIndex(static_cast<int>(candidates.size()));
		if (idx < 0 || idx >= static_cast<int>(candidates.size())) continue;
		const ContractCandidate& c = candidates[idx];
		int qty = contractQty(qtyFactor, totalEngineers, c.manufactureTime);
		double pm = priceMult(priceMultMin, priceMultMax);
		GeneratedContract gc;
		gc.itemId = c.itemId;
		gc.qty = qty;
		gc.reward = contractReward(qty, c.sellCost, pm);
		out.push_back(gc);
	}
	return out;
}

} // namespace Calypso
} // namespace OpenXcom
