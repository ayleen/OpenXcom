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
#include <list>
#include <utility>
#include <cstdint>
#include <string>
#ifdef __EMSCRIPTEN__
#  include <memory>
#endif
#include "../Engine/InteractiveSurface.h"
#include "../Engine/FastLineClip.h"
#include "Cord.h"
#ifdef __EMSCRIPTEN__
#  include "../Engine/Screen.h"
#endif

namespace OpenXcom
{

class Game;
class Polygon;
class SurfaceSet;
class Timer;
class Target;
class LocalizedText;
class RuleGlobe;
class Craft;
#ifdef __EMSCRIPTEN__
class Shader;
class GpuTexture;
#endif

/**
 * Interactive globe view of the world.
 * Takes a flat world map made out of land polygons with
 * polar coordinates and renders it as a 3D-looking globe
 * with cartesian coordinates that the player can interact with.
 */
class Globe : public InteractiveSurface
{
private:
	static const int NUM_LANDSHADES = 48;
	static const int NUM_SEASHADES = 72;
	static const int NEAR_RADIUS = 25;
	static const int MAX_DRAW_RADAR_CIRCLE_RADIUS = 10000;
	static const size_t DOGFIGHT_ZOOM = 3;
	static const int CITY_MARKER = 8;
	static const double ROTATE_LONGITUDE;
	static const double ROTATE_LATITUDE;

	RuleGlobe *_rules;
	Sint16 _cenX, _cenY;
	double _cenLon, _cenLat, _rotLon, _rotLat, _hoverLon, _hoverLat;
	double _craftLon, _craftLat, _craftRange;
	size_t _zoom, _zoomOld, _zoomTexture;
	SurfaceSet *_texture, *_markerSet;
	Game *_game;
	Surface *_markers, *_countries, *_radars;
	bool _hover, _craft;
	int _blink;
	Timer *_blinkTimer, *_rotTimer;
	std::list<Polygon*> _cacheLand;
	FastLineClip *_clipper;
	double _radius, _radiusStep;
	///normal of each pixel in earth globe per zoom level
	std::vector<std::vector<Cord> > _earthData;
	///list of dimension of earth on screen per zoom level
	std::vector<double> _zoomRadius;

