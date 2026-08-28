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
#include <cstdint>
#include <vector>
#include <string>
#include "../Engine/Unicode.h"

namespace OpenXcom
{

class Font;
class Language;
class TTFFont;

enum TextHAlign { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT };
enum TextVAlign { ALIGN_TOP, ALIGN_MIDDLE, ALIGN_BOTTOM };

/**
 * Text string displayed on screen.
 * Takes the characters from a Font and puts them together on screen
 * to display a string of text, taking care of any required aligning
 * or wrapping.
 */
class Text : public InteractiveSurface
{
private:
	Font *_big, *_small, *_font, *_fontOrig;
	Language *_lang;
	std::string _text;
	UString _processedText;
	std::vector<int> _lineWidth, _lineHeight;
	bool _wrap, _invert, _contrast, _indent, _scroll, _ignoreSeparators;
	TextHAlign _align;
	TextVAlign _valign;
	Uint8 _color, _color2;
	int _scrollY;
	Uint32 _colorRGB = 0;
	Uint32 _colorRGB2 = 0;
	bool _useRGB = false;
#ifdef __EMSCRIPTEN__
	/// Calypso: monotonic content generation; bumped by setText() only when
	/// the new content differs, so render caches can key on it without any
	/// per-frame string read or hashing.
	std::uint64_t _calypsoTextGeneration = 0;
#endif
	TTFFont *_ttf = nullptr;     ///< Calypso: opt-in HD font; null = legacy bitmap path
	float _ttfFill = 1.0f;       ///< Calypso: shrink factor within the fit box

	/// Processes the contained text.
	void processText();
	/// Gets the X position of a text line.
	int getLineX(int line) const;
#ifdef __EMSCRIPTEN__
	/// Calypso: render the (single-line) string via TTF; false ⇒ fall back to bitmap.
	bool drawTTF();
#endif
public:
	/// Creates a new text with the specified size and position.
	Text(int width, int height, int x = 0, int y = 0);
	/// Cleans up the text.
	~Text();
	/// Sets the text size to big.
	void setBig();
	/// Sets the text size to small.
	void setSmall();
	/// Gets the text's current font.
	Font *getFont() const;
	/// Initializes the resources for the text.
	void initText(Font *big, Font *small, Language *lang) override;
	/// Sets the text's string.
	void setText(const std::string &text);
	/// Gets the text's string.
	std::string getText() const;
#ifdef __EMSCRIPTEN__
	/// Calypso: allocation-free read for steady-state HD render caches.
	const std::string& calypsoTextRef() const { return _text; }
	/// Calypso: monotonic generation of the text content. Incremented by
	/// setText() only when the new content differs from the stored one.
	std::uint64_t calypsoTextGeneration() const { return _calypsoTextGeneration; }
#endif
	/// Sets the text's wordwrap setting.
	void setWordWrap(bool wrap, bool indent = false, bool ignoreSeparators = false);
	/// Sets the text's color invert setting.
	void setInvert(bool invert);
	/// Sets the text's high contrast color setting.
	void setHighContrast(bool contrast) override;
	/// Sets the text's horizontal alignment.
	void setAlign(TextHAlign align);
	/// Gets the text's horizontal alignment.
	TextHAlign getAlign() const;
	/// Sets the text's vertical alignment.
	void setVerticalAlign(TextVAlign valign);
	/// Gets the text's vertical alignment.
	TextVAlign getVerticalAlign() const;
	/// Sets the text's color.
	void setColor(Uint8 color) override;
	/// Gets the text's color.
	Uint8 getColor() const;
	/// Sets the text's ARGB primary color.
	void setColorRGB(Uint32 argb);
	/// Sets the text's ARGB secondary color (for TOK_COLOR_FLIP).
	void setColorRGB2(Uint32 argb);
	/// Calypso: opt into HD TTF rendering for this label (null restores the bitmap path).
	void setTTFFont(TTFFont *font, float fillFrac = 1.0f);
	/// Sets the text's secondary color.
	void setSecondaryColor(Uint8 color) override;
	/// Gets the text's secondary color.
	Uint8 getSecondaryColor() const;
	/// Gets the number of lines in the (wrapped, if wrapping is enabled) text
	int getNumLines() const;
	/// Gets the rendered text's width.
	int getTextWidth(int line = -1) const;
	/// Gets the rendered text's height.
	int getTextHeight(int line = -1) const;
	/// Draws the text.
	void draw() override;
#ifdef __EMSCRIPTEN__
	void blit(SDL_Surface* surface) override;
#endif
	/// Sets the text's scrollable setting.
	void setScrollable(bool scroll);
	/// Special handling for mouse presses.
	void mousePress(Action* action, State* state) override;
};

}
