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

class Text;
class Timer;
class Font;
#ifdef __EMSCRIPTEN__
class Screen;
class GpuTexture;
class Shader;
#endif

/**
 * Coloured box with text inside that fades out after it is displayed.
 * Used to display warning/error messages on the Battlescape.
 */
class WarningMessage : public Surface
{
private:
	Text *_text;
	Timer *_timer;
	Uint8 _color, _fade;
#ifdef __EMSCRIPTEN__
	GpuTexture*           _warnTex    = nullptr;
	Shader*               _warnShader = nullptr;
	unsigned int          _warnVAO    = 0;
	unsigned int          _warnVBO    = 0;
	bool                  _gpuMode    = false;
	std::shared_ptr<bool> _gpuAliveFlag;
	void _uploadWarningPixels();
	void drawGPUPass(Screen* screen);
#endif
public:
	/// Creates a new warning message with the specified size and position.
	WarningMessage(int width, int height, int x = 0, int y = 0);
	/// Cleans up the warning message.
	~WarningMessage();
	/// Sets the color for the warning message.
	void setColor(Uint8 color) override;
	/// Sets the text color for the warning message.
	void setTextColor(Uint8 color);
	/// Initializes the warning message's resources.
	void initText(Font *big, Font *small, Language *lang) override;
	/// Sets the warning message's palette.
	void setPalette(const SDL_Color *colors, int firstcolor = 0, int ncolors = 256) override;
	/// Shows the warning message.
	void showMessage(const std::string &msg, int time = 2);
	/// Handles the timers.
	void think() override;
	/// Fades the message.
	void fade();
	/// Draws the message.
	void draw() override;
#ifdef __EMSCRIPTEN__
	/// Uploads pixels to GPU and registers a post-flush pass. Call once on battle start.
	void initGPU(Screen& screen);
	/// Blits warning; skips when GPU mode is active.
	void blit(SDL_Surface* surface) override;
#endif
};

}
