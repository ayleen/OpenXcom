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
 * Phase 38 (Calypso) -- trade & economy ruleset parser + runtime state.
 *
 * A1 scope: parse the `calypsoEconomy:` ruleset node into EconomyRules, round-
 * trip the per-campaign Economy state, and implement the monthly tick's
 * contract-expiry / standing-decay path. Contract generation and the
 * counterparty market are stubbed for later slices.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoEconomy.h"

#include <algorithm>
#include <set>
#include <utility>

#include "../Engine/RNG.h"
#include "../Engine/Yaml.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleCountry.h"
#include "../Mod/RuleItem.h"
#include "../Mod/RuleManufacture.h"
#include "../Savegame/Base.h"
#include "../Savegame/Country.h"
#include "../Savegame/ItemContainer.h"
#include "../Savegame/SavedGame.h"

#include "CalypsoContractGen.h"
#include "CalypsoMarketMath.h"

namespace OpenXcom
{
namespace Calypso
{

/**
 * Parse the `calypsoEconomy:` node into `out`. Returns false (and leaves
 * `out.enabled == false`) when the node is absent -- this is the kill-switch
 * that keeps the feature off for unmodified rulesets.
 *
 * Every field is optional; defaults set in the EconomyRules struct apply
 * whenever a key is missing. Subtrees are guarded so an absent group does not
 * clear a sibling group's parsed values.
 */
bool loadEconomyRules(const YAML::YamlNodeReader& node, EconomyRules& out)
{
	if (!node)
		return false;
	out.enabled = true;

	// ---- grants ----
	if (auto grants = node["grants"])
	{
		grants["schedule"].tryReadVal<std::vector<double> >(out.grantSchedule);
	}

	// ---- standing ----
	if (auto standing = node["standing"])
	{
		standing["initial"].tryReadVal<int>(out.standingInitial);
		standing["tierHostile"].tryReadVal<int>(out.tierHostile);
		standing["tierDistrusted"].tryReadVal<int>(out.tierDistrusted);
		standing["tierNeutral"].tryReadVal<int>(out.tierNeutral);
		standing["tierPreferred"].tryReadVal<int>(out.tierPreferred);
		standing["onContractDelivered"].tryReadVal<int>(out.onContractDelivered);
		standing["onContractExpired"].tryReadVal<int>(out.onContractExpired);
		standing["activityDivisor"].tryReadVal<int>(out.activityDivisor);
	}

	// ---- contracts ----
	if (auto contracts = node["contracts"])
	{
		contracts["startMonth"].tryReadVal<int>(out.contractsStartMonth);
		contracts["perTierNeutral"].tryReadVal<int>(out.perTierNeutral);
		contracts["perTierPreferred"].tryReadVal<int>(out.perTierPreferred);
		contracts["perTierTrusted"].tryReadVal<int>(out.perTierTrusted);
		contracts["priceMultMin"].tryReadVal<double>(out.priceMultMin);
		contracts["priceMultMax"].tryReadVal<double>(out.priceMultMax);
		contracts["qtyFactor"].tryReadVal<double>(out.qtyFactor);
		contracts["deadlineMonths"].tryReadVal<int>(out.deadlineMonths);
	}

	// ---- market (counterparties + difficulty + black market) ----
	if (auto market = node["market"])
	{
		market["baseStock"].tryReadVal<int>(out.baseStock);
		market["baseDemand"].tryReadVal<int>(out.baseDemand);
		market["difficultyStockMult"].tryReadVal<std::vector<double> >(out.difficultyStockMult);

		if (auto cps = market["counterparties"])
		{
			for (const auto& cpNode : cps.children())
			{
				CounterpartyRules cp;
				cpNode["country"].tryReadVal<std::string>(cp.country);

				if (auto sells = cpNode["sells"])
				{
					sells["categories"].tryReadVal<std::vector<std::string> >(cp.sells.categories);
					sells["items"].tryReadVal<std::vector<std::string> >(cp.sells.items);
					sells["stockMult"].tryReadVal<double>(cp.sells.stockMult);
				}
				if (auto buys = cpNode["buys"])
				{
					buys["categories"].tryReadVal<std::vector<std::string> >(cp.buys.categories);
					buys["items"].tryReadVal<std::vector<std::string> >(cp.buys.items);
					buys["demandMult"].tryReadVal<double>(cp.buys.demandMult);
					buys["everything"].tryReadVal<bool>(cp.buys.everything);
				}
				out.counterparties.push_back(std::move(cp));
			}
		}

		if (auto bm = market["blackMarket"])
		{
			bm["buyMult"].tryReadVal<double>(out.bmBuyMult);
			bm["sellMult"].tryReadVal<double>(out.bmSellMult);
			if (auto bmSells = bm["sells"])
			{
				bmSells["categories"].tryReadVal<std::vector<std::string> >(out.bmSells.categories);
				bmSells["items"].tryReadVal<std::vector<std::string> >(out.bmSells.items);
				bmSells["stockMult"].tryReadVal<double>(out.bmSells.stockMult);
			}
		}
	}

	// ---- dynamics (slice C: parsed now, used later) ----
	if (auto dynamics = node["dynamics"])
	{
		dynamics["priceFloor"].tryReadVal<double>(out.priceFloor);
		dynamics["priceCeil"].tryReadVal<double>(out.priceCeil);
		dynamics["sellPressure"].tryReadVal<double>(out.sellPressure);
		dynamics["monthlyDecay"].tryReadVal<double>(out.monthlyDecay);
		dynamics["terrorDemandBoost"].tryReadVal<double>(out.terrorDemandBoost);
		dynamics["terrorCategories"].tryReadVal<std::vector<std::string> >(out.terrorCategories);
		dynamics["terrorDurationMonths"].tryReadVal<int>(out.terrorDurationMonths);
	}

	return true;
}

// ---- grants ----

void Economy::seedGrantBase(const std::string& countryId, int funding)
{
	// Capture the campaign-start funding once; a second seed for the same
	// country (e.g. after a reload) must not overwrite the original baseline.
	if (_grantBase.find(countryId) == _grantBase.end())
		_grantBase[countryId] = funding;
}

int Economy::grantForMonth(const std::string& countryId, int monthsPassed, const EconomyRules& r) const
{
	auto it = _grantBase.find(countryId);
	int base = (it != _grantBase.end()) ? it->second : 0;
	return Calypso::grantForMonth(base, monthsPassed, r.grantSchedule);
}

// ---- standing ----

int Economy::getStanding(const std::string& countryId) const
{
	auto it = _standing.find(countryId);
	return (it != _standing.end()) ? it->second : 0;
}

StandingTier Economy::getTier(const std::string& countryId, const EconomyRules& r) const
{
	return Calypso::tierFor(getStanding(countryId),
		StandingThresholds{ r.tierHostile, r.tierDistrusted, r.tierNeutral, r.tierPreferred });
}

void Economy::addStanding(const std::string& countryId, int delta)
{
	_standing[countryId] = Calypso::clampStanding(getStanding(countryId) + delta);
}

// Number of contract offers a tier earns per month (0 for hostile/distrusted).
int Economy::offersForTier(StandingTier tier, const EconomyRules& r) const
{
	switch (tier)
	{
		case StandingTier::Neutral:   return r.perTierNeutral;
		case StandingTier::Preferred: return r.perTierPreferred;
		case StandingTier::Trusted:   return r.perTierTrusted;
		default:                      return 0;   // Hostile / Distrusted: no contracts
	}
}

// ---- contracts ----

bool Economy::accept(int contractId)
{
	for (auto& c : _contracts)
	{
		if (c.id == contractId && c.status == Contract::Status::Offered)
		{
			c.status = Contract::Status::Accepted;
			return true;
		}
	}
	return false;
}

bool Economy::deliver(int contractId, Base* base, SavedGame* save, const EconomyRules& r)
{
	if (!base || !save) return false;
	for (auto& c : _contracts)
	{
		if (c.id != contractId || c.status != Contract::Status::Accepted) continue;
		ItemContainer* store = base->getStorageItems();
		if (store->getItem(c.itemId) < c.qty) return false;   // not enough in this base
		store->removeItem(c.itemId, c.qty);
		save->setFunds(save->getFunds() + c.rewardTotal);
		addStanding(c.countryId, r.onContractDelivered);
		c.status = Contract::Status::Delivered;
		return true;
	}
	return false;
}

// ---- counterparty market (slice B) ----

// File-scope helper: does this catalog match the given item? Used by both the
// sells/buys-side counterparty queries and the black-market survival kit.
static bool catalogMatches(const CounterpartyCatalog& cat, const RuleItem* item)
{
	if (cat.everything) return true;
	for (const std::string& id : cat.items) if (id == item->getType()) return true;
	for (const std::string& c : cat.categories)
		for (const std::string& ic : item->getCategories())
			if (c == ic) return true;
	return false;
}

const CounterpartyRules* Economy::findCounterparty(const std::string& cp) const
{
	if (!_rules) return nullptr;
	for (const CounterpartyRules& c : _rules->counterparties)
		if (c.country == cp) return &c;
	return nullptr;
}

double Economy::priceMod(const std::string& itemId) const
{
	auto it = _priceMods.find(itemId);
	return (it != _priceMods.end()) ? it->second : 1.0;
}

bool Economy::sellsToPlayer(const std::string& cp, const RuleItem* item, const EconomyRules& r) const
{
	if (cp == BLACK_MARKET) return catalogMatches(r.bmSells, item);
	if (getTier(cp, r) < StandingTier::Neutral) return false;   // below Neutral: they stop selling
	const CounterpartyRules* c = findCounterparty(cp);
	return c && catalogMatches(c->sells, item);
}

bool Economy::buysFromPlayer(const std::string& cp, const RuleItem* item, const EconomyRules& r) const
{
	if (cp == BLACK_MARKET) return true;                        // black market buys anything
	if (getTier(cp, r) <= StandingTier::Hostile) return false;  // Hostile: they stop buying
	const CounterpartyRules* c = findCounterparty(cp);
	return c && catalogMatches(c->buys, item);
}

int Economy::getStock(const std::string& cp, const RuleItem* item, const SavedGame* save, const EconomyRules& r) const
{
	if (cp == BLACK_MARKET) return 1000000;   // survival floor -- never stock-limited
	const CounterpartyRules* c = findCounterparty(cp);
	if (!c) return 0;
	int di = static_cast<int>(save->getDifficulty());
	double dm = (di >= 0 && di < static_cast<int>(r.difficultyStockMult.size())) ? r.difficultyStockMult[di] : 1.0;
	int cap = stockCap(r.baseStock, c->sells.stockMult, dm);
	auto it = _stockUsed.find(cp + "/" + item->getType());
	int used = (it != _stockUsed.end()) ? it->second : 0;
	int rem = cap - used;
	return rem < 0 ? 0 : rem;
}

int Economy::getDemand(const std::string& cp, const RuleItem* item, const SavedGame* save, const EconomyRules& r) const
{
	if (cp == BLACK_MARKET) return 1000000;
	const CounterpartyRules* c = findCounterparty(cp);
	if (!c) return 0;
	int di = static_cast<int>(save->getDifficulty());
	double dm = (di >= 0 && di < static_cast<int>(r.difficultyStockMult.size())) ? r.difficultyStockMult[di] : 1.0;
	int cap = stockCap(r.baseDemand, c->buys.demandMult, dm);
	auto it = _demandUsed.find(cp + "/" + item->getType());
	int used = (it != _demandUsed.end()) ? it->second : 0;
	int rem = cap - used;
	return rem < 0 ? 0 : rem;
}

int64_t Economy::buyPrice(const std::string& cp, const RuleItem* item, const EconomyRules& r) const
{
	double mult = (cp == BLACK_MARKET) ? r.bmBuyMult : 1.0;
	return marketPrice(item->getBuyCost(), mult, priceMod(item->getType()));
}

int64_t Economy::sellPrice(const std::string& cp, const RuleItem* item, const EconomyRules& r) const
{
	double mult = (cp == BLACK_MARKET) ? r.bmSellMult : 1.0;
	return marketPrice(item->getSellCost(), mult, priceMod(item->getType()));
}

void Economy::recordPurchase(const std::string& cp, const RuleItem* item, int qty)
{
	if (cp == BLACK_MARKET || qty <= 0) return;   // black market never depletes
	_stockUsed[cp + "/" + item->getType()] += qty;
}

void Economy::recordSale(const std::string& cp, const RuleItem* item, int qty, const EconomyRules& r)
{
	if (qty <= 0) return;
	const std::string& id = item->getType();
	// price pressure applies to ALL sales (global per-item modifier).
	_priceMods[id] = applySellPressure(priceMod(id), qty, r.sellPressure, r.priceFloor, r.priceCeil);
	if (cp == BLACK_MARKET) return;               // black market has no monthly demand cap
	_demandUsed[cp + "/" + id] += qty;
}

// ---- dynamic events (slice C) ----

void Economy::onTerrorSite(const std::string& /*regionId*/, const EconomyRules& /*r*/)
{
	// TODO(slice C): register a TerrorBoost expiring after terrorDurationMonths.
}

// ---- monthly tick ----

// File-scope helper: does this counterparty buy the given item? Match by explicit
// item id OR by category intersection. (Phase 38 -- buys-catalog match.)
static bool issuerBuys(const CounterpartyRules& cp, const RuleItem* item)
{
	for (const std::string& id : cp.buys.items)
		if (id == item->getType()) return true;
	for (const std::string& cat : cp.buys.categories)
		for (const std::string& ic : item->getCategories())
			if (cat == ic) return true;
	return false;
}

void Economy::ensureInitialized(SavedGame* save, const Mod* mod)
{
	if (!save || !mod) return;
	bindRules(&mod->getCalypsoEconomyRules());
	if (!active()) return;
	for (auto* c : *save->getCountries())
		seedGrantBase(c->getRules()->getType(), c->getFunding().back());
}

void Economy::onNewMonth(SavedGame* save, const Mod* mod)
{
	if (!save || !mod) return;
	ensureInitialized(save, mod);
	if (!active()) return;                       // kill-switch: no calypsoEconomy: key -> no-op
	const EconomyRules& r = rules();
	const int now = save->getMonthsPassed();

	// Fold last month's per-country activity into standing (read BEFORE Country::newMonth resets it).
	for (auto* c : *save->getCountries())
	{
		const std::string& id = c->getRules()->getType();
		if (r.activityDivisor > 0 && !c->getActivityXcom().empty() && !c->getActivityAlien().empty())
		{
			const int dx = c->getActivityXcom().back() / r.activityDivisor;
			const int da = c->getActivityAlien().back() / r.activityDivisor;
			if (dx - da != 0) addStanding(id, dx - da);
		}
	}

	// 1. Build the set of pacted-country ids once -- used by both the expiry
	//    loop (P7: pacted conglomerates cancel Accepted contracts without a
	//    standing penalty) and the generation loop (pacted issuers offer none).
	std::set<std::string> pacted;
	for (auto* c : *save->getCountries())
	{
		if (c->getPact()) pacted.insert(c->getRules()->getType());
	}

	// 2. Expire overdue contracts.
	for (auto& c : _contracts)
	{
		if (c.deadlineMonth > now) continue;
		if (c.status == Contract::Status::Accepted)
		{
			c.status = Contract::Status::Expired;
			if (!pacted.count(c.countryId)) addStanding(c.countryId, r.onContractExpired);
		}
		else if (c.status == Contract::Status::Offered)
		{
			c.status = Contract::Status::Expired;
		}
	}

	// 3. Contract generation (P4-deterministic). Precompute campaign-wide
	//    inputs ONCE: total engineers, the pacted set (built above), and the
	//    union of available productions across all bases (dedup by pointer).
	if (now >= r.contractsStartMonth)
	{
		int totalEngineers = 0;
		std::set<RuleManufacture*> prodSet;
		for (auto* b : *save->getBases())
		{
			totalEngineers += b->getTotalEngineers();
			std::vector<RuleManufacture*> tmp;
			save->getAvailableProductions(tmp, mod, b);
			for (auto* p : tmp) prodSet.insert(p);
		}

		// Iterate issuers in RULESET order (P4 determinism) -- not map order.
		for (const CounterpartyRules& cp : r.counterparties)
		{
			const std::string& issuerId = cp.country;
			if (pacted.count(issuerId)) continue;            // P7: pacted -> no offers, no RNG consumed
			int offers = offersForTier(getTier(issuerId, r), r);
			if (offers <= 0) continue;                       // no RNG consumed

			// Build candidate list from the producible items the issuer buys.
			std::vector<ContractCandidate> cands;
			for (RuleManufacture* p : prodSet)
			{
				for (const auto& kv : p->getProducedItems())
				{
					const RuleItem* item = kv.first;
					if (item->getSellCost() > 0 && issuerBuys(cp, item))
					{
						ContractCandidate cc;
						cc.itemId = item->getType();
						cc.sellCost = item->getSellCost();
						cc.manufactureTime = p->getManufactureTime();
						cands.push_back(std::move(cc));
					}
				}
			}
			// DEDUP + SORT by itemId -- getProducedItems is a map keyed by
			// RuleItem* whose iteration order is pointer-based / non-deterministic.
			std::sort(cands.begin(), cands.end(),
				[](const ContractCandidate& a, const ContractCandidate& b){ return a.itemId < b.itemId; });
			cands.erase(std::unique(cands.begin(), cands.end(),
				[](const ContractCandidate& a, const ContractCandidate& b){ return a.itemId == b.itemId; }),
				cands.end());
			if (cands.empty()) continue;

			// Generate offers. RNG functors wrap engine RNG; the pure core
			// picks THEN prices each offer, so global RNG advances in a fixed order.
			auto gen = generateContracts(cands, offers, r.qtyFactor, totalEngineers,
				r.priceMultMin, r.priceMultMax,
				[](int n){ return RNG::generate(0, n - 1); },
				[](double lo, double hi){ return RNG::generate(lo, hi); });

			for (const GeneratedContract& g : gen)
			{
				Contract ct;
				ct.id = _nextContractId++;
				ct.countryId = issuerId;
				ct.itemId = g.itemId;
				ct.qty = g.qty;
				ct.rewardTotal = g.reward;
				ct.deadlineMonth = now + r.deadlineMonths;
				ct.status = Contract::Status::Offered;
				_contracts.push_back(std::move(ct));
			}
		}
	}
	// Monthly market refresh: clear per-counterparty stock/demand usage, relax price mods toward 1.0.
	_stockUsed.clear();
	_demandUsed.clear();
	for (auto& kv : _priceMods)
		kv.second = decayPriceMod(kv.second, r.monthlyDecay);
}

// ---- persistence ----

void Economy::load(const YAML::YamlNodeReader& reader)
{
	reader.tryRead("grantBase", _grantBase);
	reader.tryRead("standing", _standing);
	reader.tryRead("nextContractId", _nextContractId);

	_contracts.clear();
	if (auto contractsNode = reader["contracts"])
	{
		for (const auto& c : contractsNode.children())
		{
			Contract ct;
			c["id"].tryReadVal<int>(ct.id);
			c["country"].tryReadVal<std::string>(ct.countryId);
			c["item"].tryReadVal<std::string>(ct.itemId);
			c["qty"].tryReadVal<int>(ct.qty);
			c["reward"].tryReadVal<int64_t>(ct.rewardTotal);
			c["deadline"].tryReadVal<int>(ct.deadlineMonth);
			int status = 0;
			c["status"].tryReadVal<int>(status);
			ct.status = (Contract::Status)status;
			_contracts.push_back(std::move(ct));
		}
	}

	reader.tryRead("stockUsed", _stockUsed);
	reader.tryRead("demandUsed", _demandUsed);
	reader.tryRead("priceMods", _priceMods);

	_terrorBoosts.clear();
	if (auto terrorNode = reader["terrorBoosts"])
	{
		for (const auto& t : terrorNode.children())
		{
			TerrorBoost tb;
			t["region"].tryReadVal<std::string>(tb.region);
			t["expires"].tryReadVal<int>(tb.expires);
			_terrorBoosts.push_back(std::move(tb));
		}
	}

	// nextContractId must always be a valid forward-looking id (>= 1).
	if (_nextContractId < 1)
		_nextContractId = 1;
}

void Economy::save(YAML::YamlNodeWriter writer) const
{
	writer.setAsMap();
	writer.write("grantBase", _grantBase);
	writer.write("standing", _standing);
	writer.write("nextContractId", _nextContractId);

	if (!_contracts.empty())
	{
		YAML::YamlNodeWriter seq = writer["contracts"];
		seq.setAsSeq();
		for (const Contract& c : _contracts)
		{
			YAML::YamlNodeWriter w = seq.write();
			w.setAsMap();
			w.write("id", c.id);
			w.write("country", c.countryId);
			w.write("item", c.itemId);
			w.write("qty", c.qty);
			w.write("reward", c.rewardTotal);
			w.write("deadline", c.deadlineMonth);
			w.write("status", (int)c.status);
		}
	}

	writer.write("stockUsed", _stockUsed);
	writer.write("demandUsed", _demandUsed);
	writer.write("priceMods", _priceMods);

	if (!_terrorBoosts.empty())
	{
		YAML::YamlNodeWriter seq = writer["terrorBoosts"];
		seq.setAsSeq();
		for (const TerrorBoost& t : _terrorBoosts)
		{
			YAML::YamlNodeWriter w = seq.write();
			w.setAsMap();
			w.write("region", t.region);
			w.write("expires", t.expires);
		}
	}
}

} // namespace Calypso
} // namespace OpenXcom

#endif /* __EMSCRIPTEN__ */
