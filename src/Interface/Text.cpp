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
#include "Text.h"
#include "../fmath.h"
#include "../Engine/Font.h"
#include "../Engine/Options.h"
#include "../Engine/Language.h"
#include "../Engine/Unicode.h"
#include "../Engine/ShaderDraw.h"
#include "../Engine/ShaderMove.h"
#include "../Engine/Action.h"
#include "../Engine/TTFFont.h"
#include "../Engine/TTFUtil.h"

namespace OpenXcom
{

/**
 * Sets up a blank text with the specified size and position.
 * @param width Width in pixels.
 * @param height Height in pixels.
 * @param x X position in pixels.
 * @param y Y position in pixels.
 */
Text::Text(int width, int height, int x, int y) : InteractiveSurface(width, height, x, y),
	_big(0), _small(0), _font(0), _fontOrig(0), _lang(0),
	_wrap(false), _invert(false), _contrast(false), _indent(false), _scroll(false), _ignoreSeparators(false),
	_align(ALIGN_LEFT), _valign(ALIGN_TOP), _color(0), _color2(0), _scrollY(0)
{
}

/**
 *
 */
Text::~Text()
{

}

/**
 * Changes the text to use the big-size font.
 */
void Text::setBig()
{
	_font = _big;
	_fontOrig = _big;
	processText();
}

/**
 * Changes the text to use the small-size font.
 */
void Text::setSmall()
{
	_font = _small;
	_fontOrig = _small;
	processText();
}

/**
 * Returns the font currently used by the text.
 * @return Pointer to font.
 */
Font *Text::getFont() const
{
	return _font;
}

/**
 * Changes the various resources needed for text rendering.
 * The different fonts need to be passed in advance since the
 * text size can change mid-text, and the language affects
 * how the text is rendered.
 * @param big Pointer to large-size font.
 * @param small Pointer to small-size font.
 * @param lang Pointer to current language.
 */
void Text::initText(Font *big, Font *small, Language *lang)
{
	_big = big;
	_small = small;
	_lang = lang;
	setSmall();
}

/**
 * Changes the string displayed on screen.
 * @param text Text string.
 */
void Text::setText(const std::string &text)
{
	_text = text;
	_font = _fontOrig;
	processText();
	// If big text won't fit the space, try small text
	if (!_text.empty())
	{
		if (_font == _big && (getTextWidth() > getWidth() || getTextHeight() > getHeight()) && _text[_text.size() - 1] != '.')
		{
			_font = _small;
			processText();
		}
	}
}

/**
 * Returns the string displayed on screen.
 * @return Text string.
 */
std::string Text::getText() const
{
	return _text;
}

/**
 * Enables/disables text wordwrapping. When enabled, lines of
 * text are automatically split to ensure they stay within the
 * drawing area, otherwise they simply go off the edge.
 * @param wrap Wordwrapping setting.
 * @param indent Indent wrapped text.
 * @param ignoreSeparators Handle separators as spaces (false) or as normal text (true)?
 */
void Text::setWordWrap(bool wrap, bool indent, bool ignoreSeparators)
{
	if (wrap != _wrap || indent != _indent || ignoreSeparators != _ignoreSeparators)
	{
		_wrap = wrap;
		_indent = indent;
		_ignoreSeparators = ignoreSeparators;
		processText();
	}
}

/**
 * Enables/disables color inverting. Mostly used to make
 * button text look pressed along with the button.
 * @param invert Invert setting.
 */
void Text::setInvert(bool invert)
{
	_invert = invert;
	_redraw = true;
}

/**
 * Enables/disables high contrast color. Mostly used for
 * Battlescape UI.
 * @param contrast High contrast setting.
 */
void Text::setHighContrast(bool contrast)
{
	_contrast = contrast;
	_redraw = true;
}

/**
 * Changes the way the text is aligned horizontally
 * relative to the drawing area.
 * @param align Horizontal alignment.
 */
void Text::setAlign(TextHAlign align)
{
	_align = align;
	_redraw = true;
}

/**
 * Returns the way the text is aligned horizontally
 * relative to the drawing area.
 * @return Horizontal alignment.
 */
TextHAlign Text::getAlign() const
{
	return _align;
}

/**
 * Changes the way the text is aligned vertically
 * relative to the drawing area.
 * @param valign Vertical alignment.
 */
void Text::setVerticalAlign(TextVAlign valign)
{
	_valign = valign;
	_redraw = true;
}

/**
 * Returns the way the text is aligned vertically
 * relative to the drawing area.
 * @return Horizontal alignment.
 */
TextVAlign Text::getVerticalAlign() const
{
	return _valign;
}

/**
 * Changes the color used to render the text. Unlike regular graphics,
 * fonts are greyscale so they need to be assigned a specific position
 * in the palette to be displayed.
 * @param color Color value.
 */
void Text::setColor(Uint8 color)
{
	_color = color;
	_color2 = color;
	_redraw = true;
}

/**
 * Returns the color used to render the text.
 * @return Color value.
 */
Uint8 Text::getColor() const
{
	return _color;
}

/**
 * Sets an ARGB color for rendering on ARGB surfaces.
 * @param argb ARGB color value (0xAARRGGBB).
 */
void Text::setColorRGB(Uint32 argb)
{
	_colorRGB = argb;
	_useRGB   = true;
	_redraw = true;
}

void Text::setColorRGB2(Uint32 argb)
{
	_colorRGB2 = argb;
	_redraw = true;
}

/**
 * Calypso: opt this label into HD TrueType rendering. While a font is set,
 * single-line text is rasterised via TTF and fit-blitted (the bitmap Font path
 * is kept intact and used for null / multi-line). @a fillFrac shrinks the glyph
 * block within the widget box.
 * @param font HD font (or null to restore the bitmap path).
 * @param fillFrac Fraction of the fit box to fill (0 < fillFrac <= 1).
 */
void Text::setTTFFont(TTFFont *font, float fillFrac)
{
	_ttf = font;
	_ttfFill = fillFrac > 0.0f ? fillFrac : 1.0f;
	_redraw = true;
}

/**
 * Changes the secondary color used to render the text. The text
 * switches between the primary and secondary color whenever there's
 * a 0x01 in the string.
 * @param color Color value.
 */
void Text::setSecondaryColor(Uint8 color)
{
	_color2 = color;
	_redraw = true;
}

/**
 * Returns the secondary color used to render the text.
 * @return Color value.
 */
Uint8 Text::getSecondaryColor() const
{
	return _color2;
}

int Text::getNumLines() const
{
	return _wrap ? _lineHeight.size() : 1;
}

/**
 * Returns the rendered text's height. Useful to check if wordwrap applies.
 * @param line Line to get the height, or -1 to get whole text height.
 * @return Height in pixels.
 */
int Text::getTextHeight(int line) const
{
	if (line == -1)
	{
		int height = 0;
		for (int lh : _lineHeight)
		{
			height += lh;
		}
		return height;
	}
	else
	{
		return _lineHeight[line];
	}
}

/**
 * Returns the rendered text's width.
 * @param line Line to get the width, or -1 to get whole text width.
 * @return Width in pixels.
 */
int Text::getTextWidth(int line) const
{
	if (line == -1)
	{
		int width = 0;
		for (int lw : _lineWidth)
		{
			if (lw > width)
			{
				width = lw;
			}
		}
		return width;
	}
	else
	{
		return _lineWidth[line];
	}
}

/**
 * Takes care of any text post-processing like converting
 * encoded text to individual codepoints and calculating
 * line metrics for alignment and wordwrapping.
 */
void Text::processText()
{
	if (_font == 0 || _lang == 0)
	{
		return;
	}

	_processedText = Unicode::convUtf8ToUtf32(_text);
	_lineWidth.clear();
	_lineHeight.clear();
	_scrollY = 0;

	int width = 0, word = 0;
	size_t space = 0, textIndentation = 0;
	bool start = true;
	Font *font = _font;
	UString &str = _processedText;

	// Go through the text character by character
	for (size_t c = 0; c <= str.size(); ++c)
	{
		// End of the line
		if (c == str.size() || Unicode::isLinebreak(str[c]))
		{
			// Add line measurements for alignment later
			_lineWidth.push_back(width);
			_lineHeight.push_back(font->getCharSize('\n').h);
			start = true;
			width = 0;
			word = 0;

			if (c == str.size())
				break;
			else if (str[c] == Unicode::TOK_NL_SMALL)
				font = _small;
		}
		// Keep track of spaces for wordwrapping
		else if (Unicode::isSpace(str[c]) || (!_ignoreSeparators && Unicode::isSeparator(str[c])))
		{
			// Store existing indentation
			if (c == textIndentation)
			{
				textIndentation++;
			}
			space = c;
			start = start && (word == 0); // consider initial spaces still as start of line until first character is met
			width += font->getCharSize(str[c]).w;
			word = 0;
		}
		// Custom format, skip 3 next chars
		else if (str[c] == Unicode::TOK_CUSTOM_FORMAT)
		{
			if (c + 3u > str.size())
			{
				str.resize(c + 3u); // add missing character
				str[c + 1u] = '\0';
				str[c + 2u] = '\0';
			}
			c += 2u;
		}
		// Keep track of the width of the last line and word
		else if (str[c] != Unicode::TOK_COLOR_FLIP)
		{
			int charWidth = font->getCharSize(str[c]).w;

			width += charWidth;
			word += charWidth;

			// Wordwrap if the last word doesn't fit the line
			if (_wrap && width >= getWidth() && (!start || _lang->getTextWrapping() == WRAP_LETTERS))
			{
				size_t indentLocation = c;
				if (_lang->getTextWrapping() == WRAP_WORDS || Unicode::isSpace(str[c]))
				{
					// Go back to the last space and put a linebreak there
					width -= word;
					indentLocation = space;
					if (Unicode::isSpace(str[space]))
					{
						width -= font->getCharSize(str[space]).w;
						str[space] = '\n';
					}
					else
					{
						str.insert(space+1, 1, '\n');
						indentLocation++;
					}
				}
				else if (_lang->getTextWrapping() == WRAP_LETTERS)
				{
					// Go back to the last letter and put a linebreak there
					str.insert(c, 1, '\n');
					width -= charWidth;
				}

				// Keep initial indentation of text
				if (textIndentation > 0)
				{
					str.insert(indentLocation+1, textIndentation, '\t');
					indentLocation += textIndentation;
				}
				// Indent due to word wrap.
				if (_indent)
				{
					str.insert(indentLocation+1, 1, '\t');
					width += font->getCharSize('\t').w;
				}

				_lineWidth.push_back(width);
				_lineHeight.push_back(font->getCharSize('\n').h);
				if (_lang->getTextWrapping() == WRAP_WORDS)
				{
					width = word;
				}
				else if (_lang->getTextWrapping() == WRAP_LETTERS)
				{
					width = 0;
				}
				start = true;
			}
		}
	}

	_redraw = true;
}

namespace
{

struct PaletteShift
{
	static inline void func(Uint8& dest, const Uint8& src, int off, int mul, int mid)
	{
		if(src)
		{
			int inverseOffset = mid ? 2 * (mid - src) : 0;
			dest = off + src * mul + inverseOffset;
		}
	}
};

} //namespace

/**
 * Calculates the starting X position for a line of text.
 * @param line The line number (0 = first, etc).
 * @return The X position in pixels.
 */
int Text::getLineX(int line) const
{
	int x = 0;
	switch (_lang->getTextDirection())
	{
	case DIRECTION_LTR:
		switch (_align)
		{
		case ALIGN_LEFT:
			break;
		case ALIGN_CENTER:
			x = (int)ceil((getWidth() + _font->getSpacing() - _lineWidth[line]) / 2.0);
			break;
		case ALIGN_RIGHT:
			x = getWidth() - 1 - _lineWidth[line];
			break;
		}
		break;
	case DIRECTION_RTL:
		switch (_align)
		{
		case ALIGN_LEFT:
			x = getWidth() - 1;
			break;
		case ALIGN_CENTER:
			x = getWidth() - (int)ceil((getWidth() + _font->getSpacing() - _lineWidth[line]) / 2.0);
			break;
		case ALIGN_RIGHT:
			x = _lineWidth[line];
			break;
		}
		break;
	}
	return x;
}

/**
 * Draws all the characters in the text with a really
 * nasty complex gritty text rendering algorithm logic stuff.
 */
#ifdef __EMSCRIPTEN__
/**
 * Calypso: rasterise the label via the opt-in TTF font and fit-blit it into this
 * surface. Returns false (keep the bitmap path) for multi-line text, a missing
 * palette, or a failed render. Colour comes from the explicit ARGB color when
 * set, otherwise the brightest step of the widget's palette colour ramp — the
 * same texel the bitmap glyph core would use — so TTF labels match their theme.
 */
bool Text::drawTTF()
{
	if (!_ttf || getNumLines() > 1)
	{
		return false;
	}
	SDL_Color rgba;
	if (_useRGB)
	{
		rgba.r = (_colorRGB >> 16) & 0xFF;
		rgba.g = (_colorRGB >> 8) & 0xFF;
		rgba.b = _colorRGB & 0xFF;
		rgba.a = (_colorRGB >> 24) & 0xFF;
		if (rgba.a == 0) rgba.a = 0xFF;
	}
	else
	{
		const SDL_Color *pal = getEffectivePalette();
		if (!pal)
		{
			return false;
		}
		const int mul = _contrast ? 3 : 1;
		int idx = (int)_color + 4 * mul;
		if (idx < 0) idx = 0; else if (idx > 255) idx = 255;
		rgba = pal[idx];
		rgba.a = 0xFF;
	}
	// Strip OXCE control tokens (TOK_COLOR_FLIP=1, TOK_NL_SMALL=2,
	// TOK_CUSTOM_FORMAT=27, …) — SDL_ttf has no glyph and would render tofu (□).
	// All are < 0x20 (single-byte, so UTF-8 multi-byte sequences stay intact).
	// The colour-flip distinction is moot here: TTF labels render in one colour.
	std::string clean;
	clean.reserve(_text.size());
	for (char c : _text)
	{
		if ((unsigned char)c >= 0x20)
		{
			clean += c;
		}
	}
	if (clean.empty())
	{
		return false;
	}
	SDL_Surface *rendered = _ttf->renderText(clean, rgba);
	if (!rendered)
	{
		return false;
	}
	const TTFUtil::HAlign h = (_align == ALIGN_CENTER) ? TTFUtil::H_CENTER
	                        : (_align == ALIGN_RIGHT)  ? TTFUtil::H_RIGHT
	                                                   : TTFUtil::H_LEFT;
	const TTFUtil::VAlign v = (_valign == ALIGN_MIDDLE) ? TTFUtil::V_MIDDLE
	                        : (_valign == ALIGN_BOTTOM) ? TTFUtil::V_BOTTOM
	                                                    : TTFUtil::V_TOP;
	TTFUtil::blitFit(rendered, this, h, v, _ttfFill);
	return true;
}
#endif

void Text::draw()
{
	Surface::draw();
	if (_text.empty() || _font == 0)
	{
		return;
	}

#ifdef __EMSCRIPTEN__
	// Calypso: HD TTF path (opt-in). Single-line labels/buttons only; multi-line
	// or a failed render falls through to the bitmap glyph path below.
	if (_ttf && drawTTF())
	{
		return;
	}
#endif

	// Show text borders for debugging
	if (Options::debugUi)
	{
		SDL_Rect r;
		r.w = getWidth();
		r.h = getHeight();
		r.x = 0;
		r.y = 0;
		this->drawRect(&r, 5);
		r.w-=2;
		r.h-=2;
		r.x++;
		r.y++;
		this->drawRect(&r, 0);
	}

	int x = 0, y = 0, line = 0, height = 0;
	Font *font = _font;
	int color = _color;
	bool isAltColor = false;
	const UString &s = _processedText;

	height = getTextHeight();

	if (_scroll && (getHeight() - height < 0))
	{
		y = _scrollY;
	}
	else
	{
		switch (_valign)
		{
		case ALIGN_TOP:
			y = 0;
			break;
		case ALIGN_MIDDLE:
			y = (int)ceil((getHeight() - height) / 2.0);
			break;
		case ALIGN_BOTTOM:
			y = getHeight() - height;
			break;
		}
	}

	x = getLineX(line);

	// Set up text color
	int mul = 1;
	if (_contrast)
	{
		mul = 3;
	}

	// Set up text direction
	int dir = 1;
	if (_lang->getTextDirection() == DIRECTION_RTL)
	{
		dir = -1;
	}

	// Invert text by inverting the font palette on index 3 (font palettes use indices 1-5)
	int mid = _invert ? 3 : 0;

	// Draw each letter one by one
	for (UString::const_iterator c = s.begin(); c != s.end(); ++c)
	{
		if (Unicode::isSpace(*c) || *c == '\t')
		{
			x += dir * font->getCharSize(*c).w;
		}
		else if (Unicode::isLinebreak(*c))
		{
			line++;
			y += font->getCharSize(*c).h;
			x = getLineX(line);
			if (*c == Unicode::TOK_NL_SMALL)
			{
				font = _small;
			}
		}
		else if (*c == Unicode::TOK_COLOR_FLIP)
		{
			if (isAltColor == false)
			{
				color = _color2;
				isAltColor = true;
			}
			else
			{
				color = _color;
				isAltColor = false;
			}
		}
		else if (*c == Unicode::TOK_CUSTOM_FORMAT)
		{
			const auto op = *(c + 1u);
			const auto arg = *(c + 2u);
			switch (op)
			{
				case 'C': // custom color like "\eC\x45"
					color = arg;
					isAltColor = false;
					break;

				case 'c': // specific color like "\ecP" - primary, "\ecS" - secondary
					if (arg == 'P')
					{
						color = _color;
						isAltColor = false;
					}
					else if (arg == 'S')
					{
						color = _color2;
						isAltColor = true;
					}
					break;

				default:
					// nothing
					break;
			}
			c += 2;
		}
		else
		{
			if (dir < 0)
				x += dir * font->getCharSize(*c).w;
			auto chr = font->getChar(*c);
			chr.setX(x);
			chr.setY(y);
			{
				// 7.F/7.K: unified ARGB glyph blit — reads brightness from the font atlas
				// (B channel of ARGB8888 LE = grayscale for all TFTD fonts).
				Uint32 colorARGB;
				if (_useRGB)
				{
					colorARGB = isAltColor ? _colorRGB2 : _colorRGB;
				}
				else
				{
					const SDL_Color *pal = getEffectivePalette();
					// R4: use shade +5 (brightest) for inverted text so it contrasts
					// against the button fill, which stays at _color+3 (mid) after invert.
					Uint8 palIdx = (Uint8)(color + (mid != 0 ? 5 : 0));
					colorARGB = pal
						? (0xFF000000u | ((Uint32)pal[palIdx].r << 16) | ((Uint32)pal[palIdx].g << 8) | (Uint32)pal[palIdx].b)
						: 0xFFFFFFFFu;
				}
				const Surface *glyphAtlas = chr.getSurface();
				const SDL_Rect *srcR = chr.getCrop();
				int gx = srcR->x, gy = srcR->y, cw = srcR->w, ch = srcR->h;
				lock();
				for (int py = 0; py < ch; ++py)
					for (int px = 0; px < cw; ++px)
					{
						// Read glyph atlas pixel as 8bpp ramp index (1..4 for OXCE fonts)
						// from the palette mirror when available — this preserves the
						// authentic per-pixel AA ramp that the legacy 8bpp PaletteShift
						// shader produced (dest = _color + src*mul + 2*(mid-src)).
						const Uint8 *mirror = glyphAtlas ? glyphAtlas->getPaletteMirror() : nullptr;
						Uint8 srcRamp = 0u;
						if (mirror)
						{
							const Uint16 mw = glyphAtlas->getPaletteMirrorWidth();
							srcRamp = mirror[(size_t)(gy + py) * (size_t)mw + (size_t)(gx + px)];
						}
						else if (glyphAtlas)
						{
							srcRamp = glyphAtlas->getPixel(gx + px, gy + py);
						}
						if (srcRamp == 0) continue;

						if (!_useRGB && mirror)
						{
							// Per-pixel palette ramp resolution — matches legacy 8bpp
							// rendering exactly. Picks a different palette colour for
							// each AA intensity step instead of alpha-blending one
							// colour, so glyphs stay solid against any background.
							const SDL_Color *pal = getEffectivePalette();
							if (pal)
							{
								int rampIdx = (int)color + (int)srcRamp * mul;
								if (mid != 0) rampIdx += 2 * ((int)mid - (int)srcRamp);
								if (rampIdx < 0) rampIdx = 0;
								else if (rampIdx > 255) rampIdx = 255;
								Uint32 outARGB =
									0xFF000000u
									| ((Uint32)pal[rampIdx].r << 16)
									| ((Uint32)pal[rampIdx].g << 8)
									| (Uint32)pal[rampIdx].b;
								setPixel32(x + px, y + py, outARGB);
								continue;
							}
						}

						// Fallback: alpha-mask blend. Two sources of srcRamp:
						//   * mirror present  → 8bpp ramp index 1..4 → must scale up
						//     (matches commit 4f8097fd0 alpha mapping) so AA pixels
						//     don't render as alpha=1..4 (effectively invisible).
						//   * mirror absent   → blue/alpha channel of HD grayscale
						//     ARGB atlas, already in 0..255 range, use as-is.
						Uint8 brightness = mirror
							? ((srcRamp >= 4u) ? 255u : (Uint8)((Uint32)srcRamp * 255u / 4u))
							: srcRamp;
						if (mul > 1)
						{
							int boosted = (int)brightness * mul;
							brightness = boosted > 255 ? (Uint8)255 : (Uint8)boosted;
						}
						Uint8 da = (Uint8)(((Uint32)(colorARGB >> 24) * brightness) / 255u);
						setPixel32(x + px, y + py, (colorARGB & 0x00FFFFFFu) | ((Uint32)da << 24));
					}
				unlock();
			}
			if (dir > 0)
				x += dir * font->getCharSize(*c).w;
		}
	}
}

/**
 * Allows the text to be scrollable via mouse wheel.
 */
void Text::setScrollable(bool scroll)
{
	_scroll = scroll;
}

/**
 * Handles scrolling.
 * @param action Pointer to an action.
 * @param state State that the action handlers belong to.
 */
void Text::mousePress(Action* action, State* state)
{
	InteractiveSurface::mousePress(action, state);
	if (_scroll &&
		(action->getDetails()->button.button == SDL_BUTTON_WHEELUP ||
		action->getDetails()->button.button == SDL_BUTTON_WHEELDOWN))
	{
		int scrollArea = getHeight() - getTextHeight();
		if (scrollArea < 0)
		{
			int scrollAmount = _font->getHeight() + _font->getSpacing();
			if (action->getDetails()->button.button == SDL_BUTTON_WHEELDOWN)
				scrollAmount = -scrollAmount;

			_scrollY = Clamp(_scrollY + scrollAmount, scrollArea, 0);
			_redraw = true;
		}
	}
}

}
