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
#include <string>

namespace OpenXcom
{

class TextButton;
class TextList;
class Window;
class Language;
#ifdef __EMSCRIPTEN__
class TTFFont;
#endif

/**
 * Text button with a list dropdown when pressed.
 * Allows selection from multiple available options.
 */
class ComboBox : public InteractiveSurface
{
private:
	static const int HORIZONTAL_MARGIN;
	static const int VERTICAL_MARGIN;
	static const int MAX_ITEMS;
	static const int BUTTON_WIDTH;
	static const int TEXT_HEIGHT;

	TextButton *_button;
	Surface *_arrow;
	Window *_window;
	TextList *_list;

	ActionHandler _change;
	size_t _sel;
	State *_state;
	Language *_lang;
	Uint8 _color;
	bool _toggled;
	bool _popupAboveButton;
#ifdef __EMSCRIPTEN__
	int _nativeW = 0, _nativeH = 0;
	float scale() const { return _nativeW > 0 ? (float)getWidth() / (float)_nativeW : 1.0f; }
	void relayout();
	std::vector<std::string> _optionsCache;
	std::vector<bool> _optionEnabled;
	bool _optionsCacheTranslate = false;
#endif

	void drawArrow();
	void setDropdown(int options);
public:
	/// Creates a combo box with the specified size and position.
	ComboBox(State *state, int width, int height, int x = 0, int y = 0, bool popupAboveButton = false);
	/// Cleans up the combo box.
	~ComboBox();
	/// Sets the X position of the surface.
	void setX(int x) override;
	/// Sets the Y position of the surface.
	void setY(int y) override;
#ifdef __EMSCRIPTEN__
	/// Calypso: HD scaling.
	void setWidth(int width) override;
	void setHeight(int height) override;
	/// Calypso: HD — forward TTF to button and list.
	void setTTFFont(TTFFont* font, float fillFrac);
#endif
	/// Sets the palette of the text list.
	void setPalette(const SDL_Color *colors, int firstcolor = 0, int ncolors = 256) override;
	/// Initializes the resources for the text list.
	void initText(Font *big, Font *small, Language *lang) override;
	/// Sets the background surface.
	void setBackground(Surface *bg);
	/// Sets the border color.
	void setColor(Uint8 color) override;
	/// Gets the border color.
	Uint8 getColor() const;
	/// Sets the high contrast color setting.
	void setHighContrast(bool contrast) override;
	/// Sets the arrow color of the text list.
	void setArrowColor(Uint8 color);
	/// Gets the selected option in the list.
	size_t getSelected() const;
	/// Gets the item that is currently hovered over in the popup list, or the current
	/// selected item if no item is hovered over.
	size_t getHoveredListIdx() const;
	/// Sets the button text without changing the selected option
	void setText(const std::string &text);
	/// Sets the selected option in the list.
	void setSelected(size_t sel);
	/// Accepts a popup-list click unless that option is disabled.
	void selectOptionFromList(size_t sel);
	/// Sets the list of options.
	void setOptions(const std::vector<std::string> &options, bool translate = false);
#ifdef __EMSCRIPTEN__
	void setOptionEnabled(size_t index, bool enabled);
#endif
	/// Blits the combo box onto another surface.
	void blit(SDL_Surface *surface) override;
	/// Thinks arrow buttons.
	void think() override;
	/// Handle arrow buttons.
	void handle(Action *action, State *state) override;
	/// Toggles the combo box state.
	void toggle(bool first, bool listClick);
	/// Hooks an action handler to when the slider changes.
	void onChange(ActionHandler handler);
	/// Hooks an action handler to moving the mouse in to the listbox when it is visible.
	void onListMouseIn(ActionHandler handler);
	/// Hooks an action handler to moving the mouse out of the listbox when it is visible.
	void onListMouseOut(ActionHandler handler);
	/// Hooks an action handler to moving the mouse over the listbox when it is visible.
	void onListMouseOver(ActionHandler handler);
};

}
