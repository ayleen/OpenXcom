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
#include "PlaceLiftState.h"
#ifdef __EMSCRIPTEN__
#include "../Calypso/CalypsoAccessLift.h"
#include "../Calypso/CalypsoHdUiOverlay.h"
#endif
#include "../Engine/Action.h"
#include "../Engine/Game.h"
#include "../Engine/Sound.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Interface/Window.h"
#include "BaseView.h"
#include "../Savegame/Base.h"
#include "../Savegame/BaseFacility.h"
#include "../Mod/RuleBaseFacility.h"
#include "BasescapeState.h"
#include "SelectStartFacilityState.h"
#include "../Savegame/SavedGame.h"
#include "../Ufopaedia/Ufopaedia.h"
#include "../Mod/RuleInterface.h"
#include "../Geoscape/Globe.h"
#include "../Mod/RuleGlobe.h"

namespace OpenXcom
{

namespace
{
void setLiftBaseTexture(Game *game, Base *base, Globe *globe)
{
	if (globe && base)
	{
		int texture, shade;
		globe->getPolygonTextureAndShade(base->getLongitude(), base->getLatitude(), &texture, &shade);
		auto* globeTexture = game->getMod()->getGlobe()->getTexture(texture);
		base->setGlobeTexture(globeTexture);
	}
}

std::vector<RuleBaseFacility *> eligibleLifts(Game *game, Base *base)
{
	std::vector<RuleBaseFacility *> lifts;
	for (auto& facilityType : game->getMod()->getBaseFacilitiesList())
	{
		auto* facilityRule = game->getMod()->getBaseFacility(facilityType);
		if ((facilityRule->isLift() && !facilityRule->isUpgradeOnly())
			&& facilityRule->isAllowedForBaseType(base->isFakeUnderwater()) && game->getSavedGame()->isResearched(facilityRule->getRequirements()))
		{
			lifts.push_back(facilityRule);
		}
	}
	return lifts;
}
}

State *PlaceLiftState::create(Base *base, Globe *globe, bool first)
{
#ifdef __EMSCRIPTEN__
	setLiftBaseTexture(_game, base, globe);
	auto lifts = eligibleLifts(_game, base);
	if (lifts.empty())
		Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Calypso: no eligible access lift for this base");
	return new CalypsoAccessLift(base, globe, first, std::move(lifts));
#else
	return new PlaceLiftState(base, globe, first);
#endif
}

/**
 * Initializes all the elements in the Place Lift screen.
 * @param game Pointer to the core game.
 * @param base Pointer to the base to get info from.
 * @param globe Pointer to the Geoscape globe.
 * @param first Is this a custom starting base?
 */
PlaceLiftState::PlaceLiftState(Base *base, Globe *globe, bool first) : _base(base), _globe(globe), _first(first)
{
	// Create objects
	_view = new BaseView(192, 192, 0, 8);
	_txtTitle = new Text(320, 9, 0, 0);
	_window = new Window(this, 128, 160, 192, 40, POPUP_NONE);
	_txtHeader = new Text(118, 17, 197, 48);
	_lstAccessLifts = new TextList(104, 104, 200, 64);

	// Set palette
	setInterface("placeFacility");

	add(_view, "baseView", "basescape");
	add(_txtTitle, "text", "placeFacility");
	add(_window, "window", "selectFacility");
	add(_txtHeader, "text", "selectFacility");
	add(_lstAccessLifts, "list", "selectFacility");

	centerAllSurfaces();

	// Set up objects
	setWindowBackground(_window, "selectFacility");

	setLiftBaseTexture(_game, _base, _globe);

	auto* itf = _game->getMod()->getInterface("basescape")->getElementOptional("trafficLights");
	if (itf)
	{
		_view->setOtherColors(itf->color, itf->color2, itf->border, !itf->TFTDMode);
	}
	_view->setTexture(_game->getMod()->getSurfaceSet("BASEBITS.PCK"));
	_view->setBase(_base);

	_lift = nullptr;
	_accessLifts = eligibleLifts(_game, _base);
	if (!_accessLifts.empty() && (_first || _accessLifts.size() == 1))
	{
		_lift = _accessLifts.front();
	}

	_txtHeader->setBig();
	_txtHeader->setAlign(ALIGN_CENTER);
	_txtHeader->setText(tr("STR_INSTALLATION"));

	_lstAccessLifts->setColumns(1, 104);
	_lstAccessLifts->setSelectable(true);
	_lstAccessLifts->setBackground(_window);
	_lstAccessLifts->setMargin(2);
	_lstAccessLifts->setWordWrap(true);
	_lstAccessLifts->setScrolling(true, 0);
	_lstAccessLifts->onMouseClick((ActionHandler)&PlaceLiftState::lstAccessLiftsClick);
	_lstAccessLifts->onMouseClick((ActionHandler)&PlaceLiftState::lstAccessLiftsClick, SDL_BUTTON_MIDDLE);

	for (const auto* facRule : _accessLifts)
	{
		_lstAccessLifts->addRow(1, tr(facRule->getType()).c_str());
	}

	if (_lift)
	{
		_lstAccessLifts->setVisible(false);
		_txtHeader->setVisible(false);
		_window->setVisible(false);

		_view->setSelectable(_lift->getSizeX(), _lift->getSizeY());
		_view->onMouseClick((ActionHandler)&PlaceLiftState::viewClick);
	}

	_txtTitle->setText(tr("STR_SELECT_POSITION_FOR_ACCESS_LIFT"));
}

/**
 *
 */
PlaceLiftState::~PlaceLiftState()
{

}

/**
 * Processes clicking on facilities.
 * @param action Pointer to an action.
 */
void PlaceLiftState::viewClick(Action *)
{
	finishPlacement(_base, _globe, _first, _lift, _view->getGridX(), _view->getGridY());
}

void PlaceLiftState::finishPlacement(Base *base, Globe *globe, bool first, const RuleBaseFacility *lift, int x, int y)
{
	BaseFacility *fac = new BaseFacility(lift, base);
	fac->setX(x);
	fac->setY(y);
	base->getFacilities()->push_back(fac);
	if (fac->getRules()->getPlaceSound() != Mod::NO_SOUND)
	{
		_game->getMod()->getSound("GEO.CAT", fac->getRules()->getPlaceSound())->play();
	}
	_game->popState();
#ifdef __EMSCRIPTEN__
	if (first && _game->getMod()->getCustomBaseFacilities(_game->getSavedGame()->getDifficulty()).empty())
		return;
#endif
	BasescapeState *bState = new BasescapeState(base, globe);
	_game->getSavedGame()->setSelectedBase(_game->getSavedGame()->getBases()->size() - 1);
	_game->pushState(bState);
	if (first)
	{
		_game->pushState(new SelectStartFacilityState(base, bState, globe));
	}
}

/**
 * Selects the access lift to place.
 * @param action Pointer to an action.
 */
void PlaceLiftState::lstAccessLiftsClick(Action *action)
{
	auto index = _lstAccessLifts->getSelectedRow();
	if (index >= _accessLifts.size())
	{
		return;
	}

	if (action->getDetails()->button.button == SDL_BUTTON_MIDDLE)
	{
		Ufopaedia::openArticle(_game, _accessLifts[index]->getUfopediaType());
		return;
	}

	_lift = _accessLifts[index];

	if (_lift)
	{
		_lstAccessLifts->setVisible(false);
		_txtHeader->setVisible(false);
		_window->setVisible(false);

		_view->setSelectable(_lift->getSizeX(), _lift->getSizeY());
		_view->onMouseClick((ActionHandler)&PlaceLiftState::viewClick);
	}
}

}
