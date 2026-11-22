#ifdef __EMSCRIPTEN__
#include "CalypsoHdOperationsRenderer.h"
#include "../Engine/Action.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Geoscape/DogfightState.h"
#include "../Geoscape/GeoscapeState.h"
#include "../Geoscape/Globe.h"
#include "../Geoscape/InterceptState.h"
#include "../Interface/ImageButton.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextList.h"
#include "../Savegame/Base.h"
#include "../Savegame/Craft.h"
#include "../Savegame/Ufo.h"

namespace OpenXcom
{
void DogfightState::interceptionAction(int command, int slot)
{
	if (_minimized)
	{
		if (command == 7) btnMinimizedIconClick(nullptr);
		return;
	}
	if (_preview->getVisible())
	{
		if (command == 6) previewClick(nullptr);
		return;
	}
	SDL_Event event{};
	event.type = SDL_MOUSEBUTTONDOWN;
	event.button.button = SDL_BUTTON_LEFT;
	Action action(&event, 1, 1, 0, 0);
	switch (command)
	{
	case 0: btnStandoffSimulateLeftPress(&action); break;
	case 1: btnCautiousSimulateLeftPress(&action); break;
	case 2: btnStandardSimulateLeftPress(&action); break;
	case 3: btnAggressiveSimulateLeftPress(&action); break;
	case 4: btnDisengageSimulateLeftPress(&action); break;
	case 5: if (_btnMinimize->getVisible()) btnMinimizeClick(nullptr); break;
	case 6: if (_btnUfo->getVisible()) btnUfoClick(nullptr); break;
	case 8:
		if (slot >= 0 && slot < _weaponNum && _craft->getWeapons()->at(slot) && _weapon[slot]->getVisible())
		{
			action.setSender(_weapon[slot]);
			weaponClick(&action);
		}
		break;
	case 9: if (slot >= 0 && slot < 5) _state->handleDogfightMultiAction(slot); break;
	}
}

void GeoscapeState::startHarnessInterception(Craft *craft, Ufo *ufo)
{
	_globe->saveZoomDogfight();
	_globe->zoomMax();
	_dogfightsToBeStarted.push_back(new DogfightState(this, craft, ufo));
	startDogfight();
}

bool InterceptState::canDispatch(const Craft *craft) const
{
	if (!craft || craft->getLowFuel() || craft->getMissionComplete()) return false;
	const auto &status = craft->getStatus();
	if (status != "STR_READY" && status != "STR_OUT" && !Options::craftLaunchAlways) return false;
	return !_target || craft->getDistance(_target) + craft->getBase()->getDistance(_target) <= craft->getBaseRange() * 2.0;
}

void InterceptState::pickShip(Action *)
{
	_picked = _lstCrafts->getSelectedRow();
	if (_game->isShiftPressed()) toggleEscort(nullptr);
}

void InterceptState::toggleEscort(Action *)
{
	if (_picked >= _crafts.size()) return;
	auto *craft = _crafts[_picked];
	auto it = std::find(_selCrafts.begin(), _selCrafts.end(), craft);
	if (it != _selCrafts.end()) _selCrafts.erase(it);
	else if (_selCrafts.size() < 3 && canDispatch(craft)) _selCrafts.push_back(craft);
}

void InterceptState::setCourse(Action *action)
{
	if (_picked >= _crafts.size() || !canDispatch(_crafts[_picked])) return;
	_lstCrafts->setSelectedRow(_picked);
	lstCraftsLeftClick(action);
}
void InterceptState::openShip(Action *action)
{
	if (_picked >= _crafts.size()) return;
	_lstCrafts->setSelectedRow(_picked);
	lstCraftsRightClick(action);
}
void InterceptState::openReference(Action *action)
{
	if (_picked >= _crafts.size()) return;
	_lstCrafts->setSelectedRow(_picked);
	lstCraftsMiddleClick(action);
}
void InterceptState::resize(int &dX, int &dY)
{
	if (_fleetView) _fleetView->setModel(fleetModel());
	else State::resize(dX, dY);
}
}
#endif
