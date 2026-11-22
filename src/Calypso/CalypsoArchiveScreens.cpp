#ifdef __EMSCRIPTEN__
#include "CalypsoBaseLedger.h"
#include "CalypsoHdImageSource.h"
#include "CalypsoReportLines.h"
#include "../Engine/Action.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Engine/Unicode.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextEdit.h"
#include "../Interface/TextList.h"
#include "../Mod/ArticleDefinition.h"
#include "../Mod/Mod.h"
#include "../Savegame/SavedGame.h"
#include "../Ufopaedia/ArticleState.h"
#include "../Ufopaedia/StatsForNerdsState.h"
#include "../Ufopaedia/Ufopaedia.h"
#include "../Ufopaedia/UfopaediaSelectState.h"
#include "../Ufopaedia/UfopaediaStartState.h"
#include "../Interface/ComboBox.h"
#include "../Interface/ToggleTextButton.h"
#include "../Interface/Window.h"

namespace OpenXcom
{
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

void StatsForNerdsState::openHd()
{
	_window->finishPopupForHd();
	_screen = true;
	_cbxRelatedStuff->setX(-4096);
	_hdRelated = new TextButton(1, 1, 0, 0);
	add(_hdRelated, "button", "statsForNerds");
	_hdRelated->onMouseClick((ActionHandler)&StatsForNerdsState::chooseRelated);
	_hdLinks = new TextList(220, 120, 0, 0);
	add(_hdLinks, "list", "statsForNerds");
	_hdLinks->setColumns(2, 65, 155);
	_hdLinks->setSelectable(true);
	_hdLinks->setVisible(false);
	_hdLinks->onMouseClick((ActionHandler)&StatsForNerdsState::openRelatedRow);
	_hdLedger = new Calypso::CalypsoBaseLedger(*this);
}

void StatsForNerdsState::chooseRelated(Action *)
{
	if (Options::oxceDisableStatsForNerds || !_cbxRelatedStuff->getVisible()) return;
	_choosingRelated = !_choosingRelated;
	_btnIncludeDebug->setVisible(!_choosingRelated);
	_btnIncludeIds->setVisible(!_choosingRelated);
	_btnIncludeDefaults->setVisible(!_choosingRelated);
	if (_choosingRelated)
	{
		_hdLinks->clearList();
		for (size_t i = 1; i < _filterOptions.size(); ++i)
			_hdLinks->addRow(2, "", tr(_filterOptions[i]).c_str());
	}
	_hdLinks->setVisible(_choosingRelated);
	_lstRawData->setVisible(!_choosingRelated);
	_hdLedger->refresh();
}

void StatsForNerdsState::openRelatedRow(Action *action)
{
	const size_t index = size_t(_hdLinks->getSelectedRow()) + 1;
	if (!_choosingRelated || index >= _filterOptions.size()) return;
	_cbxRelatedStuff->setSelected(index);
	chooseRelated(nullptr);
	cbxAmmoSelect(action);
}

void StatsForNerdsState::resize(int &dX, int &dY)
{
	if (_hdLedger) _hdLedger->refresh();
	else State::resize(dX, dY);
}

void UfopaediaStartState::init()
{
	State::init();
	_hdSections->clearList();
	auto *save = _game->getSavedGame();
	for (const auto &category : _cats)
	{
		ArticleDefinitionList articles;
		Ufopaedia::list(save, _game->getMod(), category, articles);
		size_t visible = 0, unseen = 0;
		for (auto *article : articles)
		{
			const auto status = save->getUfopediaRuleStatus(article->id);
			visible += status != ArticleDefinition::PEDIA_STATUS_HIDDEN;
			unseen += status == ArticleDefinition::PEDIA_STATUS_NEW;
		}
		_hdSections->addRow(3, tr(category).c_str(), std::to_string(visible).c_str(), std::to_string(unseen).c_str());
	}
	_hdLedger->refresh();
	_hdSections->scrollTo(_hdScroll);
}
void UfopaediaStartState::openSectionRow(Action *)
{
	const size_t row = _hdSections->getSelectedRow();
	if (row >= _cats.size()) return;
	_hdScroll = _hdSections->getScroll();
	_game->pushState(new UfopaediaSelectState(_cats[row], 0, 0));
}
void UfopaediaStartState::resize(int &dX, int &dY)
{
	if (_hdLedger) _hdLedger->refresh();
	else State::resize(dX, dY);
}
}
#endif
