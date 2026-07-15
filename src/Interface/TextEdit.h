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
#include "Text.h"
#include "../Engine/Unicode.h"
#include "../Calypso/CalypsoTextEditLayout.h"

namespace OpenXcom
{

class Timer;
class TTFFont;
enum TextEditConstraint { TEC_NONE, TEC_NUMERIC_POSITIVE, TEC_NUMERIC };
enum TextEditEnterPolicy { TEEP_INSERT_NEWLINE, TEEP_COMMIT };

/**
 * Editable version of Text.
 * Receives keyboard input to allow the player
 * to change the text himself.
 */
class TextEdit : public InteractiveSurface
{
private:
	Text *_text, *_caret;
	UString _value;
	bool _blink, _modal;
	bool _drawBackground;
	bool _multiline;
	bool _highContrast;
	Timer *_timer;
	TTFFont *_ttf;
	float _ttfFill;
	UCode _char;
	size_t _caretPos;
	size_t _firstVisibleLine;
	int _preferredCaretX;
	TextEditEnterPolicy _enterPolicy;
	TextEditConstraint _textEditConstraint;
	// Cached reflow contract: invalidated only by value, size, font/TTF/fill, or
	// renderer-availability changes. Blink/caret-only redraws must reuse it.
	Calypso::CalypsoTextEditLayout _multilineLayout;
	bool _multilineLayoutValid;
	bool _multilineDirectTTF;
	TTFFont *_multilineMetricTTF;
	int _multilineLineHeight;
	ActionHandler _change;
	ActionHandler _enter;
	State *_state;
	/// Checks if a character will exceed the maximum width.
	bool exceedsMaxWidth(UCode c) const;
	/// Checks if character is valid to be inserted at caret position.
	bool isValidChar(UCode c) const;
	/// Keeps the multiline caret inside the vertically visible row window.
	void updateMultilineViewport();
	/// Invalidates cached wrapping/metrics after a layout input changes.
	void invalidateMultilineLayout();
	/// Rebuilds cached wrapping/metrics only after invalidation.
	void ensureMultilineLayout();
	/// Inserts validated SDL_TEXTINPUT payload at the caret.
	void textInput(Action *action, State *state);
public:
	/// Creates a new text edit with the specified size and position.
	TextEdit(State *state, int width, int height, int x = 0, int y = 0);
	/// Cleans up the text edit.
	~TextEdit();
	/// Handle focus.
	void handle(Action *action, State *state) override;
	/// Sets focus on this text edit.
	void setFocus(bool focus, bool modal = true) override;
	/// Sets the text size to big.
	void setBig();
	/// Sets the text size to small.
	void setSmall();
	/// Resizes and invalidates cached wrapping.
	void setWidth(int width) override;
	/// Resizes and invalidates cached vertical capacity.
	void setHeight(int height) override;
	/// Initializes the text edit's resources.
	void initText(Font *big, Font *small, Language *lang) override;
	/// Sets the text's string.
	void setText(const std::string &text);
	/// Gets the text edit's string.
	std::string getText() const;
	/// Gets the pixel width of the current text (bitmap font), for hit-area sizing.
	int getTextWidth() const;
	/// Sets the text edit's wordwrap setting.
	void setWordWrap(bool wrap);
	/// Enables opt-in multiline editing. Disabled preserves the legacy contract.
	void setMultiline(bool multiline);
	/// Returns whether opt-in multiline editing is enabled.
	bool isMultiline() const { return _multiline; }
	/// Opts the rendered value and caret into the HD TTF path.
	void setTTFFont(TTFFont *font, float fillFrac = 1.0f);
	/// Selects whether Enter commits or inserts a newline in multiline mode.
	void setEnterPolicy(TextEditEnterPolicy policy) { _enterPolicy = policy; }
	/// Sets the text edit's color invert setting.
	void setInvert(bool invert);
	/// Sets the text edit's high contrast color setting.
	void setHighContrast(bool contrast) override;
	/// Sets the text edit's horizontal alignment.
	void setAlign(TextHAlign align);
	/// Sets the text edit's vertical alignment.
	void setVerticalAlign(TextVAlign valign);
	/// Sets the text edit constraint.
	void setConstraint(TextEditConstraint constraint);
	/// Sets the text edit's color.
	void setColor(Uint8 color) override;
	/// Gets the text edit's color.
	Uint8 getColor() const;
	/// Sets the text edit's secondary color.
	void setSecondaryColor(Uint8 color) override;
	/// Gets the text edit's secondary color.
	Uint8 getSecondaryColor() const;
	/// Sets the text edit's palette.
	void setPalette(const SDL_Color *colors, int firstcolor = 0, int ncolors = 256) override;
	/// Handles the timers.
	void think() override;
	/// Plays the blinking animation.
	void blink();
	/// Draws the text edit.
	void draw() override;
	/// Special handling for mouse presses.
	void mousePress(Action *action, State *state) override;
	/// Special handling for keyboard presses.
	void keyboardPress(Action *action, State *state) override;
	/// Hooks an action handler to when the text changes.
	void onChange(ActionHandler handler);
	/// Sets a function to be called every time ENTER is pressed.
	void onEnter(ActionHandler handler);
	/// Explicitly commits through the legacy enter callback. The callback is terminal
	/// and may synchronously destroy this edit or its owning state.
	void commit(Action *action);
	/// Sets the text edit's background drawing setting.
	void setDrawBackground(bool drawBackground) { _drawBackground = drawBackground; }
#ifdef __EMSCRIPTEN__
	/// Phase 33 (Emscripten): replace the whole value from the JS input overlay.
	void setTextExternal(const std::string &utf8);
	/// Re-send geometry only after a browser reflow without replacing the draft.
	void refreshExternalGeometry();
#endif
};

}
