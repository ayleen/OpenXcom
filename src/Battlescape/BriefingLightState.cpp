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
#include "BriefingLightState.h"
#ifdef __EMSCRIPTEN__
#include "../Calypso/CalypsoStrategicList.h"
#include "../Calypso/CalypsoReportLines.h"
#include "../Mod/RuleEnviroEffects.h"
#include "../Mod/RuleStartingCondition.h"
#endif
#include <vector>
#include "../Engine/Game.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/TextButton.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Interface/ToggleTextButton.h"
#include "../Interface/Window.h"
#include "../Mod/Mod.h"
#include "../Mod/AlienDeployment.h"
#include "../Mod/Armor.h"
#include "../Mod/ArticleDefinition.h"
#include "../Mod/RuleStartingCondition.h"
#include "../Savegame/SavedGame.h"
#include "../Engine/Options.h"
#include "../Engine/Screen.h"
#include "../Engine/Unicode.h"
#include "../Ufopaedia/Ufopaedia.h"

namespace OpenXcom
{

/**
 * Initializes all the elements in the BriefingLight screen.
 * @param deployment Pointer to the mission deployment.
 */
BriefingLightState::BriefingLightState(AlienDeployment *deployment)
{
	_screen = true;
	// Create objects
	_window = new Window(this, 320, 200, 0, 0);
	_btnOk = new TextButton(140, 18, 164, 164);
	_btnArmors = new ToggleTextButton(140, 18, 16, 164);
	_txtTitle = new Text(300, 32, 16, 24);
	_txtBriefing = new Text(288, 104, 16, 56);
	_txtArmors = new Text(288, 25, 16, 56);
	_lstArmors = new TextList(288, 80, 8, 81);

	std::string title = deployment->getType();
	std::string desc = deployment->getAlertDescription();

	BriefingData data = deployment->getBriefingData();
	setStandardPalette("PAL_GEOSCAPE", data.palette);

#ifndef __EMSCRIPTEN__
	_window->setBackground(_game->getMod()->getSurface(data.background));
#endif

	add(_window, "window", "briefing");
	add(_btnOk, "button", "briefing");
	add(_btnArmors, "button", "briefing");
	add(_txtTitle, "text", "briefing");
	add(_txtBriefing, "text", "briefing");
	add(_txtArmors, "text", "briefing");
	add(_lstArmors, "text", "briefing");

	centerAllSurfaces();

	// Set up objects
	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&BriefingLightState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&BriefingLightState::btnOkClick, Options::keyOk);
	_btnOk->onKeyboardPress((ActionHandler)&BriefingLightState::btnOkClick, Options::keyCancel);

	_btnArmors->setText(tr("STR_WHAT_CAN_I_WEAR"));
	_btnArmors->onMouseClick((ActionHandler)&BriefingLightState::btnArmorsClick);
	_btnArmors->setVisible(false);

	_txtTitle->setBig();
	_txtTitle->setText(tr(title));

	_txtBriefing->setWordWrap(true);
	_txtBriefing->setText(tr(desc));

	_txtArmors->setWordWrap(true);
	_txtArmors->setVisible(false);

	_lstArmors->setColumns(2, 148, 132);
	_lstArmors->setSelectable(true);
	_lstArmors->setBackground(_window);
	_lstArmors->setMargin(8);
	_lstArmors->setVisible(false);

	checkStartingCondition(deployment);

#ifdef __EMSCRIPTEN__
	_txtBriefing->setVisible(false);
	_btnOk->setText(tr("STR_CAL_BRIEF_CLOSE"));
	_hdBody = new TextList(1, 1, 0, 0);
	add(_hdBody, "text", "briefing");
	_hdBody->setColumns(1, 1);
	_hdBody->setBackground(_window);
	_hdList = new Calypso::CalypsoStrategicList(this, nullptr, _game->getMod(), _window, _txtTitle,
		_hdBody, _btnOk, Calypso::CalypsoStrategicList::Content::TargetBriefing, _btnArmors);
	_hdBody->setSelectable(true);
	_hdBody->onMouseClick((ActionHandler)&BriefingLightState::lstArmorsClick);
	_hdList->setArticleRows(_articleRows);
	layoutBriefing(true);
#else
	_lstArmors->onMouseClick((ActionHandler)&BriefingLightState::lstArmorsClick, SDL_BUTTON_MIDDLE);
#endif
}

