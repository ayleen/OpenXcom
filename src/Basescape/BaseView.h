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
#include "../Engine/InteractiveSurface.h"

#include <vector>

namespace OpenXcom
{

class Base;
class SurfaceSet;
class BaseFacility;
class RuleBaseFacility;
class Craft;
class Font;
class Language;
class Timer;
enum BasePlacementErrors : int;

// One craft berth assignment in traversal order (T09): the pen, the craft
// occupying its slot (null when empty), and whether it is drawn. An away
// craft keeps its slot but is not drawn; an unfinished pen never receives
// a craft. Shared by draw() and the HD presentation refresh.
struct BaseCraftDrawing
{
	BaseFacility *pen = nullptr;
	Craft *craft = nullptr;
	bool drawn = false;
};

/**
 * Interactive view of a base.
 * Takes a certain base and displays all its facilities
 * and status, allowing players to manage them.
 */
class BaseView : public InteractiveSurface
{
private:
	static const int BASE_SIZE = 6;
	static const int GRID_SIZE = 32;

	Base *_base;
	SurfaceSet *_texture;
	BaseFacility *_facilities[BASE_SIZE][BASE_SIZE], *_selFacility;
	Font *_big, *_small;
	Language *_lang;
	int _gridX, _gridY;
#ifdef __EMSCRIPTEN__
	int _calypsoHdGridW = 0;
	int _calypsoHdGridH = 0;
#endif
	int _selSizeX, _selSizeY;
	Surface *_selector;
	bool _blink;
	Timer *_timer;
	Uint8 _redColor, _yellowColor, _greenColor;
	bool _highContrast;
	Uint8 _cellColor, _selectorColor;
	/// Updates the neighborFacility's build time. This is for internal use only (reCalcQueuedBuildings()).
	void updateNeighborFacilityBuildTime(BaseFacility* facility, BaseFacility* neighbor);
public:
	/// Creates a new base view at the specified position and size.
	BaseView(int width, int height, int x = 0, int y = 0);
	/// Cleans up the base view.
	~BaseView();
	/// Initializes the base view's various resources.
	void initText(Font *big, Font *small, Language *lang) override;
	/// Sets the base to display.
	void setBase(Base *base);
	// Assigns crafts to finished hangars in traversal order (T09). Preserves
	// the exact legacy order and STR_OUT semantics; only moves the code.
	void assignCraftsForDrawing(std::vector<BaseCraftDrawing> &out);
#ifdef __EMSCRIPTEN__
	// T10 opt-in HD grid extent for this instance only (main Basescape view).
	// Legacy GRID_SIZE mapping stays the default; placement views never set it,
	// so the GRID_SIZE-based selector math below is unaffected.
	void setCalypsoHdGridExtent(int logicalWidth, int logicalHeight);
	void clearCalypsoHdGridExtent();
#endif
	/// Sets the texture for this base view.
	void setTexture(SurfaceSet *texture);
	/// Gets the currently selected facility.
	BaseFacility *getSelectedFacility() const;
	/// Prevents any mouseover bugs on dismantling base facilities before setBase has had time to update the base.
	void resetSelectedFacility();
	/// Gets the X position of the currently selected square.
	int getGridX() const;
	/// Gets the Y position of the currently selected square.
	int getGridY() const;
	/// Sets whether the base view is selectable.
	void setSelectable(int sizeX, int sizeY);
	/// Checks if a facility can be placed. Returns 0 if it can, otherwise an int for why not.
	BasePlacementErrors getPlacementError(const RuleBaseFacility *rule, BaseFacility *facilityBeingMoved = nullptr, bool isStartFacility = false) const;
	/// Checks if the placed facility is placed in queue or not.
	bool isQueuedBuilding(const RuleBaseFacility *rule) const;
	/// ReCalculates the remaining build-time of all queued buildings.
	void reCalcQueuedBuildings();
	/// Handles the timers.
	void think() override;
	/// Blinks the selector.
	void blink();
	/// Draws the base view.
	void draw() override;
	/// Blits the base view onto another surface.
	void blit(SDL_Surface *surface) override;
	/// Special handling for mouse hovers.
	void mouseOver(Action *action, State *state) override;
	/// Special handling for mouse hovering out.
	void mouseOut(Action *action, State *state) override;

	void setColor(Uint8 color) override;
	void setSecondaryColor(Uint8 color) override;
	void setOtherColors(Uint8 red, Uint8 yellow, Uint8 green, bool highContrast);
};

}
