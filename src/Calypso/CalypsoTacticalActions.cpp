#ifdef __EMSCRIPTEN__
#include "../Battlescape/ActionMenuItem.h"
#include "../Battlescape/ActionMenuState.h"
#include "../Battlescape/BattlescapeGame.h"
#include "../Battlescape/MedikitState.h"
#include "../Battlescape/MedikitView.h"
#include "../Battlescape/PrimeGrenadeState.h"
#include "../Battlescape/UnitInfoState.h"
#include "../Engine/Action.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Interface/Bar.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextList.h"
#include "../Interface/Window.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleItem.h"
#include "../Savegame/BattleItem.h"
#include "../Savegame/BattleUnit.h"
#include "../Savegame/SavedGame.h"
#include "CalypsoStrategicList.h"

namespace OpenXcom
{
void ActionMenuState::openHdMenu()
{
	_screen = true;
	for (auto* item : _actionMenu)
		item->setVisible(false);
	auto* window = new Window(this, 1, 1, 0, 0);
	auto* title = new Text(1, 1, 0, 0);
	auto* cancel = new TextButton(1, 1, 0, 0);
	_hdRows = new TextList(1, 1, 0, 0);
	add(window, "messageWindowBorder", "battlescape");
	add(title, "messageWindows", "battlescape");
	add(_hdRows, "optionLists", "battlescape");
	add(cancel, "messageWindowButtons", "battlescape");
	title->setText(_hdSkills ? std::string(tr("STR_CAL_TACTICAL_SKILLS")) : std::string(tr(_action->weapon->getRules()->getType())));
	cancel->setText(tr("STR_CANCEL_UC"));
	cancel->onMouseClick((ActionHandler)&ActionMenuState::cancelHdMenu);
	_hdRows->setColumns(4, 150, 65, 60, 45);
	_hdRows->setBackground(window);
	_hdRows->setSelectable(true);
	_hdRows->onMouseClick((ActionHandler)&ActionMenuState::chooseHdRow);
	for (const auto& row : _hdChoices)
		_hdRows->addRow(4, row.label.c_str(), row.accuracy.c_str(), row.cost.c_str(), row.key == SDLK_UNKNOWN ? "" : SDL_GetKeyName(row.key));
	using List = Calypso::CalypsoStrategicList;
	_hdList = new List(this, nullptr, _game->getMod(), window, title, _hdRows, cancel,
					   _hdSkills ? List::Content::Skills : List::Content::Actions);
	enableUiScaling(_hdList->designWidth(), _hdList->designHeight(), 1.0f, false);
	_hdList->fitInput();
}

void ActionMenuState::chooseHdRow(Action* action)
{
	const auto row = _hdRows->getSelectedRow();
	if (row >= _hdChoices.size())
		return;
	action->setSender(_actionMenu[row]);
	btnActionMenuItemClick(action);
}

void ActionMenuState::cancelHdMenu(Action*) { _game->popState(); }

void PrimeGrenadeState::openHdTimer()
{
	_screen = true;
	for (auto* surface : _surfaces)
		surface->setVisible(false);
	auto* window = new Window(this, 1, 1, 0, 0);
	auto* cancel = new TextButton(1, 1, 0, 0);
	_hdTimers = new TextList(1, 1, 0, 0);
	add(window, "messageWindowBorder", "battlescape");
	add(_hdTimers, "optionLists", "battlescape");
	add(cancel, "messageWindowButtons", "battlescape");
	cancel->setText(tr("STR_CANCEL_UC"));
	cancel->onMouseClick((ActionHandler)&PrimeGrenadeState::cancelTimer);
	_hdTimers->setColumns(8, 30, 30, 30, 30, 30, 30, 30, 30);
	_hdTimers->setBackground(window);
	_hdTimers->setSelectable(true);
	_hdTimers->onMouseClick((ActionHandler)&PrimeGrenadeState::chooseTimer);
	for (int row = 0; row < 3; ++row)
	{
		std::array<std::string, 8> cell;
		for (int col = 0; col < 8; ++col)
			cell[col] = std::to_string(row * 8 + col);
		_hdTimers->addRow(8, cell[0].c_str(), cell[1].c_str(), cell[2].c_str(), cell[3].c_str(),
						  cell[4].c_str(), cell[5].c_str(), cell[6].c_str(), cell[7].c_str());
	}
	_hdList = new Calypso::CalypsoStrategicList(this, nullptr, _game->getMod(), window, _title,
												_hdTimers, cancel, Calypso::CalypsoStrategicList::Content::Timer);
	enableUiScaling(_hdList->designWidth(), _hdList->designHeight(), 1.0f, false);
	_hdList->fitInput();
}

void PrimeGrenadeState::chooseTimer(Action* action)
{
	const auto row = _hdTimers->getSelectedRow();
	const int column = _hdList->columnAt(action->getAbsoluteXMouse());
	if (row >= 3 || column < 0)
		return;
	action->setSender(_button[row * 8 + column]);
	btnClick(action);
}
void PrimeGrenadeState::cancelTimer(Action*)
{
	if (!_inInventoryView)
		_action->value = -1;
	_game->popState();
}

void UnitInfoState::openHdSheet()
{
	_exit->setVisible(false);
	auto* window = new Window(this, 1, 1, 0, 0);
	auto* close = new TextButton(1, 1, 0, 0);
	_hdRows = new TextList(1, 1, 0, 0);
	add(window, "messageWindowBorder", "battlescape");
	add(_hdRows, "optionLists", "battlescape");
	add(close, "messageWindowButtons", "battlescape");
	close->setText(tr("STR_CAL_UNIT_CLOSE"));
	close->onMouseClick((ActionHandler)&UnitInfoState::exitClick);
	close->onKeyboardPress((ActionHandler)&UnitInfoState::exitClick, Options::keyCancel);
	close->onKeyboardPress((ActionHandler)&UnitInfoState::exitClick, Options::keyBattleStats);
	if (!_mindProbe)
	{
		_btnPrev->setText(tr("STR_CAL_UNIT_PREVIOUS"));
		_btnNext->setText(tr("STR_CAL_UNIT_NEXT"));
	}
	_hdRows->setColumns(3, 160, 80, 80);
	_hdRows->setBackground(window);
	using List = Calypso::CalypsoStrategicList;
	_hdList = new List(this, nullptr, _game->getMod(), window, _txtName, _hdRows, close,
					   List::Content::UnitInfo, _mindProbe ? nullptr : _btnNext, _mindProbe ? nullptr : _btnPrev);
	enableUiScaling(_hdList->designWidth(), _hdList->designHeight(), 1.0f, false);
	_hdList->fitInput();
}

void UnitInfoState::updateHdSheet()
{
	_hdRows->clearList();
	auto row = [this](Text* label, Text* value, Bar* bar)
	{
		const std::string base = bar ? std::to_string(int(bar->getMax())) : "";
		_hdRows->addRow(3, label->getText().c_str(), value->getText().c_str(), base.c_str());
	};
	row(_txtTimeUnits, _numTimeUnits, _barTimeUnits);
	row(_txtEnergy, _numEnergy, _barEnergy);
	row(_txtHealth, _numHealth, _barHealth);
	_hdRows->addRow(3, tr("STR_STUN_LEVEL_CURRENT").c_str(), std::to_string(_unit->getStunlevel()).c_str(), "");
	row(_txtFatalWounds, _numFatalWounds, nullptr);
	row(_txtMorale, _numMorale, _barMorale);
	row(_txtBravery, _numBravery, _barBravery);
	row(_txtReactions, _numReactions, _barReactions);
	row(_txtFiring, _numFiring, _barFiring);
	row(_txtThrowing, _numThrowing, _barThrowing);
	row(_txtMelee, _numMelee, _barMelee);
	row(_txtStrength, _numStrength, _barStrength);
	if (_game->getMod()->isManaFeatureEnabled() && _txtMana->getVisible())
		row(_txtMana, _numMana, _barMana);
	if (_txtPsiStrength->getVisible())
		row(_txtPsiStrength, _numPsiStrength, _barPsiStrength);
	if (_txtPsiSkill->getVisible())
		row(_txtPsiSkill, _numPsiSkill, _barPsiSkill);
	row(_txtFrontArmor, _numFrontArmor, _barFrontArmor);
	row(_txtLeftArmor, _numLeftArmor, _barLeftArmor);
	row(_txtRightArmor, _numRightArmor, _barRightArmor);
	row(_txtRearArmor, _numRearArmor, _barRearArmor);
	row(_txtUnderArmor, _numUnderArmor, _barUnderArmor);
}

void MedikitState::openHdSheet()
{
	_screen = true;
	for (auto* surface : _surfaces)
		surface->setVisible(false);
	auto* window = new Window(this, 1, 1, 0, 0);
	auto* title = new Text(1, 1, 0, 0);
	auto* close = new TextButton(1, 1, 0, 0);
	_hdRows = new TextList(1, 1, 0, 0);
	add(window, "messageWindowBorder", "battlescape");
	add(title, "messageWindows", "battlescape");
	add(_hdRows, "optionLists", "battlescape");
	add(close, "messageWindowButtons", "battlescape");
	title->setText(std::string(tr(_action->weapon->getRules()->getType())) + " · " + _targetUnit->getName(_game->getLanguage()));
	close->setText(tr("STR_CAL_UNIT_CLOSE"));
	close->onMouseClick((ActionHandler)&MedikitState::onEndClick);
	close->onKeyboardPress((ActionHandler)&MedikitState::onEndClick, Options::keyCancel);
	close->onKeyboardPress((ActionHandler)&MedikitState::onPainKillerClick, SDLK_1);
	close->onKeyboardPress((ActionHandler)&MedikitState::onStimulantClick, SDLK_2);
	close->onKeyboardPress((ActionHandler)&MedikitState::onHealClick, SDLK_3);
	_hdRows->setColumns(3, 140, 70, 110);
	_hdRows->setSelectable(true);
	_hdRows->setBackground(window);
	_hdRows->onMouseClick((ActionHandler)&MedikitState::selectHdRow);
	using List = Calypso::CalypsoStrategicList;
	_hdList = new List(this, nullptr, _game->getMod(), window, title, _hdRows, close, List::Content::Medikit);
	enableUiScaling(_hdList->designWidth(), _hdList->designHeight(), 1.0f, false);
	_hdList->fitInput();
}

void MedikitState::updateHdSheet()
{
	const auto scroll = _hdRows->getScroll();
	_hdRows->clearList();
	const auto* item = _action->weapon;
	const auto* rules = item->getRules();
	const std::string cost = tr("STR_TIME_UNITS_SHORT").arg(_action->Time);
	auto row = [this](const std::string& label, int value, const std::string& detail = {})
	{
		_hdRows->addRow(3, label.c_str(), std::to_string(value).c_str(), detail.c_str());
	};
	row(tr(rules->getPainKillerActionName()), item->getPainKillerQuantity(), cost);
	row(tr(rules->getStimulantActionName()), item->getStimulantQuantity(), cost);
	row(tr(rules->getHealActionName()), item->getHealQuantity(), cost);
	for (int part = 0; part < BODYPART_MAX; ++part)
	{
		auto label = std::string(tr(PARTS_STRING[part]));
		if (part == _medikitView->getSelectedPart())
			label += " · " + std::string(tr("STR_CAL_SELECTED_CONTACT"));
		row(label, _targetUnit->getFatalWound(UnitBodyPart(part)), tr("STR_FATAL_WOUNDS"));
	}
	row(tr("STR_HEALTH"), _targetUnit->getHealth());
	row(tr("STR_STUN_LEVEL_CURRENT"), _targetUnit->getStunlevel());
	row(tr("STR_MORALE"), _targetUnit->getMorale());
	row(tr("STR_ENERGY"), _targetUnit->getEnergy());
	if (_game->getMod()->isManaFeatureEnabled() && _game->getSavedGame()->isManaUnlocked(_game->getMod()))
		row(tr("STR_MANA"), _targetUnit->getMana());
	_hdRows->scrollTo(scroll);
}

void MedikitState::selectHdRow(Action* action)
{
	const auto row = _hdRows->getSelectedRow();
	if (row == 0)
		onPainKillerClick(action);
	else if (row == 1)
		onStimulantClick(action);
	else if (row == 2)
		onHealClick(action);
	else if (row < 3 + BODYPART_MAX)
	{
		_medikitView->selectPart(int(row) - 3);
		updateHdSheet();
	}
}

}
#endif