/**
* Checks the starting condition.
*/
void BriefingLightState::checkStartingCondition(AlienDeployment *deployment)
{
	const RuleStartingCondition *startingCondition = _game->getMod()->getStartingCondition(deployment->getStartingCondition());
	if (startingCondition != 0)
	{
		auto& list = startingCondition->getForbiddenArmors().empty() ? startingCondition->getAllowedArmors() : startingCondition->getForbiddenArmors();
		const std::string messageCode = startingCondition->getForbiddenArmors().empty() ? "STR_STARTING_CONDITION_ARMORS_ALLOWED" : "STR_STARTING_CONDITION_ARMORS_FORBIDDEN";
		if (!list.empty())
		{
			_txtArmors->setText(tr(messageCode).arg("")); // passing empty argument, because it is obsolete since a list display was introduced
			_btnArmors->setVisible(true);

			for (auto& armorType : list)
			{
				Armor* armor = _game->getMod()->getArmor(armorType, false);
				ArticleDefinition* article = _game->getMod()->getUfopaediaArticle(armor ? armor->getUfopediaType() : armorType, false);
				if (article && Ufopaedia::isArticleAvailable(_game->getSavedGame(), article))
				{
					std::string translation = tr(armorType);
					_armorNameList.push_back(std::make_pair(armorType, translation));
				}
			}
			if (_armorNameList.empty())
			{
				// no suitable armor yet
				std::string translation = tr("STR_UNKNOWN");
				_armorNameList.push_back(std::make_pair("STR_UNKNOWN", translation));
			}
			std::sort(_armorNameList.begin(), _armorNameList.end(), [&](std::pair<std::string, std::string>& a, std::pair<std::string, std::string>& b) { return Unicode::naturalCompare(a.second, b.second); });
			if (_armorNameList.size() % 2 != 0)
			{
				_armorNameList.push_back(std::make_pair("", "")); // just padding, we want an even number of items in the list
			}
			size_t halfSize = _armorNameList.size() / 2;
			for (size_t i = 0; i < halfSize; ++i)
			{
				_lstArmors->addRow(2, _armorNameList[i].second.c_str(), _armorNameList[i + halfSize].second.c_str());
			}
		}
	}
}

/**
 *
 */
BriefingLightState::~BriefingLightState()
{
#ifdef __EMSCRIPTEN__
	delete _hdList;
#endif
}

/**
 * Closes the window.
 * @param action Pointer to an action.
 */
void BriefingLightState::btnOkClick(Action *)
{
	_game->popState();
}

/**
 * Shows allowed armors.
 * @param action Pointer to an action.
 */
void BriefingLightState::btnArmorsClick(Action *)
{
#ifdef __EMSCRIPTEN__
	_btnArmors->setText(tr(_btnArmors->getPressed() ? "STR_BRIEFING" : "STR_WHAT_CAN_I_WEAR"));
	layoutBriefing(false);
	_hdBody->scrollTo(0);
#else
	_txtArmors->setVisible(_btnArmors->getPressed());
	_lstArmors->setVisible(_btnArmors->getPressed());
	_txtBriefing->setVisible(!_btnArmors->getPressed());
#endif
}

/**
 * Shows corresponding Ufopaedia article.
 * @param action Pointer to an action.
 */
void BriefingLightState::lstArmorsClick(Action* action)
{
#ifdef __EMSCRIPTEN__
	(void)action;
	const auto row = _hdBody->getSelectedRow();
	if (!_btnArmors->getPressed() || row >= _articleRows.size() || _articleRows[row].empty()) return;
	std::string articleId = _articleRows[row];
	Ufopaedia::openArticle(_game, articleId);
#else
	size_t halfSize = _armorNameList.size() / 2;

	double mx = action->getAbsoluteXMouse();
	if (mx < _btnOk->getX())
	{
		halfSize = 0;
	}

	auto idx = halfSize + _lstArmors->getSelectedRow();
	if (idx >= _armorNameList.size() || _armorNameList[idx].first.empty()) return;
	const std::string& armorType = _armorNameList[idx].first;
	Armor* armor = _game->getMod()->getArmor(armorType, false);
	if (armor)
	{
		std::string articleId = armor->getUfopediaType();
		Ufopaedia::openArticle(_game, articleId);
	}
#endif
}

#ifdef __EMSCRIPTEN__
void BriefingLightState::layoutBriefing(bool initial)
{
	const auto scroll = _hdBody->getScroll();
	if (!initial) _hdList->layout();
	_hdBody->clearList();
	auto append = [&](const std::string &text) {
		for (const auto &line : Calypso::calypsoWrapReportText(*_game->getMod(), text, _hdList->firstColumnWidth(), _hdList->bodyFontSize()))
			_hdBody->addRow(1, line.c_str());
	};
	_articleRows.clear();
	const bool restrictions = _btnArmors->getPressed();
	const auto &description = restrictions ? _txtArmors->getText() : _txtBriefing->getText();
	if (description.empty()) append(tr("STR_CAL_BRIEF_NO_INTELLIGENCE"));
	else append(description);
	_articleRows.resize(_hdBody->getTexts());
	if (restrictions) for (const auto &entry : _armorNameList)
	{
		if (entry.first.empty()) continue;
		const auto *armor = _game->getMod()->getArmor(entry.first, false);
		const std::string articleId = armor ? armor->getUfopediaType() : entry.first;
		auto *article = _game->getMod()->getUfopaediaArticle(articleId, false);
		const bool known = article && Ufopaedia::isArticleAvailable(_game->getSavedGame(), article);
		append(entry.second);
		_articleRows.resize(_hdBody->getTexts(), known ? articleId : std::string());
	}
	if (initial) enableUiScaling(_hdList->designWidth(), _hdList->designHeight(), 1.0f, false);
	else recaptureUiScaling(_hdList->designWidth(), _hdList->designHeight(), 1.0f, false);
	_hdList->fitInput();
	_hdBody->scrollTo(scroll);
}

void BriefingLightState::resize(int &dx, int &dy)
{
	if (_hdList) layoutBriefing(false);
	else State::resize(dx, dy);
}
#endif

}
