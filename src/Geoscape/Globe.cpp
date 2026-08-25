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
#include "Globe.h"
#include "../fmath.h"
#include "../Engine/Action.h"
#include "../Engine/SurfaceSet.h"
#include "../Engine/Timer.h"
#include "../Mod/Mod.h"
#include "../Mod/Polygon.h"
#include "../Mod/Polyline.h"
#include "../Engine/FastLineClip.h"
#include "../Engine/Game.h"
#include "../Engine/RNG.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/GameTime.h"
#include "../Savegame/Base.h"
#include "../Savegame/Country.h"
#include "../Mod/RuleCountry.h"
#include "../Interface/Text.h"
#include "../Mod/RuleRegion.h"
#include "../Savegame/Region.h"
#include "../Mod/City.h"
#include "../Savegame/Target.h"
#include "../Savegame/Ufo.h"
#include "../Savegame/Craft.h"
#include "../Savegame/Waypoint.h"
#include "../Engine/ShaderMove.h"
#include "../Engine/ShaderRepeat.h"
#include "../Engine/Options.h"
#include "../Savegame/MissionSite.h"
#include "../Savegame/AlienBase.h"
#include "../Engine/Language.h"
#include "../Savegame/BaseFacility.h"
#include "../Mod/RuleBaseFacility.h"
#include "../Mod/RuleCraft.h"
#include "../Mod/RuleGlobe.h"
#include "../Mod/Texture.h"
#include "../Interface/Cursor.h"
#include "../Engine/Screen.h"
#include "../Calypso/CalypsoGeoscapeProjection.h"
#include "../Calypso/CalypsoGeoscapeQaPresentation.h"
#include <limits>
#include <new>
#ifdef __EMSCRIPTEN__
#  include "../Engine/GpuInit.h"
#  include "../Engine/GpuTexture.h"
#  include "../Engine/GpuTimer.h"
#  include "../Engine/Shader.h"
#  include "../Engine/ShaderManager.h"
#  include "../Engine/Logger.h"
#  include "../Engine/ShadeTable.h"
#  include <GLES3/gl3.h>
#  include <SDL.h>
#  include <cmath>
#  include <algorithm>
#  include <vector>

/* Phase 8c §C2 perf-log gate. Definition lives in Calypso/EmscriptenHarness.cpp
 * inside `extern "C" { … }`, which puts it in the global namespace.  The
 * forward declaration must therefore also be in the global namespace and
 * carry C linkage; placing it outside `namespace OpenXcom { … }` below is
 * what makes the link symbol resolve. */
extern "C" int g_calypsoProfileGlobe;
extern "C" int g_calypsoGlobeGpuDirect;
extern "C" int calypso_context_reset_sentinel_pending(void);
extern "C" void calypso_context_reset_sentinel_consumed(void);
extern "C" void calypso_context_reset_boundary_close(void);
#endif

#include "../Calypso/CalypsoGeoscapeHdGlobeDirect.h"

namespace OpenXcom
{

void Globe::setGpuDirect(bool on)
{
#ifdef __EMSCRIPTEN__
	CalypsoGeoscapeHdGlobeDirect::setGpuDirect(this, on);
#else
	(void)on;
#endif
}

const double Globe::ROTATE_LONGITUDE = 0.10;
const double Globe::ROTATE_LATITUDE = 0.06;

Uint8 Globe::OCEAN_COLOR;
bool Globe::OCEAN_SHADING;
Uint8 Globe::COUNTRY_LABEL_COLOR;
Uint8 Globe::LINE_COLOR;
Uint8 Globe::CITY_LABEL_COLOR;
Uint8 Globe::BASE_LABEL_COLOR;

namespace
{

///helper class for `Globe` for drawing earth globe with shadows
struct GlobeStaticData
{
	static const int random_surf_size = 60;
	static const int random_multiplier_noise_bits = 4;
	static const int random_distance_noise_bits = 3;
	static const int random_value_noise_bits = 5;

	static const int shade_gradient_max = 256;
	static const int shade_step_max = 1 << random_value_noise_bits;
	///array of shading gradient
	Sint16 shade_gradient[shade_gradient_max];
	Sint16 shade_step[shade_gradient_max];
	Sint16 shade_seq[shade_gradient_max];
	Sint16 shade_diff[shade_gradient_max];
	///size of x & y of noise surface
	Sint16 random_noise[random_surf_size*random_surf_size];

	/**
	 * Function returning normal vector of sphere surface
	 * @param ox x cord of sphere center
	 * @param oy y cord of sphere center
	 * @param r radius of sphere
	 * @param x cord of point where we getting this vector
	 * @param y cord of point where we getting this vector
	 * @return normal vector of sphere surface
	 */
	static inline Cord circle_norm(double ox, double oy, double r, double x, double y)
	{
		const double limit = r*r;
		const double norm = 1./r;
		Cord ret;
		ret.x = (x-ox);
		ret.y = (y-oy);
		const double temp = (ret.x)*(ret.x) + (ret.y)*(ret.y);
		if (limit > temp)
		{
			ret.x *= norm;
			ret.y *= norm;
			ret.z = sqrt(limit - temp)*norm;
			return ret;
		}
		else
		{
			ret.x = 0.;
			ret.y = 0.;
			ret.z = 0.;
			return ret;
		}
	}

	static inline Sint16 shadeCurve(int i)
	{
		const int shadeOffset = 15;
		const int j = i - shade_gradient_max / 2;

		const int stepSize = 16;
		const int steps[stepSize] =
		{
			1,
			2,
			2,
			3,
			3,
			4,
			4,
			5,
			5,
			6,
			6,
			9,
			12,
			16,
			20,
			30,
		};

		const int adjustemt = (j >= 0 ? 1 : 0);
		const int d = (adjustemt ? 1 : -1);
		int offset = (adjustemt ? j + adjustemt : -j);
		int shadeFinal = shadeOffset + adjustemt;
		for (int k = 0; k < stepSize; ++k)
		{
			int p = steps[k];
			if (offset < p)
			{
				break;
			}
			shadeFinal += d;
			offset -= p;
		}
		return shadeFinal;
	}

	static int bitMask(int i)
	{
		return ((1<< i) - 1);
	}

	int getMultiplierNoise(Sint16 n)
	{
		return ((n >> (random_value_noise_bits + random_distance_noise_bits)) & bitMask(random_multiplier_noise_bits));
	}

	int getDistanceNoise(Sint16 n)
	{
		return ((n >> random_value_noise_bits) & bitMask(random_distance_noise_bits)) - random_distance_noise_bits / 2;
	}

	int getValueNoise(Sint16 n)
	{
		return n &  bitMask(random_value_noise_bits);
	}

	//initialization
	GlobeStaticData()
	{
		int iLastVal = shadeCurve(0);
		int iLast = 0;
		//filling terminator gradient LUT
		for (int i=0; i < shade_gradient_max; ++i)
		{
			int t = shadeCurve(i);
			if (t != iLastVal)
			{
				for (int p = iLast; p < i; ++p)
				{
					shade_diff[p] = t - iLastVal;
					shade_step[p] = shade_step_max / (i - iLast);
					shade_seq[p] = shade_step_max * (p - iLast) / (i - iLast);
				}
				iLastVal = t;
				iLast = i;
			}
			shade_gradient[i] = t;
		}

		int tLast = shadeCurve(shade_gradient_max);
		for (int p = iLast; p < shade_gradient_max; ++p)
		{
			shade_diff[p] = tLast - iLastVal;
			shade_step[p] = shade_step_max / (shade_gradient_max - iLast);
			shade_seq[p] = shade_step_max * (p - iLast) / (shade_gradient_max - iLast);
		}

		RNG::RandomState randomState;
		for (size_t i = 0; i < (size_t)random_surf_size*random_surf_size; ++i)
			random_noise[i] = randomState.generate(0, bitMask(random_multiplier_noise_bits + random_distance_noise_bits + random_value_noise_bits));
	}
};

GlobeStaticData static_data;

struct Ocean
{
	static inline void func(Uint8& dest, const int&, const int&, const int&, const int&)
	{
		dest = Globe::OCEAN_COLOR;
	}
};

struct CreateShadow
{
	static inline Uint8 getShadowValue(const Cord& earth, const Cord& sun, const Sint16& noise)
	{
		Cord temp = earth;
		//diff
		temp -= sun;
		//norm
		temp.x *= temp.x;
		temp.y *= temp.y;
		temp.z *= temp.z;
		temp.x += temp.z + temp.y;
		//we have norm of distance between 2 vectors, now stored in `x`

		temp.x -= 2;
		temp.x *= 125.;
		temp.x += GlobeStaticData::shade_gradient_max / 2;
		//random noise that go in any direction
		temp.x -= static_data.getDistanceNoise(noise);
		//random noise than increase with distance from middle of twilight
		temp.x += static_data.getMultiplierNoise(noise) * 4 * (temp.x - GlobeStaticData::shade_gradient_max / 2) / GlobeStaticData::shade_gradient_max;

		double full = 0;
		double rem = std::modf(temp.x, &full);
		int offset = Clamp((int)full, 0, GlobeStaticData::shade_gradient_max - 1);
		int i = static_data.shade_gradient[offset];

		int middle = (static_data.shade_seq[offset] + static_data.shade_step[offset] * rem) - GlobeStaticData::shade_step_max / 2;
		i += middle / GlobeStaticData::shade_step_max;
		i += (static_data.getValueNoise(noise) < (middle % GlobeStaticData::shade_step_max));

		return Clamp(i, 0, 31);
	}

	static inline Uint8 getOceanShadow(const Uint8& shadow)
	{
		return Globe::OCEAN_COLOR + shadow;
	}

	static inline Uint8 getLandShadow(const Uint8& dest, const Uint8& shadow)
	{
		if (shadow == 0) return dest;
		const int s = shadow / 3;
		const int e = dest + s;
		const int d = dest & helper::ColorGroup;
		if (e > d + helper::ColorShade)
			return d + helper::ColorShade;
		return e;
	}

	static inline bool isOcean(const Uint8& dest)
	{
		return Globe::OCEAN_SHADING && dest >= Globe::OCEAN_COLOR && dest < Globe::OCEAN_COLOR + 32;
	}

