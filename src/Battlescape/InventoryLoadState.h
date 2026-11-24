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

namespace OpenXcom
{

class TextButton;
class Window;
class Text;
class TextList;
class EquipmentLayoutItem;
class InventoryState;

/**
* Inventory Load window that allows changing of the equipment on a soldier.
*/
#ifdef __EMSCRIPTEN__
namespace Calypso { class CalypsoStrategicList; }
#endif
class InventoryLoadState : public State
{
private:
#ifdef __EMSCRIPTEN__
	Calypso::CalypsoStrategicList *_hdList = nullptr;
	TextButton *_hdApply = nullptr;
	int _pendingSlot = -1;
	void applySlot(Action *action);
	void previewSlot(int index);
#endif
	void loadLayout(int index);
	InventoryState *_parent;
	Window *_window;
	Text *_txtTitle;
	TextList *_lstLayout;
	TextButton *_btnCancel;
public:
#ifdef __EMSCRIPTEN__
	void resize(int &dx, int &dy) override;
#endif
	/// Creates the Load Inventory state.
	InventoryLoadState(InventoryState *parent);
	/// Cleans up the Load Inventory state.
	~InventoryLoadState();
	/// Handler for clicking the Cancel button.
	void btnCancelClick(Action *action);
	/// Handler for clicking the Layout list.
	void lstLayoutClick(Action *action);
};

}
