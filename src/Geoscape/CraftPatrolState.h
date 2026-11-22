#pragma once
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
#include "../Engine/State.h"

namespace OpenXcom
{

#ifdef __EMSCRIPTEN__
namespace Calypso { class CalypsoF18PatrolArrivalUi; }
#endif
class TextButton;
class Window;
class Text;
class Craft;
class Globe;

/**
 * Window displayed when a craft
 * starts patrolling a waypoint.
 */
class CraftPatrolState : public State
{
private:
#ifdef __EMSCRIPTEN__
	friend class Calypso::CalypsoF18PatrolArrivalUi;
	Calypso::CalypsoF18PatrolArrivalUi *_hdAdapter = nullptr;
	bool _hdLayout = false;
	bool _hdWideLayout = false;
#endif
	Craft *_craft;
	Globe *_globe;

	TextButton *_btnOk, *_btnRedirect;
	Window *_window;
	Text *_txtDestination, *_txtPatrolling;
public:
	/// Creates the Geoscape Craft state.
	CraftPatrolState(Craft *craft, Globe *globe);
	/// Cleans up the Geoscape Craft state.
	~CraftPatrolState();
	/// Handler for clicking the OK button.
	void btnOkClick(Action *action);
	/// Handler for clicking the Redirect Craft button.
	void btnRedirectClick(Action *action);
#ifdef __EMSCRIPTEN__
	void resize(int &dX, int &dY) override;
#endif
};

}