	static inline void func(Uint8& dest, const Cord& earth, const Cord& sun, const Sint16& noise)
	{
		if (dest && earth.z)
		{
			const Uint8 shadow = getShadowValue(earth, sun, noise);
			//this pixel is ocean
			if (isOcean(dest))
			{
				dest = getOceanShadow(shadow);
			}
			//this pixel is land
			else
			{
				dest = getLandShadow(dest, shadow);
			}
		}
		else
		{
			dest = 0;
		}
	}
};

struct CreateShadowWithoutCache
{
	static inline void func(Uint8& dest, const helper::Offset& offset, const Cord& sun, const Sint16& noise, const int& radius)
	{
		Cord earth = static_data.circle_norm(0., 0., radius, offset.x, offset.y);
		CreateShadow::func(dest, earth, sun, noise);
	}
};

// ARGB shadow shaders — operate on 32bpp pixels using the same sun-angle
// darkness logic as CreateShadow, but write a brightness-scaled ARGB result.
// Shadow 0=full brightness, shadow 31=fully dark.
struct CreateShadow32
{
	static inline void func(Uint32& dest, const Cord& earth, const Cord& sun, const Sint16& noise)
	{
		if (!(dest >> 24) || !earth.z)
		{
			dest = 0;
			return;
		}
		const Uint8 shadow = CreateShadow::getShadowValue(earth, sun, noise);
		// Linear brightness factor: shadow 0 → 1.0, shadow 31 → 0.0
		const int factor = (32 - shadow) * 8; // 0–256 range, avoid float
		const Uint8 a = (dest >> 24) & 0xFF;
		const Uint8 r = ((((dest >> 16) & 0xFF) * factor) >> 8);
		const Uint8 g = ((((dest >>  8) & 0xFF) * factor) >> 8);
		const Uint8 b = ((( dest        & 0xFF) * factor) >> 8);
		dest = (Uint32(a) << 24) | (Uint32(r) << 16) | (Uint32(g) << 8) | b;
	}
};

struct CreateShadowWithoutCache32
{
	static inline void func(Uint32& dest, const helper::Offset& offset, const Cord& sun, const Sint16& noise, const int& radius)
	{
		Cord earth = static_data.circle_norm(0., 0., radius, offset.x, offset.y);
		CreateShadow32::func(dest, earth, sun, noise);
	}
};

static bool isGlobePanButton(Uint8 button)
{
#ifdef __EMSCRIPTEN__
	return button == SDL_BUTTON_LEFT || button == Options::geoDragScrollButton;
#else
	return button == Options::geoDragScrollButton;
#endif
}

static bool isGlobePanButtonPressed()
{
	const Uint32 buttons = SDL_GetMouseState(0, 0);
#ifdef __EMSCRIPTEN__
	return (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) || (buttons & SDL_BUTTON(Options::geoDragScrollButton));
#else
	return buttons & SDL_BUTTON(Options::geoDragScrollButton);
#endif
}

}//namespace


/**
 * Sets up a globe with the specified size and position.
 * @param game Pointer to core game.
 * @param cenX X position of the center of the globe.
 * @param cenY Y position of the center of the globe.
 * @param width Width in pixels.
 * @param height Height in pixels.
 * @param x X position in pixels.
 * @param y Y position in pixels.
 */
Globe::Globe(Game* game, int cenX, int cenY, int width, int height, int x, int y) : InteractiveSurface(width, height, x, y), _cenX(cenX), _cenY(cenY), _rotLon(0.0), _rotLat(0.0), _hoverLon(0.0), _hoverLat(0.0), _craftLon(0.0), _craftLat(0.0), _craftRange(0.0), _game(game), _hover(false), _craft(false), _blink(-1),
																					_isMouseScrolling(false), _isMouseScrolled(false), _xBeforeMouseScrolling(0), _yBeforeMouseScrolling(0), _lonBeforeMouseScrolling(0.0), _latBeforeMouseScrolling(0.0), _mouseScrollingStartTime(0), _totalMouseMoveX(0), _totalMouseMoveY(0), _mouseMovedOverThreshold(false)
{
	_rules = game->getMod()->getGlobe();
	_texture = new SurfaceSet(*_game->getMod()->getSurfaceSet("TEXTURE.DAT"));
	_markerSet = _game->getMod()->getSurfaceSet("GlobeMarkers");

	_countries = new Surface(width, height, x, y);
	_markers = new Surface(width, height, x, y);
	_radars = new Surface(width, height, x, y);
#ifdef __EMSCRIPTEN__
	/* Warm the physical world batches once; steady-state frame publication only
	 * clears and rewrites these existing vectors. */
	_gpuBorderLines.reserve(GPU_BORDER_LINE_CAPACITY);
	_gpuBorderVertices.reserve(GPU_BORDER_VERTEX_FLOAT_CAPACITY);
	_gpuDebugLines.reserve(GPU_DEBUG_LINE_CAPACITY);
	_gpuDebugVertices.reserve(GPU_DEBUG_VERTEX_FLOAT_CAPACITY);
	_gpuLabelTextures.reserve(GPU_LABEL_TEXTURE_CAPACITY);
	_gpuLabelIconPendingDraws.reserve(GPU_LABEL_DRAW_CAPACITY);
	_gpuLabelIconCommittedDraws.reserve(GPU_LABEL_DRAW_CAPACITY);
#endif
	_clipper = new FastLineClip(x, x+width, y, y+height);

	// ARGB pipeline: setPixel(idx) only writes to _paletteMirror if it exists.
	// drawOcean → drawCircle(setPixel(OCEAN_COLOR)), drawLand → drawTexturedPolygon
	// (setPixel of polygon palette indices) and drawShadow all expect getPixel()
	// to return palette indices later — XuLine reads them to decide isOcean vs
	// land shadow.  Without an initialised mirror, getPixel returns the ARGB
	// B channel and CreateShadow::getLandShadow paints flight paths bright red.
	initPaletteMirror();
	_countries->initPaletteMirror();
	_markers->initPaletteMirror();
	_radars->initPaletteMirror();

	// Animation timers
	_blinkTimer = new Timer(100);
	_blinkTimer->onTimer((SurfaceHandler)&Globe::blink);
	_blinkTimer->start();
	_rotTimer = new Timer(10);
	_rotTimer->onTimer((SurfaceHandler)&Globe::rotate);

	_cenLon = _game->getSavedGame()->getGlobeLongitude();
	_cenLat = _game->getSavedGame()->getGlobeLatitude();
	_zoom = _game->getSavedGame()->getGlobeZoom();
	_zoomOld = _zoom;

	setupRadii(width, height);
	setZoom(_zoom);

	cachePolygons();
}

/**
 * Deletes the contained surfaces.
 */
Globe::~Globe()
{
	delete _blinkTimer;
	delete _rotTimer;
	delete _countries;
	delete _markers;
	delete _texture;
	delete _radars;
	delete _clipper;

	for (auto* polygon : _cacheLand)
	{
		delete polygon;
	}

#ifdef __EMSCRIPTEN__
	CalypsoGeoscapeHdGlobeDirect::setGpuDirect(this, false);
	_gpuAliveFlag.reset();  // M6: expire reset callback before deleting GL objects
	delete _globeShader;
	delete _markerShader;
	delete _borderShader;
	for (auto& entry : _gpuMarkerTextures) delete entry.texture;
	_gpuMarkerTextures.clear();
	for (auto& entry : _gpuLabelTextures)
	{
		delete entry.texture;
		delete entry.frame;
	}
	_gpuLabelTextures.clear();
	if (_sphereFBO)    glDeleteFramebuffers(1,  &_sphereFBO);
	if (_sphereFBOTex) glDeleteTextures(1,      &_sphereFBOTex);
	if (_sphereVAO)    glDeleteVertexArrays(1,  &_sphereVAO);
	if (_markerVAO)    glDeleteVertexArrays(1,  &_markerVAO);
	if (_markerVBO)    glDeleteBuffers(1, &_markerVBO);
	if (_borderVAO)    glDeleteVertexArrays(1,  &_borderVAO);
	if (_borderVBO)    glDeleteBuffers(1, &_borderVBO);
#endif
}

/**
 * Converts a polar point into a cartesian point for
 * mapping a polygon onto the 3D-looking globe.
 * @param lon Longitude of the polar point.
 * @param lat Latitude of the polar point.
 * @param x Pointer to the output X position.
 * @param y Pointer to the output Y position.
 */
void Globe::polarToCart(double lon, double lat, Sint16 *x, Sint16 *y) const
{
	const Calypso::GeoscapeProjection projection{{(double)_cenX, (double)_cenY}, _radius, _cenLon, _cenLat};
	const auto projected = Calypso::calypsoGeoscapeProject(projection, {lon, lat});
	*x = (Sint16)floor(projected.point.x);
	*y = (Sint16)floor(projected.point.y);
}

void Globe::polarToCart(double lon, double lat, double *x, double *y) const
{
	const Calypso::GeoscapeProjection projection{{(double)_cenX, (double)_cenY}, _radius, _cenLon, _cenLat};
	const auto projected = Calypso::calypsoGeoscapeProject(projection, {lon, lat});
	*x = projected.point.x;
	*y = projected.point.y;
}


/**
 * Converts a cartesian point into a polar point for
 * mapping a globe click onto the flat world map.
 * @param x X position of the cartesian point.
 * @param y Y position of the cartesian point.
 * @param lon Pointer to the output longitude.
 * @param lat Pointer to the output latitude.
 */
void Globe::cartToPolar(Sint16 x, Sint16 y, double *lon, double *lat) const
{
	const Calypso::GeoscapeProjection projection{{(double)_cenX, (double)_cenY}, _radius, _cenLon, _cenLat};
	const auto unprojected = Calypso::calypsoGeoscapeUnproject(projection, {(double)x, (double)y});
	if (!unprojected.valid)
	{
		*lon = std::numeric_limits<double>::quiet_NaN();
		*lat = std::numeric_limits<double>::quiet_NaN();
		return;
	}
	*lon = unprojected.point.lon;
	*lat = unprojected.point.lat;
}

/**
 * Checks if a polar point is on the back-half of the globe,
 * invisible to the player.
 * @param lon Longitude of the point.
 * @param lat Latitude of the point.
 * @return True if it's on the back, False if it's on the front.
 */
bool Globe::pointBack(double lon, double lat) const
{
	double c = cos(_cenLat) * cos(lat) * cos(lon - _cenLon) + sin(_cenLat) * sin(lat);

	return c < 0.0;
}

Polygon* Globe::getPolygonFromLonLat(double lon, double lat) const
{
	const double zDiscard=0.75f;
	double coslat = cos(lat);
	double sinlat = sin(lat);

	for (auto* polygon : *_rules->getPolygons())
	{
		double x, y, z, x2, y2;
		double clat, clon;
		z = 0;
		for (int j = 0; j < polygon->getPoints(); ++j)
		{
			z = coslat * cos(polygon->getLatitude(j)) * cos(polygon->getLongitude(j) - lon) + sinlat * sin(polygon->getLatitude(j));
			if (z<zDiscard) break; //discarded
		}
		if (z<zDiscard) continue; //discarded

		bool odd = false;

		clat = polygon->getLatitude(0); //initial point
		clon = polygon->getLongitude(0);
		x = cos(clat) * sin(clon - lon);
		y = coslat * sin(clat) - sinlat * cos(clat) * cos(clon - lon);

		for (int j = 0; j < polygon->getPoints(); ++j)
		{
			int k = (j + 1) % polygon->getPoints(); //index of next point in poly
			clat = polygon->getLatitude(k);
			clon = polygon->getLongitude(k);

			x2 = cos(clat) * sin(clon - lon);
			y2 = coslat * sin(clat) - sinlat * cos(clat) * cos(clon - lon);
			if ( ((y>0)!=(y2>0)) && (0 < (x2-x)*(0-y)/(y2-y)+x) )
				odd = !odd;
			x = x2;
			y = y2;

		}
		if (odd) return polygon;
	}
	return NULL;
}

/**
 * Sets a leftwards rotation speed and starts the timer.
 */
void Globe::rotateLeft()
{
	_rotLon = -ROTATE_LONGITUDE;
	if (!_rotTimer->isRunning()) _rotTimer->start();
}

/**
 * Sets a rightwards rotation speed and starts the timer.
 */
void Globe::rotateRight()
{
	_rotLon = ROTATE_LONGITUDE;
	if (!_rotTimer->isRunning()) _rotTimer->start();
}

/**
 * Sets a upwards rotation speed and starts the timer.
 */
void Globe::rotateUp()
{
	_rotLat = -ROTATE_LATITUDE;
	if (!_rotTimer->isRunning()) _rotTimer->start();
}

/**
 * Sets a downwards rotation speed and starts the timer.
 */
void Globe::rotateDown()
{
	_rotLat = ROTATE_LATITUDE;
	if (!_rotTimer->isRunning()) _rotTimer->start();
}

/**
 * Resets the rotation speed and timer.
 */
void Globe::rotateStop()
{
	_rotLon = 0.0;
	_rotLat = 0.0;
	_rotTimer->stop();
}

/**
 * Resets longitude rotation speed and timer.
 */
void Globe::rotateStopLon()
{
	_rotLon = 0.0;
	if (AreSame(_rotLat, 0.0))
	{
		_rotTimer->stop();
	}
}

/**
 * Resets latitude rotation speed and timer.
 */
void Globe::rotateStopLat()
{
	_rotLat = 0.0;
	if (AreSame(_rotLon, 0.0))
	{
		_rotTimer->stop();
	}
}

/**
 * Changes the current globe zoom factor.
 * @param zoom New zoom.
 */
void Globe::setZoom(size_t zoom)
{
	_zoom = Clamp(zoom, (size_t)0u, _zoomRadius.size() - 1);
	_zoomTexture = (2 - (int)floor(_zoom / 2.0)) * (_texture->getTotalFrames() / 3);
	_radius = _zoomRadius[_zoom];
	_game->getSavedGame()->setGlobeZoom(_zoom);
	if (_isMouseScrolling)
	{
		_lonBeforeMouseScrolling = _cenLon;
		_latBeforeMouseScrolling = _cenLat;
		_totalMouseMoveX = 0; _totalMouseMoveY = 0;
	}
	invalidate();
}

/**
 * Increases the zoom level on the globe.
 */
void Globe::zoomIn()
{
	if (_zoom < _zoomRadius.size() - 1)
	{
		setZoom(_zoom + 1);
	}
}

/**
 * Decreases the zoom level on the globe.
 */
void Globe::zoomOut()
{
	if (_zoom > 0)
	{
		setZoom(_zoom - 1);
	}
}

/**
 * Zooms the globe out as far as possible.
 */
void Globe::zoomMin()
{
	if (_zoom > 0)
	{
		setZoom(0);
	}
}

/**
 * Zooms the globe in as close as possible.
 */
void Globe::zoomMax()
{
	if (_zoom < _zoomRadius.size() - 1)
	{
		setZoom(_zoomRadius.size() - 1);
	}
}

/**
 * Stores the zoom used before a dogfight.
 */
void Globe::saveZoomDogfight()
{
	_zoomOld = _zoom;
}

/**
 * Zooms the globe smoothly into dogfight level.
 * @return Is the globe already zoomed in?
 */
bool Globe::zoomDogfightIn()
{
	if (_zoom < DOGFIGHT_ZOOM)
	{
		double radiusNow = _radius;
		if (radiusNow + _radiusStep >= _zoomRadius[DOGFIGHT_ZOOM])
		{
			setZoom(DOGFIGHT_ZOOM);
		}
		else
		{
			if (radiusNow + _radiusStep >= _zoomRadius[_zoom + 1])
				_zoom++;
			setZoom(_zoom);
			_radius = radiusNow + _radiusStep;
		}
		return false;
	}
	return true;
}

/**
 * Zooms the globe smoothly out of dogfight level.
 * @return Is the globe already zoomed out?
 */
bool Globe::zoomDogfightOut()
{
	if (_zoom > _zoomOld)
	{
		double radiusNow = _radius;
		if (radiusNow - _radiusStep <= _zoomRadius[_zoomOld])
		{
			setZoom(_zoomOld);
		}
		else
		{
			if (radiusNow - _radiusStep <= _zoomRadius[_zoom - 1])
				_zoom--;
			setZoom(_zoom);
			_radius = radiusNow - _radiusStep;
		}
		return false;
	}
	return true;
}

/**
 * Rotates the globe to center on a certain
 * polar point on the world map.
 * @param lon Longitude of the point.
 * @param lat Latitude of the point.
 */
void Globe::center(double lon, double lat)
{
	_cenLon = lon;
	_cenLat = lat;
	_game->getSavedGame()->setGlobeLongitude(_cenLon);
	_game->getSavedGame()->setGlobeLatitude(_cenLat);
	invalidate();
}

/**
 * Checks if a polar point is inside the globe's landmass.
 * @param lon Longitude of the point.
 * @param lat Latitude of the point.
 * @return True if it's inside, False if it's outside.
 */
bool Globe::insideLand(double lon, double lat) const
{
	auto* polygon = getPolygonFromLonLat(lon, lat);
	if (!polygon)
	{
		return false;
	}
	auto* textureRule = _rules->getTexture(polygon->getTexture());
	if (textureRule && textureRule->isCosmeticOcean())
	{
		return false;
	}
	return true;
}

/**
 * Checks if a polar point is inside the fakeUnderwater texture.
 * @param lon Longitude of the point.
 * @param lat Latitude of the point.
 * @return True if it's inside, False if it's outside.
 */
bool Globe::insideFakeUnderwaterTexture(double lon, double lat) const
{
	auto* polygon = getPolygonFromLonLat(lon, lat);
	if (!polygon)
	{
		return false;
	}
	auto* textureRule = _rules->getTexture(polygon->getTexture());
	if (textureRule && textureRule->isFakeUnderwater())
	{
		return true;
	}
	return false;
}

/**
 * Switches the amount of detail shown on the globe.
 * With detail on, country and city details are shown when zoomed in.
 */
void Globe::toggleDetail()
{
	Options::globeDetail = !Options::globeDetail;
	drawDetail();
}

/**
 * Checks if a certain target is near a certain cartesian point
 * (within a circled area around it) over the globe.
 * @param target Pointer to target.
 * @param x X coordinate of point.
 * @param y Y coordinate of point.
 * @return True if it's near, false otherwise.
 */
bool Globe::targetNear(Target* target, int x, int y) const
{
	Sint16 tx, ty;
	if (pointBack(target->getLongitude(), target->getLatitude()))
		return false;
	polarToCart(target->getLongitude(), target->getLatitude(), &tx, &ty);

	int dx = x - tx;
	int dy = y - ty;
	return (dx * dx + dy * dy <= NEAR_RADIUS);
}

/**
 * Returns a list of all the targets currently near a certain
 * cartesian point over the globe.
 * @param x X coordinate of point.
 * @param y Y coordinate of point.
 * @param craft Only get craft targets.
 * @return List of pointers to targets.
 */
std::vector<Target*> Globe::getTargets(int x, int y, bool craft, Craft *currentCraft) const
{
	std::vector<Target*> v;
	{
		for (auto* xbase : *_game->getSavedGame()->getBases())
		{
			if (xbase->getLongitude() == 0.0 && xbase->getLatitude() == 0.0)
				continue;

			if (targetNear(xbase, x, y))
			{
				v.push_back(xbase);
			}

			for (auto* xcraft : *xbase->getCrafts())
			{
				if (xcraft == currentCraft)
					continue;
				if (xcraft->getLongitude() == xbase->getLongitude() && xcraft->getLatitude() == xbase->getLatitude() && xcraft->getDestination() == 0)
					continue;

				if (targetNear(xcraft, x, y))
				{
					v.push_back(xcraft);
				}
			}
		}
	}
	for (auto* ufo : *_game->getSavedGame()->getUfos())
	{
		if (!ufo->getDetected() || ufo->getStatus() == Ufo::IGNORE_ME)
			continue;

		if (targetNear(ufo, x, y))
		{
			v.push_back(ufo);
		}
	}
	for (auto* wp : *_game->getSavedGame()->getWaypoints())
	{
		if (targetNear(wp, x, y))
		{
			v.push_back(wp);
		}
	}
	for (auto* site : *_game->getSavedGame()->getMissionSites())
	{
		if (targetNear(site, x, y))
		{
			v.push_back(site);
		}
	}
	for (auto* ab : *_game->getSavedGame()->getAlienBases())
	{
		if (!ab->isDiscovered())
		{
			continue;
		}
		if (targetNear(ab, x, y))
		{
			v.push_back(ab);
		}
	}
	return v;
}

/**
 * Takes care of pre-calculating all the polygons currently visible
 * on the globe and caching them so they only need to be recalculated
 * when the globe is actually moved.
 */
void Globe::cachePolygons()
{
	cache(_rules->getPolygons(), &_cacheLand);
}

/**
 * Caches a set of polygons.
 * @param polygons Pointer to list of polygons.
 * @param cache Pointer to cache.
 */
void Globe::cache(std::list<Polygon*> *polygons, std::list<Polygon*> *cache)
{
	// Clear existing cache
	for (auto* polygon : *cache)
	{
		delete polygon;
	}
	cache->clear();

	// Pre-calculate values to cache
	for (auto* polygon : *polygons)
	{
		// Is quad on the back face?
		double closest = 0.0;
		double z;
		double furthest = 0.0;
		for (int j = 0; j < polygon->getPoints(); ++j)
		{
			z = cos(_cenLat) * cos(polygon->getLatitude(j)) * cos(polygon->getLongitude(j) - _cenLon) + sin(_cenLat) * sin(polygon->getLatitude(j));
			if (z > closest)
				closest = z;
			else if (z < furthest)
				furthest = z;
		}
		if (-furthest > closest)
			continue;

		Polygon* p = new Polygon(*polygon);

		// Convert coordinates
		for (int j = 0; j < p->getPoints(); ++j)
		{
			Sint16 x, y;
			polarToCart(p->getLongitude(j), p->getLatitude(j), &x, &y);
			p->setX(j, x);
			p->setY(j, y);
		}

		cache->push_back(p);
	}
}

/**
 * Replaces a certain amount of colors in the palette of the globe.
 * @param colors Pointer to the set of colors.
 * @param firstcolor Offset of the first color to replace.
 * @param ncolors Amount of colors to replace.
 */
void Globe::setPalette(const SDL_Color *colors, int firstcolor, int ncolors)
{
	Surface::setPalette(colors, firstcolor, ncolors);

	_texture->setPalette(colors, firstcolor, ncolors);

	_countries->setPalette(colors, firstcolor, ncolors);
	_markers->setPalette(colors, firstcolor, ncolors);
	_radars->setPalette(colors, firstcolor, ncolors);
#ifdef __EMSCRIPTEN__
	/* Marker RGBA uploads are immutable snapshots of the palette.  Drop the
	 * old GPU entries at the palette boundary so a subsequent direct world pass
	 * cannot sample colours from the previous revision. */
	for (auto& entry : _gpuMarkerTextures) delete entry.texture;
	_gpuMarkerTextures.clear();
	++_gpuMarkerPaletteGeneration;
	/* SS15.4.3: the radar/flight snapshot key rides the same palette
	 * boundary, so a palette revision rebuilds and reuploads exactly once. */
	++_gpuRadarPaletteGeneration;
	for (auto& entry : _gpuLabelTextures)
	{
		delete entry.texture;
		delete entry.frame;
	}
	_gpuLabelTextures.clear();
	_gpuLabelIconPendingDraws.clear();
	_gpuLabelIconCommittedDraws.clear();
	_gpuDebugLines.clear();
	_gpuDebugVertices.clear();
	_gpuDebugCapacityExceeded = false;
	++_gpuLabelPaletteGeneration;
#endif
}

/**
 * Keeps the animation timers running.
 */
void Globe::think()
{
	_blinkTimer->think(0, this);
	_rotTimer->think(0, this);
}

/**
 * Makes the globe markers blink.
 */
void Globe::blink()
{
	_blink = -_blink;

	drawMarkers();
}

/**
 * Rotates the globe by a set amount. Necessary
 * since the globe keeps rotating while a button
 * is pressed down.
 */
void Globe::rotate()
{
	_cenLon += _rotLon * ((110 - Options::geoScrollSpeed) / 100.0) / (_zoom+1);
	_cenLat += _rotLat * ((110 - Options::geoScrollSpeed) / 100.0) / (_zoom+1);
	_game->getSavedGame()->setGlobeLongitude(_cenLon);
	_game->getSavedGame()->setGlobeLatitude(_cenLat);
	invalidate();
}

#ifdef __EMSCRIPTEN__
/* ── GL state save/restore (local helper) ───────────────────────────────── */
static std::string calypsoGlFailure(const char *operation, GLenum error)
{
	std::ostringstream detail;
	detail << operation << " (0x" << std::hex << error << ")";
	return detail.str();
}

struct GlobeSphereGlSave
{
	GLfloat lineWidth = 1.0f;
	const char *errorOperation = nullptr;
	const char *restoreErrorOperation = nullptr;
	GLenum restoreError = GL_NO_ERROR;
	bool saved = false;
	GLenum save()
	{
		glGetFloatv(GL_LINE_WIDTH, &lineWidth);
		const GLenum error = glGetError();
		if (error != GL_NO_ERROR) errorOperation = "glGetFloatv(GL_LINE_WIDTH)";
		saved = true;
		return error;
	}
	void restore()
	{
		if (!saved) return;
		auto check = [&](const char *operation) {
			const GLenum error = glGetError();
			if (error != GL_NO_ERROR && restoreError == GL_NO_ERROR)
			{
				restoreError = error;
				restoreErrorOperation = operation;
				Log(LOG_ERROR) << "Globe physical GL restore failed at " << operation
				               << " (0x" << std::hex << (unsigned)error << std::dec << ")";
			}
		};
		/* WebGL2 rejects several state queries in the first post-swap frame.
		 * The physical world owns a deterministic baseline instead: all passes
		 * bind their own program/VAO/FBO/textures, and downstream HD chrome
		 * establishes its own state after this slot. */
		glUseProgram(0);
		check("glUseProgram(0)");
		glBindVertexArray(0);
		check("glBindVertexArray(0)");
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		check("glBindFramebuffer(0)");
		glActiveTexture(GL_TEXTURE0);
		check("glActiveTexture(GL_TEXTURE0)");
		glBindTexture(GL_TEXTURE_2D, 0);
		check("glBindTexture(0)");
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		check("glBindBuffer(0)");
		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
			GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		check("glBlendFuncSeparate");
		glDisable(GL_SCISSOR_TEST);
		check("glDisable(GL_SCISSOR_TEST)");
		glDisable(GL_DEPTH_TEST);
		check("glDisable(GL_DEPTH_TEST)");
		glDisable(GL_BLEND);
		check("glDisable(GL_BLEND)");
		glLineWidth(lineWidth);
		check("glLineWidth");
		glViewport(0, 0, Options::displayWidth, Options::displayHeight);
		check("glViewport");
		saved = false;
	}
	~GlobeSphereGlSave() { restore(); }
};

/* SDL may surface the one post-swap WebGL reset token on the first raw call
 * after its flush. Consume it only while the recovery gate owns the token;
 * every other error remains owned by the current Earth/world operation. */
static GLenum calypsoOwnedResetError()
{
	static const GLenum CALYPSO_CONTEXT_LOST_WEBGL = 0x9242;
	const GLenum error = glGetError();
	/* The browser can defer the reset token until the first physical-world
	 * query, after Screen's SDL-flush query has already drained an earlier
	 * token. Consume it only when the reset-boundary observer transferred the
	 * one-shot ownership; consuming it also ends the bounded ownership window
	 * at this boundary. The same numeric value later is a real pass error. */
	if (error == CALYPSO_CONTEXT_LOST_WEBGL && calypso_context_reset_sentinel_pending())
	{
		calypso_context_reset_sentinel_consumed();
		calypso_context_reset_boundary_close();
		return GL_NO_ERROR;
	}
	return error;
}

/* Stage 13 QA (loopback-only): 1x1 fully transparent cloud input used when a
 * capture row selects GeoscapeQaCloudMode::Hidden. The existing shader derives
 * cloud density from the alpha channel, so alpha 0 renders zero clouds with no
 * shader change. GpuTexture registers itself with ShaderManager, so context
 * loss/reupload stays owned by the existing recovery machinery. Never created
 * unless the QA export switched the cloud input away from Live; production
 * always binds the mod clouds texture. */
static GpuTexture* calypsoGlobeQaHiddenCloudsTexture()
{
	static GpuTexture* tex = nullptr;
	if (tex == nullptr)
	{
		tex = new GpuTexture(/*srgb=*/false);
		const std::uint8_t transparent[4] = { 0, 0, 0, 0 };
		tex->uploadRGBA(transparent, 1, 1);
	}
	return tex;
}

/* Stage 13 QA (loopback-only): shared seam helpers for BOTH globe passes
 * (readback Globe::drawSphereGPU() and direct drawPass()). With every control
 * at its production default each helper returns its input unchanged, so the
 * live paths keep their exact existing math and texture bindings. */

/* Effective decorative milliseconds for the shader clocks (`u_time`): a
 * frozen or reduced-motion capture row replaces ONLY the millisecond source
 * through calypsoGeoscapeQaPresentationSeconds() (live seconds =
 * SDL_GetTicks() * 0.001); the callers' uniform expressions keep their exact
 * production form. */
static float calypsoGlobeQaEffectiveMs(float liveMs)
{
	const auto& qa = Calypso::calypsoGeoscapeQaPresentation();
	if (!qa.frozenClock && !qa.reducedMotion) return liveMs;
	return (float)(Calypso::calypsoGeoscapeQaPresentationSeconds(qa, liveMs * 0.001) * 1000.0);
}

/* GeoscapeQaVec3 -> shader-world Cord conversion for the deterministic
 * day/night rows produced by calypsoGeoscapeQaSunDirection(). Live rows never
 * reach this helper; they keep the verbatim getSunDirectionWorld() call. */
static Cord calypsoGlobeQaCord(const Calypso::GeoscapeQaVec3& v)
{
	return Cord(v.x, v.y, v.z);
}

