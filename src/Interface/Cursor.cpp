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
#include "Cursor.h"
#include <cmath>
#include <SDL.h>
#include "../Engine/Action.h"
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
 * Sets up a cursor with the specified size and position
 * and hides the system cursor.
 * @note The size and position don't really matter since
 * it's a 9x13 shape, they're just there for inheritance.
 * @param width Width in pixels.
 * @param height Height in pixels.
 * @param x X position in pixels.
 * @param y Y position in pixels.
 */
Cursor::Cursor(int width, int height, int x, int y) : Surface(width, height, x, y), _color(0)
{
}

/**
 *
 */
Cursor::~Cursor()
{
#ifdef __EMSCRIPTEN__
	_gpuAliveFlag.reset();
	delete _cursorShader; _cursorShader = nullptr;
	delete _cursorTex;    _cursorTex    = nullptr;
	if (_cursorVAO) { glDeleteVertexArrays(1, &_cursorVAO); _cursorVAO = 0; }
	if (_cursorVBO) { glDeleteBuffers(1, &_cursorVBO);      _cursorVBO = 0; }
#endif
}

/**
 * Automatically updates the cursor position
 * when the mouse moves.
 * @param action Pointer to an action.
 */
void Cursor::handle(Action *action)
{
	if (action->getDetails()->type == SDL_MOUSEMOTION)
	{
		setX((int)floor((action->getDetails()->motion.x - action->getLeftBlackBand()) / action->getXScale()));
		setY((int)floor((action->getDetails()->motion.y - action->getTopBlackBand()) / action->getYScale()));
	}
}

/**
 * Changes the cursor's base color.
 * @param color Color value.
 */
void Cursor::setColor(Uint8 color)
{
	_color = color;
	_redraw = true;
}

/**
 * Returns the cursor's base color.
 * @return Color value.
 */
Uint8 Cursor::getColor() const
{
	return _color;
}

/**
 * Draws a pointer-shaped cursor graphic.
 */
void Cursor::draw()
{
	Surface::draw();
	Uint8 color = _color;
	int x1 = 0, y1 = 0, x2 = getWidth() - 1, y2 = getHeight() - 1;

	lock();
	for (int i = 0; i < 4; ++i)
	{
		drawLine(x1, y1, x1, y2, color);
		drawLine(x1, y1, x2, getWidth() - 1, color);
		x1++;
		y1 += 2;
		y2--;
		x2--;
		color++;
	}
	this->setPixel(4, 8, --color);
	unlock();
}

/**
 * Blits cursor to the target surface.
 * When GPU mode is active the cursor is rendered via a GL post-flush pass
 * registered in initGPU(), so CPU-side blitting is skipped.
 */
void Cursor::blit(SDL_Surface *surface)
{
#ifdef __EMSCRIPTEN__
	if (_gpuMode) return;
#endif
	Surface::blit(surface);
}

#ifdef __EMSCRIPTEN__

/**
 * Converts the cursor SDL surface (ARGB8888) to a packed RGBA byte buffer
 * and uploads it to the GPU texture.
 */
void Cursor::_uploadCursorPixels()
{
	if (!_cursorTex) return;
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
	_cursorTex->uploadRGBA(rgba.data(), w, h);
}

/**
 * Uploads cursor sprite to GPU and registers a post-flush pass that renders
 * the cursor on top of GPU tile layers.
 *
 * Called once per BattlescapeState construction, AFTER Map::init() registers
 * the tile pass, so the cursor pass always runs after the tile pass in the
 * Screen::_gpuPasses list.  Each call invalidates the previous cursor pass
 * (alive-flag reset → weak_ptr.lock() returns nullptr → old entry becomes a
 * no-op) and registers a fresh pass at the end of the list.  GL resources
 * (texture, VAO/VBO, shader) are created on the first call and reused
 * thereafter.
 */
void Cursor::initGPU(Screen& screen)
{
	if (!GpuInit::ready()) return;

	// Invalidate any previously registered cursor pass so it becomes a no-op.
	// This ensures the new pass (registered below) is the only active one.
	_gpuAliveFlag.reset();

	// Ensure cursor surface pixels are up-to-date before upload.
	if (_redraw) draw();

	// Create GL resources on first call; reuse on subsequent calls.
	if (!_cursorTex)
	{
		_cursorTex = new GpuTexture(/*srgb=*/false,
		                            GpuTexture::Wrap::ClampToEdge,
		                            GpuTexture::Filter::Nearest);
	}
	_uploadCursorPixels();

	if (!_cursorVAO)
	{
		// 6-vertex quad (2 triangles): each vertex is pos.xy + uv.xy (4 floats).
		glGenVertexArrays(1, &_cursorVAO);
		glGenBuffers(1, &_cursorVBO);
		glBindVertexArray(_cursorVAO);
		glBindBuffer(GL_ARRAY_BUFFER, _cursorVBO);
		glBufferData(GL_ARRAY_BUFFER, 6 * 4 * (GLsizeiptr)sizeof(float), nullptr, GL_DYNAMIC_DRAW);
		glEnableVertexAttribArray(0); // a_pos
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * (GLsizei)sizeof(float), (void*)0);
		glEnableVertexAttribArray(1); // a_uv
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * (GLsizei)sizeof(float), (void*)(2 * sizeof(float)));
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	if (!_cursorShader)
	{
		_cursorShader = new Shader();
		if (!_cursorShader->loadFromEmbedded("textured"))
		{
			Log(LOG_ERROR) << "Cursor::initGPU: failed to load 'textured' shader";
			delete _cursorShader; _cursorShader = nullptr;
			delete _cursorTex;    _cursorTex    = nullptr;
			glDeleteVertexArrays(1, &_cursorVAO); _cursorVAO = 0;
			glDeleteBuffers(1, &_cursorVBO);      _cursorVBO = 0;
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
	// to create fresh ones.  _cursorShader/_cursorTex are rebuilt by reuploadAll().
	ShaderManager::instance().registerResetCallback(_gpuAliveFlag, [this, screenPtr]() {
		_cursorVAO = 0;
		_cursorVBO = 0;
		initGPU(*screenPtr);
	});

	Log(LOG_DEBUG) << "Cursor::initGPU: cursor GPU pass registered";
}

/**
 * GPU post-flush pass: renders the cursor as a textured quad on top of all
 * other GPU layers.  Called by the Screen GPU-pass loop each frame.
 */
void Cursor::drawGPUPass(Screen* screen)
{
	if (!_visible || _hidden) return;
	if (!_cursorTex || !_cursorVAO) return;
	if (!_cursorShader || !_cursorShader->isValid()) return;

	// Re-render and re-upload if cursor colour changed.
	if (_redraw)
	{
		draw();
		_uploadCursorPixels();
	}

	// Convert game-space cursor rect to NDC using screen scale + black-band offsets.
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
	const float ndcY0 = -(2.0f * dispY / dH - 1.0f);        // flip Y: SDL top-left → GL bottom-left
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

	glBindBuffer(GL_ARRAY_BUFFER, _cursorVBO);
	glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)sizeof(verts), verts);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// Save / restore only the GL program — blend state is managed explicitly.
	GLint prevProgram = 0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	_cursorShader->use();
	_cursorShader->setUniform1f("u_darken", 0.0f);
	_cursorTex->bind(0);
	_cursorShader->setUniform1i("u_tex", 0);

	glBindVertexArray(_cursorVAO);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);

	glDisable(GL_BLEND);
	glUseProgram(static_cast<GLuint>(prevProgram));
}

#endif // __EMSCRIPTEN__

}
