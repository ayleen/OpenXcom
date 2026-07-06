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

#include <utility>

#include "../Engine/Yaml.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleCountry.h"
#include "../Savegame/Country.h"
#include "../Savegame/SavedGame.h"

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

bool Economy::deliver(int /*contractId*/, Base* /*base*/, SavedGame* /*save*/, const EconomyRules& /*r*/)
{
	// TODO(38.2): move items from base stockpile into the contract, pay the
	// reward, flip status to Delivered, and apply onContractDelivered standing.
	return false;
}

// ---- counterparty market (slice B) ----

bool Economy::sellsToPlayer(const std::string& /*cp*/, const RuleItem* /*item*/, const EconomyRules& /*r*/) const
{
	// TODO(38.4): consult the counterparty's sells catalog + stock accounting.
	return true;
}

bool Economy::buysFromPlayer(const std::string& /*cp*/, const RuleItem* /*item*/, const EconomyRules& /*r*/) const
{
	// TODO(38.4): consult the counterparty's buys catalog + demand accounting.
	return true;
}

// ---- dynamic events (slice C) ----

void Economy::onTerrorSite(const std::string& /*regionId*/, const EconomyRules& /*r*/)
{
	// TODO(slice C): register a TerrorBoost expiring after terrorDurationMonths.
}

// ---- monthly tick ----

void Economy::onNewMonth(SavedGame* save, const Mod* mod)
{
	if (!save || !mod) return;
	bindRules(&mod->getCalypsoEconomyRules());
	if (!active()) return;                       // kill-switch: no calypsoEconomy: key -> no-op
	const EconomyRules& r = rules();
	const int now = save->getMonthsPassed();

	// 1. Seed each country's campaign-start funding ONCE (idempotent) and fold last
	//    month's per-country activity into standing. Read activity BEFORE Country::newMonth
	//    (which runs later this tick in MonthlyReportState) resets it.
	for (auto* c : *save->getCountries())
	{
		const std::string& id = c->getRules()->getType();
		seedGrantBase(id, c->getFunding().back());
		if (r.activityDivisor > 0 && !c->getActivityXcom().empty() && !c->getActivityAlien().empty())
		{
			const int dx = c->getActivityXcom().back() / r.activityDivisor;
			const int da = c->getActivityAlien().back() / r.activityDivisor;
			if (dx - da != 0) addStanding(id, dx - da);
		}
	}

	// 2. Expire overdue contracts.
	for (auto& c : _contracts)
	{
		if (c.deadlineMonth > now) continue;
		if (c.status == Contract::Status::Accepted)
		{
			c.status = Contract::Status::Expired;
			addStanding(c.countryId, r.onContractExpired);
		}
		else if (c.status == Contract::Status::Offered)
		{
			c.status = Contract::Status::Expired;
		}
	}
	// TODO(38.2): contract generation.  TODO(slice B): market stock/demand refresh.
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
