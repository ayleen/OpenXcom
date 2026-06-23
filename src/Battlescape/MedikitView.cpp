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
#include "MedikitView.h"
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleInterface.h"
#include "../Engine/SurfaceSet.h"
#include "../Engine/Action.h"
#include "../Engine/Language.h"
#include "../Savegame/BattleUnit.h"
#include "../Interface/Text.h"

namespace OpenXcom
{
/**
 * Initializes the Medikit view.
 * @param w The MinikitView width.
 * @param h The MinikitView height.
 * @param x The MinikitView x origin.
 * @param y The MinikitView y origin.
 * @param game Pointer to the core game.
 * @param unit The wounded unit.
 * @param partTxt A pointer to a Text. Will be updated with the selected body part.
 * @param woundTxt A pointer to a Text. Will be updated with the amount of fatal wound.
 */
MedikitView::MedikitView (int w, int h, int x, int y, Game * game, BattleUnit *unit, Text *partTxt, Text *woundTxt) : InteractiveSurface(w, h, x, y), _game(game), _selectedPart(0), _unit(unit), _partTxt(partTxt), _woundTxt(woundTxt), _baseW(w), _baseH(h)
{
	updateSelectedPart();
	_redraw = true;
}

/**
 * Draws the medikit view.
 */
void MedikitView::draw()
{
	SurfaceSet *set = _game->getMod()->getSurfaceSet("MEDIBITS.DAT");
	int fatal_wound = _unit->getFatalWound((UnitBodyPart)_selectedPart);
	std::ostringstream ss, ss1;
	int green = 0;
	int red = 3;
	if (_game->getMod()->getInterface("medikit", false) && _game->getMod()->getInterface("medikit")->getElementOptional("body"))
	{
		green = _game->getMod()->getInterface("medikit")->getElement("body")->color;
		red = _game->getMod()->getInterface("medikit")->getElement("body")->color2;
	}
#ifdef __EMSCRIPTEN__
	// Calypso: when scaled up (enableUiScaling), composite the body at its native
	// size, then scale-blit it to fill — keeps the 8bpp MEDIBITS frames + per-part
	// shading correct, just larger.
	if (getWidth() != _baseW || getHeight() != _baseH)
	{
		Surface scratch(_baseW, _baseH, 0, 0);
		scratch.lock();
		for (unsigned int i = 0; i < set->getTotalFrames(); i++)
		{
			int wound = _unit->getFatalWound((UnitBodyPart)i);
			Surface * surface = set->getFrame (i);
			int baseColor = wound ? red : green;
			surface->blitNShade(&scratch, 0, 0, 0, false, baseColor);
		}
		scratch.unlock();
		this->clear();
		if (scratch.getSurface() && this->getSurface())
		{
			SDL_BlitScaled(scratch.getSurface(), nullptr, this->getSurface(), nullptr);
		}
		this->setRedraw(false);
	}
	else
#endif
	{
		this->lock();
		for (unsigned int i = 0; i < set->getTotalFrames(); i++)
		{
			int wound = _unit->getFatalWound((UnitBodyPart)i);
			Surface * surface = set->getFrame (i);
			int baseColor = wound ? red : green;
			surface->blitNShade(this, 0, 0, 0, false, baseColor);
		}
		this->unlock();
		_redraw = false;
	}

	if (_selectedPart == -1)
	{
		return;
	}
	ss << _game->getLanguage()->getString(PARTS_STRING[_selectedPart]);
	ss1 << fatal_wound;
	_partTxt->setText(ss.str());
	_woundTxt->setText(ss1.str());
}

/**
 * Handles clicks on the medikit view.
 * @param action Pointer to an action.
 * @param state State that the action handlers belong to.
 */
void MedikitView::mouseClick (Action *action, State *)
{
	SurfaceSet *set = _game->getMod()->getSurfaceSet("MEDIBITS.DAT");
	int x = action->getRelativeXMouse() / action->getXScale();
	int y = action->getRelativeYMouse() / action->getYScale();
#ifdef __EMSCRIPTEN__
	// map scaled-view coords back to native frame space for the pixel hit-test
	if (getWidth()  > 0 && getWidth()  != _baseW) x = x * _baseW / getWidth();
	if (getHeight() > 0 && getHeight() != _baseH) y = y * _baseH / getHeight();
#endif
	for (unsigned int i = 0; i < set->getTotalFrames(); i++)
	{
		Surface * surface = set->getFrame (i);
		if (surface->getPixel(x, y))
		{
			_selectedPart = i;
			_redraw = true;
			break;
		}
	}
}

/**
 * Gets the selected body part.
 * @return The selected body part.
 */
int MedikitView::getSelectedPart() const
{
	return _selectedPart;
}

/**
 * Updates the selected body part.
 * If there is a wounded body part, selects that.
 * Otherwise does not change the selected part.
 */
void MedikitView::updateSelectedPart()
{
	for (int i = 0; i < BODYPART_MAX; ++i)
	{
		if (_unit->getFatalWound((UnitBodyPart)i))
		{
			_selectedPart = i;
			break;
		}
	}
}

}