	void CalypsoGeoscapeHdGlobeDirect::drawPass(Globe* globe)
	{
		if (!globe || !globe->_gpuDirectMode || !globe->_directScreen) return;
		if (Calypso::calypsoRadarCountersEnabled())
			++Calypso::calypsoRadarCounters().frames;
		const GLenum worldPreflightError = calypsoOwnedResetError();
		if (worldPreflightError != GL_NO_ERROR)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
				calypsoGlFailure("WebGL context restore world preflight failed", worldPreflightError));
		GlobeSphereGlSave preflightState;
		const GLenum stateSaveError = preflightState.save();
		if (stateSaveError != GL_NO_ERROR)
		{
			std::string detail = calypsoGlFailure("Geoscape world GL state snapshot failed", stateSaveError);
			if (preflightState.errorOperation)
				detail += std::string(" at ") + preflightState.errorOperation;
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute(detail);
		}
		if (!GpuInit::ready() || !globe->_gpuSphereOK || !globe->_sphereVAO)
		{
			if (!globe->initSphereGPU() || !globe->_gpuSphereOK || !globe->_sphereVAO)
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape Earth GPU resources unavailable");
		}
		if (!globe->_globeShader || !globe->_globeShader->isValid())
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape Earth shader unavailable");
		CalypsoGeoscapeHdGlobeDirect::ensureLogicalWorldComplete(globe);
		CalypsoGeoscapeHdGlobeDirect::ensureBorderResources(globe);
		CalypsoGeoscapeHdGlobeDirect::ensureColoredLineResources(globe);
		CalypsoGeoscapeHdGlobeDirect::ensureMarkerResources(globe);
		CalypsoGeoscapeHdGlobeDirect::ensureLabelResources(globe);
		preflightState.restore();
		if (preflightState.restoreError != GL_NO_ERROR)
		{
			std::string detail = calypsoGlFailure("Geoscape world GL state restore failed", preflightState.restoreError);
			if (preflightState.restoreErrorOperation)
				detail += std::string(" at ") + preflightState.restoreErrorOperation;
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute(detail);
		}
		Mod* mod = globe->_game->getMod();
		GpuTexture* bathyTex = mod->getGlobeTexture("bathymetry");
		GpuTexture* diffuseTex = mod->getGlobeTexture("diffuse");
		GpuTexture* nightTex = mod->getGlobeTexture("night");
		GpuTexture* cloudsTex = mod->getGlobeTexture("clouds");
		if (!bathyTex || !diffuseTex || !nightTex || !cloudsTex
			|| !bathyTex->isValid() || !diffuseTex->isValid()
			|| !nightTex->isValid() || !cloudsTex->isValid())
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape Earth textures unavailable");
		CalypsoGeoscapeHdGlobeDirect::PhysicalGlobeRect globeRect;
		if (!CalypsoGeoscapeHdGlobeDirect::physicalGlobeRect(globe, globeRect))
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape physical globe rect is invalid");
		const double xs = globe->_directScreen->getXScale();
		const double ys = globe->_directScreen->getYScale();
		const int dispX = globeRect.x;
		const int dispY = globeRect.y;
		const int dispW = globeRect.w;
		const int dispH = globeRect.h;
		const Uint64 calypsoEarthStart = Calypso::calypsoPassTimersEnabled()
			? SDL_GetPerformanceCounter() : 0;
		GlobeSphereGlSave st; st.save();
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(dispX, (int)Options::displayHeight - dispY - dispH, dispW, dispH);
		glDisable(GL_BLEND);
		glDisable(GL_DEPTH_TEST);
		globe->_globeShader->use();
		bathyTex->bind(0);   globe->_globeShader->setUniform1i("u_bathymetry", 0);
		diffuseTex->bind(1); globe->_globeShader->setUniform1i("u_diffuse", 1);
		nightTex->bind(2);   globe->_globeShader->setUniform1i("u_night", 2);
		/* Stage 13 QA (loopback-only): capture rows may bind a transparent
		 * cloud input; production always samples the mod clouds texture.
		 * Hidden allocates/uploads its persistent 1x1 input once on mode
		 * entry and reuses it steady-state; context loss re-creates it via
		 * the existing ShaderManager recovery path. */
		GpuTexture* cloudsBound = cloudsTex;
		const auto& qa = Calypso::calypsoGeoscapeQaPresentation();
		if (qa.cloudMode == Calypso::GeoscapeQaCloudMode::Hidden)
		{
			if (!calypsoGlobeQaHiddenCloudsTexture()->isValid()) cloudsBound = cloudsTex;
			else cloudsBound = calypsoGlobeQaHiddenCloudsTexture();
		}
		cloudsBound->bind(3); globe->_globeShader->setUniform1i("u_clouds", 3);
		globe->_globeShader->setUniform1i("u_background", 1);
		globe->_globeShader->setUniform2f("u_viewportSize", (float)dispW, (float)dispH);
		globe->_globeShader->setUniform2f("u_globeCenter", (float)(globe->_cenX * xs), (float)(globe->_cenY * ys));
		globe->_globeShader->setUniform1f("u_globeRadius", (float)(globe->_zoomRadius[globe->_zoom] * std::min(xs, ys)));
		globe->_globeShader->setUniform1f("u_camLat", (float)globe->_cenLat);
		globe->_globeShader->setUniform1f("u_camLon", (float)globe->_cenLon);
		Cord sd = globe->getSunDirectionWorld();
		/* Stage 13 QA (loopback-only): deterministic day/night rows replace the
		 * fed value only; campaign time is never read or mutated. */
		if (qa.sunMode != Calypso::GeoscapeQaSunMode::Live)
			sd = calypsoGlobeQaCord(Calypso::calypsoGeoscapeQaSunDirection(qa.sunMode, globe->_cenLon, globe->_cenLat));
		globe->_globeShader->setUniform3f("u_sunDir", (float)sd.x, (float)sd.y, (float)sd.z);
		/* Stage 13 QA (loopback-only): fixed/reduced decorative clock for the
		 * cloud drift. Production keeps its exact expression when no QA
		 * control is active. */
		float timeMs = calypsoGlobeQaEffectiveMs((float)SDL_GetTicks());
		globe->_globeShader->setUniform1f("u_time", timeMs * 0.001f);
		float mipLvl = std::max(0.f, std::min(1.35f, 1.35f - (float)globe->_zoom * 0.27f));
		globe->_globeShader->setUniform1f("u_mipLevel", mipLvl);
		glBindVertexArray(globe->_sphereVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0u);
		for (int i = 3; i >= 0; --i) { glActiveTexture(GL_TEXTURE0 + i); glBindTexture(GL_TEXTURE_2D, 0u); }
		if (glGetError() != GL_NO_ERROR)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape Earth draw failed");
		st.restore();
		if (calypsoEarthStart)
			Calypso::calypsoPassTimers().earthUs +=
				(Uint64)((SDL_GetPerformanceCounter() - calypsoEarthStart) * 1000000ull / SDL_GetPerformanceFrequency());
		const Uint64 calypsoBorderStart = Calypso::calypsoPassTimersEnabled()
			? SDL_GetPerformanceCounter() : 0;
		/* SS15.4.6 consolidation (Option A): ONE state guard serves the border
		 * and radar passes together; each previously snapshotted/restored the
		 * same GL state every frame. */
		GlobeSphereGlSave stLines; stLines.save();
		CalypsoGeoscapeHdGlobeDirect::setPhysicalGlobeClip(globe);
		CalypsoGeoscapeHdGlobeDirect::drawBorderPass(globe);
		if (calypsoBorderStart)
			Calypso::calypsoPassTimers().borderUs +=
				(Uint64)((SDL_GetPerformanceCounter() - calypsoBorderStart) * 1000000ull / SDL_GetPerformanceFrequency());
		const Uint64 calypsoRadarStart = Calypso::calypsoPassTimersEnabled()
			? SDL_GetPerformanceCounter() : 0;
		CalypsoGeoscapeHdGlobeDirect::prepareRadarFlightSnapshot(globe);
		CalypsoGeoscapeHdGlobeDirect::drawRadarFlightPass(globe);
		if (calypsoRadarStart)
			Calypso::calypsoPassTimers().radarUs +=
				(Uint64)((SDL_GetPerformanceCounter() - calypsoRadarStart) * 1000000ull / SDL_GetPerformanceFrequency());
		stLines.restore();
		const Uint64 calypsoLabelStart = Calypso::calypsoPassTimersEnabled()
			? SDL_GetPerformanceCounter() : 0;
		if (calypsoLabelStart)
			Calypso::calypsoPassTimers().labelUs +=
				(Uint64)((SDL_GetPerformanceCounter() - calypsoLabelStart) * 1000000ull / SDL_GetPerformanceFrequency());
		const Uint64 calypsoMarkerStart = Calypso::calypsoPassTimersEnabled()
			? SDL_GetPerformanceCounter() : 0;
		CalypsoGeoscapeHdGlobeDirect::drawDebugPass(globe);
		CalypsoGeoscapeHdGlobeDirect::drawMarkerPass(globe);
		if (calypsoMarkerStart)
			Calypso::calypsoPassTimers().markerUs +=
				(Uint64)((SDL_GetPerformanceCounter() - calypsoMarkerStart) * 1000000ull / SDL_GetPerformanceFrequency());
	}

	void CalypsoGeoscapeHdGlobeDirect::recordMarker(Globe* globe, Surface* frame, int x, int y, int shade)
	{
		if (!globe || !frame)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape marker frame unavailable");
		globe->_gpuMarkerPendingDraws.push_back({frame, x, y, shade});
	}

	void CalypsoGeoscapeHdGlobeDirect::recordBorderLine(Globe* globe, int x1, int y1, int x2, int y2)
	{
		if (!globe)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape border owner unavailable");
		if (globe->_gpuBorderLines.size() >= Globe::GPU_BORDER_LINE_CAPACITY
			|| globe->_gpuBorderLines.size() >= globe->_gpuBorderLines.capacity())
		{
			globe->_gpuBorderCapacityExceeded = true;
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape border batch capacity exhausted");
		}
		globe->_gpuBorderLines.push_back({(float)x1, (float)y1, (float)x2, (float)y2});
	}

	void CalypsoGeoscapeHdGlobeDirect::recordDebugLine(Globe* globe, double lon1, double lat1,
		double lon2, double lat2, Uint8 color)
	{
		if (!globe)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape debug geometry owner unavailable");
		double sx = lon2 - lon1;
		double sy = lat2 - lat1;
		if (sx < 0.0) sx += 2.0 * M_PI;
		const int segments = std::max(1, std::abs(sx) < 0.01
			? (int)std::abs(sy / (2.0 * M_PI) * 48.0)
			: (int)std::abs(sx / (2.0 * M_PI) * 96.0));
		sx /= segments;
		sy /= segments;
		for (int i = 0; i < segments; ++i)
		{
			const double ln1 = lon1 + sx * i;
			const double lt1 = lat1 + sy * i;
			const double ln2 = lon1 + sx * (i + 1);
			const double lt2 = lat1 + sy * (i + 1);
			if (globe->pointBack(ln2, lt2) || globe->pointBack(ln1, lt1)) continue;
			Sint16 px1, py1, px2, py2;
			globe->polarToCart(ln1, lt1, &px1, &py1);
			globe->polarToCart(ln2, lt2, &px2, &py2);
			if (globe->_gpuDebugLines.size() >= Globe::GPU_DEBUG_LINE_CAPACITY
				|| globe->_gpuDebugLines.size() >= globe->_gpuDebugLines.capacity())
			{
				globe->_gpuDebugCapacityExceeded = true;
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape debug geometry batch capacity exhausted");
			}
			globe->_gpuDebugLines.push_back({(float)px1, (float)py1, (float)px2, (float)py2, color});
		}
	}

	void CalypsoGeoscapeHdGlobeDirect::recordRadarFlightLine(Globe* globe, double x1, double y1, double x2, double y2,
		double lon1, double lat1, double lon2, double lat2, int shade)
	{
		if (!globe)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape radar/flight owner unavailable");
		const bool front1 = !globe->pointBack(lon1, lat1);
		const bool front2 = !globe->pointBack(lon2, lat2);
		if (!front1 && !front2) return;
		/* Effective palette resolves once per logical segment; every raster
		 * step records its final RGBA bytes directly into the batch. */
		const SDL_Color* radarPalette = globe->getEffectivePalette();
		if (front1 != front2)
		{
			const Cord a(CordPolar(lon1, lat1));
			const Cord b(CordPolar(lon2, lat2));
			double lo = front1 ? 0.0 : 1.0;
			double hi = front1 ? 1.0 : 0.0;
			for (int i = 0; i < 24; ++i)
			{
				const double mid = (lo + hi) * 0.5;
				Cord m(a.x + (b.x - a.x) * mid, a.y + (b.y - a.y) * mid, a.z + (b.z - a.z) * mid);
				const double norm = m.norm();
				if (norm > 0.0) m /= norm;
				const CordPolar p(m);
				const bool front = !globe->pointBack(p.lon, p.lat);
				if (front == front1) lo = mid;
				else hi = mid;
			}
			const double limb = (lo + hi) * 0.5;
			Cord m(a.x + (b.x - a.x) * limb, a.y + (b.y - a.y) * limb, a.z + (b.z - a.z) * limb);
			const double norm = m.norm();
			if (norm > 0.0) m /= norm;
			const CordPolar p(m);
			double lx = 0.0, ly = 0.0;
			globe->polarToCart(p.lon, p.lat, &lx, &ly);
			if (front1) { x2 = lx; y2 = ly; }
			else { x1 = lx; y1 = ly; }
		}
		if (!globe->_clipper || globe->_clipper->LineClip(&x1, &y1, &x2, &y2) != 1)
			return;

		/* XuLine advances one floating-point raster step at a time and samples
		 * the source pixel before advancing.  Keep that progression here rather
		 * than assigning one shade to an entire physical segment: a path can
		 * cross land/ocean palette boundaries inside one logical segment. */
		const double deltax = x2 - x1;
		const double deltay = y2 - y1;
		const bool yDominant = std::abs((int)y2 - (int)y1) > std::abs((int)x2 - (int)x1);
		double len = yDominant
			? std::abs((int)y2 - (int)y1)
			: std::abs((int)x2 - (int)x1);
		if (len <= 0.0) return;
		double stepX = 0.0;
		double stepY = 0.0;
		if (yDominant)
		{
			stepX = deltax / len;
			stepY = y2 < y1 ? -1.0 : (std::abs(deltay) < 1e-12 ? 0.0 : 1.0);
		}
		else
		{
			stepX = x2 < x1 ? -1.0 : (std::abs(deltax) < 1e-12 ? 0.0 : 1.0);
			stepY = deltay / len;
		}

		const auto resolveStepColor = [globe, shade](double sampleX, double sampleY) -> Uint8
		{
			const Sint16 px = (Sint16)std::floor(sampleX);
			const Sint16 py = (Sint16)std::floor(sampleY);
			double sampleLon = 0.0, sampleLat = 0.0;
			globe->cartToPolar(px, py, &sampleLon, &sampleLat);
			if (!std::isfinite(sampleLon) || !std::isfinite(sampleLat)) return 0;
			int texture = -1, unusedShade = 0;
			globe->getPolygonTextureAndShade(sampleLon, sampleLat, &texture, &unusedShade);
			Uint8 dest = Globe::OCEAN_COLOR;
			if (texture >= 0 && globe->_texture)
			{
				Surface* frame = globe->_texture->getFrame(texture + globe->_zoomTexture);
				if (frame && frame->getWidth() > 0 && frame->getHeight() > 0)
				{
					int tx = (int)px % frame->getWidth();
					int ty = (int)py % frame->getHeight();
					if (tx < 0) tx += frame->getWidth();
					if (ty < 0) ty += frame->getHeight();
					dest = frame->getPixel(tx, ty);
				}
			}
			if (!dest) return 0;
			return CreateShadow::isOcean(dest)
				? CreateShadow::getOceanShadow((Uint8)(shade + 8))
				: CreateShadow::getLandShadow(dest, (Uint8)(shade * 3));
		};

		double sampleX = x1;
		double sampleY = y1;
		while (len > 0.0)
		{
			const Uint8 color = resolveStepColor(sampleX, sampleY);
			if (color)
			{
				if (!radarPalette)
					Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape radar/flight palette unavailable");
				/* Fail closed before publication: the hard command bound is
				 * checked before the append is attempted, and the batch refuses
				 * growth itself as the backstop. */
				if (globe->_coloredLineBatch.commandCount()
					>= OpenXcom::Calypso::COLORED_LINE_COMMAND_CAPACITY)
				{
					globe->_gpuRadarFlightCapacityExceeded = true;
					Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape radar/flight batch capacity exhausted");
				}
				const double nextX = len > 1.0 ? sampleX + stepX : x2;
				const double nextY = len > 1.0 ? sampleY + stepY : y2;
				const SDL_Color resolved = radarPalette[color];
				if (!globe->_coloredLineBatch.tryRecordCommand(sampleX, sampleY, nextX, nextY,
					resolved.r, resolved.g, resolved.b, resolved.a))
				{
					globe->_gpuRadarFlightCapacityExceeded = true;
					Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape radar/flight batch capacity exhausted");
				}
			}
			sampleX += stepX;
			sampleY += stepY;
			len -= 1.0;
		}
	}

	void CalypsoGeoscapeHdGlobeDirect::recordLabelText(Globe* globe, const std::string& text,
		int width, int height, int x, int y, Uint8 color)
	{
		if (!globe)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape label owner unavailable");
		if (globe->_gpuLabelIconPendingDraws.size() >= Globe::GPU_LABEL_DRAW_CAPACITY
			|| globe->_gpuLabelIconPendingDraws.size() >= globe->_gpuLabelIconPendingDraws.capacity())
		{
			globe->_gpuLabelCapacityExceeded = true;
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape label batch capacity exhausted");
		}
		Globe::LabelTexture* found = nullptr;
		for (auto& entry : globe->_gpuLabelTextures)
		{
			if (entry.text == text && entry.width == width && entry.height == height
				&& entry.color == color && entry.paletteGeneration == globe->_gpuLabelPaletteGeneration)
			{
				found = &entry;
				break;
			}
		}
		if (!found)
		{
			if (globe->_gpuLabelTextures.size() >= Globe::GPU_LABEL_TEXTURE_CAPACITY
				|| globe->_gpuLabelTextures.size() >= globe->_gpuLabelTextures.capacity())
			{
				globe->_gpuLabelCapacityExceeded = true;
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape label texture capacity exhausted");
			}
			Surface* frame = new (std::nothrow) Surface(width, height, 0, 0);
			if (!frame)
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape label surface allocation failed");
			Text label(width, height, 0, 0);
			label.setPalette(globe->getEffectivePalette());
			label.initText(globe->_game->getMod()->getFont("FONT_BIG"),
				globe->_game->getMod()->getFont("FONT_SMALL"), globe->_game->getLanguage());
			label.setAlign(ALIGN_CENTER);
			label.setColor(color);
			label.setText(text);
			label.blit(frame->getSurface());
			globe->_gpuLabelTextures.push_back({text, width, height, color,
				globe->_gpuLabelPaletteGeneration, frame, nullptr});
			found = &globe->_gpuLabelTextures.back();
		}
		globe->_gpuLabelIconPendingDraws.push_back({found, nullptr, x, y, 0});
	}

	void CalypsoGeoscapeHdGlobeDirect::recordLabelIcon(Globe* globe, Surface* frame,
		int x, int y, int shade)
	{
		if (!globe || !frame)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape city marker frame unavailable");
		if (globe->_gpuLabelIconPendingDraws.size() >= Globe::GPU_LABEL_DRAW_CAPACITY
			|| globe->_gpuLabelIconPendingDraws.size() >= globe->_gpuLabelIconPendingDraws.capacity())
		{
			globe->_gpuLabelCapacityExceeded = true;
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape label batch capacity exhausted");
		}
		globe->_gpuLabelIconPendingDraws.push_back({nullptr, frame, x, y, shade});
	}

	void CalypsoGeoscapeHdGlobeDirect::ensureLogicalWorldComplete(Globe* globe)
	{
		if (!globe || globe->_gpuDebugCapacityExceeded)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
				"Geoscape HD debug geometry batch capacity exhausted");
		if (!globe || !globe->_gpuLogicalWorldComplete)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
				"Geoscape HD world incomplete: an unclaimed logical layer remains");
	}

	void CalypsoGeoscapeHdGlobeDirect::ensureBorderResources(Globe* globe)
	{
		if (!globe || !GpuInit::ready())
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape border GPU is unavailable");
		if (globe->_gpuBorderCapacityExceeded)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape border batch capacity exhausted");
		if (globe->_gpuRadarFlightCapacityExceeded)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape radar/flight batch capacity exhausted");
		if (globe->_gpuDebugCapacityExceeded)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape debug geometry batch capacity exhausted");
		if (globe->_gpuBorderLines.empty()
			&& globe->_coloredLineBatch.commandCount() == 0u
			&& globe->_gpuDebugLines.empty())
		{
			globe->_gpuBorderReady = true;
			return;
		}
		if (!globe->_borderShader)
		{
			globe->_borderShader = new Shader();
			if (!globe->_borderShader->loadFromEmbedded("colorquad"))
			{
				delete globe->_borderShader;
				globe->_borderShader = nullptr;
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape border shader compilation failed");
			}
		}
		if (!globe->_borderShader->isValid())
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape border shader is invalid");
		if (!globe->_borderVAO || !globe->_borderVBO)
		{
			glGenVertexArrays(1, &globe->_borderVAO);
			glGenBuffers(1, &globe->_borderVBO);
			const GLenum resourceError = glGetError();
			if (!globe->_borderVAO || !globe->_borderVBO || resourceError != GL_NO_ERROR)
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
					resourceError == GL_NO_ERROR
						? "Geoscape border vertex resources unavailable"
						: calypsoGlFailure("Geoscape border vertex allocation failed", resourceError));
			glBindVertexArray(globe->_borderVAO);
			glBindBuffer(GL_ARRAY_BUFFER, globe->_borderVBO);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * (GLsizei)sizeof(float), (void*)0);
			const GLenum attributeError = glGetError();
			glBindVertexArray(0);
			const GLenum vertexUnbindError = glGetError();
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			const GLenum attributeUnbindError = glGetError();
			if (attributeError != GL_NO_ERROR)
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
					calypsoGlFailure("Geoscape border vertex attribute setup failed", attributeError));
			if (vertexUnbindError != GL_NO_ERROR)
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
					calypsoGlFailure("Geoscape border vertex array unbind failed", vertexUnbindError));
			if (attributeUnbindError != GL_NO_ERROR)
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
					calypsoGlFailure("Geoscape border array buffer unbind failed", attributeUnbindError));
		}
		const size_t requiredBorderVertices = globe->_gpuBorderLines.size() * 2u;
		const size_t requiredDebugVertices = globe->_gpuDebugLines.size() * 2u;
		/* Radar/flight vertices live in the dedicated coloured-line batch since
		 * Phase 46.4 §15; the shared border buffer never resizes for them. */
		const size_t requiredVertices = std::max(requiredBorderVertices, requiredDebugVertices);
		if (requiredBorderVertices * 2u > Globe::GPU_BORDER_VERTEX_FLOAT_CAPACITY
			|| requiredDebugVertices * 2u > Globe::GPU_DEBUG_VERTEX_FLOAT_CAPACITY)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape world vertex capacity exhausted");
		if (requiredVertices > globe->_gpuBorderCapacity)
		{
			if (requiredVertices * 2u > Globe::GPU_BORDER_VERTEX_FLOAT_CAPACITY)
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape border vertex capacity exhausted");
			glBindBuffer(GL_ARRAY_BUFFER, globe->_borderVBO);
			glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(requiredVertices * 2u * sizeof(float)), nullptr, GL_DYNAMIC_DRAW);
			const GLenum bufferError = glGetError();
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			const GLenum bufferUnbindError = glGetError();
			if (bufferError != GL_NO_ERROR)
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
					calypsoGlFailure("Geoscape border vertex buffer allocation failed", bufferError));
			if (bufferUnbindError != GL_NO_ERROR)
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
					calypsoGlFailure("Geoscape border vertex buffer unbind failed", bufferUnbindError));
			globe->_gpuBorderCapacity = requiredVertices;
		}
		if (requiredDebugVertices > globe->_gpuDebugCapacity)
			globe->_gpuDebugCapacity = requiredDebugVertices;
		const GLenum borderPreflightError = glGetError();
		if (borderPreflightError != GL_NO_ERROR)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
				calypsoGlFailure("Geoscape border GL preflight failed", borderPreflightError));
		globe->_gpuBorderReady = true;
	}

	void CalypsoGeoscapeHdGlobeDirect::drawBorderPass(Globe* globe)
	{
		if (!globe || globe->_gpuBorderLines.empty() || !globe->_directScreen) return;
		if (!globe->_gpuBorderReady || !globe->_borderShader || !globe->_borderShader->isValid())
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape border resources disappeared");
		const double xs = globe->_directScreen->getXScale();
		const double ys = globe->_directScreen->getYScale();
		const float displayW = static_cast<float>(Options::displayWidth);
		const float displayH = static_cast<float>(Options::displayHeight);
		CalypsoGeoscapeHdGlobeDirect::PhysicalGlobeRect rect;
		if (!CalypsoGeoscapeHdGlobeDirect::physicalGlobeRect(globe, rect))
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape physical globe rect is invalid");
		const SDL_Color* palette = globe->getEffectivePalette();
		if (!palette)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape border palette unavailable");
		const SDL_Color color = palette[Globe::LINE_COLOR];
		const size_t requiredVertexFloats = globe->_gpuBorderLines.size() * 4u;
		if (requiredVertexFloats > Globe::GPU_BORDER_VERTEX_FLOAT_CAPACITY
			|| requiredVertexFloats > globe->_gpuBorderVertices.capacity())
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape border vertex capacity exhausted");
		globe->_gpuBorderVertices.resize(requiredVertexFloats);
		size_t vertexIndex = 0;
		for (const auto& line : globe->_gpuBorderLines)
		{
			const float x1 = 2.0f * ((rect.x + line.x1 * (float)xs) / displayW) - 1.0f;
			const float x2 = 2.0f * ((rect.x + line.x2 * (float)xs) / displayW) - 1.0f;
			const float y1 = -(2.0f * ((rect.y + line.y1 * (float)ys) / displayH) - 1.0f);
			const float y2 = -(2.0f * ((rect.y + line.y2 * (float)ys) / displayH) - 1.0f);
			globe->_gpuBorderVertices[vertexIndex++] = x1;
			globe->_gpuBorderVertices[vertexIndex++] = y1;
			globe->_gpuBorderVertices[vertexIndex++] = x2;
			globe->_gpuBorderVertices[vertexIndex++] = y2;
		}
		/* Option A: state setup hoisted into the shared line-guard in drawPass. */
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glLineWidth(1.0f);
		globe->_borderShader->use();
		globe->_borderShader->setUniform4f("u_color", color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f);
		glBindBuffer(GL_ARRAY_BUFFER, globe->_borderVBO);
		glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(globe->_gpuBorderVertices.size() * sizeof(float)), globe->_gpuBorderVertices.data());
		glBindVertexArray(globe->_borderVAO);
		glDrawArrays(GL_LINES, 0, (GLsizei)(globe->_gpuBorderLines.size() * 2u));
		glBindVertexArray(0u);
		if (glGetError() != GL_NO_ERROR)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape border draw failed");
	}

	/* Attribute 0 = vec2 position; attribute 1 = four normalised unsigned
	 * bytes taken from the locked portable vertex layout. Bound once at VAO
	 * creation so steady-state draws never touch vertex-array state. */
	static void enableColoredLineAttributes(Globe* globe)
	{
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
			(GLsizei)sizeof(OpenXcom::Calypso::CalypsoGeoscapeColoredLineVertex),
			(void*)OpenXcom::Calypso::COLORED_LINE_POSITION_OFFSET);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE,
			(GLsizei)sizeof(OpenXcom::Calypso::CalypsoGeoscapeColoredLineVertex),
			(void*)OpenXcom::Calypso::COLORED_LINE_COLOR_OFFSET);
	}

	void CalypsoGeoscapeHdGlobeDirect::ensureColoredLineResources(Globe* globe)
	{
		if (!globe || !GpuInit::ready())
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape coloured-line GPU is unavailable");
		if (globe->_gpuRadarFlightCapacityExceeded)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape radar/flight batch capacity exhausted");
		if (!globe->_coloredLineShader)
		{
			globe->_coloredLineShader = new Shader();
			if (!globe->_coloredLineShader->loadFromEmbedded("geoscape_colored_lines"))
			{
				delete globe->_coloredLineShader;
				globe->_coloredLineShader = nullptr;
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape coloured-line shader compilation failed");
			}
		}
		if (!globe->_coloredLineShader->isValid())
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape coloured-line shader is invalid");
		if (!globe->_coloredLineVAO || !globe->_coloredLineVBO)
		{
			glGenVertexArrays(1, &globe->_coloredLineVAO);
			glGenBuffers(1, &globe->_coloredLineVBO);
			const GLenum resourceError = glGetError();
			if (!globe->_coloredLineVAO || !globe->_coloredLineVBO || resourceError != GL_NO_ERROR)
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
					resourceError == GL_NO_ERROR
						? "Geoscape coloured-line vertex resources unavailable"
						: calypsoGlFailure("Geoscape coloured-line vertex allocation failed", resourceError));
			/* One fixed-capacity GPU allocation at creation time: steady-state
			 * frames never resize or reallocate storage, they only sub-update
			 * the committed prefix of the interleaved vertex buffer. */
			glBindBuffer(GL_ARRAY_BUFFER, globe->_coloredLineVBO);
			glBufferData(GL_ARRAY_BUFFER,
				(GLsizeiptr)(OpenXcom::Calypso::COLORED_LINE_VERTEX_CAPACITY
					* sizeof(OpenXcom::Calypso::CalypsoGeoscapeColoredLineVertex)),
				nullptr, GL_DYNAMIC_DRAW);
			const GLenum bufferError = glGetError();
			glBindVertexArray(globe->_coloredLineVAO);
			enableColoredLineAttributes(globe);
			glBindVertexArray(0u);
			const GLenum attributeUnbindError = glGetError();
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			const GLenum bufferUnbindError = glGetError();
			if (bufferError != GL_NO_ERROR)
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
					calypsoGlFailure("Geoscape coloured-line buffer allocation failed", bufferError));
			if (attributeUnbindError != GL_NO_ERROR)
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
					calypsoGlFailure("Geoscape coloured-line attribute setup failed", attributeUnbindError));
			if (bufferUnbindError != GL_NO_ERROR)
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
					calypsoGlFailure("Geoscape coloured-line array buffer unbind failed", bufferUnbindError));
			globe->_coloredLineResourcesReady = true;
		}
	}