	bool _isMouseScrolling, _isMouseScrolled;
	int _xBeforeMouseScrolling, _yBeforeMouseScrolling;
	double _lonBeforeMouseScrolling, _latBeforeMouseScrolling;
	Uint32 _mouseScrollingStartTime;
	int _totalMouseMoveX, _totalMouseMoveY;
	bool _mouseMovedOverThreshold;

#ifdef __EMSCRIPTEN__
	/* Phase 8c — HD GPU sphere */
	/* These are hard per-frame bounds, not merely warm-up reserves.  Recording
	 * refuses to grow a command vector after preparation, so an immutable
	 * production snapshot can only fail closed before Earth publication. */
	static constexpr size_t GPU_BORDER_LINE_CAPACITY = 16384u;
	static constexpr size_t GPU_BORDER_VERTEX_FLOAT_CAPACITY = 65536u;
	static constexpr size_t GPU_RADAR_FLIGHT_LINE_CAPACITY = 16384u;
	static constexpr size_t GPU_RADAR_FLIGHT_VERTEX_FLOAT_CAPACITY = 65536u;
	static constexpr size_t GPU_DEBUG_LINE_CAPACITY = 16384u;
	static constexpr size_t GPU_DEBUG_VERTEX_FLOAT_CAPACITY = 65536u;
	static constexpr size_t GPU_LABEL_TEXTURE_CAPACITY = 1024u;
	static constexpr size_t GPU_LABEL_DRAW_CAPACITY = 2048u;
	unsigned  _sphereVAO    = 0u;
	unsigned  _sphereFBO    = 0u;
	unsigned  _sphereFBOTex = 0u;
	bool      _gpuSphereOK  = false;
	Shader*   _globeShader  = nullptr; // owned; created in initSphereGPU()
	std::shared_ptr<bool> _gpuAliveFlag;   // M6: lifetime token for the ShaderManager reset callback
	bool      _gpuResetCallbackRegistered = false;
	bool      _gpuDirectAck = false;   // Stage 10.2.1: acknowledged opt-in request
	bool      _gpuDirectMode = false;
	Screen*   _directScreen  = nullptr;
	ScreenWorldPassHandle _gpuWorldPass;
	struct MarkerDraw
	{
		Surface* frame = nullptr;
		int x = 0;
		int y = 0;
		int shade = 0;
	};
	std::vector<MarkerDraw> _gpuMarkerPendingDraws;
	std::vector<MarkerDraw> _gpuMarkerCommittedDraws;
	struct BorderLine
	{
		float x1 = 0.f;
		float y1 = 0.f;
		float x2 = 0.f;
		float y2 = 0.f;
	};
	std::vector<BorderLine> _gpuBorderLines;
	std::vector<float> _gpuBorderVertices;
	size_t _gpuBorderCapacity = 0;
	bool _gpuBorderCapacityExceeded = false;
	struct RadarFlightLine
	{
		double x1 = 0.0;
		double y1 = 0.0;
		double x2 = 0.0;
		double y2 = 0.0;
		Uint8 shade = 0;
		Uint8 color = 0;
	};
	std::vector<RadarFlightLine> _gpuRadarFlightLines;
	std::vector<float> _gpuRadarFlightVertices;
	size_t _gpuRadarFlightCapacity = 0;
	bool _gpuRadarFlightCapacityExceeded = false;
	struct DebugLine
	{
		float x1 = 0.f;
		float y1 = 0.f;
		float x2 = 0.f;
		float y2 = 0.f;
		Uint8 color = 0;
	};
	std::vector<DebugLine> _gpuDebugLines;
	std::vector<float> _gpuDebugVertices;
	size_t _gpuDebugCapacity = 0;
	bool _gpuDebugCapacityExceeded = false;
	bool _gpuLogicalWorldComplete = true;
	std::uint64_t _gpuMarkerPaletteGeneration = 0;
	std::uint64_t _gpuLabelPaletteGeneration = 0;
	struct MarkerTexture
	{
		Surface* frame = nullptr;
		int shade = 0;
		std::uint64_t paletteGeneration = 0;
		GpuTexture* texture = nullptr;
	};
	std::vector<MarkerTexture> _gpuMarkerTextures;
	struct LabelTexture
	{
		std::string text;
		int width = 0;
		int height = 0;
		Uint8 color = 0;
		std::uint64_t paletteGeneration = 0;
		Surface* frame = nullptr;
		GpuTexture* texture = nullptr;
	};
	struct LabelIconDraw
	{
		LabelTexture* label = nullptr;
		Surface* frame = nullptr;
		int x = 0;
		int y = 0;
		int shade = 0;
	};
	std::vector<LabelTexture> _gpuLabelTextures;
	std::vector<LabelIconDraw> _gpuLabelIconPendingDraws;
	std::vector<LabelIconDraw> _gpuLabelIconCommittedDraws;
	bool _gpuLabelCapacityExceeded = false;
	unsigned  _markerVAO     = 0u;
	unsigned  _markerVBO     = 0u;
	Shader*   _markerShader  = nullptr;
	bool      _gpuMarkerReady = false;
	unsigned  _borderVAO     = 0u;
	unsigned  _borderVBO     = 0u;
	Shader*   _borderShader  = nullptr;
	bool      _gpuBorderReady = false;
	friend struct CalypsoGeoscapeHdGlobeDirect;   // Stage 10.2.1

	/// One-time GPU resource initialisation for the sphere.
	bool initSphereGPU();
	/// Draws a deterministic pixel-art space background behind the HD sphere.
	void drawHDStarfield();
	/// Renders the sphere via GPU and reads back pixels into this surface.
	void drawSphereGPU();
	/// Stage 10.2.1: physical-resolution direct composite (opt-in).
	/// Sun direction in the fixed world frame the shader uses.
	Cord getSunDirectionWorld() const;
#endif

	/// Sets the globe zoom factor.
	void setZoom(size_t zoom);
	/// Checks if a point is behind the globe.
	bool pointBack(double lon, double lat) const;
	/// Get polygon pointer
	Polygon* getPolygonFromLonLat(double lon, double lat) const;
	/// Checks if a target is near a point.
	bool targetNear(Target* target, int x, int y) const;
	/// Caches a set of polygons.
	void cache(std::list<Polygon*> *polygons, std::list<Polygon*> *cache);
	/// Get position of sun relative to given position in polar cords and date.
	Cord getSunDirection(double lon, double lat) const;
	/// Draw globe range circle.
	void drawGlobeCircle(double lat, double lon, double radius, int segments, int frac = 1);
	/// Special "transparent" line.
	void XuLine(Surface* surface, Surface* src, double x1, double y1, double x2, double y2, int shade);
	/// Draw line on globe surface.
	void drawVHLine(Surface *surface, double lon1, double lat1, double lon2, double lat2, Uint8 color);
	/// Draw flight path.
	void drawPath(Surface *surface, double lon1, double lat1, double lon2, double lat2);
	/// Draw target marker.
	void drawTarget(Target *target, Surface *surface);
	/// Set up the radius of earth and stuff.
	void setupRadii(int width, int height);
	/// Rebuild the per-zoom surface-normal cache used by drawShadow.
	void rebuildEarthData();
public:
	static Uint8 OCEAN_COLOR;
	static bool OCEAN_SHADING;
	static Uint8 COUNTRY_LABEL_COLOR;
	static Uint8 LINE_COLOR;
	static Uint8 CITY_LABEL_COLOR;
	static Uint8 BASE_LABEL_COLOR;

