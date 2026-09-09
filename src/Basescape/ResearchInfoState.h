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
#include "ResearchInfoTransaction.h"

namespace OpenXcom
{

namespace Calypso { class CalypsoF09ResearchUi; }

class TextButton;
class Window;
class Text;
class Base;
class RuleResearch;
class ResearchProject;
class ArrowButton;
class Timer;
class InteractiveSurface;


/**
 * Window which allows changing of the number of assigned scientist to a project.
 */
class ResearchInfoState : public State
{
private:
#ifdef __EMSCRIPTEN__
	friend class Calypso::CalypsoF09ResearchUi;
#endif
	Base *_base;
	TextButton *_btnOk;
	TextButton *_btnCancel;
#ifdef __EMSCRIPTEN__
	TextButton *_btnAllAvailable = nullptr, *_btnRemoveAll = nullptr;
#endif
	ArrowButton *_btnMore, *_btnLess;
	InteractiveSurface *_surfaceScientists;
	Window *_window;
	Text *_txtTitle, *_txtAvailableScientist, *_txtAvailableSpace, *_txtAllocatedScientist, *_txtMore, *_txtLess;
	ResearchProject *_project;
	RuleResearch *_rule;
	ResearchInfoTransaction _transaction;
	void buildUi();
	void cancelPreview();
	void commitPreview();
	void setAssignedScientist();
	Timer *_timerMore, *_timerLess;
#ifdef __EMSCRIPTEN__
	bool _hdLayout = false;
	bool _hdWideLayout = false;
	Calypso::CalypsoF09ResearchUi *_hdAdapter = nullptr;
#endif
public:
	/// Creates the ResearchProject state.
	ResearchInfoState(Base *base, RuleResearch *rule);
	ResearchInfoState(Base *base, ResearchProject *project);
	/// Cleans up the ResearchInfo state
	~ResearchInfoState();
	/// Handler for clicking the OK button.
	void btnOkClick(Action *action);
	/// Handler for clicking the Cancel button.
	void btnCancelClick(Action *action);
	/// Function called every time the _timerMore timer is triggered.
	void more();
	/// Add given number of scientists to the project if possible
	void moreByValue(int change);
	/// Function called every time the _timerLess timer is triggered.
	void less();
	/// Remove the given number of scientists from the project if possible
	void lessByValue(int change);
	/// Handler for using the mouse wheel.
	void handleWheel(Action *action);
	/// Handler for pressing the More button.
	void morePress(Action *action);
	/// Handler for releasing the More button.
	void moreRelease(Action *action);
	/// Handler for clicking the More button.
	void moreClick(Action *action);
	/// Handler for pressing the Less button.
	void lessPress(Action *action);
	/// Handler for releasing the Less button.
	void lessRelease(Action *action);
	/// Handler for clicking the Less button.
	void lessClick(Action *action);
	/// Assign/remove all scientists for the HD staffing action owners.
	void allAvailableClick(Action *action);
	void removeAllClick(Action *action);
	/// Runs state functionality every cycle(used to update the timer).
#ifdef __EMSCRIPTEN__
	void resize(int &dX, int &dY) override;
#endif
	void think() override;
};

}