/* SS15.4.3: fold every dynamic campaign input that can change radar/flight
 * output into one deterministic POD signature. The walk is linear in the
 * number of relevant entities, allocates nothing, reads no rendered pixels,
 * and performs no projection or shade lookup; floating-point fields hash
 * their exact bit patterns. Fixed presentation inputs ride the key struct.
 * This complete key is the correctness backstop: existing direct mutation
 * hooks may invalidate early, but omitting one never leaves stale geometry. */
static std::uint64_t calypsoBuildRadarFlightSignature(SavedGame* save)
{
	Calypso::CalypsoGeoscapeColoredLineSignature sig;
	for (auto* xbase : *save->getBases())
	{
		sig.mixDouble(xbase->getLatitude());
		sig.mixDouble(xbase->getLongitude());
		/* Completed facility build state and radar range. */
		for (auto* fac : *xbase->getFacilities())
		{
			const bool completed = fac->getBuildTime() == 0;
			sig.mixBool(completed);
			if (completed)
				sig.mixDouble(fac->getRules()->getRadarRange());
		}
		/* Every relevant craft: status, position, radar range, destination,
		 * meet-calculated flag, and meet position. */
		for (auto* craft : *xbase->getCrafts())
		{
			const bool out = craft->getStatus() == "STR_OUT";
			sig.mixBool(out);
			if (!out)
				continue;
			sig.mixDouble(craft->getLongitude());
			sig.mixDouble(craft->getLatitude());
			sig.mixDouble(craft->getCraftStats().radarRange);
			sig.mixBool(craft->getDestination() != 0);
			if (craft->getDestination() != 0)
			{
				sig.mixDouble(craft->getDestination()->getLongitude());
				sig.mixDouble(craft->getDestination()->getLatitude());
			}
			sig.mixBool(craft->isMeetCalculated());
			if (craft->isMeetCalculated())
			{
				sig.mixDouble(craft->getMeetLongitude());
				sig.mixDouble(craft->getMeetLatitude());
			}
		}
	}
	/* Every relevant UFO: hunter/detection state, position, radar range,
	 * hunting state, and destination position. */
	for (auto* ufo : *save->getUfos())
	{
		sig.mixBool(ufo->getStatus() == Ufo::IGNORE_ME);
		sig.mixInt64((std::int64_t)ufo->getDetected());
		sig.mixBool(ufo->getHyperDetected());
		sig.mixBool(ufo->isHunterKiller());
		sig.mixBool(ufo->isHunting());
		sig.mixDouble(ufo->getLongitude());
		sig.mixDouble(ufo->getLatitude());
		sig.mixDouble(ufo->getCraftStats().radarRange);
		sig.mixBool(ufo->getDestination() != 0);
		if (ufo->getDestination() != 0)
		{
			sig.mixDouble(ufo->getDestination()->getLongitude());
			sig.mixDouble(ufo->getDestination()->getLatitude());
		}
	}
	/* Every discovered alien-base position and detection range. */
	for (auto* ab : *save->getAlienBases())
	{
		sig.mixBool(ab->isDiscovered());
		if (!ab->isDiscovered())
			continue;
		sig.mixDouble(ab->getLatitude());
		sig.mixDouble(ab->getLongitude());
		sig.mixDouble(ab->getDeployment()->getBaseDetectionRange());
	}
	return sig.value();
}

	void CalypsoGeoscapeHdGlobeDirect::prepareRadarFlightSnapshot(Globe* globe)
	{
		if (!globe || !globe->_directScreen)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape radar/flight preparation owner unavailable");
		CalypsoGeoscapeHdGlobeDirect::PhysicalGlobeRect rect;
		if (!CalypsoGeoscapeHdGlobeDirect::physicalGlobeRect(globe, rect))
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape physical globe rect is invalid");
		Calypso::CalypsoGeoscapeRadarCounters& counters = Calypso::calypsoRadarCounters();
		const bool instrumented = Calypso::calypsoRadarCountersEnabled();
		const Uint64 prepareStart = instrumented ? SDL_GetPerformanceCounter() : 0;
		/* SS15.4.3 snapshot key: every fixed presentation input plus the
		 * dynamic campaign signature. Concrete geometry values ride explicit
		 * fields, so no mutation owner has to remember a dirty setter.
		 * viewportGeneration stays reserved: rect/scale/display values are
		 * compared directly below. */
		Calypso::CalypsoGeoscapeColoredLineSnapshotKey key = Calypso::CalypsoGeoscapeColoredLineSnapshotKey();
		key.viewportGeneration = 0u;
		key.rectX = rect.x;
		key.rectY = rect.y;
		key.rectW = rect.w;
		key.rectH = rect.h;
		key.displayWidth = Options::displayWidth;
		key.displayHeight = Options::displayHeight;
		key.sdlScaleX = globe->_directScreen->getXScale();
		key.sdlScaleY = globe->_directScreen->getYScale();
		key.centreLongitude = globe->_cenLon;
		key.centreLatitude = globe->_cenLat;
		key.zoomLevel = (double)globe->_zoom;
		key.globeRadius = globe->_zoomRadius[globe->_zoom];
		key.textureZoom = (double)globe->_zoomTexture;
		key.hoverEnabled = globe->_hover;
		key.hoverLongitude = globe->_hoverLon;
		key.hoverLatitude = globe->_hoverLat;
		key.craftRangeEnabled = globe->_craft;
		key.craftLongitude = globe->_craftLon;
		key.craftLatitude = globe->_craftLat;
		key.craftRange = globe->_craftRange;
		key.optionRadarLines = Options::globeRadarLines;
		key.optionFlightPaths = Options::globeFlightPaths;
		key.optionAllRadarsOnBaseBuild = Options::globeAllRadarsOnBaseBuild;
		key.paletteGeneration = globe->_gpuRadarPaletteGeneration;
		key.enemyRadarMode = (std::int64_t)globe->_game->getMod()->getDrawEnemyRadarCircles();
		key.debugMode = globe->_game->getSavedGame()->getDebugMode();
		key.dynamicSignature = calypsoBuildRadarFlightSignature(globe->_game->getSavedGame());
		const Calypso::ColoredLinePrepareResult verdict = globe->_coloredLineCache.prepare(key);
		if (instrumented)
		{
			++counters.radarFingerprintChecks;
			if (verdict == Calypso::COLORED_LINE_CACHE_HIT) ++counters.radarCacheHits;
		}
		if (verdict != Calypso::COLORED_LINE_REBUILT)
			return; /* Cache hit: reuse CPU commands and the uploaded VBO. */
		/* Rebuild path: pack from the same frozen physical globe rectangle
		 * used by Earth, markers, labels, and hit testing (SS15.10 risk 7).
		 * Pure CPU work; the single upload/draw boundary stays inside
		 * drawRadarFlightPass. */
		Calypso::CalypsoGeoscapeColoredLineViewport viewport;
		viewport.rectX = rect.x;
		viewport.rectY = rect.y;
		viewport.scaleX = key.sdlScaleX;
		viewport.scaleY = key.sdlScaleY;
		viewport.displayWidth = key.displayWidth;
		viewport.displayHeight = key.displayHeight;
		const size_t vertices = globe->_coloredLineBatch.packVertices(viewport);
		if (vertices == static_cast<size_t>(-1))
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape radar/flight pack preflight exhausted capacity");
		if (instrumented)
		{
			++counters.radarRebuilds;
			counters.radarPreparedCommands += globe->_coloredLineBatch.commandCount();
			counters.radarPreparedVertices += vertices;
			counters.radarPrepareUs += (std::uint64_t)((SDL_GetPerformanceCounter() - prepareStart) * 1000000ull / SDL_GetPerformanceFrequency());
		}
	}

	void CalypsoGeoscapeHdGlobeDirect::commitLabelIconSnapshot(Globe* globe)
	{
		if (!globe || !globe->_gpuDirectMode) return;
		/* SS15.4.5: labels/icons publish exactly once per frame, here -- never
		 * as a drawFlights() side effect -- so a radar/flight cache hit can
		 * never freeze or erase label publication. */
		globe->_gpuLabelIconPendingDraws.swap(globe->_gpuLabelIconCommittedDraws);
		globe->_gpuLabelIconPendingDraws.clear();
	}

	void CalypsoGeoscapeHdGlobeDirect::drawRadarFlightPass(Globe* globe)
	{
		if (!globe || globe->_coloredLineBatch.vertexCount() == 0u || !globe->_directScreen) return;
		if (!globe->_coloredLineResourcesReady || !globe->_coloredLineShader
			|| !globe->_coloredLineShader->isValid())
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape coloured-line resources disappeared");
		/* SS15.4.6: the single upload/draw boundary lives inside ONE shared
		 * state guard owned by drawPass (border+radar), so steady-state frames
		 * perform no capacity queries, no shader metadata lookups, and no
		 * redundant state snapshots here. */
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glLineWidth(1.0f);
		globe->_coloredLineShader->use();
		Calypso::CalypsoGeoscapeRadarCounters& counters = Calypso::calypsoRadarCounters();
		const bool instrumented = Calypso::calypsoRadarCountersEnabled();
		/* SS15.4.2: an unchanged snapshot performs zero uploads; a context
		 * restore reuploads the committed CPU snapshot exactly once. */
		if (!globe->_coloredLineCache.uploadCurrent())
		{
			const Uint64 uploadStart = instrumented ? SDL_GetPerformanceCounter() : 0;
			glBindBuffer(GL_ARRAY_BUFFER, globe->_coloredLineVBO);
			glBufferSubData(GL_ARRAY_BUFFER, 0,
				(GLsizeiptr)globe->_coloredLineBatch.packedVertexBytes(),
				globe->_coloredLineBatch.packedVertices());
			globe->_coloredLineCache.markUploaded();
			if (instrumented)
			{
				++counters.radarUploads;
				counters.radarUploadBytes += globe->_coloredLineBatch.packedVertexBytes();
				counters.radarUploadUs += (std::uint64_t)((SDL_GetPerformanceCounter() - uploadStart) * 1000000ull / SDL_GetPerformanceFrequency());
			}
		}
		const GLenum uploadError = glGetError();
		glBindVertexArray(globe->_coloredLineVAO);
		/* The literal contract: a non-empty snapshot is submitted by exactly
		 * one WebGL draw call. No colour-run scan remains. */
		glDrawArrays(GL_LINES, 0, (GLsizei)globe->_coloredLineBatch.vertexCount());
		glBindVertexArray(0u);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		const GLenum drawError = glGetError();
		if (instrumented)
			++counters.radarDrawCalls;
		if (uploadError != GL_NO_ERROR)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
				calypsoGlFailure("Geoscape coloured-line upload failed", uploadError));
		if (drawError != GL_NO_ERROR)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
				calypsoGlFailure("Geoscape coloured-line draw failed", drawError));
	}

	void CalypsoGeoscapeHdGlobeDirect::drawDebugPass(Globe* globe)
	{
		if (!globe || globe->_gpuDebugLines.empty() || !globe->_directScreen) return;
		if (!globe->_gpuBorderReady || !globe->_borderShader || !globe->_borderShader->isValid())
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape debug geometry resources disappeared");
		if (globe->_gpuDebugCapacityExceeded)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape debug geometry batch capacity exhausted");
		const double xs = globe->_directScreen->getXScale();
		const double ys = globe->_directScreen->getYScale();
		const float displayW = static_cast<float>(Options::displayWidth);
		const float displayH = static_cast<float>(Options::displayHeight);
		PhysicalGlobeRect rect;
		if (!physicalGlobeRect(globe, rect))
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape physical globe rect is invalid");
		const SDL_Color* palette = globe->getEffectivePalette();
		if (!palette)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape debug geometry palette unavailable");
		const size_t requiredVertexFloats = globe->_gpuDebugLines.size() * 4u;
		if (requiredVertexFloats > Globe::GPU_DEBUG_VERTEX_FLOAT_CAPACITY
			|| requiredVertexFloats > globe->_gpuDebugVertices.capacity())
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape debug geometry vertex capacity exhausted");
		globe->_gpuDebugVertices.resize(requiredVertexFloats);
		size_t vertexIndex = 0;
		for (const auto& line : globe->_gpuDebugLines)
		{
			globe->_gpuDebugVertices[vertexIndex++] = (float)(2.0 * ((rect.x + line.x1 * xs) / displayW) - 1.0);
			globe->_gpuDebugVertices[vertexIndex++] = (float)-(2.0 * ((rect.y + line.y1 * ys) / displayH) - 1.0);
			globe->_gpuDebugVertices[vertexIndex++] = (float)(2.0 * ((rect.x + line.x2 * xs) / displayW) - 1.0);
			globe->_gpuDebugVertices[vertexIndex++] = (float)-(2.0 * ((rect.y + line.y2 * ys) / displayH) - 1.0);
		}
		GlobeSphereGlSave st; st.save();
		setPhysicalGlobeClip(globe);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glLineWidth(1.0f);
		globe->_borderShader->use();
		glBindBuffer(GL_ARRAY_BUFFER, globe->_borderVBO);
		glBufferSubData(GL_ARRAY_BUFFER, 0,
			(GLsizeiptr)(globe->_gpuDebugVertices.size() * sizeof(float)), globe->_gpuDebugVertices.data());
		glBindVertexArray(globe->_borderVAO);
		size_t begin = 0;
		while (begin < globe->_gpuDebugLines.size())
		{
			const Uint8 colorIndex = globe->_gpuDebugLines[begin].color;
			size_t end = begin + 1;
			while (end < globe->_gpuDebugLines.size()
				&& globe->_gpuDebugLines[end].color == colorIndex) ++end;
			const SDL_Color color = palette[colorIndex];
			globe->_borderShader->setUniform4f("u_color", color.r / 255.0f, color.g / 255.0f,
				color.b / 255.0f, color.a / 255.0f);
			glDrawArrays(GL_LINES, (GLint)(begin * 2u), (GLsizei)((end - begin) * 2u));
			begin = end;
		}
		glBindVertexArray(0u);
		if (glGetError() != GL_NO_ERROR)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape debug geometry draw failed");
		st.restore();
	}

	void CalypsoGeoscapeHdGlobeDirect::ensureMarkerResources(Globe* globe)
	{
		if (!globe || !GpuInit::ready())
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape marker GPU is unavailable");
		if (!globe->_markerShader)
		{
			globe->_markerShader = new Shader();
			if (!globe->_markerShader->loadFromEmbedded("textured"))
			{
				delete globe->_markerShader;
				globe->_markerShader = nullptr;
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape marker shader compilation failed");
			}
		}
		if (!globe->_markerShader->isValid())
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape marker shader is invalid");
		if (!globe->_markerVAO || !globe->_markerVBO)
		{
			glGenVertexArrays(1, &globe->_markerVAO);
			glGenBuffers(1, &globe->_markerVBO);
			if (!globe->_markerVAO || !globe->_markerVBO || glGetError() != GL_NO_ERROR)
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape marker vertex resources unavailable");
			glBindVertexArray(globe->_markerVAO);
			glBindBuffer(GL_ARRAY_BUFFER, globe->_markerVBO);
			glBufferData(GL_ARRAY_BUFFER, 6 * 4 * (GLsizeiptr)sizeof(float), nullptr, GL_DYNAMIC_DRAW);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * (GLsizei)sizeof(float), (void*)0);
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * (GLsizei)sizeof(float), (void*)(2 * sizeof(float)));
			glBindVertexArray(0);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			if (glGetError() != GL_NO_ERROR)
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape marker vertex setup failed");
		}
		globe->_gpuMarkerReady = true;
		for (const auto& command : globe->_gpuMarkerCommittedDraws)
		{
			if (!command.frame || command.frame->getWidth() <= 0 || command.frame->getHeight() <= 0
				|| !CalypsoGeoscapeHdGlobeDirect::markerTexture(globe, command.frame, command.shade))
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape marker texture upload failed");
		}
		if (glGetError() != GL_NO_ERROR)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape marker GL preflight failed");
	}

	GpuTexture* CalypsoGeoscapeHdGlobeDirect::markerTexture(Globe* globe, Surface* frame, int shade)
	{
		for (const auto& entry : globe->_gpuMarkerTextures)
		{
			if (entry.frame == frame && entry.shade == shade
				&& entry.paletteGeneration == globe->_gpuMarkerPaletteGeneration
				&& entry.texture && entry.texture->isValid())
				return entry.texture;
		}
		const int w = frame->getWidth();
		const int h = frame->getHeight();
		if (w <= 0 || h <= 0) return nullptr;
		const ShadeTable* table = frame->getShadeTable();
		const Uint8* mirror = frame->getPaletteMirror();
		std::vector<uint8_t> rgba(static_cast<size_t>(w * h * 4), 0u);
		for (int py = 0; py < h; ++py)
		{
			for (int px = 0; px < w; ++px)
			{
				Uint32 argb = frame->getPixel32(px, py);
				if (mirror && table) argb = table->get(mirror[py * w + px], shade);
				const size_t i = static_cast<size_t>((py * w + px) * 4);
				rgba[i + 0] = static_cast<uint8_t>((argb >> 16) & 0xffu);
				rgba[i + 1] = static_cast<uint8_t>((argb >> 8) & 0xffu);
				rgba[i + 2] = static_cast<uint8_t>(argb & 0xffu);
				rgba[i + 3] = static_cast<uint8_t>((argb >> 24) & 0xffu);
			}
		}
		GpuTexture* texture = new GpuTexture(false, GpuTexture::Wrap::ClampToEdge, GpuTexture::Filter::Nearest);
		if (!texture->uploadRGBA(rgba.data(), w, h))
		{
			delete texture;
			return nullptr;
		}
		globe->_gpuMarkerTextures.push_back({frame, shade, globe->_gpuMarkerPaletteGeneration, texture});
		return texture;
	}

	GpuTexture* CalypsoGeoscapeHdGlobeDirect::labelTexture(Globe* globe, Globe::LabelTexture& entry)
	{
		if (!globe || !entry.frame || entry.width <= 0 || entry.height <= 0)
			return nullptr;
		if (entry.texture && entry.paletteGeneration == globe->_gpuLabelPaletteGeneration
			&& entry.texture->isValid())
			return entry.texture;
		const int w = entry.frame->getWidth();
		const int h = entry.frame->getHeight();
		if (w <= 0 || h <= 0) return nullptr;
		std::vector<uint8_t> rgba(static_cast<size_t>(w * h * 4), 0u);
		for (int py = 0; py < h; ++py)
		{
			for (int px = 0; px < w; ++px)
			{
				const Uint32 argb = entry.frame->getPixel32(px, py);
				const size_t i = static_cast<size_t>((py * w + px) * 4);
				rgba[i + 0] = static_cast<uint8_t>((argb >> 16) & 0xffu);
				rgba[i + 1] = static_cast<uint8_t>((argb >> 8) & 0xffu);
				rgba[i + 2] = static_cast<uint8_t>(argb & 0xffu);
				rgba[i + 3] = static_cast<uint8_t>((argb >> 24) & 0xffu);
			}
		}
		GpuTexture* texture = new (std::nothrow) GpuTexture(false,
			GpuTexture::Wrap::ClampToEdge, GpuTexture::Filter::Nearest);
		if (!texture || !texture->uploadRGBA(rgba.data(), w, h))
		{
			delete texture;
			return nullptr;
		}
		entry.texture = texture;
		entry.paletteGeneration = globe->_gpuLabelPaletteGeneration;
		return texture;
	}

	void CalypsoGeoscapeHdGlobeDirect::ensureLabelResources(Globe* globe)
	{
		if (!globe || !GpuInit::ready())
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape label GPU is unavailable");
		if (globe->_gpuLabelCapacityExceeded)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape label batch capacity exhausted");
		if (globe->_gpuLabelIconCommittedDraws.empty()) return;
		CalypsoGeoscapeHdGlobeDirect::ensureMarkerResources(globe);
		for (auto& command : globe->_gpuLabelIconCommittedDraws)
		{
			if (command.label)
			{
				if (!CalypsoGeoscapeHdGlobeDirect::labelTexture(globe, *command.label))
					Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape label texture upload failed");
			}
			else if (!command.frame || command.frame->getWidth() <= 0 || command.frame->getHeight() <= 0
				|| !CalypsoGeoscapeHdGlobeDirect::markerTexture(globe, command.frame, command.shade))
			{
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape city marker texture upload failed");
			}
		}
		if (glGetError() != GL_NO_ERROR)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape label GL preflight failed");
	}

	void CalypsoGeoscapeHdGlobeDirect::drawLabelIconPass(Globe* globe)
	{
		if (!globe || globe->_gpuLabelIconCommittedDraws.empty() || !globe->_directScreen) return;
		ensureLabelResources(globe);
		const double xs = globe->_directScreen->getXScale();
		const double ys = globe->_directScreen->getYScale();
		const int lbb = globe->_directScreen->getCursorLeftBlackBand();
		const int tbb = globe->_directScreen->getCursorTopBlackBand();
		const float displayW = static_cast<float>(Options::displayWidth);
		const float displayH = static_cast<float>(Options::displayHeight);
		GlobeSphereGlSave st; st.save();
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		CalypsoGeoscapeHdGlobeDirect::setPhysicalGlobeClip(globe);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		globe->_markerShader->use();
		globe->_markerShader->setUniform1f("u_darken", 0.0f);
		globe->_markerShader->setUniform1i("u_tex", 0);
		for (const auto& command : globe->_gpuLabelIconCommittedDraws)
		{
			Surface* frame = command.label ? command.label->frame : command.frame;
			GpuTexture* texture = command.label
				? labelTexture(globe, *command.label)
				: markerTexture(globe, command.frame, command.shade);
			if (!frame || !texture)
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape label texture disappeared during draw");
			const float x = static_cast<float>((globe->getX() + command.x) * xs + lbb);
			const float y = static_cast<float>((globe->getY() + command.y) * ys + tbb);
			const float w = static_cast<float>(frame->getWidth() * xs);
			const float h = static_cast<float>(frame->getHeight() * ys);
			const float x0 = 2.0f * x / displayW - 1.0f;
			const float x1 = 2.0f * (x + w) / displayW - 1.0f;
			const float y0 = -(2.0f * y / displayH - 1.0f);
			const float y1 = -(2.0f * (y + h) / displayH - 1.0f);
			const float verts[6 * 4] = {x0, y0, 0.f, 0.f, x1, y0, 1.f, 0.f,
				x0, y1, 0.f, 1.f, x0, y1, 0.f, 1.f, x1, y0, 1.f, 0.f,
				x1, y1, 1.f, 1.f};
			glBindBuffer(GL_ARRAY_BUFFER, globe->_markerVBO);
			glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)sizeof(verts), verts);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			texture->bind(0);
			glBindVertexArray(globe->_markerVAO);
			glDrawArrays(GL_TRIANGLES, 0, 6);
			glBindVertexArray(0);
		}
		if (glGetError() != GL_NO_ERROR)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape label draw failed");
		st.restore();
	}

	void CalypsoGeoscapeHdGlobeDirect::drawMarkerPass(Globe* globe)
	{
		if (!globe || globe->_gpuMarkerCommittedDraws.empty() || !globe->_directScreen) return;
		CalypsoGeoscapeHdGlobeDirect::ensureMarkerResources(globe);
		const double xs = globe->_directScreen->getXScale();
		const double ys = globe->_directScreen->getYScale();
		const int lbb = globe->_directScreen->getCursorLeftBlackBand();
		const int tbb = globe->_directScreen->getCursorTopBlackBand();
		const float displayW = static_cast<float>(Options::displayWidth);
		const float displayH = static_cast<float>(Options::displayHeight);
		GlobeSphereGlSave st; st.save();
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		CalypsoGeoscapeHdGlobeDirect::setPhysicalGlobeClip(globe);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		globe->_markerShader->use();
		globe->_markerShader->setUniform1f("u_darken", 0.0f);
		globe->_markerShader->setUniform1i("u_tex", 0);
		for (const auto& command : globe->_gpuMarkerCommittedDraws)
		{
			GpuTexture* texture = markerTexture(globe, command.frame, command.shade);
			if (!texture)
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape marker texture disappeared during draw");
			const float x = static_cast<float>((globe->getX() + command.x) * xs + lbb);
			const float y = static_cast<float>((globe->getY() + command.y) * ys + tbb);
			const float w = static_cast<float>(command.frame->getWidth() * xs);
			const float h = static_cast<float>(command.frame->getHeight() * ys);
			const float x0 = 2.0f * x / displayW - 1.0f;
			const float x1 = 2.0f * (x + w) / displayW - 1.0f;
			const float y0 = -(2.0f * y / displayH - 1.0f);
			const float y1 = -(2.0f * (y + h) / displayH - 1.0f);
			const float verts[6 * 4] = {x0, y0, 0.f, 0.f, x1, y0, 1.f, 0.f, x0, y1, 0.f, 1.f, x0, y1, 0.f, 1.f, x1, y0, 1.f, 0.f, x1, y1, 1.f, 1.f};
			glBindBuffer(GL_ARRAY_BUFFER, globe->_markerVBO);
			glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)sizeof(verts), verts);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			texture->bind(0);
			glBindVertexArray(globe->_markerVAO);
			glDrawArrays(GL_TRIANGLES, 0, 6);
			glBindVertexArray(0);
		}
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, 0u);
		if (glGetError() != GL_NO_ERROR)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape marker draw failed");
		st.restore();
	}