	/// Creates a new globe at the specified position and size.
	Globe(Game* game, int cenX, int cenY, int width, int height, int x = 0, int y = 0);
	/// Cleans up the globe.
	~Globe();
	/// Converts polar coordinates to cartesian coordinates.
	void polarToCart(double lon, double lat, Sint16 *x, Sint16 *y) const;
	/// Converts polar coordinates to cartesian coordinates.
	void polarToCart(double lon, double lat, double *x, double *y) const;
	/// Converts cartesian coordinates to polar coordinates.
	void cartToPolar(Sint16 x, Sint16 y, double *lon, double *lat) const;
	/// Starts rotating the globe left.
	void rotateLeft();
	/// Starts rotating the globe right.
	void rotateRight();
	/// Starts rotating the globe up.
	void rotateUp();
	/// Starts rotating the globe down.
	void rotateDown();
	/// Stops rotating the globe.
	void rotateStop();
	/// Stops longitude rotation of the globe.
	void rotateStopLon();
	/// Stops latitude rotation of the globe.
	void rotateStopLat();
	/// Zooms the globe in.
	void zoomIn();
	/// Zooms the globe out.
	void zoomOut();
	/// Zooms the globe minimum.
	void zoomMin();
	/// Zooms the globe maximum.
	void zoomMax();
	/// Saves the zoom level for dogfights.
	void saveZoomDogfight();
	/// Zooms the globe in for dogfights.
	bool zoomDogfightIn();
	/// Zooms the globe out for dogfights.
	bool zoomDogfightOut();
	/// Gets the current zoom.
	size_t getZoom() const;
	/// Centers the globe on a point.
	void center(double lon, double lat);
	/// Checks if a point is inside land.
	bool insideLand(double lon, double lat) const;
	/// Checks if a point is inside fakeUnderwater texture.
	bool insideFakeUnderwaterTexture(double lon, double lat) const;
	/// Turns on/off the globe detail.
	void toggleDetail();
	/// Gets all the targets near a point on the globe.
	std::vector<Target*> getTargets(int x, int y, bool craft, Craft *currentCraft) const;
	/// Caches visible globe polygons.
	void cachePolygons();
	/// Sets the palette of the globe.
	void setPalette(const SDL_Color *colors, int firstcolor = 0, int ncolors = 256) override;
	/// Handles the timers.
	void think() override;
	/// Blinks the markers.
	void blink();
	/// Rotates the globe.
	void rotate();
	/// Draws the whole globe.
	/// Stage 10.2.1: physical-resolution direct composite (opt-in).
	void setGpuDirect(bool on);

	void draw() override;
	/// Draws the ocean of the globe.
	void drawOcean();
	/// Draws the land of the globe.
	void drawLand();
	/// Draws the shadow.
	void drawShadow();
	/// Draws the radar ranges of the globe.
	void drawRadars();
	/// Draws the flight paths of the globe.
	void drawFlights();
	/// Draws the country details of the globe.
	void drawDetail();
	/// Draws all the markers over the globe.
	void drawMarkers();
	/// Blits the globe onto another surface.
	void blit(SDL_Surface *surface) override;
	/// Special handling for mouse hover.
	void mouseOver(Action *action, State *state) override;
	/// Special handling for mouse presses.
	void mousePress(Action *action, State *state) override;
	/// Special handling for mouse releases.
	void mouseRelease(Action *action, State *state) override;
	/// Special handling for mouse clicks.
	void mouseClick(Action *action, State *state) override;
	/// Special handling for key presses.
	void keyboardPress(Action *action, State *state) override;
	/// Get the polygons texture and shade at the given point.
	void getPolygonTextureAndShade(double lon, double lat, int *texture, int *shade) const;
	/// Sets hover base position.
	void setNewBaseHoverPos(double lon, double lat);
	/// Turns on new base hover mode.
	void setNewBaseHover(bool hover);
	/// Sets craft range mode.
	void setCraftRange(double lon, double lat, double range);
	/// set the _radarLines variable
	void toggleRadarLines();
	/// Update the resolution settings, we just resized the window.
	void resize();
	/// Move the mouse back to where it started after we finish drag scrolling.
	void stopScrolling(Action *action);
};

}
