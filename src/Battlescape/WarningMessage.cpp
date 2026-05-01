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
#include "WarningMessage.h"
#include <SDL.h>
#include <string>
#include "../fmath.h"
#include "../Engine/Timer.h"
#include "../Interface/Text.h"
#ifdef __EMSCRIPTEN__
#include <vector>
#include <GLES3/gl3.h>
#include "../Engine/GpuTexture.h"
#include "../Engine/GpuInit.h"
#include "../Engine/Shader.h"
#include "../Engine/Screen.h"
#include "../Engine/Options.h"
#include "../Engine/Logger.h"
#include "../Engine/ShaderManager.h"
#endif

namespace OpenXcom
{

/**
 * Sets up a blank warning message with the specified size and position.
 * @param width Width in pixels.
 * @param height Height in pixels.
 * @param x X position in pixels.
 * @param y Y position in pixels.
 */
WarningMessage::WarningMessage(int width, int height, int x, int y) : Surface(width, height, x, y), _color(0), _fade(0)
{
	_text = new Text(width, height, 0, 0);
	_text->setHighContrast(true);
	_text->setAlign(ALIGN_CENTER);
	_text->setVerticalAlign(ALIGN_MIDDLE);
	_text->setWordWrap(true);

	_timer = new Timer(50);
	_timer->onTimer((SurfaceHandler)&WarningMessage::fade);

	setVisible(false);
}

/**
 * Deletes timers.
 */
WarningMessage::~WarningMessage()
{
#ifdef __EMSCRIPTEN__
	_gpuAliveFlag.reset();
	delete _warnShader; _warnShader = nullptr;
	delete _warnTex;    _warnTex    = nullptr;
	if (_warnVAO) { glDeleteVertexArrays(1, &_warnVAO); _warnVAO = 0; }
	if (_warnVBO) { glDeleteBuffers(1, &_warnVBO);      _warnVBO = 0; }
#endif
	delete _timer;
	delete _text;
}

/**
 * Changes the color for the message background.
 * @param color Color value.
 */
void WarningMessage::setColor(Uint8 color)
{
	_color = color;
}

/**
 * Changes the color for the message text.
 * @param color Color value.
 */
void WarningMessage::setTextColor(Uint8 color)
{
	_text->setColor(color);
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
void WarningMessage::initText(Font *big, Font *small, Language *lang)
{
	_text->initText(big, small, lang);
}

/**
 * Replaces a certain amount of colors in the surface's palette.
 * @param colors Pointer to the set of colors.
 * @param firstcolor Offset of the first color to replace.
 * @param ncolors Amount of colors to replace.
 */
void WarningMessage::setPalette(const SDL_Color *colors, int firstcolor, int ncolors)
{
	Surface::setPalette(colors, firstcolor, ncolors);
	_text->setPalette(colors, firstcolor, ncolors);
}

/**
 * Displays the warning message.
 * @param msg Message string.
 * @param time How long message will be visible.
 */
void WarningMessage::showMessage(const std::string &msg, int time)
{
	_text->setText(msg);
	_fade = time * 12;
	_redraw = true;
	setVisible(true);
	_timer->start();
}

/**
 * Keeps the animation timers running.
 */
void WarningMessage::think()
{
	_timer->think(0, this);
}

/**
 * Plays the message fade animation.
 */
void WarningMessage::fade()
{
	_fade--;
	_redraw = true;
	if (_fade == 0)
	{
		setVisible(false);
		_timer->stop();
	}
}

/**
 * Draws the warning message.
 */
void WarningMessage::draw()
{
	Surface::draw();
	drawRect(0, 0, getWidth(), getHeight(), _color + Clamp(24 - _fade, 0, 12));
	_text->blit(this->getSurface());
}

#ifdef __EMSCRIPTEN__

/**
 * Blits warning message; skips CPU blit when GPU mode is active.
 */
void WarningMessage::blit(SDL_Surface *surface)
{
	if (_gpuMode) return;
	Surface::blit(surface);
}

/**
 * Converts the warning SDL surface (ARGB8888) to packed RGBA and uploads to GPU texture.
 */
void WarningMessage::_uploadWarningPixels()
{
	if (!_warnTex) return;
	const SDL_Surface* sdl = getSurface();
	if (!sdl || !sdl->pixels) return;

	const int w = sdl->w;
	const int h = sdl->h;
	std::vector<uint8_t> rgba(static_cast<size_t>(w * h * 4));
	for (int y = 0; y < h; ++y)
	{
		const Uint32* row = reinterpret_cast<const Uint32*>(
		    static_cast<const uint8_t*>(sdl->pixels) + y * sdl->pitch);
		for (int x = 0; x < w; ++x)
		{
			const Uint32 argb = row[x];
			const size_t i = static_cast<size_t>((y * w + x) * 4);
			rgba[i+0] = static_cast<uint8_t>((argb >> 16) & 0xFF); // R
			rgba[i+1] = static_cast<uint8_t>((argb >>  8) & 0xFF); // G
			rgba[i+2] = static_cast<uint8_t>( argb        & 0xFF); // B
			rgba[i+3] = static_cast<uint8_t>((argb >> 24) & 0xFF); // A
		}
	}
	_warnTex->uploadRGBA(rgba.data(), w, h);
}

/**
 * Uploads warning pixels to GPU and registers a post-flush pass that renders
 * the overlay on top of GPU tile layers.  Call once after _map->init().
 */
void WarningMessage::initGPU(Screen& screen)
{
	if (!GpuInit::ready()) return;

	_gpuAliveFlag.reset();

	if (_redraw) draw();

	if (!_warnTex)
	{
		_warnTex = new GpuTexture(/*srgb=*/false,
		                          GpuTexture::Wrap::ClampToEdge,
		                          GpuTexture::Filter::Nearest);
	}
	_uploadWarningPixels();

	if (!_warnVAO)
	{
		glGenVertexArrays(1, &_warnVAO);
		glGenBuffers(1, &_warnVBO);
		glBindVertexArray(_warnVAO);
		glBindBuffer(GL_ARRAY_BUFFER, _warnVBO);
		glBufferData(GL_ARRAY_BUFFER, 6 * 4 * (GLsizeiptr)sizeof(float), nullptr, GL_DYNAMIC_DRAW);
		glEnableVertexAttribArray(0); // a_pos
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * (GLsizei)sizeof(float), (void*)0);
		glEnableVertexAttribArray(1); // a_uv
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * (GLsizei)sizeof(float), (void*)(2 * sizeof(float)));
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	if (!_warnShader)
	{
		_warnShader = new Shader();
		if (!_warnShader->loadFromEmbedded("textured"))
		{
			Log(LOG_ERROR) << "WarningMessage::initGPU: failed to load 'textured' shader";
			delete _warnShader; _warnShader = nullptr;
			delete _warnTex;    _warnTex    = nullptr;
			glDeleteVertexArrays(1, &_warnVAO); _warnVAO = 0;
			glDeleteBuffers(1, &_warnVBO);      _warnVBO = 0;
			return;
		}
	}

	_gpuAliveFlag = std::make_shared<bool>(true);
	_gpuMode = true;

	std::weak_ptr<bool> wf = _gpuAliveFlag;
	Screen* screenPtr = &screen;
	screen.registerGPUPass([this, wf, screenPtr]()
	{
		if (!wf.lock()) return;
		drawGPUPass(screenPtr);
	});

	// Block 11.13: on context restore, zero stale VAO handles and re-enter initGPU
	// to create fresh ones.  _warnShader/_warnTex are rebuilt by reuploadAll().
	ShaderManager::instance().registerResetCallback(_gpuAliveFlag, [this, screenPtr]() {
		_warnVAO = 0;
		_warnVBO = 0;
		initGPU(*screenPtr);
	});

	Log(LOG_DEBUG) << "WarningMessage::initGPU: warning GPU pass registered";
}

/**
 * GPU post-flush pass: renders the warning overlay as a textured quad on top
 * of tile layers but below the mouse cursor.  Called each frame.
 */
void WarningMessage::drawGPUPass(Screen* screen)
{
	if (!_visible || _hidden) return;
	if (!_warnTex || !_warnVAO) return;
	if (!_warnShader || !_warnShader->isValid()) return;

	if (_redraw)
	{
		draw();
		_uploadWarningPixels();
	}

	const double xScale = screen->getXScale();
	const double yScale = screen->getYScale();
	const int    lbb    = screen->getCursorLeftBlackBand();
	const int    tbb    = screen->getCursorTopBlackBand();
	const int    dW     = Options::displayWidth;
	const int    dH     = Options::displayHeight;

	const float dispX = static_cast<float>(getX() * xScale) + lbb;
	const float dispY = static_cast<float>(getY() * yScale) + tbb;
	const float dispW = static_cast<float>(getWidth()  * xScale);
	const float dispH = static_cast<float>(getHeight() * yScale);

	const float ndcX0 =  2.0f * dispX / dW - 1.0f;
	const float ndcY0 = -(2.0f * dispY / dH - 1.0f);
	const float ndcX1 =  2.0f * (dispX + dispW) / dW - 1.0f;
	const float ndcY1 = -(2.0f * (dispY + dispH) / dH - 1.0f);

	const float verts[6 * 4] = {
		ndcX0, ndcY0,  0.0f, 0.0f,   // top-left
		ndcX1, ndcY0,  1.0f, 0.0f,   // top-right
		ndcX0, ndcY1,  0.0f, 1.0f,   // bottom-left
		ndcX0, ndcY1,  0.0f, 1.0f,   // bottom-left
		ndcX1, ndcY0,  1.0f, 0.0f,   // top-right
		ndcX1, ndcY1,  1.0f, 1.0f,   // bottom-right
	};

	glBindBuffer(GL_ARRAY_BUFFER, _warnVBO);
	glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)sizeof(verts), verts);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	GLint prevProgram = 0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	_warnShader->use();
	_warnShader->setUniform1f("u_darken", 0.0f);
	_warnTex->bind(0);
	_warnShader->setUniform1i("u_tex", 0);

	glBindVertexArray(_warnVAO);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);

	glDisable(GL_BLEND);
	glUseProgram(static_cast<GLuint>(prevProgram));
}

#endif // __EMSCRIPTEN__

}
