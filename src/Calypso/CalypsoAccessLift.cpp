#include "CalypsoAccessLift.h"
#include "../Basescape/BaseView.h"
#include "../Basescape/BuildFacilitiesState.h"
#include "../Basescape/PlaceLiftState.h"
#include "../Engine/Action.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Engine/Unicode.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextList.h"
#include "../Mod/RuleBaseFacility.h"
#include "../Ufopaedia/Ufopaedia.h"

namespace OpenXcom
{
namespace
{
class LiftPicker : public BuildFacilitiesState
{
public:
	LiftPicker(Base *base, CalypsoAccessLift *owner, const std::vector<RuleBaseFacility *> &lifts)
		: BuildFacilitiesState(base, owner), _owner(owner)
	{
		_facilities = lifts;
		_txtTitle->setText(tr("STR_INSTALLATION"));
	}

	void init() override
	{
		State::init();
		_lstFacilities->clearList();
		for (const auto *lift : _facilities)
			_lstFacilities->addRow(1, tr(lift->getType()).c_str());
	}

	void lstFacilitiesClick(Action *action) override
	{
		const auto index = _lstFacilities->getSelectedRow();
		if (index >= _facilities.size()) return;
		auto *lift = _facilities[index];
		if (action->getDetails()->button.button == SDL_BUTTON_MIDDLE)
		{
			Ufopaedia::openArticle(_game, lift->getUfopediaType());
			return;
		}
		_owner->choose(lift);
		_game->popState();
	}
private:
	CalypsoAccessLift *_owner;
};
}

CalypsoAccessLift::CalypsoAccessLift(Base *base, Globe *globe, bool first, std::vector<RuleBaseFacility *> lifts)
	: PlaceFacilityState(base, lifts.front()), _globe(globe), _first(first),
	  _needChoice(!first && lifts.size() > 1), _lifts(std::move(lifts))
{
	choose(_lifts.front());
	_view->onMouseClick((ActionHandler)&CalypsoAccessLift::placeLift);
	_btnCancel->setVisible(_needChoice);
	_btnCancel->setText(tr("STR_INSTALLATION"));
	_btnCancel->onMouseClick((ActionHandler)&CalypsoAccessLift::openPicker);
	_btnCancel->onKeyboardPress(nullptr, Options::keyCancel);
}

void CalypsoAccessLift::init()
{
	PlaceFacilityState::init();
	if (_needChoice)
	{
		_needChoice = false;
		openPicker(nullptr);
	}
}

void CalypsoAccessLift::openPicker(Action *)
{
	if (!_first && _lifts.size() > 1)
		_game->pushState(new LiftPicker(_base, this, _lifts));
}

void CalypsoAccessLift::choose(RuleBaseFacility *rule)
{
	_rule = rule;
	_view->setSelectable(rule->getSizeX(), rule->getSizeY());
	_txtFacility->setText(tr(rule->getType()));
	_numCost->setText(Unicode::formatFunding(0));
	_numResources->setText("");
	_numTime->setText(tr("STR_NONE"));
	_numMaintenance->setText(Unicode::formatFunding(rule->getMonthlyCost()));
}

void CalypsoAccessLift::placeLift(Action *)
{
	if (_view->getPlacementError(_rule, nullptr, true)) return;
	PlaceLiftState::finishPlacement(_base, _globe, _first, _rule, _view->getGridX(), _view->getGridY());
}
}
