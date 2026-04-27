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
#include "../Engine/Yaml.h"
#include <string>
#include <map>
#include <vector>
#include <functional>

struct SDL_Color;

namespace OpenXcom
{

class Surface;
class SurfaceSet;
struct ModData;

/**
 * For adding a set of extra sprite data to the game.
 */
class ExtraSprites
{
private:
	std::string _type;
	std::map<int, std::string> _sprites;
	const ModData* _current;
	int _width, _height;
	bool _singleImage;
	int _subX, _subY;
	bool _loaded;
	bool _hd;
	// Phase 7.A.4: palette-cycle phase palette names.
	// Each entry names a Mod palette (looked up via paletteLookup in
	// buildCycleTables). Empty means no palette cycling for this asset.
	std::vector<std::string> _paletteCycle;

	Surface *getFrame(SurfaceSet *set, int index) const;
public:
	/// Creates a blank external sprite set.
	ExtraSprites();
	/// Cleans up the external sprite set.
	virtual ~ExtraSprites();
	/// Loads the data from YAML.
	void load(const YAML::YamlNodeReader& reader, const ModData* current);
	/// Gets the sprite's type.
	const std::string& getType() const;
	/// Gets the list of sprites defined by this mod.
	std::map<int, std::string> *getSprites();
	/// Gets the width of the surfaces (used for single images and new spritesets).
	int getWidth() const;
	/// Gets the height of the surfaces (used for single images and new spritesets).
	int getHeight() const;
	/// Checks if this is a single surface, or a set of surfaces.
	bool getSingleImage() const;
	/// Gets the x subdivision.
	int getSubX() const;
	/// Gets the y subdivision.
	int getSubY() const;
	/// Has this sprite been loaded?
	bool isLoaded() const;
	/// Is this an HD (32-bit RGBA) sprite?
	bool isHD() const;
	/// Returns the palette-cycle phase list (may be empty).
	const std::vector<std::string>& getPaletteCycle() const { return _paletteCycle; }
	/// Builds and attaches cycle-phase ShadeTable objects to all frames of a surface set.
	/// paletteLookup(name) must return a pointer to 256 SDL_Color entries or nullptr.
	void buildCycleTables(SurfaceSet *set,
	                      const std::function<const SDL_Color*(const std::string&)>& paletteLookup) const;
	/// Overload for single-image surfaces.
	void buildCycleTables(Surface *surface,
	                      const std::function<const SDL_Color*(const std::string&)>& paletteLookup) const;
	/// Checks if a filename is a valid image file.
	static bool isImageFile(const std::string &filename);
	/// Load the external sprite into a surface.
	Surface *loadSurface(Surface *surface);
	/// Load the external sprite into a surface set.
	SurfaceSet *loadSurfaceSet(SurfaceSet *set);
	/// Gets mod data that define this surface.
	const ModData* getModOwner() { return _current; }
};

}
