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
#include <string>
#include <memory>
#include <SDL.h>
#include "SDL2Helpers.h"
#include "LocalizedText.h"
#include "../Calypso/CalypsoFocusCoordinator.h"
#ifdef __EMSCRIPTEN__
#include "../Calypso/CalypsoViewportOwner.h"
#endif

namespace OpenXcom
{

class Game;
class Surface;
class InteractiveSurface;
class Window;
class Action;
class SavedBattleGame;
class RuleInterface;
class Sound;
class TTFFont;

enum SoldierGender : char;

/**
 * A game state that receives user input and reacts accordingly.
 * Game states typically represent a whole window or screen that
 * the user interacts with, making the game... well, interactive.
 * They automatically handle child elements used to transmit
 * information from/to the user, and are linked to the core game
 * engine which manages them.
 */
class State
{
	friend class Timer;

protected:
	static Game *_game;
	std::vector<Surface*> _surfaces;
	std::vector<Surface*> _surfacesOwned;
	bool _screen;
	bool _soundPlayed;
	InteractiveSurface *_modal;
	RuleInterface *_ruleInterface;
	RuleInterface *_ruleInterfaceParent;
	const Sound* _customSound;
	std::unique_ptr<Calypso::CalypsoFocusCoordinator> _calypsoFocus;
	Uint8 _calypsoConsumedFocusKeys = 0;

	SDL_Color _palette[256];
	Uint8 _cursorColor;
#ifdef __EMSCRIPTEN__
	/// Calypso: one captured native (design-space) rect for uniform UI scaling.
	struct UiScaledRect { Surface* surf; int x, y, w, h; };
	std::vector<UiScaledRect> _uiNative;   ///< native (design-space) geometry, captured once
	int _uiDesignW = 0;                    ///< design canvas width  (e.g. 320)
	int _uiDesignH = 0;                    ///< design canvas height (e.g. 200)
	bool _uiCaptured = false;
	float _uiScale = 1.0f;                 ///< last applied uniform scale
	float _uiFactor = 1.0f;                ///< per-screen multiplier on the fill scale (1.0 = fill)
	/// Calypso: capture native geometry of every added surface (call AFTER
	/// centerAllSurfaces) and lay the state out scaled to fill the logical buffer.
	/// @param factor per-screen size multiplier (>1 bigger, <1 smaller than fill).
	void enableUiScaling(int designW = 320, int designH = 200, float factor = 1.0f,
		bool subtractVanillaCenter = true);
	/// Calypso: re-apply the uniform UI scale (call from a resize() override).
	void applyUiScaling();
	/// Calypso: re-capture native (design-space) geometry against a NEW design
	/// canvas, then apply. Unlike enableUiScaling (one-shot, ignored once
	/// captured), this is for a state that swaps its whole layout at runtime --
	/// e.g. an F34 screen crossing the Compact<->Wide threshold, which re-applies
	/// a different design-space rect set and must re-snapshot it (external review
	/// #3). Call it AFTER re-applying the new rects.
	void recaptureUiScaling(int designW, int designH, float factor = 1.0f,
		bool subtractVanillaCenter = true);
	/// Calypso: opt every Text / TextButton in the state into HD TTF rendering.
	void applyTTFToTexts(TTFFont* font, float fillFrac = 1.0f);
	/// Calypso: drop a surface from the UI-scaling capture so applyUiScaling
	/// leaves its geometry alone (e.g. a full-frame overlay in base-res space).
	void excludeFromUiScaling(Surface* surf);
#endif
public:
	/// Creates a new state linked to a game.
	State();
	/// Cleans up the state.
	virtual ~State();
	/// Set interface rules.
	void setInterface(const std::string &s, bool alterPal = false, SavedBattleGame *battleGame = 0);
	/// Set window background.
	void setWindowBackground(Window *window, const std::string &s);
	/// Set window background by image name (instead of by interface name).
	void setWindowBackgroundImage(Window* window, const std::string& bgImageName);
	/// Add a optional child element but it will not be displayed.
	template<typename T>
	T* preAdd(T *surface)
	{
		static_assert(std::is_base_of_v<Surface, T>, "Type need to be surface");
		preAdd(static_cast<Surface*>(surface));
		return surface;
	}
	/// Add a optional child element but it will not be displayed.
	void preAdd(Surface *surface);
	/// Adds a child element to the state.
	void add(Surface *surface);
	/// Adds a child element to the state.
	void add(Surface *surface, const std::string &id, const std::string &category, Surface *parent = 0);
	/// Gets whether the state is a full-screen.
	bool isScreen() const;
	/// Toggles whether the state is a full-screen.
	void toggleScreen();
	/// Initializes the state.
	virtual void init();
	/// Handles any events.
	virtual void handle(Action *action);
	/// Runs state functionality every cycle.
	virtual void think();
	/// Blits the state to the screen.
	virtual void blit();
	/// Hides all the state surfaces.
	void hideAll();
	/// Shows all the state surfaces.
	void showAll();
	/// Resets all the state surfaces.
	void resetAll();
	/// Get the localized text.
	LocalizedText tr(const std::string &id) const;
	/// Get the localized text.
	LocalizedText trAlt(const std::string &id, int alt) const;
	/// Get the localized text.
	LocalizedText tr(const std::string &id, unsigned n) const;
	/// Get the localized text.
	LocalizedText tr(const std::string &id, SoldierGender gender) const;
	/// redraw all the text-type surfaces.
	void redrawText();
	/// does the state only have one text list (to scroll)?
	bool hasOnlyOneScrollableTextList() const;
	/// center all surfaces relative to the screen.
	void centerAllSurfaces();
	/// lower all surfaces by half the screen height.
	void lowerAllSurfaces();
	/// switch the colours to use the battlescape palette.
	void applyBattlescapeTheme(const std::string& category);
	/// Sets game object pointer
	static void setGamePtr(Game* game);
	/// Sets a modal surface.
	void setModal(InteractiveSurface *surface);
	/// Clears the modal only when it still belongs to the supplied surface.
	void clearModal(InteractiveSurface *surface);

