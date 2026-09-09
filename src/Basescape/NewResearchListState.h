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
#include "../Engine/TouchState.h"
#include <vector>

namespace OpenXcom
{

namespace Calypso { class CalypsoF09ResearchUi; }

class TextButton;
class ToggleTextButton;
class Window;
class Text;
class TextEdit;
class TextList;
class Base;
class RuleResearch;
class ComboBox;

/**
 * Window which displays possible research projects.
 */
class NewResearchListState : public TouchState
{
private:
#ifdef __EMSCRIPTEN__
	friend class Calypso::CalypsoF09ResearchUi;
#endif
	Base *_base;
	bool _sortByCost;
	TextButton *_btnOK;
#ifdef __EMSCRIPTEN__
	TextButton *_btnReview = nullptr, *_btnChangeVisibility = nullptr;
	TextButton *_btnTechTree = nullptr, *_btnMarkAllSeen = nullptr;
#endif
	ComboBox *_cbxSort;
	ToggleTextButton *_btnShowOnlyNew;
	TextEdit *_btnQuickSearch;
	Window *_window;
	Text *_txtTitle;
	TextList *_lstResearch;
	size_t _lstScroll;
	Uint8 _colorNormal, _colorNew, _colorHidden;
	bool _isSortingEnabled;
#ifdef __EMSCRIPTEN__
	bool _hdLayout = false;
	bool _hdWideLayout = false;
	Calypso::CalypsoF09ResearchUi *_hdAdapter = nullptr;
#endif
	void onClick(Action* action);
	void onSelectProject(Action *action);
	void onToggleProjectStatus(Action *action);
	void onOpenTechTreeViewer(Action *action);
	std::vector<RuleResearch *> _projects;
public:
	/// Creates the New research list state.
	NewResearchListState(Base *base, bool sortByCost);
	~NewResearchListState();
	/// Handler for clicking the OK button.
	void btnOKClick(Action *action);
	/// Handlers for Quick Search.
	void btnQuickSearchToggle(Action *action);
	void btnQuickSearchApply(Action *action);
	/// Handler for changing the sorting.
	void cbxSortChange(Action *action);
	/// Handler for clicking the [Show Only New] button.
	void btnShowOnlyNewClick(Action *action);
	/// Handler for clicking the [Mark All As Seen] button.
	void btnMarkAllAsSeenClick(Action *action);
	/// Fills the ResearchProject list with possible ResearchProjects.
	void fillProjectList(bool markAllAsSeen);
	/// Initializes the state.
	void init() override;
#ifdef __EMSCRIPTEN__
	void resize(int &dX, int &dY) override;
#endif
};
}
