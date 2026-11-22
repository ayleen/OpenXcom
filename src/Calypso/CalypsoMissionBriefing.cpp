#ifdef __EMSCRIPTEN__
#include "../Battlescape/BriefingLightState.h"
#include "../Battlescape/BriefingState.h"
#include "CalypsoReportLines.h"
#include "CalypsoStrategicList.h"
#include "../Engine/Game.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextList.h"
#include "../Interface/Window.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleEnviroEffects.h"
#include "../Mod/RuleStartingCondition.h"
#include "../Savegame/SavedBattleGame.h"
#include "../Savegame/SavedGame.h"
#include "../Interface/ToggleTextButton.h"
#include "../Mod/Armor.h"
#include "../Ufopaedia/Ufopaedia.h"

namespace OpenXcom
{
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

void BriefingState::layoutBriefing(bool initial)
{
	const auto scroll = _hdBody->getScroll();
	if (!initial) _hdList->layout();
	_hdBody->clearList();
	auto append = [&](const std::string &text) {
		for (const auto &line : Calypso::calypsoWrapReportText(*_game->getMod(), text, _hdList->firstColumnWidth(), _hdList->bodyFontSize()))
			_hdBody->addRow(1, line.c_str());
	};
	for (const auto &paragraph : _briefingParagraphs) append(paragraph);
	if (initial) enableUiScaling(_hdList->designWidth(), _hdList->designHeight(), 1.0f, false);
	else recaptureUiScaling(_hdList->designWidth(), _hdList->designHeight(), 1.0f, false);
	_hdList->fitInput();
	_hdBody->scrollTo(scroll);
}

void BriefingState::resize(int &dx, int &dy)
{
	if (_hdList) layoutBriefing(false);
	else State::resize(dx, dy);
}

void BriefingState::openBriefing(bool isPreview)
{
	auto *battleSave = _game->getSavedGame()->getSavedBattle();
	_btnOk->setText(tr(_infoOnly ? "STR_CAL_BRIEF_CLOSE" : "STR_CAL_BRIEF_BEGIN"));
	if (isPreview) _briefingParagraphs.push_back(tr("STR_PREVIEW"));
	if (_txtTarget->getVisible() && !_txtTarget->getText().empty()) _briefingParagraphs.push_back(_txtTarget->getText());
	if (_txtCraft->getVisible() && !_txtCraft->getText().empty()) _briefingParagraphs.push_back(_txtCraft->getText());
	_briefingParagraphs.push_back(_txtBriefing->getText());
	_briefingParagraphs.push_back(tr("STR_CAL_BRIEF_DEPTH").arg(battleSave->getDepth()));
	_briefingParagraphs.push_back(tr("STR_CAL_BRIEF_SHADE").arg(battleSave->getGlobalShade()));
	if (!battleSave->getLastUsedMapScript().empty())
		_briefingParagraphs.push_back(tr("STR_CAL_BRIEF_MAP").arg(battleSave->getLastUsedMapScript()));
	if (const auto *condition = battleSave->getStartingCondition())
		_briefingParagraphs.push_back(tr(condition->getType()));
	if (const auto *environment = battleSave->getEnviroEffects())
		_briefingParagraphs.push_back(tr(environment->getType()));
	for (Text *text : {_txtTarget, _txtCraft, _txtBriefing}) text->setVisible(false);
	_hdBody = new TextList(1, 1, 0, 0);
	add(_hdBody, "text", "briefing");
	_hdBody->setColumns(1, 1);
	_hdBody->setBackground(_window);
	_hdList = new Calypso::CalypsoStrategicList(this, nullptr, _game->getMod(), _window, _txtTitle,
		_hdBody, _btnOk, Calypso::CalypsoStrategicList::Content::MissionBriefing);
	layoutBriefing(true);
}
}
#endif