	/// Opt-in semantic focus for HD family adapters. Disabled legacy states keep
	/// the original InteractiveSurface dispatch unchanged.
	void enableCalypsoFocus();
	void disableCalypsoFocus();
	bool rebuildCalypsoFocus(std::vector<Calypso::CalypsoFocusBinding> bindings,
	                         std::uint64_t generation);
	bool restoreCalypsoFocus(const std::string& id, std::uint64_t generation);
	bool handleCalypsoFocusCommand(Calypso::CalypsoFocusCommand command,
	                               std::uint64_t generation, bool wrap = true);
	const std::string* getCalypsoFocusedId() const;
	InteractiveSurface* getCalypsoFocusedTarget() const;
	std::uint64_t getCalypsoFocusGeneration() const;

	/// Changes a set of colors on the state's 8bpp palette.
	void setStatePalette(const SDL_Color *colors, int firstcolor = 0, int ncolors = 256);
	/// Changes a set of colors on the state's 8bpp palette of helper surfaces.
	void setModPalette();

	/// Changes the state's 8bpp palette with certain resources.
	void setStandardPalette(const std::string &palette, int backpals = -1);
	/// Changes the state's 8bpp palette with certain resources.
	void setCustomPalette(SDL_Color *colors, int cursorColor);

	/// Gets the state's 8bpp palette.
	SDL_Color *getPalette();

	/// Let the state know the window has been resized.
	virtual void resize(int &dX, int &dY);
#ifdef __EMSCRIPTEN__
	/// Declares which base-resolution family this visible state owns. Most
	/// overlays inherit the first explicit state below them.
	virtual Calypso::CalypsoViewportAffinity calypsoViewportAffinity() const
	{
		return Calypso::CalypsoViewportAffinity::Inherit;
	}
#endif
	/// Re-orients all the surfaces in the state.
	virtual void recenter(int dX, int dY);

	/// Gets cursor X coordinate.
	int getCursorX() const;
	/// Gets cursor Y coordinate.
	int getCursorY() const;
};

}
