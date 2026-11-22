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

#include "Ufopaedia.h"
#include "ArticleState.h"
#ifdef __EMSCRIPTEN__
#include "../Calypso/CalypsoBaseLedger.h"
#include "../Interface/TextList.h"
#include "../Interface/Text.h"
#endif
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Engine/Surface.h"
#include "../Interface/TextButton.h"
#include "../Mod/ArticleDefinition.h"
#include "../Mod/Mod.h"
#include "../Savegame/SavedGame.h"

namespace OpenXcom
{
	/**
	 * Change index position to next article.
	 */
	void ArticleCommonState::nextArticle()
	{
		if (current_index >= articleList.size() - 1)
		{
			// goto first
			current_index = 0;
		}
		else
		{
			current_index++;
		}
		if (isCurrentArticleHidden())
		{
			nextArticle();
		}
	}

	/**
	 * Change page to next in article or move to next index position.
	 * Each article can have multiple pages, if we are on last page we will move to first page of next article.
	 */
	void ArticleCommonState::nextArticlePage()
	{
		if (!hasNextArticlePage())
		{
			// goto to first page of next article
			nextArticle();
			current_page = 0;
		}
		else
		{
			current_page++;
		}
	}

	bool ArticleCommonState::hasNextArticlePage()
	{
		return !(current_page >= getCurrentArticle()->getNumberOfPages() - 1);
	}

	/**
	 * Change index position to previous article.
	 */
	void ArticleCommonState::prevArticle()
	{
		if (current_index == 0 || current_index > articleList.size() - 1)
		{
			// goto last
			current_index = articleList.size() - 1;
		}
		else
		{
			current_index--;
		}
		if (isCurrentArticleHidden())
		{
			prevArticle();
		}
	}

	/**
	 * Change page to previous in article or move to previous index position.
	 * Each article can have multiple pages, if we are on first page we will move to last page of previous article.
	 */
	void ArticleCommonState::prevArticlePage()
	{
		if (!hasPrevArticlePage())
		{
			// goto last page of previous article
			prevArticle();
			current_page = getCurrentArticle()->getNumberOfPages() - 1;
		}
		else
		{
			current_page--;
		}
	}

	bool ArticleCommonState::hasPrevArticlePage()
	{
		return !(current_page == 0 || current_page > getCurrentArticle()->getNumberOfPages() - 1);
	}

	/**
	 * Constructor
	 * @param game Pointer to current game.
	 * @param article_id The article id of this article state instance.
	 */
	ArticleState::ArticleState(const std::string &article_id, std::shared_ptr<ArticleCommonState> state) : _id(article_id)
	{
		// init background and navigation elements
		_bg = new Surface(320, 200, 0, 0);
		_btnOk = new TextButton(30, 14, 5, 5);
		_btnPrev = new TextButton(30, 14, 40, 5);
		_btnNext = new TextButton(30, 14, 75, 5);
		_btnInfo = new TextButton(40, 14, 110, 5);

		_state = std::move(state);

		// remember this article as seen/normal
		int ruleStatus = _game->getSavedGame()->getUfopediaRuleStatus(_id);
		if (ruleStatus != ArticleDefinition::PEDIA_STATUS_HIDDEN)
		{
			_game->getSavedGame()->setUfopediaRuleStatus(_id, ArticleDefinition::PEDIA_STATUS_NORMAL);
		}
	}

	/**
	 * Destructor
	 */
	ArticleState::~ArticleState()
	{
#ifdef __EMSCRIPTEN__
		delete _hdLedger;
#endif
	}

#ifdef __EMSCRIPTEN__
	std::string ArticleState::articleSprite(Surface *sprite)
	{
		_hdArticleSprite = sprite;
		return "article-sprite:" + _id + "/" + std::to_string(_state->current_page);
	}

	Surface *ArticleState::articleSpriteFor(const std::string &source) const
	{
		return source == _hdIllustration ? _hdArticleSprite : nullptr;
	}

	std::string ArticleState::articleIllustration(const std::string &resource) const
	{
		if (resource.empty() || !_game->getMod()->getSurface(resource, false)) return {};
		return "ruleset-art:" + resource;
	}

	TextList *ArticleState::articleItemDetails(TextList *shots, Text *const (&damageTypes)[3], Text *const (&powers)[3])
	{
		auto *details = new TextList(260, 100, 0, 0);
		add(details);
		details->setColumns(3, 120, 70, 70);
		if (shots && !shots->getCellTextsSnapshot().empty())
		{
			details->addRow(3, tr("STR_SHOT_TYPE").c_str(), tr("STR_ACCURACY_UC").c_str(), tr("STR_TIME_UNIT_COST").c_str());
			for (const auto &row : shots->getCellTextsSnapshot())
				details->addRow(3, row[0]->getText().c_str(), row[1]->getText().c_str(), row[2]->getText().c_str());
		}
		for (size_t i = 0; i < 3; ++i)
		{
			if (!damageTypes[i]->getText().empty())
				details->addRow(3, damageTypes[i]->getText().c_str(), powers[i]->getText().c_str(), "");
		}
		return details;
	}