/**
 * One-time GPU initialisation for the HD sphere path.
 * Called lazily from drawSphereGPU() on the first frame.
 */
bool Globe::initSphereGPU()
{
	if (!GpuInit::ready()) return false;

	if (!_globeShader || !_globeShader->isValid())
	{
		delete _globeShader;
		_globeShader = new Shader();
		if (!_globeShader->loadFromEmbedded("globe_sphere"))
		{
			Log(LOG_ERROR) << "Globe::initSphereGPU: shader compile failed";
			delete _globeShader; _globeShader = nullptr;
			return false;
		}
	}

	/* Fullscreen-quad VAO (NDC -1..+1, UV 0..1). */
	float verts[] = {
		-1.f,-1.f, 0.f,0.f,   1.f,-1.f, 1.f,0.f,  -1.f, 1.f, 0.f,1.f,
		-1.f, 1.f, 0.f,1.f,   1.f,-1.f, 1.f,0.f,   1.f, 1.f, 1.f,1.f,
	};
	GLuint vbo = 0u;
	glGenVertexArrays(1, &_sphereVAO);
	glBindVertexArray(_sphereVAO);
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));
	glEnableVertexAttribArray(1);
	glBindVertexArray(0);
	/* VBO is owned by the VAO after bind; no need to keep a separate handle. */

	/* FBO + colour attachment (same size as globe surface). */
	int w = 0, h = 0; CalypsoGeoscapeHdGlobeDirect::computeSphereRes(this, w, h);
	glGenTextures(1, &_sphereFBOTex);
	glBindTexture(GL_TEXTURE_2D, _sphereFBOTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0u);

	glGenFramebuffers(1, &_sphereFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, _sphereFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _sphereFBOTex, 0);
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	glBindFramebuffer(GL_FRAMEBUFFER, 0u);
	if (status != GL_FRAMEBUFFER_COMPLETE)
	{
		Log(LOG_ERROR) << "Globe::initSphereGPU: FBO incomplete (status=" << (int)status << ")";
		return false;
	}
	if (glGetError() != GL_NO_ERROR || !_sphereVAO || !_sphereFBO || !_sphereFBOTex)
	{
		Log(LOG_ERROR) << "Globe::initSphereGPU: Earth GL resources unavailable";
		return false;
	}

	_gpuSphereOK = true;
	Log(LOG_INFO) << "Globe::initSphereGPU: ready (" << w << "x" << h << ")";

	/* M6: register a ShaderManager reset callback so a real WebGL context
	 * loss (GPU crash, iOS tab switch) is handled correctly.  On restore,
	 * reuploadAll() re-compiles the shaders/textures; this callback nulls raw
	 * GL handles and clears both pass-ready flags so the next frame rebuilds
	 * the sphere and marker overlay without stale commands. */
	if (!_gpuAliveFlag) _gpuAliveFlag = std::make_shared<bool>(true);
	if (!_gpuResetCallbackRegistered)
	{
	ShaderManager::instance().registerResetCallback(_gpuAliveFlag, [this]() {
		_sphereVAO    = 0u;
		_sphereFBO    = 0u;
		_sphereFBOTex = 0u;
		_gpuSphereOK  = false;
		_markerVAO    = 0u;
		_markerVBO    = 0u;
		_gpuMarkerReady = false;
		_borderVAO    = 0u;
		_borderVBO    = 0u;
		_gpuBorderReady = false;
		_gpuBorderCapacity = 0u;
		_gpuDebugCapacity = 0u;
		_coloredLineVAO = 0u;
		_coloredLineVBO = 0u;
		_coloredLineResourcesReady = false;
		_coloredLineCache.notifyContextReset();
		_gpuBorderCapacityExceeded = false;
		_gpuDebugCapacityExceeded = false;
		_gpuRadarFlightCapacityExceeded = false;
		for (auto& entry : _gpuMarkerTextures) delete entry.texture;
		_gpuMarkerTextures.clear();
		++_gpuMarkerPaletteGeneration;
		for (auto& entry : _gpuLabelTextures)
		{
			delete entry.texture;
			entry.texture = nullptr;
		}
		_gpuLabelCapacityExceeded = false;
		/* Keep the committed command snapshot. Context restore invalidates raw
		 * resources only; gameplay-owned commands remain the source for the
		 * first rebuilt physical frame. */
		});
		_gpuResetCallbackRegistered = true;
	}

	return true;
}

