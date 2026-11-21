#pragma once
/*
 * Copyright 2010-2015 OpenXcom Developers.
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
#include <string>
#include <vector>

namespace OpenXcom
{

#ifdef __EMSCRIPTEN__
namespace Calypso { class CalypsoF10ProductionUi; }
#endif

class Window;
class Text;
class TextButton;
class TextList;

/**
 * Window which displays manufacture dependencies tree.
 */
class ManufactureDependenciesTreeState : public State
{
private:
#ifdef __EMSCRIPTEN__
	friend class Calypso::CalypsoF10ProductionUi;
#endif
	Window *_window;
	Text *_txtTitle;
	TextList *_lstTopics;
	TextButton *_btnOk, *_btnShowAll;
	std::string _selectedItem;
	bool _showAll;
	void initList();
public:
	/// Typed presentation snapshot for the HD dependency list (external review
	/// R08). Parallel to the native rows: one entry per addRow, emitted at the
	/// traversal site so row kind and depth never come from translated text.
	/// `text` carries only already-authorized display text ("***" when hidden).
	/// Plain data with no HD dependency, so native builds keep it too.
	enum class RowKind { Item, Header, Separator, NoDependencies, End, More, FeatureDisabled };
	struct PresentationRow
	{
		RowKind kind = RowKind::Item;
		int depth = 0; ///< 0 for structural rows, 1..4 for content rows
		std::string text;
	};
	const std::vector<PresentationRow> &calypsoPresentationRows() const
	{
		return _calypsoPresentationRows;
	}
private:
	std::vector<PresentationRow> _calypsoPresentationRows;
	/// Adds one native row and its parallel typed metadata.
	void calypsoAddRow(RowKind kind, int depth, const std::string &text);
#ifdef __EMSCRIPTEN__
	Calypso::CalypsoF10ProductionUi *_hdAdapter = nullptr;
	bool _hdLayout = false;
	bool _hdWideLayout = false;
#endif
public:
	/// Creates the ManufactureDependenciesTree state.
	ManufactureDependenciesTreeState(const std::string &selectedItem);
	/// Cleans up the ManufactureDependenciesTree state
	~ManufactureDependenciesTreeState();
	/// Initializes the state.
	void init() override;
	/// Handler for clicking the OK button.
	void btnOkClick(Action *action);
	/// Handler for clicking the [Show All] button.
	void btnShowAllClick(Action *action);
#ifdef __EMSCRIPTEN__
	void resize(int &dX, int &dY) override;
#endif
};
}
