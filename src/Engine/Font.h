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
#include <unordered_map>
#include <vector>
#include <utility>
#include <SDL.h>
#include "../Engine/Yaml.h"
#include "Unicode.h"

#include "Surface.h"

namespace OpenXcom
{

class Surface;
class Palette;

struct FontImage
{
	int width, height, spacing;
	Surface *surface;
};

#ifdef __EMSCRIPTEN__
/// Metadata for a not-yet-decoded font atlas image (L12b lazy loading).
struct DeferredFontImage
{
	std::string file;   // "Language/<name>.png" — empty once materialised
	UString chars;      // char list from YAML (needed to run init() after decode)
};
#endif

/**
 * Takes care of loading and storing each character in a sprite font.
 * Sprite fonts consist of a set of characters split in fixed-size regions.
 * @note The characters don't all need to be the same size, they can
 * have blank space and will be automatically lined up properly.
 */
class Font
{
private:
	std::vector<FontImage> _images;
	std::unordered_map< UCode, std::pair<size_t, SDL_Rect> > _chars;
	bool _monospace;
	/// Determines the size and position of each character in the font.
	void init(size_t index, const UString &str);
#ifdef __EMSCRIPTEN__
	/// Parallel to _images: entry with empty .file means already decoded.
	std::vector<DeferredFontImage> _deferred;
	/// Maps each deferred glyph codepoint to its _images index (erased on materialise).
	std::unordered_map<UCode, size_t> _deferredCharToSlot;
	/// L12b: decode a deferred atlas PNG on first glyph access.
	void materializeFontImage(size_t imageIdx);
#endif
public:

	/// Default palette for terminal text.
	static const SDL_Color TerminalColors[2];

	/// Creates a blank font.
	Font();
	/// Cleans up the font.
	~Font();
	/// Loads the font from YAML.
	void load(const YAML::YamlNodeReader& reader);
	/// Generate the terminal font.
	void loadTerminal();
	/// Gets a particular character from the font, with its real size.
	/// Non-const: may trigger lazy atlas decode under Emscripten (L12b).
	SurfaceCrop getChar(UCode c);
	/// Gets the font's character width.
	int getWidth() const;
	/// Gets the font's character height.
	int getHeight() const;
	/// Gets the spacing between characters.
	int getSpacing() const;
	/// Gets the size of a particular character.
	/// Non-const: may trigger lazy atlas decode under Emscripten (L12b).
	SDL_Rect getCharSize(UCode c);
};

}