/**
 * Sun direction in the fixed world frame the GPU shader uses.
 * World frame: Y = north pole, X = +90° lon (east), Z = 0° lon (prime meridian).
 * This is independent of the observer position — unlike getSunDirection(lon, lat)
 * which returns a camera-relative vector.
 */
Cord Globe::getSunDirectionWorld() const
{
	const double rot = _game->getSavedGame()->getTime()->getDaylight() * 2*M_PI;
	double decl = 0;
	if (Options::globeSeasons)
	{
		const int MonthDays1[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365};
		const int MonthDays2[] = {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366};

		int year  = _game->getSavedGame()->getTime()->getYear();
		int month = _game->getSavedGame()->getTime()->getMonth()-1;
		int day   = _game->getSavedGame()->getTime()->getDay()-1;

		double tm = (double)((_game->getSavedGame()->getTime()->getHour() * 60
			+ _game->getSavedGame()->getTime()->getMinute()) * 60
			+ _game->getSavedGame()->getTime()->getSecond()) / 86400.0;

		double CurDay;
		if (year%4 == 0 && !(year%100 == 0 && year%400 != 0))
			CurDay = (MonthDays2[month] + day + tm)/366 - 0.219;
		else
			CurDay = (MonthDays1[month] + day + tm)/365 - 0.219;
		if (CurDay < 0) CurDay += 1.;

		decl = -0.261 * sin(CurDay * 2*M_PI);
	}
	// Subsolar point lon = π/2 − rot, lat = decl.
	// getDaylight()=0 corresponds to 6h GMT (sub-solar at 90° E), daylight=0.25
	// is noon at Greenwich (sub-solar at 0°), so the offset from rot is +π/2.
	const double sunLon = M_PI / 2.0 - rot;
	return Cord(cos(decl) * sin(sunLon),
	            sin(decl),
	            cos(decl) * cos(sunLon));
}

void Globe::drawHDStarfield()
{
	if (!isARGB()) return;

	const int w = getWidth();
	const int h = getHeight();
	const double globeLimit = (_zoomRadius[_zoom] + 5.0) * (_zoomRadius[_zoom] + 5.0);

	lock();
	for (int y = 0; y < h; ++y)
	{
		const float t = (h > 1) ? (float)y / (float)(h - 1) : 0.f;
		const Uint8 r = (Uint8)(1 + t * 2);
		const Uint8 g = (Uint8)(5 + t * 9);
		const Uint8 b = (Uint8)(17 + t * 18);
		const Uint32 bg = 0xFF000000u | ((Uint32)r << 16) | ((Uint32)g << 8) | (Uint32)b;
		for (int x = 0; x < w; ++x)
		{
			setPixel32(x, y, bg);
		}
	}

	/* Deterministic sparse stars: bright enough to give the globe a space
	 * setting, sparse enough to avoid fighting Geoscape labels and markers.
	 *
	 * Stage 13 QA (loopback-only): freeze/reduced-motion replace ONLY the
	 * twinkle clock's millisecond source (liveSeconds = SDL_GetTicks()*0.001,
	 * resolved through calypsoGeoscapeQaPresentationSeconds(); twinkleTime is
	 * that value scaled by 1.7). With defaults the expression below stays
	 * exactly the production SDL_GetTicks() * 0.0017f math. */
	float twinkleMs = (float)SDL_GetTicks();
	const auto& qa = Calypso::calypsoGeoscapeQaPresentation();
	if (qa.frozenClock || qa.reducedMotion)
		twinkleMs = (float)(Calypso::calypsoGeoscapeQaPresentationSeconds(qa, twinkleMs * 0.001) * 1000.0);
	const float twinkleTime = twinkleMs * 0.0017f;
	for (unsigned i = 0; i < 125; ++i)
	{
		unsigned n = i * 747796405u + 2891336453u;
		n = ((n >> ((n >> 28u) + 4u)) ^ n) * 277803737u;
		n = (n >> 22u) ^ n;
		const int x = (int)(n % (unsigned)w);
		const int y = (int)((n / (unsigned)w) % (unsigned)h);
		const double dx = (double)x - (double)_cenX;
		const double dy = (double)y - (double)_cenY;
		if (dx * dx + dy * dy < globeLimit) continue;

		const float phase = (float)((n >> 8u) & 0xFFu) * 0.024543693f;
		const float pulse = 0.62f + 0.38f * (0.5f + 0.5f * sinf(twinkleTime + phase));
		const Uint8 v = (Uint8)((100 + (n & 0x7Fu)) * pulse);
		const Uint32 star = 0xFF000000u
			| ((Uint32)(v * 78 / 100) << 16)
			| ((Uint32)(v * 92 / 100) << 8)
			| (Uint32)v;
		setPixel32(x, y, star);
		if ((n & 0x0Fu) == 0 && x + 1 < w) setPixel32(x + 1, y, star);
		if ((n & 0x1Fu) == 0 && y + 1 < h) setPixel32(x, y + 1, star);
	}
	unlock();
}

/**
 * Renders the HD sphere using the GPU shader pipeline and reads the pixels
 * back into this Surface so the existing CPU overlay (polylines, markers,
 * text) can be composited on top in the same Globe::draw() call.
 *
 * Performance: glReadPixels for the globe surface is ~0.2–1 ms on typical
 * hardware; acceptable for a 60 fps Geoscape.
 */
void Globe::drawSphereGPU()
{
	if (!_gpuSphereOK && !initSphereGPU()) return;
	if (_gpuDirectMode)
	{
		/* Direct mode is owned by Screen's registered world slot. The slot
		 * performs Earth resource preflight and the ordered draw after SDL's
		 * composite, so do not paint an early duplicate here. */
		return;
	}

	Mod* mod = _game->getMod();
	GpuTexture* bathyTex   = mod->getGlobeTexture("bathymetry");
	GpuTexture* diffuseTex = mod->getGlobeTexture("diffuse");
	GpuTexture* nightTex   = mod->getGlobeTexture("night");
	GpuTexture* cloudsTex  = mod->getGlobeTexture("clouds");
	if (!bathyTex || !diffuseTex || !nightTex || !cloudsTex) return;

	/* Stage 13 QA (loopback-only): loopback capture controls; every field
	 * defaults to the production inputs consumed below. */
	const auto& qa = Calypso::calypsoGeoscapeQaPresentation();

	int w = 0, h = 0; CalypsoGeoscapeHdGlobeDirect::computeSphereRes(this, w, h);

	/* Phase 8c.10 perf instrumentation: wall-clock GPU pass time.  ENTIRELY
	 * gated on ::g_calypsoProfileGlobe — when the flag is 0 (production
	 * default) the GpuTimer object is never constructed and steady_clock is
	 * never read, so the path costs one int load + one branch-not-taken.
	 * Sampled at the local level instead of Screen::registerGPUPass because
	 * Globe's draw cycle does FBO render + glReadPixels synchronously into
	 * _surface; restructuring would have been disproportionate. */
	const int profileGlobe = ::g_calypsoProfileGlobe;
	GpuTimer perfTimer;
	if (profileGlobe) perfTimer.start();

	GlobeSphereGlSave st; st.save();

	/* Render sphere to FBO. */
	glBindFramebuffer(GL_FRAMEBUFFER, _sphereFBO);
	glViewport(0, 0, w, h);
	glClearColor(0.f, 0.f, 0.f, 0.f);
	glClear(GL_COLOR_BUFFER_BIT);
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);

	_globeShader->use();

	bathyTex ->bind(0);
	diffuseTex->bind(1);
	nightTex ->bind(2);
	/* Stage 13 QA (loopback-only): capture rows may bind the persistent
	 * transparent 1x1 input; production always samples the mod clouds
	 * texture, and an upload failure falls back to it identically. */
	GpuTexture* cloudsBound = cloudsTex;
	if (qa.cloudMode == Calypso::GeoscapeQaCloudMode::Hidden)
	{
		if (!calypsoGlobeQaHiddenCloudsTexture()->isValid()) cloudsBound = cloudsTex;
		else cloudsBound = calypsoGlobeQaHiddenCloudsTexture();
	}
	cloudsBound->bind(3);
	_globeShader->setUniform1i("u_bathymetry", 0);
	_globeShader->setUniform1i("u_diffuse",    1);
	_globeShader->setUniform1i("u_night",      2);
	_globeShader->setUniform1i("u_clouds",     3);
	_globeShader->setUniform1i("u_background", 0);

	/* Viewport and globe geometry. */
	_globeShader->setUniform2f("u_viewportSize", (float)w, (float)h);
	_globeShader->setUniform2f("u_globeCenter",  (float)_cenX, (float)_cenY);
	_globeShader->setUniform1f("u_globeRadius",  (float)_zoomRadius[_zoom]);
	_globeShader->setUniform1f("u_camLat",       (float)_cenLat);
	_globeShader->setUniform1f("u_camLon",       (float)_cenLon);

	/* Sun direction in world frame (8c.5 fix: was camera-relative, now world frame). */
	Cord sd = getSunDirectionWorld();
	/* Stage 13 QA (loopback-only): deterministic day/night rows replace the
	 * fed value only; campaign time is never read or mutated. */
	if (qa.sunMode != Calypso::GeoscapeQaSunMode::Live)
		sd = calypsoGlobeQaCord(Calypso::calypsoGeoscapeQaSunDirection(qa.sunMode, _cenLon, _cenLat));
	_globeShader->setUniform3f("u_sunDir", (float)sd.x, (float)sd.y, (float)sd.z);

	/* Cloud drift time. */
	float timeMs = calypsoGlobeQaEffectiveMs((float)SDL_GetTicks());
	_globeShader->setUniform1f("u_time", timeMs * 0.001f);

	/* Mip level curve: keep the overview detailed enough that land does not
	 * read as a low-res smear; the globe is small, but 1k mips are too soft. */
	float mipLvl = std::max(0.f, std::min(1.35f, 1.35f - (float)_zoom * 0.27f));
	_globeShader->setUniform1f("u_mipLevel", mipLvl);

	glBindVertexArray(_sphereVAO);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0u);

	/* Read back RGBA pixels from FBO; FBO rows are bottom-up, SDL is top-down. */
	std::vector<uint8_t> rgba((size_t)w * h * 4);
	glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());

	/* Unbind our textures from units 0..3 and reset the active unit to 0.
	 * SDL2's renderer reuses these units for SDL_Texture rendering and would
	 * otherwise pick up our globe textures, blasting them across the canvas
	 * (sphere shader output is overridden by raw bathymetry on UI blits). */
	for (int i = 3; i >= 0; --i) {
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, 0u);
	}

	st.restore();

	/* Convert RGBA (GL) → ARGB8888 (SDL little-endian) and flip Y.
	 * SDL_PIXELFORMAT_ARGB8888 memory layout: byte0=B, byte1=G, byte2=R, byte3=A. */
	lock();
	uint8_t* dst   = reinterpret_cast<uint8_t*>(getSurface()->pixels);
	int      pitch = getSurface()->pitch;
	for (int y = 0; y < h; ++y)
	{
		const uint8_t* src = rgba.data() + (size_t)(h - 1 - y) * w * 4;
		uint8_t*       row = dst + y * pitch;
		for (int x = 0; x < w; ++x)
		{
			const uint8_t a = src[x*4 + 3];
			if (a == 0) continue; // discarded by shader — preserve starfield
			if (a == 255)
			{
				row[x*4 + 0] = src[x*4 + 2]; /* B */
				row[x*4 + 1] = src[x*4 + 1]; /* G */
				row[x*4 + 2] = src[x*4 + 0]; /* R */
				row[x*4 + 3] = 255;
			}
			else
			{
				const int inv = 255 - a;
				row[x*4 + 0] = (uint8_t)((src[x*4 + 2] * a + row[x*4 + 0] * inv) / 255);
				row[x*4 + 1] = (uint8_t)((src[x*4 + 1] * a + row[x*4 + 1] * inv) / 255);
				row[x*4 + 2] = (uint8_t)((src[x*4 + 0] * a + row[x*4 + 2] * inv) / 255);
				row[x*4 + 3] = 255;
			}
		}
	}
	unlock();

	/* Perf log is opt-in via JS-side calypso_set_profile_globe(1)
	 * (EmscriptenHarness).  Production builds never call the setter so
	 * g_calypsoProfileGlobe stays 0, perfTimer was never started, and
	 * the entire branch below is skipped — zero clock reads, zero
	 * accumulator math, zero log output. */
	if (profileGlobe)
	{
		perfTimer.stop();
		static long long s_accumUs = 0;
		static unsigned  s_frameCount = 0;
		s_accumUs += perfTimer.elapsedUs();
		const unsigned BATCH = 30u;
		if (++s_frameCount >= BATCH)
		{
			Log(LOG_INFO) << "Globe::drawSphereGPU avg: "
			              << (s_accumUs / (long long)s_frameCount) << " us/frame"
			              << " (" << w << "x" << h << ", n=" << s_frameCount
			              << ", readback included)";
			s_accumUs    = 0;
			s_frameCount = 0;
		}
	}
}
#endif /* __EMSCRIPTEN__ */

/**
 * Draws the whole globe, part by part.
 * When globeTextures are loaded (HD mod active), drawSphereGPU() replaces
 * drawOcean()+drawLand()+drawShadow().  All CPU overlay passes (radars,
 * flights, markers, detail/polylines) continue to run on top.
 */
void Globe::draw()
{
	if (_redraw)
	{
		cachePolygons();
	}
	Surface::draw();
#ifdef __EMSCRIPTEN__
	if (_game->getMod()->hasGlobeTextures())
	{
        if (_gpuDirectMode)
        {
            clear(); // overlay-only surface; sphere arrives via PreComposite pass
        }
        else
        {
            drawHDStarfield();
            drawSphereGPU(); /* renders sphere + reads back; CPU overlays follow */
        }
	}
	else
#endif
	{
		drawOcean();
		drawLand();
	}
	drawRadars();
	drawFlights();
#ifdef __EMSCRIPTEN__
	if (!_game->getMod()->hasGlobeTextures())
#endif
	{
		drawShadow(); /* GPU path handles terminator in shader */
	}
	drawMarkers();
	drawDetail();
#ifdef __EMSCRIPTEN__
	if (_gpuDirectMode)
		CalypsoGeoscapeHdGlobeDirect::commitLabelIconSnapshot(this);
#endif
}


/**
 * Renders the ocean, shading it according to the time of day.
 */
void Globe::drawOcean()
{
	lock();
	drawCircle(_cenX+1, _cenY, _radius+20, OCEAN_COLOR);
//	ShaderDraw<Ocean>(ShaderSurface(this));
	unlock();
}




/**
 * Renders the land, taking all the visible world polygons
 * and texturing and shading them accordingly.
 */
void Globe::drawLand()
{
	Sint16 x[4], y[4];

	for (auto* polygon : _cacheLand)
	{
		// Convert coordinates
		for (int j = 0; j < polygon->getPoints(); ++j)
		{
			x[j] = polygon->getX(j);
			y[j] = polygon->getY(j);
		}

		// Apply textures according to zoom and shade
		drawTexturedPolygon(x, y, polygon->getPoints(), _texture->getFrame(polygon->getTexture() + _zoomTexture), 0, 0);
	}
}

/**
 * Get position of sun from point on globe
 * @param lon longitude of position
 * @param lat latitude of position
 * @return position of sun
 */
Cord Globe::getSunDirection(double lon, double lat) const
{
	const double curTime = _game->getSavedGame()->getTime()->getDaylight();
	const double rot = curTime * 2*M_PI;
	double sun;

	if (Options::globeSeasons)
	{
		const int MonthDays1[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365};
		const int MonthDays2[] = {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366};

		int year=_game->getSavedGame()->getTime()->getYear();
		int month=_game->getSavedGame()->getTime()->getMonth()-1;
		int day=_game->getSavedGame()->getTime()->getDay()-1;

		double tm = (double)(( _game->getSavedGame()->getTime()->getHour() * 60
			+ _game->getSavedGame()->getTime()->getMinute() ) * 60
			+ _game->getSavedGame()->getTime()->getSecond() ) / 86400; //day fraction is also taken into account

		double CurDay;
		if (year%4 == 0 && !(year%100 == 0 && year%400 != 0))
			CurDay = (MonthDays2[month] + day + tm )/366 - 0.219; //spring equinox (start of astronomic year)
		else
			CurDay = (MonthDays1[month] + day + tm )/365 - 0.219;
		if (CurDay<0) CurDay += 1.;

		sun = -0.261 * sin(CurDay*2*M_PI);
	}
	else
		sun = 0;

	Cord sun_direction(cos(rot+lon), sin(rot+lon)*-sin(lat), sin(rot+lon)*cos(lat));

	Cord pole(0, cos(lat), sin(lat));

	if (sun>0)
		 sun_direction *= 1. - sun;
	else
		 sun_direction *= 1. + sun;

	pole *= sun;
	sun_direction += pole;
	double norm = sun_direction.norm();
	//norm should be always greater than 0
	norm = 1./norm;
	sun_direction *= norm;
	return sun_direction;
}


void Globe::drawShadow()
{
	ShaderRepeat<Sint16> noise(SurfaceRaw<Sint16>(
		static_data.random_noise, static_data.random_surf_size, static_data.random_surf_size));
	lock();
	if (_zoom < _earthData.size() && !_earthData[_zoom].empty())
	{
		// Cached: surface normal precomputed per zoom in rebuildEarthData().
		ShaderDraw<CreateShadow32>(
			ShaderSurface32(this),
			ShaderSurface(SurfaceRaw<const Cord>(_earthData[_zoom], getWidth(), getHeight())),
			ShaderScalar(getSunDirection(_cenLon, _cenLat)),
			noise);
	}
	else
	{
		// Fallback: per-pixel recompute (cache empty or zoom out-of-bounds).
		ShaderDraw<CreateShadowWithoutCache32>(
			ShaderSurface32(this),
			helper::Offset(_cenX, _cenY),
			ShaderScalar(getSunDirection(_cenLon, _cenLat)),
			noise,
			ShaderScalar(_zoomRadius[_zoom]));
	}
	unlock();
}


void Globe::XuLine(Surface* surface, Surface* src, double x1, double y1, double x2, double y2, int shade)
{
	if (_clipper->LineClip(&x1,&y1,&x2,&y2) != 1) return; //empty line

	double deltax = x2-x1, deltay = y2-y1;
	bool inv;
	Sint16 tcol;
	double len,x0,y0,SX,SY;
	if (abs((int)y2-(int)y1) > abs((int)x2-(int)x1))
	{
		len=abs((int)y2-(int)y1);
		inv=false;
	}
	else
	{
		len=abs((int)x2-(int)x1);
		inv=true;
	}

	if (y2 < y1) {
		SY = -1;
	}
	else if (AreSame(deltay, 0.0)) {
		SY = 0;
	}
	else {
		SY = 1;
	}

	if (x2 < x1) {
		SX = -1;
	}
	else if (AreSame(deltax, 0.0)) {
		SX = 0;
	}
	else {
		SX = 1;
	}

	x0=x1;  y0=y1;
	if (inv)
		SY=(deltay/len);
	else
		SX=(deltax/len);

	while (len>0)
	{
		tcol=src->getPixel((int)x0,(int)y0);
		if (tcol)
		{
			if (CreateShadow::isOcean(tcol))
			{
				tcol = CreateShadow::getOceanShadow(shade + 8);
			}
			else
			{
				tcol = CreateShadow::getLandShadow(tcol, shade * 3);
			}
			surface->setPixel((int)x0,(int)y0,tcol);
		}
		x0+=SX;
		y0+=SY;
		len-=1.0;
	}
}

