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
#include "../Menu/ErrorMessageState.h"

namespace OpenXcom
{

class Base;
class BaseFacility;
class RuleBaseFacility;
class BaseView;
class TextButton;
class Window;
class Text;
#ifdef __EMSCRIPTEN__
namespace Calypso { class CalypsoBasescapeHdUi; class CalypsoHdScreenRenderer; }
#endif

/**
 * Window shown when the player tries to
 * build a facility.
 */
class PlaceFacilityState : public State
{
#ifdef __EMSCRIPTEN__
	friend class Calypso::CalypsoBasescapeHdUi;
	friend class Calypso::CalypsoHdScreenRenderer;
#endif
protected:
	Base *_base;
	const RuleBaseFacility *_rule;
	BaseFacility *_origFac;

	BaseView *_view;
	TextButton *_btnCancel;
	Window *_window;
	Text *_txtFacility, *_txtCost, *_numCost, *_numResources, *_txtTime, *_numTime, *_txtMaintenance, *_numMaintenance;
#ifdef __EMSCRIPTEN__
	Calypso::CalypsoBasescapeHdUi *_calypsoHdUi = nullptr;
#endif
#ifdef __EMSCRIPTEN__
	/// Canonical HD form for a placement validation error (Emscripten only):
	/// placed facility as protocol/title, native reason as body lines, native
	/// OK label. Native builds keep the original constructor arguments.
	ErrorMessageHdForm placementErrorForm(const std::string &reason) const;
#endif
	/// State-local placement error push: original native arguments and the
	/// original pop/push order on every build, canonical HD form attached on
	/// Emscripten only. Preserves the PlaceStartFacilityState overrides.
	void pushPlacementError(const std::string &reason, const std::string &bg, int errorColor1, int errorColor2, bool popFirst);
public:
	/// Creates the Place Facility state.
	PlaceFacilityState(Base *base, const RuleBaseFacility *rule, BaseFacility *origFac = 0);
	/// Cleans up the Place Facility state.
	~PlaceFacilityState();
	/// Initializes the placement HD shell (Emscripten); native is unchanged.
	void init() override;
	/// HD layout refresh hook: the holder consumes it, else legacy.
	void resize(int &dX, int &dY) override;
	/// No construction blackout: suppressed native widgets paint nothing, the
	/// live HD base stays underneath (canonical HD errors bring own scrim).
	void blit() override;
#ifdef __EMSCRIPTEN__
	/// Fullscreen strategic UI owns resize independently of covered states.
	Calypso::CalypsoViewportAffinity calypsoViewportAffinity() const override
	{
		return Calypso::CalypsoViewportAffinity::Strategic;
	}
#endif
	/// Handler for clicking the Cancel button.
	void btnCancelClick(Action *action);
	/// Handler for clicking the base view.
	void viewClick(Action *action);
};

}