	void ArticleState::openHdReader(Text &title, Text &body, std::vector<Text *> facts,
		std::vector<TextList *> tables, std::string illustration)
	{
		_hdTitle = &title;
		_hdBody = &body;
		_hdFacts = std::move(facts);
		_hdTables = std::move(tables);
		_hdIllustration = std::move(illustration);
		body.setVisible(false);
		for (auto *list : _hdTables) list->setVisible(false);
		_paragraphs = new TextList(220, 120, 0, 0);
		add(_paragraphs);
		_paragraphs->setColumns(2, 44, 176);
		_hdLedger = new Calypso::CalypsoBaseLedger(*this);
	}

	void ArticleState::resize(int &dX, int &dY)
	{
		if (_hdLedger) _hdLedger->refresh();
		else State::resize(dX, dY);
	}
#endif

	std::string ArticleState::getDamageTypeText(ItemDamageType dt) const
	{
		std::string type;
		switch (dt)
		{
		case DT_NONE:
			type = "STR_DAMAGE_NONE";
			break;
		case DT_AP:
			type = "STR_DAMAGE_ARMOR_PIERCING";
			break;
		case DT_IN:
			type = "STR_DAMAGE_INCENDIARY";
			break;
		case DT_HE:
			type = "STR_DAMAGE_HIGH_EXPLOSIVE";
			break;
		case DT_LASER:
			type = "STR_DAMAGE_LASER_BEAM";
			break;
		case DT_PLASMA:
			type = "STR_DAMAGE_PLASMA_BEAM";
			break;
		case DT_STUN:
			type = "STR_DAMAGE_STUN";
			break;
		case DT_MELEE:
			type = "STR_DAMAGE_MELEE";
			break;
		case DT_ACID:
			type = "STR_DAMAGE_ACID";
			break;
		case DT_SMOKE:
			type = "STR_DAMAGE_SMOKE";
			break;
		case DT_10:
			type = "STR_DAMAGE_10";
			break;
		case DT_11:
			type = "STR_DAMAGE_11";
			break;
		case DT_12:
			type = "STR_DAMAGE_12";
			break;
		case DT_13:
			type = "STR_DAMAGE_13";
			break;
		case DT_14:
			type = "STR_DAMAGE_14";
			break;
		case DT_15:
			type = "STR_DAMAGE_15";
			break;
		case DT_16:
			type = "STR_DAMAGE_16";
			break;
		case DT_17:
			type = "STR_DAMAGE_17";
			break;
		case DT_18:
			type = "STR_DAMAGE_18";
			break;
		case DT_19:
			type = "STR_DAMAGE_19";
			break;
		default:
			type = "STR_UNKNOWN";
			break;
		}
		return type;
	}

	/**
	 * Set captions and click handlers for the common control elements.
	 */
	void ArticleState::initLayout()
	{
		add(_bg);
		add(_btnOk);
		add(_btnPrev);
		add(_btnNext);
		add(_btnInfo);

		_btnOk->setText(tr("STR_OK"));
		_btnOk->onMouseClick((ActionHandler)&ArticleState::btnOkClick);
		_btnOk->onKeyboardPress((ActionHandler)&ArticleState::btnOkClick,Options::keyOk);
		_btnOk->onKeyboardPress((ActionHandler)&ArticleState::btnOkClick,Options::keyCancel);
		_btnOk->onKeyboardPress((ActionHandler)&ArticleState::btnResetMusicClick, Options::keySelectMusicTrack);
		_btnPrev->setText("<<");
		_btnPrev->onMouseClick((ActionHandler)&ArticleState::btnPrevClick);
		_btnPrev->onKeyboardPress((ActionHandler)&ArticleState::btnPrevClick, Options::keyGeoLeft);
		_btnNext->setText(">>");
		_btnNext->onMouseClick((ActionHandler)&ArticleState::btnNextClick);
		_btnNext->onKeyboardPress((ActionHandler)&ArticleState::btnNextClick, Options::keyGeoRight);
		_btnInfo->setText(tr("STR_INFO_UFOPEDIA"));
		_btnInfo->onMouseClick((ActionHandler)&ArticleState::btnInfoClick);
		_btnInfo->onKeyboardPress((ActionHandler)&ArticleState::btnInfoClick, Options::keyGeoUfopedia);
		_btnInfo->setVisible(false);
	}

	/**
	 * Returns to the previous screen.
	 * @param action Pointer to an action.
	 */
	void ArticleState::btnOkClick(Action *)
	{
		_game->popState();
	}

	/**
	 * Resets the music to a random geoscape music.
	 * @param action Pointer to an action.
	 */
	void ArticleState::btnResetMusicClick(Action *)
	{
		// reset that pesky interception music!
		_game->getMod()->playMusic("GMGEO");
	}

	/**
	 * Shows the previous available article.
	 * @param action Pointer to an action.
	 */
	void ArticleState::btnPrevClick(Action *)
	{
		Ufopaedia::prev(_game, _state);
	}

	/**
	 * Shows the next available article. Loops to the first.
	 * @param action Pointer to an action.
	 */
	void ArticleState::btnNextClick(Action *)
	{
		Ufopaedia::next(_game, _state);
	}

	/**
	 * Shows the detailed (raw) information about the current topic.
	 * @param action Pointer to an action.
	 */
	void ArticleState::btnInfoClick(Action *)
	{
		Ufopaedia::openArticleDetail(_game, _id);
	}

}
