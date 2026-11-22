#ifdef __EMSCRIPTEN__
#include "../Basescape/BaseInfoState.h"
#include "../Basescape/BasescapeState.h"
#include "../Basescape/GlobalAlienContainmentState.h"
#include "../Basescape/ManageAlienContainmentState.h"
#include "../Basescape/MiniBaseView.h"
#include "../Basescape/MonthlyCostsState.h"
#include "CalypsoBaseLedger.h"
#include "../Engine/Action.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Engine/Unicode.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextList.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleBaseFacility.h"
#include "../Mod/RuleItem.h"
#include "../Mod/RuleResearch.h"
#include "../Savegame/Base.h"
#include "../Savegame/ResearchProject.h"
#include "../Savegame/SavedGame.h"

namespace OpenXcom
{
void BaseInfoState::changeBase(Action *action)
{
	const auto &bases = *_game->getSavedGame()->getBases();
	if (bases.empty()) return;
	auto it = std::find(bases.begin(), bases.end(), _base);
	if (it == bases.end()) return;
	const auto index = static_cast<size_t>(it - bases.begin());
	const auto next = (index + (action->getSender() == _nextBase ? 1 : bases.size() - 1)) % bases.size();
	_mini->setSelectedBase(next);
	_base = bases[next];
	if (_state) _state->setBase(_base);
	init();
}

void BaseInfoState::refreshHdRows()
{
	_hdRows->clearList();
	auto add = [&](const Text *label, bool capacity, int current, int total)
	{
		const auto excess = capacity && current > total ? std::to_string(current - total) : std::string();
		_hdRows->addRow(5, label->getText().c_str(),
			std::string(tr(capacity ? "STR_CAL_BASE_USED" : "STR_CAL_BASE_AVAILABLE")).c_str(),
			std::to_string(current).c_str(), std::to_string(total).c_str(), excess.c_str());
	};
	add(_txtSoldiers, false, _base->getAvailableSoldiers(), _base->getTotalSoldiers());
	add(_txtEngineers, false, _base->getAvailableEngineers(), _base->getTotalEngineers());
	add(_txtScientists, false, _base->getAvailableScientists(), _base->getTotalScientists());
	add(_txtQuarters, true, _base->getUsedQuarters(), _base->getAvailableQuarters());
	add(_txtStores, true, static_cast<int>(floor(_base->getUsedStores() + 0.05)), _base->getAvailableStores());
	add(_txtLaboratories, true, _base->getUsedLaboratories(), _base->getAvailableLaboratories());
	add(_txtWorkshops, true, _base->getUsedWorkshops(), _base->getAvailableWorkshops());
	if (Options::storageLimitsEnforced)
		add(_txtContainment, true, _base->getUsedContainment(0), _base->getAvailableContainment(0));
	add(_txtHangars, true, _base->getUsedHangars(), _base->getAvailableHangars());
}

void BaseInfoState::resize(int &dX, int &dY)
{
	if (_hdLedger) _hdLedger->refresh();
	else State::resize(dX, dY);
}

void GlobalAlienContainmentState::fillBaseOverview()
{
	struct Location
	{
		Base *base;
		int type, held, capacity, study;
	};
	std::set<int> types{0};
	for (const auto &name : _game->getMod()->getBaseFacilitiesList())
	{
		const auto *rule = _game->getMod()->getBaseFacility(name);
		if (rule->getAliens() > 0) types.insert(rule->getPrisonType());
	}
	// A mod may remove its facility while a save still carries a specimen.
	for (const auto &name : _game->getMod()->getItemsList())
	{
		const auto *rule = _game->getMod()->getItem(name, true);
		if (rule->isAlien()) types.insert(rule->getPrisonType());
	}
	std::vector<Location> rows;
	_hdHeld = _hdCapacity = _hdStudy = 0;
	for (auto *base : *_game->getSavedGame()->getBases())
	{
		const auto before = rows.size();
		for (int type : types)
		{
			const int held = base->getUsedContainment(type);
			const int capacity = base->getAvailableContainment(type);
			if (!held && !capacity) continue;
			int study = 0;
			for (const auto *project : base->getResearch())
			{
				const auto *research = project->getRules();
				const auto *item = research->getNeededItem();
				if (research->isHoldingNeededItem() && item && item->isAlien() && item->getPrisonType() == type) ++study;
			}
			rows.push_back({base, type, held, capacity, study});
			_hdHeld += held;
			_hdCapacity += capacity;
			_hdStudy += study;
		}
		if (rows.size() == before) rows.push_back({base, 0, 0, 0, 0});
	}
	std::stable_sort(rows.begin(), rows.end(), [](const Location &a, const Location &b)
	{
		return a.capacity - a.held < b.capacity - b.held;
	});
	_topics.clear();
	_lstPrisoners->clearList();
	for (const auto &r : rows)
	{
		auto label = r.base->getName(_game->getLanguage());
		if (types.size() > 1) label += " · " + std::string(trAlt("STR_PRISON_TYPE", r.type));
		_lstPrisoners->addRow(5, label.c_str(), std::to_string(r.held).c_str(),
			std::to_string(r.capacity).c_str(), std::to_string(r.capacity - r.held).c_str(), std::to_string(r.study).c_str());
		_topics.emplace_back("", r.base, r.type);
	}
}

void GlobalAlienContainmentState::resize(int &dX, int &dY)
{
	if (_hdLedger) _hdLedger->refresh();
	else State::resize(dX, dY);
}

void MonthlyCostsState::openCostLedger()
{
	_screen = true;
	_hdRows = new TextList(300, 110, 0, 0);
	add(_hdRows, "list", "costsInfo");
	_hdRows->setColumns(4, 125, 70, 44, 61);
	_hdRows->setSelectable(true);
	auto copyRows = [&](TextList *source)
	{
		for (const auto &cells : source->getCellTextsSnapshot())
		{
			if (cells.size() == 4)
				_hdRows->addRow(4, cells[0]->getText().c_str(), cells[1]->getText().c_str(),
					cells[2]->getText().c_str(), cells[3]->getText().c_str());
			else if (cells.size() == 2)
				_hdRows->addRow(4, cells[0]->getText().c_str(), "", "", cells[1]->getText().c_str());
		}
	};
	copyRows(_lstCrafts);
	copyRows(_lstSalaries);
	copyRows(_lstMaintenance);
	auto *save = _game->getSavedGame();
	auto addTotal = [&](const char *key, int64_t amount)
	{
		_hdRows->addRow(4, tr(key).c_str(), "", "", Unicode::formatFunding(amount).c_str());
	};
	addTotal("STR_CAL_BASE_MONTHLY_TOTAL", _base->getMonthlyMaintenace());
	addTotal("STR_CAL_CAMPAIGN_INCOME", save->getCountryFunding());
	addTotal("STR_CAL_CAMPAIGN_COSTS", save->getBaseMaintenance());
	addTotal("STR_CAL_CAMPAIGN_BALANCE", save->getCountryFunding() - save->getBaseMaintenance());
	_hdLedger = new Calypso::CalypsoBaseLedger(*this);
}
}
#endif
