#ifdef __EMSCRIPTEN__
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
 * Phase 38 (Calypso) -- trade & economy core.
 *
 * Header-only data model for the per-mod economy ruleset (parsed from the
 * `calypsoEconomy:` YAML key) plus the per-campaign runtime state owned by
 * SavedGame (wired in task A2).  All heavy engine types are forward-declared
 * so this header stays cheap to include from Mod.h.
 */

#include <string>
#include <vector>
#include <map>
#include <cstdint>

#include "CalypsoEconomyMath.h"

namespace OpenXcom
{

// Forward declarations only -- definitions pulled in by the .cpp.
class Mod;
class SavedGame;
class Base;
class Country;
class RuleItem;
namespace YAML
{
	class YamlNodeReader;
	class YamlNodeWriter;
}

namespace Calypso
{

// ---- ruleset-driven configuration (parsed from the `calypsoEconomy:` key) ----

/// One side of a counterparty's catalog: which items it trades and the supply/demand knobs.
struct CounterpartyCatalog
{
	std::vector<std::string> categories;   // item category ids
	std::vector<std::string> items;        // explicit item ids
	double stockMult = 1.0;                // for 'sells'
	double demandMult = 1.0;               // for 'buys'
	bool   everything = false;             // black-market 'buys: { everything: true }'
};

/// One country counterparty (or the black market) and its sell/buy catalogs.
struct CounterpartyRules
{
	std::string country;                   // STR_USA ...
	CounterpartyCatalog sells;
	CounterpartyCatalog buys;
};

/// All tunables parsed from the `calypsoEconomy:` node. Defaults yield a sane campaign.
struct EconomyRules
{
	bool enabled = false;                  // KILL-SWITCH: false unless `calypsoEconomy:` present

	// grants
	std::vector<double> grantSchedule { 1.0, 0.6, 0.3 };

	// standing
	int standingInitial = 0;
	int tierHostile = -60, tierDistrusted = -20, tierNeutral = 20, tierPreferred = 60; // upper bounds
	int onContractDelivered = 8;
	int onContractExpired = -12;
	int activityDivisor = 25;

	// contracts
	int    contractsStartMonth = 1;
	int    perTierNeutral = 1, perTierPreferred = 2, perTierTrusted = 3;
	double priceMultMin = 3.0, priceMultMax = 5.0;
	double qtyFactor = 0.75;
	int    deadlineMonths = 2;

	// market
	std::vector<double> difficultyStockMult { 1.3, 1.15, 1.0, 0.85, 0.7 };
	std::vector<CounterpartyRules> counterparties;
	// black market
	double bmBuyMult = 2.5, bmSellMult = 0.4;
	CounterpartyCatalog bmSells;           // survival kit (items)
	// (bm buys everything -> handled in code)

	// dynamics (slice C -- parse now, use later)
	double priceFloor = 0.5, priceCeil = 2.0, sellPressure = 0.02, monthlyDecay = 0.5;
	double terrorDemandBoost = 0.4;
	std::vector<std::string> terrorCategories;
	int    terrorDurationMonths = 1;
};

/// Parse `calypsoEconomy:` node into `out`; returns true and sets out.enabled=true if node present.
bool loadEconomyRules(const YAML::YamlNodeReader& node, EconomyRules& out);

// ---- per-campaign runtime state (owned by SavedGame; wired in task A2) ----

/// One procurement contract offered to / accepted by the player.
struct Contract
{
	int id = 0;
	std::string countryId;
	std::string itemId;
	int qty = 0;
	int64_t rewardTotal = 0;
	int deadlineMonth = 0;
	enum class Status { Offered, Accepted, Delivered, Expired };
	Status status = Status::Offered;
};

/// Counterparty id reserved for the black market pseudo-country.
/// [[maybe_unused]]: consumed by later Calypso tasks (A2 / slice B); each
/// translation unit that includes this header gets its own internal-linkage
/// copy, and the annotation keeps -Wunused-const-variable quiet in the meantime.
[[maybe_unused]] static const char* const BLACK_MARKET = "BLACK_MARKET";

/**
 * Per-campaign economy runtime: grant schedule, per-country standing, contracts,
 * monthly counterparty stock/demand accounting, dynamic price modifiers, and
 * terror-site demand boosts. Owned by SavedGame; serialised via load()/save().
 */
class Economy
{
public:
	Economy() = default;

	// Rules binding: the EconomyRules live inside Mod, which is not available at every call
	// site (e.g. Country::newMonth only sees a const SavedGame*). The rules pointer is bound
	// once where a Mod IS available (onNewMonth / SavedGame::load) and cached for the rest of
	// the tick. It is intentionally NOT serialised -- it is a non-owning view into Mod config.
	void bindRules(const EconomyRules* r) { _rules = r; }
	bool active() const { return _rules && _rules->enabled; }
	const EconomyRules& rules() const { return *_rules; }   // only call when active()

	/// Bind ruleset config + seed each country's grant base (idempotent). Safe to call
	/// on every campaign entry (Geoscape ctor) and every monthly tick.
	void ensureInitialized(SavedGame* save, const Mod* mod);

	// monthly tick (implemented incrementally; A1 = expiry+standing only, contracts/market are later tasks)
	void onNewMonth(SavedGame* save, const Mod* mod);

	// grants
	void seedGrantBase(const std::string& countryId, int funding); // capture campaign-start funding once
	int  grantForMonth(const std::string& countryId, int monthsPassed, const EconomyRules& r) const;

	// standing
	int          getStanding(const std::string& countryId) const;
	StandingTier getTier(const std::string& countryId, const EconomyRules& r) const;
	void         addStanding(const std::string& countryId, int delta); // clamp [-100,100]

	// contracts (declared; bodies land in task 38.2)
	const std::vector<Contract>& getContracts() const { return _contracts; }
	bool accept(int contractId);
	bool deliver(int contractId, Base* base, SavedGame* save, const EconomyRules& r);

	// counterparty market (declared; bodies land in slice B) -- keep simple stubs returning safe defaults
	bool    sellsToPlayer(const std::string& cp, const RuleItem* item, const EconomyRules& r) const;
	bool    buysFromPlayer(const std::string& cp, const RuleItem* item, const EconomyRules& r) const;

	// dynamic events (slice C)
	void onTerrorSite(const std::string& regionId, const EconomyRules& r);

	// persistence
	void load(const YAML::YamlNodeReader& reader);
	void save(YAML::YamlNodeWriter writer) const;

private:
	int offersForTier(StandingTier tier, const EconomyRules& r) const;   // contract offers for a tier (0 for hostile/distrusted)

	const EconomyRules* _rules = nullptr;   // non-owning, not serialised

	std::map<std::string,int>     _grantBase;   // countryId -> campaign-start funding
	std::map<std::string,int>     _standing;    // countryId -> [-100,100]
	std::vector<Contract>         _contracts;
	int                           _nextContractId = 1;
	std::map<std::string,int>     _stockUsed;    // "cp/item" -> qty this month
	std::map<std::string,int>     _demandUsed;   // "cp/item" -> qty this month
	std::map<std::string,double>  _priceMods;    // itemId -> modifier
	struct TerrorBoost { std::string region; int expires; };
	std::vector<TerrorBoost>      _terrorBoosts;
};

} // namespace Calypso
} // namespace OpenXcom

#endif /* __EMSCRIPTEN__ */
