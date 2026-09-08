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
#include <vector>
#include <cstdint>
#include "../Engine/State.h"

namespace OpenXcom
{
namespace Calypso { class CalypsoF03BuildFacilitiesUi; }
class Base;
class TextButton;
class Window;
class Text;
class TextList;
class RuleBaseFacility;


/**
 * Window shown with all the facilities
 * available to build.
 */
class BuildFacilitiesState : public State
{
#ifdef __EMSCRIPTEN__
friend class Calypso::CalypsoF03BuildFacilitiesUi;
#endif
protected:
	Base *_base;
	State *_state;
	std::vector<RuleBaseFacility*> _facilities, _disabledFacilities;
	size_t _lstScroll;

	TextButton *_btnOk;
	Window *_window;
	Text *_txtTitle;
	TextList *_lstFacilities;
public:
	/// Creates the Build Facilities state.
	BuildFacilitiesState(Base *base, State *state);
	/// Cleans up the Build Facilities state.
	~BuildFacilitiesState();
	/// Populates the build option list.
	virtual void populateBuildList();
	/// Updates the base stats.
	void init() override;
	/// Handler for clicking the OK button.
	void btnOkClick(Action *action);
	/// Handler for clicking the Facilities list.
	virtual void lstFacilitiesClick(Action *action);
#ifdef __EMSCRIPTEN__
private:
	bool _hdLayout = false;
	bool _hdWideLayout = false;
	bool _hdOwnFixture = false;
	Calypso::CalypsoF03BuildFacilitiesUi *_hdAdapter = nullptr;
	std::uint64_t _hdHarnessGeneration = 0;
public:
	void calypsoOwnHarnessFixture() { _hdOwnFixture = true; }
	void resize(int &dX, int &dY) override;
#endif
};

} // namespace OpenXcom
