#pragma once
/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * Phase 38 (Calypso) -- pure economy math, dependency-free so the native
 * doctest suite can exercise the real formulas (CalypsoEconomy.cpp delegates
 * here). No engine, YAML, or GL includes may ever be added to this header.
 */
#include <cmath>
#include <cstdint>
#include <vector>

namespace OpenXcom
{
namespace Calypso
{

/// Standing tier, derived from an integer standing via ruleset thresholds.
enum class StandingTier { Hostile, Distrusted, Neutral, Preferred, Trusted };

/// Upper bounds for the first four tiers (Trusted = above `preferred`).
struct StandingThresholds { int hostile; int distrusted; int neutral; int preferred; };

/// Ruleset-owned buy/sell multipliers for each standing tier.
struct StandingPriceMultipliers
{
	double buyHostile = 1.0;
	double buyDistrusted = 1.0;
	double buyNeutral = 1.0;
	double buyPreferred = 1.0;
	double buyTrusted = 1.0;
	double sellHostile = 1.0;
	double sellDistrusted = 1.0;
	double sellNeutral = 1.0;
	double sellPreferred = 1.0;
	double sellTrusted = 1.0;
};

/// Invalid or non-positive ruleset multipliers fail closed to neutral pricing.
inline double normalizeStandingPriceMultiplier(double multiplier)
{
	return std::isfinite(multiplier) && multiplier > 0.0 ? multiplier : 1.0;
}

inline double standingBuyMultiplier(StandingTier tier,
	const StandingPriceMultipliers& multipliers)
{
	switch (tier)
	{
		case StandingTier::Distrusted: return multipliers.buyDistrusted;
		case StandingTier::Neutral:    return multipliers.buyNeutral;
		case StandingTier::Preferred:  return multipliers.buyPreferred;
		case StandingTier::Trusted:    return multipliers.buyTrusted;
		default:                       return multipliers.buyHostile;
	}
}

inline double standingSellMultiplier(StandingTier tier,
	const StandingPriceMultipliers& multipliers)
{
	switch (tier)
	{
		case StandingTier::Distrusted: return multipliers.sellDistrusted;
		case StandingTier::Neutral:    return multipliers.sellNeutral;
		case StandingTier::Preferred:  return multipliers.sellPreferred;
		case StandingTier::Trusted:    return multipliers.sellTrusted;
		default:                       return multipliers.sellHostile;
	}
}

/// Grant for a given campaign month: base * schedule[monthsPassed], 0 past the schedule.
inline int grantForMonth(int base, int monthsPassed, const std::vector<double>& schedule)
{
	if (monthsPassed < 0) monthsPassed = 0;
	if (monthsPassed >= static_cast<int>(schedule.size())) return 0;
	return static_cast<int>(base * schedule[monthsPassed]);
}

/// Map an integer standing to a tier using upper-bound thresholds.
inline StandingTier tierFor(int standing, const StandingThresholds& t)
{
	if (standing <= t.hostile)     return StandingTier::Hostile;
	if (standing <= t.distrusted)  return StandingTier::Distrusted;
	if (standing <= t.neutral)     return StandingTier::Neutral;
	if (standing <= t.preferred)   return StandingTier::Preferred;
	return StandingTier::Trusted;
}

/// Clamp a standing value to the valid range.
inline int clampStanding(int v)
{
	if (v < -100) return -100;
	if (v >  100) return  100;
	return v;
}

/// Contract quantity: qtyFactor of the monthly manufacture capacity, clamped [1,20].
/// 720 = engineer-hours per month; manufactureTime is in engineer-hours.
inline int contractQty(double qtyFactor, int totalEngineers, int manufactureTime)
{
	if (manufactureTime <= 0) return 1;
	int q = static_cast<int>(qtyFactor * totalEngineers * 720.0 / manufactureTime);
	if (q < 1)  q = 1;
	if (q > 20) q = 20;
	return q;
}

/// Total contract reward, fixed at generation.
inline int64_t contractReward(int qty, int sellCost, double priceMult)
{
	return static_cast<int64_t>(static_cast<double>(qty) * sellCost * priceMult);
}

} // namespace Calypso
} // namespace OpenXcom