/**
 * Draws the radar ranges of player bases, player craft, alien bases and UFO hunter-killers on the globe.
 */
void Globe::drawRadars()
{
	_radars->clear();
#ifdef __EMSCRIPTEN__
	if (_gpuDirectMode)
	{
		_coloredLineBatch.clearCommands();
		_gpuRadarFlightCapacityExceeded = false;
	}
#endif

	if (!Options::globeRadarLines)
		return;

	double tr, range;
	double lat, lon;
	std::vector<double> ranges;
#ifdef __EMSCRIPTEN__
	/* SS15.4.4: reserve the seen-range storage from the loaded facility count
	 * before the hot path; canonicalization below never grows it. */
	ranges.reserve(_game->getMod()->getBaseFacilitiesList().size());
#endif

	_radars->lock();

	// Draw craft range
	if (_craft)
	{
		if (_craftRange < M_PI)
		{
			drawGlobeCircle(_craftLat, _craftLon, _craftRange, 64);
			drawGlobeCircle(_craftLat, _craftLon, _craftRange - 0.025, 64, 2);
		}
	}

	if (_hover)
	{
#ifdef __EMSCRIPTEN__
		/* SS15.4.4 New Base canonicalization (permanent model cleanup): drop
		 * non-positive ranges and keep the first occurrence of each exact
		 * range in source order; distinct ranges stay visible. */
		for (auto& facType : _game->getMod()->getBaseFacilitiesList())
		{
			ranges.push_back(Nautical(_game->getMod()->getBaseFacility(facType)->getRadarRange()));
		}
		const size_t distinctRanges = ranges.empty()
			? 0u
			: OpenXcom::Calypso::calypsoCanonicalizeHoverRanges(&ranges[0], ranges.size());
		ranges.resize(distinctRanges);
		for (size_t j = 0; j < ranges.size(); ++j)
			drawGlobeCircle(_hoverLat, _hoverLon, ranges[j], 48);
#else
		for (auto& facType : _game->getMod()->getBaseFacilitiesList())
		{
			range = Nautical(_game->getMod()->getBaseFacility(facType)->getRadarRange());
			drawGlobeCircle(_hoverLat,_hoverLon,range,48);
			if (Options::globeAllRadarsOnBaseBuild) ranges.push_back(range);
		}
#endif
	}

	// Draw radars around bases
	for (auto* xbase : *_game->getSavedGame()->getBases())
	{
		lat = xbase->getLatitude();
		lon = xbase->getLongitude();
		// Cheap hack to hide bases when they haven't been placed yet
		if (( !(AreSame(lon, 0.0) && AreSame(lat, 0.0)) )/* &&
			!pointBack(xbase->getLongitude(), xbase->getLatitude())*/)
		{
			if (_hover && Options::globeAllRadarsOnBaseBuild)
			{
				for (size_t j=0; j<ranges.size(); j++) drawGlobeCircle(lat,lon,ranges[j],48);
			}
			else
			{
				range = 0;
				for (auto* fac : *xbase->getFacilities())
				{
					if (fac->getBuildTime() == 0)
					{
						tr = fac->getRules()->getRadarRange();
						if (tr < MAX_DRAW_RADAR_CIRCLE_RADIUS && tr > range) range = tr;
					}
				}
				range = Nautical(range);

				if (range>0) drawGlobeCircle(lat,lon,range,48);
			}

		}

		// Draw radars around player craft
		for (auto* xcraft : *xbase->getCrafts())
		{
			if (xcraft->getStatus() != "STR_OUT")
				continue;
			lat = xcraft->getLatitude();
			lon = xcraft->getLongitude();
			range = Nautical(xcraft->getCraftStats().radarRange);

			if (range>0) drawGlobeCircle(lat,lon,range,24);
		}
	}

	if (_game->getMod()->getDrawEnemyRadarCircles() > 0)
	{
		// Draw radars around UFO hunter-killers
		for (auto* ufo : *_game->getSavedGame()->getUfos())
		{
			if (ufo->isHunterKiller() && ufo->getDetected() && ufo->getStatus() != Ufo::IGNORE_ME)
			{
				if (_game->getMod()->getDrawEnemyRadarCircles() == 1 && !ufo->getHyperDetected())
				{
					continue;
				}
				lat = ufo->getLatitude();
				lon = ufo->getLongitude();
				range = Nautical(ufo->getCraftStats().radarRange);

				if (range > 0) drawGlobeCircle(lat, lon, range, 24);
			}
		}

		// Draw radars around alien bases
		for (auto* ab : *_game->getSavedGame()->getAlienBases())
		{
			if (ab->getDeployment()->getBaseDetectionRange() > 0 && ab->isDiscovered())
			{
				lat = ab->getLatitude();
				lon = ab->getLongitude();
				range = Nautical(ab->getDeployment()->getBaseDetectionRange());

				if (range > 0) drawGlobeCircle(lat, lon, range, 24);
			}
		}
	}

	_radars->unlock();
}

/**
 *	Draw globe range circle
 */
void Globe::drawGlobeCircle(double lat, double lon, double radius, int segments, int frac)
{
	double x, y, x2 = 0, y2 = 0;
	double lat1, lon1, lat2 = 0, lon2 = 0;
	double seg = M_PI / (static_cast<double>(segments) / 2);
	int i = 0;
	for (double az = 0; az <= M_PI*2+0.01; az+=seg) //48 circle segments
	{
		//calculating sphere-projected circle
		lat1 = asin(sin(lat) * cos(radius) + cos(lat) * sin(radius) * cos(az));
		lon1 = lon + atan2(sin(az) * sin(radius) * cos(lat), cos(radius) - sin(lat) * sin(lat1));
		polarToCart(lon1, lat1, &x, &y);
		if ( AreSame(az, 0.0) ) //first vertex is for initialization only
		{
			x2=x;
			y2=y;
			lon2=lon1;
			lat2=lat1;
			continue;
		}
		if (i % frac == 0)
		{
#ifdef __EMSCRIPTEN__
			if (_gpuDirectMode)
				CalypsoGeoscapeHdGlobeDirect::recordRadarFlightLine(
					this, x, y, x2, y2, lon1, lat1, lon2, lat2, 6);
			else
#endif
			if (!pointBack(lon1,lat1))
				XuLine(_radars, this, x, y, x2, y2, 6);
		}
		x2=x; y2=y; lon2=lon1; lat2=lat1;
		i++;
	}
}

void Globe::setNewBaseHover(bool hover)
{
	_hover=hover;
}

void Globe::setNewBaseHoverPos(double lon, double lat)
{
	_hoverLon=lon;
	_hoverLat=lat;
}

void Globe::drawVHLine(Surface *surface, double lon1, double lat1, double lon2, double lat2, Uint8 color)
{
#ifdef __EMSCRIPTEN__
	if (_gpuDirectMode && surface == _countries)
	{
		CalypsoGeoscapeHdGlobeDirect::recordDebugLine(this, lon1, lat1, lon2, lat2, color);
		return;
	}
#endif
	double sx = lon2 - lon1;
	double sy = lat2 - lat1;
	double ln1, lt1, ln2, lt2;
	int seg;
	Sint16 x1, y1, x2, y2;

	if (sx<0) sx += 2*M_PI;

	if (fabs(sx)<0.01)
	{
		seg = std::abs(sy/(2*M_PI)*48);
		if (seg == 0) ++seg;
	}
	else
	{
		seg = std::abs(sx/(2*M_PI)*96);
		if (seg == 0) ++seg;
	}

	sx /= seg;
	sy /= seg;

	for (int i = 0; i < seg; ++i)
	{
		ln1 = lon1 + sx*i;
		lt1 = lat1 + sy*i;
		ln2 = lon1 + sx*(i+1);
		lt2 = lat1 + sy*(i+1);

		if (!pointBack(ln2, lt2)&&!pointBack(ln1, lt1))
		{
			polarToCart(ln1,lt1,&x1,&y1);
			polarToCart(ln2,lt2,&x2,&y2);
			surface->drawLine(x1, y1, x2, y2, color);
		}
	}
}


/**
 * Draws the details of the countries on the globe,
 * based on the current zoom level.
 */
void Globe::drawDetail()
{
	_countries->clear();
#ifdef __EMSCRIPTEN__
	if (_gpuDirectMode)
	{
		_gpuBorderLines.clear();
		_gpuDebugLines.clear();
		_gpuDebugVertices.clear();
		_gpuLabelIconPendingDraws.clear();
		_gpuLabelCapacityExceeded = false;
		_gpuBorderCapacityExceeded = false;
		_gpuDebugCapacityExceeded = false;
		_gpuLogicalWorldComplete = true;
	}
#endif

	if (!Options::globeDetail)
	{
		return;
	}

	// Draw the country borders
	if (_zoom >= 1)
	{
		// Lock the surface
		_countries->lock();

		for (auto* polyline : *_rules->getPolylines())
		{
			Sint16 x[2], y[2];
			for (int j = 0; j < polyline->getPoints() - 1; ++j)
			{
				// Don't draw if polyline is facing back
				if (pointBack(polyline->getLongitude(j), polyline->getLatitude(j)) || pointBack(polyline->getLongitude(j + 1), polyline->getLatitude(j + 1)))
					continue;

				// Convert coordinates
				polarToCart(polyline->getLongitude(j), polyline->getLatitude(j), &x[0], &y[0]);
				polarToCart(polyline->getLongitude(j + 1), polyline->getLatitude(j + 1), &x[1], &y[1]);

#ifdef __EMSCRIPTEN__
				if (_gpuDirectMode)
					CalypsoGeoscapeHdGlobeDirect::recordBorderLine(this, x[0], y[0], x[1], y[1]);
				else
#endif
					_countries->drawLine(x[0], y[0], x[1], y[1], LINE_COLOR);
			}
		}

		// Unlock the surface
		_countries->unlock();
	}

	// Draw the country names
	if (_zoom >= 2)
	{
		Text *label = nullptr;
#ifdef __EMSCRIPTEN__
		if (!_gpuDirectMode)
#endif
		{
			label = new Text(150, 9, 0, 0);
			label->setPalette(getEffectivePalette());
			label->initText(_game->getMod()->getFont("FONT_BIG"), _game->getMod()->getFont("FONT_SMALL"), _game->getLanguage());
			label->setAlign(ALIGN_CENTER);
		}

		Sint16 x, y;
		for (auto* country : *_game->getSavedGame()->getCountries())
		{
			// Don't draw if label is facing back
			if (pointBack(country->getRules()->getLabelLongitude(), country->getRules()->getLabelLatitude()))
				continue;

			// Convert coordinates
			polarToCart(country->getRules()->getLabelLongitude(), country->getRules()->getLabelLatitude(), &x, &y);

			if (label)
			{
				label->setX(x - 75);
				label->setY(y);
				label->setText(_game->getLanguage()->getString(country->getRules()->getType()));
				label->setColor(COUNTRY_LABEL_COLOR);
				if (country->getRules()->getLabelColor() > 0)
					label->setColor(country->getRules()->getLabelColor());
			}
#ifdef __EMSCRIPTEN__
			if (_gpuDirectMode)
				CalypsoGeoscapeHdGlobeDirect::recordLabelText(this,
					_game->getLanguage()->getString(country->getRules()->getType()), 150, 9,
					x - 75, y, country->getRules()->getLabelColor() > 0
						? country->getRules()->getLabelColor() : COUNTRY_LABEL_COLOR);
			else
#endif
				label->blit(_countries->getSurface());
		}

		delete label;
	}

	// Draw extra globe labels
	{
		Text *label = nullptr;
#ifdef __EMSCRIPTEN__
		if (!_gpuDirectMode)
#endif
		{
			label = new Text(120, 18, 0, 0);
			label->setPalette(getEffectivePalette());
			label->initText(_game->getMod()->getFont("FONT_BIG"), _game->getMod()->getFont("FONT_SMALL"), _game->getLanguage());
			label->setAlign(ALIGN_CENTER);
		}

		Sint16 x, y;
		for (auto& extraLabelType : _game->getMod()->getExtraGlobeLabelsList())
		{
			RuleCountry *rule = _game->getMod()->getExtraGlobeLabel(extraLabelType, true);
			if ((int)(_zoom) >= rule->getZoomLevel())
			{
				// Don't draw if label is facing back
				if (pointBack(rule->getLabelLongitude(), rule->getLabelLatitude()))
					continue;

				// Convert coordinates
				polarToCart(rule->getLabelLongitude(), rule->getLabelLatitude(), &x, &y);
				if (label)
				{
					label->setX(x - 60);
					label->setY(y);
					label->setText(_game->getLanguage()->getString(rule->getType()));
					label->setColor(COUNTRY_LABEL_COLOR);
					if (rule->getLabelColor() > 0)
						label->setColor(rule->getLabelColor());
				}
#ifdef __EMSCRIPTEN__
				if (_gpuDirectMode)
					CalypsoGeoscapeHdGlobeDirect::recordLabelText(this,
						_game->getLanguage()->getString(rule->getType()), 120, 18,
						x - 60, y, rule->getLabelColor() > 0
							? rule->getLabelColor() : COUNTRY_LABEL_COLOR);
				else
#endif
					label->blit(_countries->getSurface());
			}
		}
		delete label;
	}

	// Draw the city and base markers
	if (_zoom >= 3)
	{
		Text *label = nullptr;
#ifdef __EMSCRIPTEN__
		if (!_gpuDirectMode)
#endif
		{
			label = new Text(100, 9, 0, 0);
			label->setPalette(getEffectivePalette());
			label->initText(_game->getMod()->getFont("FONT_BIG"), _game->getMod()->getFont("FONT_SMALL"), _game->getLanguage());
			label->setAlign(ALIGN_CENTER);
			label->setColor(CITY_LABEL_COLOR);
		}

		Sint16 x, y;
		for (auto* region : *_game->getSavedGame()->getRegions())
		{
			for (auto* city : *region->getRules()->getCities())
			{
				drawTarget(city, _countries);

				// Don't draw if city is facing back
				if (pointBack(city->getLongitude(), city->getLatitude()))
					continue;

				// Convert coordinates
				polarToCart(city->getLongitude(), city->getLatitude(), &x, &y);

				if (label)
				{
					label->setX(x - 50);
					label->setY(y + 2);
					label->setText(city->getName(_game->getLanguage()));
				}
#ifdef __EMSCRIPTEN__
				if (_gpuDirectMode)
					CalypsoGeoscapeHdGlobeDirect::recordLabelText(this,
						city->getName(_game->getLanguage()), 100, 9, x - 50, y + 2, CITY_LABEL_COLOR);
				else
#endif
					label->blit(_countries->getSurface());
			}
		}
		// Draw bases names
		for (auto* xbase : *_game->getSavedGame()->getBases())
		{
			if (xbase->getMarker() == -1 || pointBack(xbase->getLongitude(), xbase->getLatitude()))
				continue;
			polarToCart(xbase->getLongitude(), xbase->getLatitude(), &x, &y);
			if (label)
			{
				label->setX(x - 50);
				label->setY(y + 2);
				label->setColor(BASE_LABEL_COLOR);
				label->setText(xbase->getName());
			}
#ifdef __EMSCRIPTEN__
			if (_gpuDirectMode)
				CalypsoGeoscapeHdGlobeDirect::recordLabelText(this,
					xbase->getName(), 100, 9, x - 50, y + 2, BASE_LABEL_COLOR);
			else
#endif
				label->blit(_countries->getSurface());
		}

		delete label;
	}

	int& debugType = _game->getSavedGame()->debugType;
	static bool canSwitchDebugType = false;
	/* Stage 13 QA (loopback-only): the loopback capture switch can ADD debug
	 * geometry presentation while the save is non-debug. The saved-game debug
	 * mode stays the sole canonical owner: nothing below mutates SavedGame,
	 * and forced-on neither suppresses nor rewrites canonical bookkeeping. */
	const bool qaDebugGeometry =
		Calypso::calypsoGeoscapeQaDebugGeometry(Calypso::calypsoGeoscapeQaPresentation(), false);
	if (_game->getSavedGame()->getDebugMode())
	{
		canSwitchDebugType = true;
		drawDebugRectangles(debugType);
	}
	else if (qaDebugGeometry)
	{
		/* Presentation-only QA arm: same canonical rectangle owner, zero
		 * campaign bookkeeping. */
		drawDebugRectangles(debugType);
	}
	else
	{
		if (canSwitchDebugType)
		{
			++debugType;
			if (debugType > 2) debugType = 0;
			canSwitchDebugType = false;
		}
	}
}

/**
 * Draws the canonical debug country/region/mission-zone rectangles for one
 * debugType. Shared verbatim owner of the saved-game-debug gate and the
 * loopback-only Stage 13 QA capture switch: callers decide visibility, this
 * function only draws and never touches SavedGame state.
 * Direct mode records these existing debug owners into the physical world
 * batch through drawVHLine(); direct-off remains SDL-native.
 */
void Globe::drawDebugRectangles(int debugType)
{
	int color;
	if (debugType == 0)
	{
		color = 0;
		for (auto* country : *_game->getSavedGame()->getCountries())
		{
			if (_game->getSavedGame()->debugCountry && _game->getSavedGame()->debugCountry != country)
				continue;

			color += 10;
			for (size_t k = 0; k != country->getRules()->getLatMax().size(); ++k)
			{
				double lon2 = country->getRules()->getLonMax().at(k);
				double lon1 = country->getRules()->getLonMin().at(k);
				double lat2 = country->getRules()->getLatMax().at(k);
				double lat1 = country->getRules()->getLatMin().at(k);

				drawVHLine(_countries, lon1, lat1, lon2, lat1, color);
				drawVHLine(_countries, lon1, lat2, lon2, lat2, color);
				drawVHLine(_countries, lon1, lat1, lon1, lat2, color);
				drawVHLine(_countries, lon2, lat1, lon2, lat2, color);
			}
		}
	}
	else if (debugType == 1)
	{
		color = 0;
		for (auto* region : *_game->getSavedGame()->getRegions())
		{
			if (_game->getSavedGame()->debugRegion && _game->getSavedGame()->debugRegion != region)
				continue;

			color += 10;
			for (size_t k = 0; k != region->getRules()->getLatMax().size(); ++k)
			{
				double lon2 = region->getRules()->getLonMax().at(k);
				double lon1 = region->getRules()->getLonMin().at(k);
				double lat2 = region->getRules()->getLatMax().at(k);
				double lat1 = region->getRules()->getLatMin().at(k);

				drawVHLine(_countries, lon1, lat1, lon2, lat1, color);
				drawVHLine(_countries, lon1, lat2, lon2, lat2, color);
				drawVHLine(_countries, lon1, lat1, lon1, lat2, color);
				drawVHLine(_countries, lon2, lat1, lon2, lat2, color);
			}
		}
	}
	else if (debugType == 2)
	{
		for (auto* region : *_game->getSavedGame()->getRegions())
		{
			if (_game->getSavedGame()->debugRegion && _game->getSavedGame()->debugRegion != region)
				continue;

			color = -1;
			size_t zoneNumber = 0;
			for (const auto& missionZone : region->getRules()->getMissionZones())
			{
				++zoneNumber;
				if (_game->getSavedGame()->debugZone > 0 && _game->getSavedGame()->debugZone != zoneNumber)
					continue;

				color += 2;
				size_t areaNumber = 0;
				for (const auto& missionArea : missionZone.areas)
				{
					++areaNumber;
					if (_game->getSavedGame()->debugArea > 0 && _game->getSavedGame()->debugArea != areaNumber)
						continue;

					double lon2 = missionArea.lonMax;
					double lon1 = missionArea.lonMin;
					double lat2 = missionArea.latMax;
					double lat1 = missionArea.latMin;

					drawVHLine(_countries, lon1, lat1, lon2, lat1, color);
					drawVHLine(_countries, lon1, lat2, lon2, lat2, color);
					drawVHLine(_countries, lon1, lat1, lon1, lat2, color);
					drawVHLine(_countries, lon2, lat1, lon2, lat2, color);
				}
			}
		}
	}
}

