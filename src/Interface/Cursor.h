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
#include "../Engine/Surface.h"
#ifdef __EMSCRIPTEN__
#include <memory>
#endif

namespace OpenXcom
{

class Action;
#ifdef __EMSCRIPTEN__
class Screen;
class GpuTexture;
class Shader;
#endif

/**
 * Mouse cursor that replaces the system cursor.
 * Drawn as a shaded triangle-like shape, automatically
 * matches the mouse coordinates.
 */
class Cursor : public Surface
{
private:
	Uint8 _color;
#ifdef __EMSCRIPTEN__
	GpuTexture*           _cursorTex   = nullptr;
	Shader*               _cursorShader= nullptr;
	unsigned int          _cursorVAO   = 0;
	unsigned int          _cursorVBO   = 0;
	bool                  _gpuMode     = false;
	std::shared_ptr<bool> _gpuAliveFlag;

	void _uploadCursorPixels();
	void drawGPUPass(Screen* screen);
#endif

public:
	/// Creates a new cursor with the specified size and position.
	Cursor(int width, int height, int x = 0, int y = 0);
	/// Cleans up the cursor.
	~Cursor();
	/// Handles mouse events.
	void handle(Action *action);
	/// Sets the cursor's color.
	void setColor(Uint8 color) override;
	/// Gets the cursor's color.
	Uint8 getColor() const;
	/// Draws the cursor.
	void draw() override;
	/// Blits cursor; skips when GPU mode is active.
	void blit(SDL_Surface *surface) override;
#ifdef __EMSCRIPTEN__
	/// Uploads cursor sprite to GPU and registers post-flush pass. Call once on battle start.
	void initGPU(Screen& screen);
#endif
};

}
