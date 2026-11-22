#ifdef __EMSCRIPTEN__
#include "CalypsoBaseLedger.h"
#include "CalypsoReportLines.h"
#include "CalypsoStrategicList.h"
#include "../Engine/Action.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Geoscape/ExtendedGeoscapeLinksState.h"
#include "../Geoscape/GeoscapeState.h"
#include "../Geoscape/Globe.h"
#include "../Geoscape/MonthlyReportState.h"
#include "../Geoscape/MultipleTargetsState.h"
#include "../Geoscape/NewPossibleResearchState.h"
#include "../Geoscape/UfoTrackerState.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextList.h"
#include "../Interface/Window.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleResearch.h"
#include "../Mod/RuleUfo.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Target.h"
#include "../Savegame/Ufo.h"
#include "../Ufopaedia/Ufopaedia.h"

namespace OpenXcom
{
void ExtendedGeoscapeLinksState::openDestinations()
{
	auto *training = new TextButton(1, 1);
	add(training, "button", "oxceLinks");
	training->setText(tr("STR_TRAINING"));
	training->onMouseClick((ActionHandler)&ExtendedGeoscapeLinksState::trainingClick);
	_destinations = new TextList(220, 120);
	add(_destinations, "list", "oxceLinks");
	_destinations->setColumns(2, 120, 90);
	_destinations->setSelectable(true);
	_destinations->setBackground(_window);
	for (Surface *surface : _surfaces)
	{
		auto *button = dynamic_cast<TextButton*>(surface);
		if (!button || button == _btnOk || !button->getVisible()) continue;
		_destinations->addRow(2, button->getText().c_str(), tr("STR_GEOSCAPE").c_str());
		_routes.push_back(button);
		button->setVisible(false);
	}
	_destinations->onMouseClick((ActionHandler)&ExtendedGeoscapeLinksState::openDestination);
	_destinations->onKeyboardPress((ActionHandler)&ExtendedGeoscapeLinksState::openDestination, Options::keyOk);
	_hdList = new Calypso::CalypsoStrategicList(this, _parent, _game->getMod(), _window, _txtTitle, _destinations, _btnOk, Calypso::CalypsoStrategicList::Content::Destinations);
	enableUiScaling(_hdList->designWidth(), _hdList->designHeight(), 1.0f, false);
	_hdList->fitInput();
}

void MonthlyReportState::prepareReportRows(int width, int fontSize)
{
	const bool failure = _txtFailure->getVisible();
	if (_reportWidth == width && _failureShown == failure) return;
	const auto scroll = _failureShown == failure ? _reportRows->getScroll() : 0;
	_reportWidth = width;
	_failureShown = failure;
	_reportRows->clearList();
	for (Text *field : {_txtMonth, _txtRating, _txtIncome, _txtMaintenance, _txtBalance,
		_txtBonus, _txtDesc, _txtFailure})
	{
		if (!field->getVisible()) continue;
		const bool narrative = field == _txtDesc || field == _txtFailure;
		std::string value = field->getText();
		std::string caption = narrative ? std::string(tr("STR_SUMMARY")) : "";
		const auto marker = narrative ? std::string::npos : value.find('>');
		if (marker != std::string::npos)
		{
			caption = value.substr(0, marker);
			value.erase(0, marker + 1);
		}
		Calypso::calypsoAppendReportText(*_reportRows, *_game->getMod(), caption, value, width, fontSize);
	}
	_reportRows->scrollTo(scroll);
}

void MonthlyReportState::resize(int &dx, int &dy)
{
	if (_ledger) _ledger->refresh();
	else State::resize(dx, dy);
}

void MultipleTargetsState::openContactList()
{
	_screen = true;
	setInterface("multipleTargets");
	_window = new Window(this, 160, 120);
	add(_window, "window", "multipleTargets");
	_contacts = new TextList(136, 86);
	add(_contacts, "button", "multipleTargets");
	_contacts->setColumns(2, 82, 54);
	_contacts->setSelectable(true);
	_contacts->setBackground(_window);
	_contacts->onMouseClick((ActionHandler)&MultipleTargetsState::chooseContact);
	_contacts->onKeyboardPress((ActionHandler)&MultipleTargetsState::chooseContact, SDLK_SPACE);
	for (auto *target : _targets)
		_contacts->addRow(2, target->getName(_game->getLanguage()).c_str(), tr(target->getType()).c_str());
	_details = new TextButton(60, 16);
	_open = new TextButton(60, 16);
	_cancel = new TextButton(60, 16);
	for (auto *button : {_details, _open, _cancel}) add(button, "button", "multipleTargets");
	_details->setText(tr("STR_INFO"));
	_details->onMouseClick((ActionHandler)&MultipleTargetsState::inspectPicked);
	_open->setText(tr("STR_CAL_OPEN_CONTACT"));
	_open->onMouseClick((ActionHandler)&MultipleTargetsState::openPicked);
	_open->onKeyboardPress((ActionHandler)&MultipleTargetsState::openPicked, Options::keyOk);
	_cancel->setText(tr("STR_CANCEL"));
	_cancel->onMouseClick((ActionHandler)&MultipleTargetsState::btnCancelClick);
	_cancel->onKeyboardPress((ActionHandler)&MultipleTargetsState::btnCancelClick, Options::keyCancel);
	_hdLedger = new Calypso::CalypsoBaseLedger(*this);
}

NewPossibleResearchState::~NewPossibleResearchState()
{
	delete _ledger;
}

void NewPossibleResearchState::resize(int &dx, int &dy)
{
	if (_ledger) _ledger->refresh();
	else State::resize(dx, dy);
}

bool NewPossibleResearchState::eligible(SavedGame &save, const RuleResearch *rule)
{
	return rule && rule->getRequirements().empty() && !save.wasResearchPopped(rule)
		&& !save.isResearched(rule, false);
}

void NewPossibleResearchState::init()
{
	State::init();
	if (_opened) return;
	_opened = true;
	auto &save = *_game->getSavedGame();
	_pending.erase(std::remove_if(_pending.begin(), _pending.end(),
		[&save](const RuleResearch *rule) { return !eligible(save, rule); }), _pending.end());
	if (_pending.empty())
	{
		_game->popState();
		return;
	}
	for (const auto *rule : _pending)
		Calypso::calypsoAddUnlockRow(*this, *_lstPossibilities, tr(rule->getName()), rule->getRequireBaseFunc());
	_ledger = new Calypso::CalypsoBaseLedger(*this, _base, *_txtTitle, *_lstPossibilities, *_btnResearch, *_btnOk);
	for (const auto *rule : _pending) save.addPoppedResearch(rule);
	_pending.clear();
}

void UfoTrackerState::chooseContact(Action *)
{
	_picked = _lstObjects->getSelectedRow();
	_reference->setVisible(canReference());
}

bool UfoTrackerState::canReference() const
{
	if (_picked >= _objects.size()) return false;
	const auto *ufo = dynamic_cast<const Ufo*>(_objects[_picked]);
	return ufo && ufo->getHyperDetected();
}

void UfoTrackerState::openPicked(Action *)
{
	if (_picked < _objects.size()) popupTarget(_objects[_picked]);
}

void UfoTrackerState::centerPicked(Action *)
{
	if (_picked >= _objects.size()) return;
	auto *target = _objects[_picked];
	_globe->center(target->getLongitude(), target->getLatitude());
	_game->popState();
}

void UfoTrackerState::referencePicked(Action *)
{
	if (!canReference()) return;
	auto *ufo = static_cast<Ufo*>(_objects[_picked]);
	std::string article = ufo->getRules()->getType();
	Ufopaedia::openArticle(_game, article);
}
}
#endif