void Globe::drawPath(Surface *surface, double lon1, double lat1, double lon2, double lat2)
{
	double length;
	Sint16 count;
	double x1, y1, x2, y2;
	CordPolar p1, p2;
	Cord a(CordPolar(lon1, lat1));
	Cord b(CordPolar(lon2, lat2));

	if (-b == a)
		return;

	b -= a;

	//longer path have more parts
	length = b.norm();
	length *= length*15;
	count = length + 1;
	b /= count;
	p1 = CordPolar(a);
	polarToCart(p1.lon, p1.lat, &x1, &y1);
	for (int i = 0; i < count; ++i)
	{
		a += b;
		p2 = CordPolar(a);
		polarToCart(p2.lon, p2.lat, &x2, &y2);

#ifdef __EMSCRIPTEN__
		if (_gpuDirectMode && surface == _radars)
		{
			CalypsoGeoscapeHdGlobeDirect::recordRadarFlightLine(
				this, x1, y1, x2, y2, p1.lon, p1.lat, p2.lon, p2.lat, 8);
		}
		else
#endif
		if (!pointBack(p1.lon, p1.lat) && !pointBack(p2.lon, p2.lat))
				XuLine(surface, this, x1, y1, x2, y2, 8);

		p1 = p2;
		x1 = x2;
		y1 = y2;
	}
}

/**
 * Draws the flight paths of player craft (and hunting UFOs) flying on the globe.
 */
void Globe::drawFlights()
{
	//_radars->clear();

	if (!Options::globeFlightPaths)
		return;

	// Lock the surface
	_radars->lock();

	// Draw the craft flight paths
	for (auto* xbase : *_game->getSavedGame()->getBases())
	{
		for (auto* xcraft : *xbase->getCrafts())
		{
			// Hide crafts docked at base
			if (xcraft->getStatus() != "STR_OUT" || xcraft->getDestination() == 0 /*|| pointBack(xcraft->getLongitude(), xcraft->getLatitude())*/)
				continue;

			double lon1 = xcraft->getLongitude();
			double lat1 = xcraft->getLatitude();
			double lon2 = xcraft->getDestination()->getLongitude();
			double lat2 = xcraft->getDestination()->getLatitude();

			if (xcraft->isMeetCalculated())
			{
				lon2 = xcraft->getMeetLongitude();
				lat2 = xcraft->getMeetLatitude();
			}
			drawPath(_radars, lon1, lat1, lon2, lat2);

			if (xcraft->isMeetCalculated())
			{
				lon1 = xcraft->getDestination()->getLongitude();
				lat1 = xcraft->getDestination()->getLatitude();

				drawPath(_radars, lon1, lat1, lon2, lat2);
			}
		}
	}

	// Draw the hunting UFO flight paths
	for (auto* ufo : *_game->getSavedGame()->getUfos())
	{
		if (ufo->getDestination() && (ufo->isHunting() || _game->getSavedGame()->getDebugMode()) && ufo->getDetected() && ufo->getStatus() != Ufo::IGNORE_ME)
		{
			double lon1 = ufo->getLongitude();
			double lon2 = ufo->getDestination()->getLongitude();
			double lat1 = ufo->getLatitude();
			double lat2 = ufo->getDestination()->getLatitude();

			drawPath(_radars, lon1, lat1, lon2, lat2);
		}
	}

	/* SS15.4.5: label/icon publication moved out of drawFlights(); it is
	 * committed explicitly once per frame after drawDetail() records. */

	// Unlock the surface
	_radars->unlock();
}

/**
 * Draws the marker for a specified target on the globe.
 * @param target Pointer to globe target.
 */
void Globe::drawTarget(Target *target, Surface *surface)
{
	if (target->getMarker() != -1 && !pointBack(target->getLongitude(), target->getLatitude()))
	{
		Sint16 x, y;
		polarToCart(target->getLongitude(), target->getLatitude(), &x, &y);
		auto i = target->getMarker();
		auto marker = _markerSet->getFrame(i);
		// Classic OXCE used a +1 palette-index shift on _blink to give markers a
		// subtle two-frame colour cycle.  The pre-ARGB code skipped rendering on
		// blink-off, which made markers flicker on/off — that's far more
		// distracting than the original gentle pulse.  Approximate the original
		// effect by varying the shade attenuation in blitNShade instead, so the
		// marker stays continuously visible but gets a tiny brightness bob.
		const int shade = (_blink > 0) ? 0 : 1;
#ifdef __EMSCRIPTEN__
		if (_gpuDirectMode && surface == _markers)
		{
			CalypsoGeoscapeHdGlobeDirect::recordMarker(this, marker,
					x - marker->getWidth() / 2, y - marker->getHeight() / 2, shade);
			return;
		}
		if (_gpuDirectMode && surface == _countries)
		{
			CalypsoGeoscapeHdGlobeDirect::recordLabelIcon(this, marker,
				x - marker->getWidth() / 2, y - marker->getHeight() / 2, shade);
			return;
		}
#endif
		marker->blitNShade(SurfaceRaw<Uint32>(surface), x - marker->getWidth() / 2, y - marker->getHeight() / 2, shade);
	}
}

/**
 * Draws the markers of all the various things going
 * on around the world on top of the globe.
 */
void Globe::drawMarkers()
{
	_markers->clear();
#ifdef __EMSCRIPTEN__
	if (_gpuDirectMode) _gpuMarkerPendingDraws.clear();
#endif
	_markers->lock();
	// Draw the base markers
	for (auto* xbase : *_game->getSavedGame()->getBases())
	{
		drawTarget(xbase, _markers);
	}

	// Draw the waypoint markers
	for (auto* wp : *_game->getSavedGame()->getWaypoints())
	{
		drawTarget(wp, _markers);
	}

	// Draw the mission site markers
	for (auto* site : *_game->getSavedGame()->getMissionSites())
	{
		drawTarget(site, _markers);
	}

	// Draw the alien base markers
	for (auto* ab : *_game->getSavedGame()->getAlienBases())
	{
		drawTarget(ab, _markers);
	}

	// Draw the UFO markers
	for (auto* ufo : *_game->getSavedGame()->getUfos())
	{
		if (ufo->getStatus() == Ufo::IGNORE_ME) continue;
		drawTarget(ufo, _markers);
	}

	// Draw the craft markers
	for (auto* xbase : *_game->getSavedGame()->getBases())
	{
		for (auto* xcraft : *xbase->getCrafts())
		{
			drawTarget(xcraft, _markers);
		}
	}
	_markers->unlock();
#ifdef __EMSCRIPTEN__
	if (_gpuDirectMode)
	{
		_gpuMarkerPendingDraws.swap(_gpuMarkerCommittedDraws);
		_gpuMarkerPendingDraws.clear();
	}
#endif
}

/**
 * Blits the globe onto another surface.
 * @param surface Pointer to another surface.
 */
void Globe::blit(SDL_Surface *surface)
{
	Surface::blit(surface);
#ifdef __EMSCRIPTEN__
	if (_gpuDirectMode)
		return; // all visible overlays must be physical or the route fails closed before Earth
#endif
	_radars->blit(surface);
	_countries->blit(surface);
#ifdef __EMSCRIPTEN__
	if (!_gpuDirectMode)
#endif
	{
		_markers->blit(surface);
	}
}

/**
 * Ignores any mouse hovers that are outside the globe.
 * @param action Pointer to an action.
 * @param state State that the action handlers belong to.
 */
void Globe::mouseOver(Action *action, State *state)
{
	double lon, lat;
	cartToPolar((Sint16)floor(action->getAbsoluteXMouse()), (Sint16)floor(action->getAbsoluteYMouse()), &lon, &lat);

	if (_isMouseScrolling && action->getDetails()->type == SDL_MOUSEMOTION)
	{
		// The following is the workaround for a rare problem where sometimes
		// the mouse-release event is missed for any reason.
		// (checking: is the dragScroll-mouse-button still pressed?)
		// However if the SDL is also missed the release event, then it is to no avail :(
		if (!isGlobePanButtonPressed())
		{ // so we missed again the mouse-release :(
			// Check if we have to revoke the scrolling, because it was too short in time, so it was a click
			if ((!_mouseMovedOverThreshold) && ((int)(SDL_GetTicks() - _mouseScrollingStartTime) <= (Options::dragScrollTimeTolerance)))
			{
				center(_lonBeforeMouseScrolling, _latBeforeMouseScrolling);
			}
			_isMouseScrolled = _isMouseScrolling = false;
			stopScrolling(action);
			return;
		}

		_isMouseScrolled = true;

#ifndef __EMSCRIPTEN__
		if (Options::touchEnabled == false)
		{
			// Set the mouse cursor back
			SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);
			SDL_WarpMouse((_game->getScreen()->getWidth() - 100) / 2 , _game->getScreen()->getHeight() / 2);
			SDL_EventState(SDL_MOUSEMOTION, SDL_ENABLE);
		}
#endif

		// Check the threshold
		_totalMouseMoveX += action->getDetails()->motion.xrel;
		_totalMouseMoveY += action->getDetails()->motion.yrel;

		if (!_mouseMovedOverThreshold)
			_mouseMovedOverThreshold = ((std::abs(_totalMouseMoveX) > Options::dragScrollPixelTolerance) || (std::abs(_totalMouseMoveY) > Options::dragScrollPixelTolerance));

		// Scrolling
		if (Options::geoDragScrollInvert)
		{
			double newLon = ((double)_totalMouseMoveX / action->getXScale()) * ROTATE_LONGITUDE/(_zoom+1)/2;
			double newLat = ((double)_totalMouseMoveY / action->getYScale()) * ROTATE_LATITUDE/(_zoom+1)/2;
			center(_lonBeforeMouseScrolling + newLon / (Options::geoScrollSpeed / 10), _latBeforeMouseScrolling + newLat / (Options::geoScrollSpeed / 10));
		}
		else
		{
			double newLon = -action->getDetails()->motion.xrel * ROTATE_LONGITUDE/(_zoom+1)/2;
			double newLat = -action->getDetails()->motion.yrel * ROTATE_LATITUDE/(_zoom+1)/2;
			center(_cenLon + newLon / (Options::geoScrollSpeed / 10), _cenLat + newLat / (Options::geoScrollSpeed / 10));
		}

#ifndef __EMSCRIPTEN__
		if (Options::touchEnabled == false)
		{
			// We don't want to see the mouse-cursor jumping :)
			action->setMouseAction(_xBeforeMouseScrolling, _yBeforeMouseScrolling, getX(), getY());
			action->getDetails()->motion.x = _xBeforeMouseScrolling; action->getDetails()->motion.y = _yBeforeMouseScrolling;
		}
#endif

		_game->getCursor()->handle(action);
	}

#ifndef __EMSCRIPTEN__
	if (Options::touchEnabled == false &&
		_isMouseScrolling &&
		(action->getDetails()->motion.x != _xBeforeMouseScrolling ||
		action->getDetails()->motion.y != _yBeforeMouseScrolling))
	{
		action->setMouseAction(_xBeforeMouseScrolling, _yBeforeMouseScrolling, getX(), getY());
		action->getDetails()->motion.x = _xBeforeMouseScrolling; action->getDetails()->motion.y = _yBeforeMouseScrolling;
	}
#endif
	// Check for errors
	if (lat == lat && lon == lon)
	{
		InteractiveSurface::mouseOver(action, state);
	}
}

/**
 * Ignores any mouse clicks that are outside the globe.
 * @param action Pointer to an action.
 * @param state State that the action handlers belong to.
 */
void Globe::mousePress(Action *action, State *state)
{
	if (action->getDetails()->button.button == SDL_BUTTON_WHEELUP)
	{
		zoomIn();
		return;
	}
	else if (action->getDetails()->button.button == SDL_BUTTON_WHEELDOWN)
	{
		zoomOut();
		return;
	}

	double lon, lat;
	cartToPolar((Sint16)floor(action->getAbsoluteXMouse()), (Sint16)floor(action->getAbsoluteYMouse()), &lon, &lat);

	if (isGlobePanButton(action->getDetails()->button.button))
	{
		_isMouseScrolling = true;
		_isMouseScrolled = false;
		SDL_GetMouseState(&_xBeforeMouseScrolling, &_yBeforeMouseScrolling);
		_lonBeforeMouseScrolling = _cenLon;
		_latBeforeMouseScrolling = _cenLat;
		_totalMouseMoveX = 0; _totalMouseMoveY = 0;
		_mouseMovedOverThreshold = false;
		_mouseScrollingStartTime = SDL_GetTicks();
	}
	// Check for errors
	if (lat == lat && lon == lon)
	{
		InteractiveSurface::mousePress(action, state);
	}
}

/**
 * Ignores any mouse clicks that are outside the globe.
 * @param action Pointer to an action.
 * @param state State that the action handlers belong to.
 */
void Globe::mouseRelease(Action *action, State *state)
{
	double lon, lat;
	cartToPolar((Sint16)floor(action->getAbsoluteXMouse()), (Sint16)floor(action->getAbsoluteYMouse()), &lon, &lat);
	if (isGlobePanButton(action->getDetails()->button.button))
	{
		stopScrolling(action);
	}
	// Check for errors
	if (lat == lat && lon == lon)
	{
		InteractiveSurface::mouseRelease(action, state);
	}
}

/**
 * Ignores any mouse clicks that are outside the globe
 * and handles globe rotation and zooming.
 * @param action Pointer to an action.
 * @param state State that the action handlers belong to.
 */
void Globe::mouseClick(Action *action, State *state)
{
	if (action->getDetails()->button.button == SDL_BUTTON_WHEELUP)
	{
		zoomIn();
	}
	else if (action->getDetails()->button.button == SDL_BUTTON_WHEELDOWN)
	{
		zoomOut();
	}

	double lon, lat;
	cartToPolar((Sint16)floor(action->getAbsoluteXMouse()), (Sint16)floor(action->getAbsoluteYMouse()), &lon, &lat);

	// The following is the workaround for a rare problem where sometimes
	// the mouse-release event is missed for any reason.
	// However if the SDL is also missed the release event, then it is to no avail :(
	// (this part handles the release if it is missed and now an other button is used)
	if (_isMouseScrolling)
	{
		if (!isGlobePanButton(action->getDetails()->button.button)
			&& !isGlobePanButtonPressed())
		{ // so we missed again the mouse-release :(
			// Check if we have to revoke the scrolling, because it was too short in time, so it was a click
			if ((!_mouseMovedOverThreshold) && ((int)(SDL_GetTicks() - _mouseScrollingStartTime) <= (Options::dragScrollTimeTolerance)))
			{
				center(_lonBeforeMouseScrolling, _latBeforeMouseScrolling);
			}
			_isMouseScrolled = _isMouseScrolling = false;
			stopScrolling(action);
		}
	}

	// DragScroll-Button release: release mouse-scroll-mode
	if (_isMouseScrolling)
	{
		// While scrolling, other buttons are ineffective
		if (isGlobePanButton(action->getDetails()->button.button))
		{
			_isMouseScrolling = false;
			stopScrolling(action);
		}
		else
		{
			return;
		}
		// Check if we have to revoke the scrolling, because it was too short in time, so it was a click
		if ((!_mouseMovedOverThreshold) && ((int)(SDL_GetTicks() - _mouseScrollingStartTime) <= (Options::dragScrollTimeTolerance)))
		{
			_isMouseScrolled = false;
			stopScrolling(action);
			center(_lonBeforeMouseScrolling, _latBeforeMouseScrolling);
		}
		if (_isMouseScrolled) return;
	}

	// Check for errors
	if (lat == lat && lon == lon)
	{
		InteractiveSurface::mouseClick(action, state);
		if (action->getDetails()->button.button == SDL_BUTTON_RIGHT)
		{
			center(lon, lat);
		}
	}
}

/**
 * Handles globe keyboard shortcuts.
 * @param action Pointer to an action.
 * @param state State that the action handlers belong to.
 */
void Globe::keyboardPress(Action *action, State *state)
{
	InteractiveSurface::keyboardPress(action, state);
	if (action->getDetails()->key.keysym.sym == Options::keyGeoToggleDetail)
	{
		toggleDetail();
	}
	if (action->getDetails()->key.keysym.sym == Options::keyGeoToggleRadar)
	{
		toggleRadarLines();
	}
}

/**
 * Get the polygons texture at a given point
 * @param lon Longitude of the point.
 * @param lat Latitude of the point.
 * @param texture pointer to texture ID returns -1 when polygon not found
 * @param shade pointer to shade
 */
void Globe::getPolygonTextureAndShade(double lon, double lat, int *texture, int *shade) const
{
	///this is shade conversion from 0..31 levels of geoscape to battlescape levels 0..15
	int worldshades[32] = {  0, 0, 0, 0, 1, 1, 2, 2,
							 3, 3, 4, 4, 5, 5, 6, 6,
							 7, 7, 8, 8, 9, 9,10,11,
							11,12,12,13,13,14,15,15};

	*shade = worldshades[ CreateShadow::getShadowValue(Cord(0.,0.,1.), getSunDirection(lon, lat), 0) ];
	Polygon *t = getPolygonFromLonLat(lon,lat);
	*texture = (t==NULL)? -1 : t->getTexture();
}

/**
 * Returns the current globe zoom factor.
 * @return Current zoom (0-5).
 */
size_t Globe::getZoom() const
{
	return _zoom;
}

/*
 * Turns Radar lines on or off.
 */
void Globe::toggleRadarLines()
{
	Options::globeRadarLines = !Options::globeRadarLines;
	drawRadars();
}

/*
 * Resizes the geoscape.
 */
void Globe::resize()
{
	Surface *surfaces[4] = {this, _markers, _countries, _radars};
	int width = Options::baseXGeoscape - 64;
	int height = Options::baseYGeoscape;

	for (int i = 0; i < 4; ++i)
	{
		surfaces[i]->setWidth(width);
		surfaces[i]->setHeight(height);
		surfaces[i]->invalidate();
	}
	_clipper->Wxrig = width;
	_clipper->Wybot = height;
	_cenX = width / 2;
	_cenY = height / 2;
	setupRadii(width, height);
	invalidate();
}

/*
 * Set up the Radius of earth at the various zoom levels.
 * @param width the new width of the globe.
 * @param height the new height of the globe.
 */
void Globe::setupRadii(int width, int height)
{
	_zoomRadius.clear();

	_zoomRadius.push_back(0.45*height);
	_zoomRadius.push_back(0.60*height);
	_zoomRadius.push_back(0.90*height);
	_zoomRadius.push_back(1.40*height);
	_zoomRadius.push_back(2.25*height);
	_zoomRadius.push_back(3.60*height);

	_radius = _zoomRadius[_zoom];
	_radiusStep = (_zoomRadius[DOGFIGHT_ZOOM] - _zoomRadius[0]) / 10.0;

	if (Options::globeSurfaceCache)
	{
		rebuildEarthData();
	}
	else
	{
		_earthData.clear();
	}
}

void Globe::rebuildEarthData()
{
	const int w = getWidth(), h = getHeight();
	_earthData.assign(_zoomRadius.size(), {});
	for (size_t r = 0; r < _zoomRadius.size(); ++r)
	{
		_earthData[r].resize(w * h);
		for (int j = 0; j < h; ++j)
			for (int i = 0; i < w; ++i)
				_earthData[r][w*j + i] = static_data.circle_norm(w/2, h/2, _zoomRadius[r], i+.5, j+.5);
	}
}

/**
 * Move the mouse back to where it started after we finish drag scrolling.
 * @param action Pointer to an action.
 */
void Globe::stopScrolling(Action *action)
{
	SDL_WarpMouse(_xBeforeMouseScrolling, _yBeforeMouseScrolling);
	action->setMouseAction(_xBeforeMouseScrolling, _yBeforeMouseScrolling, getX(), getY());
}

void Globe::setCraftRange(double lon, double lat, double range)
{
	_craft = (range > 0.0);
	_craftLon = lon;
	_craftLat = lat;
	_craftRange = range;
}

}
