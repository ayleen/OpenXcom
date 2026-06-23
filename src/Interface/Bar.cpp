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
#include "Bar.h"
#include <SDL.h>

namespace OpenXcom
{

/**
 * Sets up a blank bar with the specified size and position.
 * @param width Width in pixels.
 * @param height Height in pixels.
 * @param x X position in pixels.
 * @param y Y position in pixels.
 */
Bar::Bar(int width, int height, int x, int y) : Surface(width, height, x, y), _color(0), _color2(0), _borderColor(0), _scale(0), _max(0), _value(0), _value2(0), _secondOnTop(true)
{

}

/**
 *
 */
Bar::~Bar()
{
}

/**
 * Changes the color used to draw the border and contents.
 * @param color Color value.
 */
void Bar::setColor(Uint8 color)
{
	_color = color;
	_redraw = true;
}

/**
 * Returns the color used to draw the bar.
 * @return Color value.
 */
Uint8 Bar::getColor() const
{
	return _color;
}

/**
 * Changes the color used to draw the second contents.
 * @param color Color value.
 */
void Bar::setSecondaryColor(Uint8 color)
{
	_color2 = color;
	_redraw = true;
}

/**
 * Returns the second color used to draw the bar.
 * @return Color value.
 */
Uint8 Bar::getSecondaryColor() const
{
	return _color2;
}

/**
 * Changes the scale factor used to draw the bar values.
 * @param scale Scale in pixels/unit.
 */
void Bar::setScale(double scale)
{
	_scale = scale;
	_redraw = true;
}

/**
 * Returns the scale factor used to draw the bar values.
 * @return Scale in pixels/unit.
 */
double Bar::getScale() const
{
	return _scale;
}

/**
 * Changes the maximum value used to draw the outer border.
 * @param max Maximum value.
 */
void Bar::setMax(double max)
{
	_max = max;
	_redraw = true;
}

/**
 * Returns the maximum value used to draw the outer border.
 * @return Maximum value.
 */
double Bar::getMax() const
{
	return _max;
}

/**
 * Changes the value used to draw the inner contents.
 * @param value Current value.
 */
void Bar::setValue(double value)
{
	_value = (value < 0.0)? 0.0 : value;
	_redraw = true;
}

/**
 * Returns the value used to draw the inner contents.
 * @return Current value.
 */
double Bar::getValue() const
{
	return _value;
}

/**
 * Changes the value used to draw the second inner contents.
 * @param value Current value.
 */
void Bar::setValue2(double value)
{
	_value2 = (value < 0.0)? 0.0 : value;
	_redraw = true;
}

/**
 * Returns the value used to draw the second inner contents.
 * @return Current value.
 */
double Bar::getValue2() const
{
	return _value2;
}

/**
 * Defines whether the second value should be drawn on top. Default this is true.
 * @param onTop Second value on top?
 */
void Bar::setSecondValueOnTop(bool onTop)
{
	_secondOnTop = onTop;
}

/**
 * Draws the bordered bar filled according
 * to its values.
 */
void Bar::draw()
{
	Surface::draw();
	SDL_Rect square;

	square.x = 0;
	square.y = 0;
	square.w = (Uint16)(_scale * _max) + 1;
	square.h = getHeight();

	if (_borderColor)
		drawRect(&square, _borderColor);
	else
		drawRect(&square, _color + 4);

	square.y++;
	square.w--;
	square.h -= 2;

	drawRect(&square, 0);

	if (_secondOnTop)
	{
		square.w = (Uint16)(_scale * _value);
		drawRect(&square, _color);
		square.w = (Uint16)(_scale * _value2);
		drawRect(&square, _color2);
	}
	else
	{
		square.w = (Uint16)(_scale * _value2);
		drawRect(&square, _color2);
		square.w = (Uint16)(_scale * _value);
		drawRect(&square, _color);
	}

#ifdef __EMSCRIPTEN__
	// Calypso: shade the filled portion left->right from dark to light. drawRect
	// has written ARGB straight into _surface in this build, so we re-modulate the
	// non-transparent fill pixels by a horizontal position ramp.
	if (_gradient)
	{
		SDL_Surface* s = getSurface();
		if (s && s->format && s->format->BitsPerPixel == 32)
		{
			const double fv = _value > _value2 ? _value : _value2;
			int fillW = (int)(_scale * fv);
			if (fillW > s->w) fillW = s->w;
			const int hh = getHeight();
			if (fillW > 1)
			{
				SDL_LockSurface(s);
				for (int yy = 1; yy < hh - 1 && yy < s->h; ++yy)
				{
					Uint32* row = (Uint32*)((Uint8*)s->pixels + yy * s->pitch);
					for (int xx = 1; xx < fillW; ++xx)
					{
						const Uint32 px = row[xx];
						if ((px >> 24) == 0) continue;           // skip empty/transparent
						const float t = (float)(xx - 1) / (float)(fillW - 1);  // 0 left .. 1 right
						const float f = 0.40f + 0.60f * t;        // dark -> light
						const Uint8 a = (px >> 24) & 0xFF;
						const Uint8 r = (Uint8)(((px >> 16) & 0xFF) * f);
						const Uint8 g = (Uint8)(((px >>  8) & 0xFF) * f);
						const Uint8 b = (Uint8)(( px        & 0xFF) * f);
						row[xx] = ((Uint32)a << 24) | ((Uint32)r << 16) | ((Uint32)g << 8) | b;
					}
				}
				SDL_UnlockSurface(s);
			}
		}
	}
#endif
}

/**
 * sets the border color for the bar.
 * @param bc the color for the outline of the bar.
 * @note will use base colour + 4 if none is defined here.
 */
void Bar::setBorderColor(Uint8 bc)
{
	_borderColor = bc;
}

}
