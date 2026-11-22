#ifdef __EMSCRIPTEN__
#include "ContainmentReviewState.h"
#include "ManageAlienContainmentState.h"
#include "../Calypso/CalypsoBaseLedger.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Engine/Unicode.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextList.h"
#include "../Mod/Armor.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleItem.h"
#include "../Mod/RuleResearch.h"
#include "../Mod/Unit.h"
#include "../Savegame/Base.h"
#include "../Savegame/ItemContainer.h"
#include "../Savegame/ResearchProject.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Transfer.h"

namespace OpenXcom
{
ContainmentReviewState::ContainmentReviewState(ManageAlienContainmentState &owner, ContainmentAction action)
	: _owner(owner), _action(action)
{
	_screen = true;
	setInterface("manageContainment");
	_title = new Text(280, 16);
	_rows = new TextList(284, 100);
	_confirm = new TextButton(80, 16);
	_cancel = new TextButton(80, 16);
	add(_title, "text", "manageContainment");
	add(_rows, "list", "manageContainment");
	add(_confirm, "button", "manageContainment");
	add(_cancel, "button", "manageContainment");
	_title->setText(tr(action == ContainmentAction::Cleanup ? "STR_PRISON_CLEANUP" : "STR_CAL_CONTAINMENT_REVIEW"));
	_rows->setColumns(3, 150, 34, 100);
	_rows->setSelectable(true);
	_confirm->setText(tr(action == ContainmentAction::Cleanup ? "STR_PRISON_CLEANUP" :
		action == ContainmentAction::Sell ? "STR_SELL_SELECTED" : "STR_KILL_SELECTED"));
	_confirm->onMouseClick((ActionHandler)&ContainmentReviewState::confirm);
	_cancel->setText(tr("STR_CANCEL"));
	_cancel->onMouseClick((ActionHandler)&ContainmentReviewState::cancel);
	_cancel->onKeyboardPress((ActionHandler)&ContainmentReviewState::cancel, Options::keyCancel);
	_review = collectReview();
	showRows();
	_ledger = new Calypso::CalypsoBaseLedger(*this);
}

std::vector<ContainmentReviewState::Row> ContainmentReviewState::collectReview()
{
	std::vector<Row> result;
	auto *base = _owner._base;
	const int type = _owner._prisonType;
	_free = base->getAvailableContainment(type) - base->getUsedContainment(type);
	_proceeds = 0;
	if (_action == ContainmentAction::Cleanup)
	{
		for (const auto *project : base->getResearch())
		{
			const auto *research = project->getRules();
			const auto *rule = research->getNeededItem();
			if (research->isHoldingNeededItem() && rule && rule->isAlien() && rule->getPrisonType() == type)
				result.push_back({tr(research->getName()), "1", tr("STR_CAL_CANCEL_STUDY")});
		}
		for (const auto *transfer : *base->getTransfers())
		{
			const auto *rule = transfer->getItems();
			if (transfer->getType() == TRANSFER_ITEM && rule && rule->isAlien() && rule->getPrisonType() == type)
				result.push_back({transfer->getName(_game->getLanguage()), std::to_string(transfer->getQuantity()), tr("STR_CAL_END_SHIPMENT")});
		}
		return result;
	}
	double storesDelta = 0;
	for (size_t i = 0; i < _owner._qtys.size(); ++i)
	{
		const int quantity = _owner._qtys[i];
		if (quantity <= 0) continue;
		const auto *rule = _game->getMod()->getItem(_owner._aliens[i], true);
		if (base->getStorageItems()->getItem(rule) < quantity) return {};
		_free += quantity;
		storesDelta -= rule->getSize() * quantity;
		std::string outcome = tr("STR_CAL_REMOVED");
		if (_action == ContainmentAction::Sell)
		{
			const int64_t amount = static_cast<int64_t>(quantity) * rule->getSellCostAdjusted(base, _game->getSavedGame());
			_proceeds += amount;
			outcome = Unicode::formatFunding(amount);
		}
		else if (const auto *unit = _game->getMod()->getUnit(_owner._aliens[i], false))
		{
			const auto *remains = unit->getArmor()->getCorpseGeoscape();
			if (remains && remains->isRecoverable() && remains->isCorpseRecoverable())
			{
				outcome = tr(remains->getType());
				storesDelta += remains->getSize() * quantity;
			}
		}
		result.push_back({tr(rule->getType()), std::to_string(quantity), outcome});
	}
	if (Options::storageLimitsEnforced && base->storesOverfull(storesDelta) && !result.empty())
		result.push_back({tr("STR_STORES"), "", tr("STR_CAL_STORES_OVER")});
	return result;
}

void ContainmentReviewState::showRows()
{
	_rows->clearList();
	for (const auto &row : _review)
		_rows->addRow(3, row[0].c_str(), row[1].c_str(), row[2].c_str());
	_confirm->setVisible(!_review.empty());
}

void ContainmentReviewState::confirm(Action *)
{
	if (_game->getTopState() != this || _review.empty()) return;
	const int previousFree = _free;
	const auto previousProceeds = _proceeds;
	auto current = collectReview();
	if (_review != current || previousFree != _free || previousProceeds != _proceeds)
	{
		_review = std::move(current);
		showRows();
		return;
	}
	_game->popState();
	if (_action == ContainmentAction::Cleanup)
	{
		_owner._base->cleanupPrisons(_owner._prisonType);
		_owner.resetListAndTotals();
	}
	else
		_owner.dealWithSelectedAliens(_action == ContainmentAction::Sell);
}

void ContainmentReviewState::cancel(Action *)
{
	_owner._doNotReset = true;
	_game->popState();
}

ContainmentReviewState::~ContainmentReviewState() { delete _ledger; }
void ContainmentReviewState::resize(int &dX, int &dY)
{
	if (_ledger) _ledger->refresh();
	else State::resize(dX, dY);
}
}
#endif
