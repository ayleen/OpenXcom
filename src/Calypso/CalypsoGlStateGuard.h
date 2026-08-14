#pragma once
/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * Phase 46.2-HD (Calypso) -- RAII GL/SDL interop guard (remediation A4).
 *
 * SDL2's GL renderer caches its own GL state; issuing raw GL between
 * SDL_RenderFlush() and the next SDL_RenderCopy() desyncs that cache and makes
 * later SDL draws invisible or wrong (the lesson recorded in Screen.cpp). This
 * guard snapshots every piece of GL state the overlay's boundary-zero section
 * touches on construction and restores the EXACT saved values on destruction --
 * program, VAO, array buffer, active texture unit, unit-0 binding, blend enable,
 * blend func (separate), and blend equation (separate). It restores to the saved
 * state, never to an assumed default.
 *
 * Whole-file Emscripten guard (Phase 36); the overlay is the only user.
 */
#ifdef __EMSCRIPTEN__

#include <GLES3/gl3.h>

namespace OpenXcom
{
namespace Calypso
{

class CalypsoGlStateGuard
{
public:
	CalypsoGlStateGuard()
	{
		glGetIntegerv(GL_CURRENT_PROGRAM, &_program);
		glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &_vao);
		glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &_arrayBuffer);
		glGetIntegerv(GL_ACTIVE_TEXTURE, &_activeTexture);
		glActiveTexture(GL_TEXTURE0);
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &_tex0);
		_blend = glIsEnabled(GL_BLEND);
		glGetIntegerv(GL_BLEND_SRC_RGB, &_blendSrcRgb);
		glGetIntegerv(GL_BLEND_DST_RGB, &_blendDstRgb);
		glGetIntegerv(GL_BLEND_SRC_ALPHA, &_blendSrcAlpha);
		glGetIntegerv(GL_BLEND_DST_ALPHA, &_blendDstAlpha);
		glGetIntegerv(GL_BLEND_EQUATION_RGB, &_blendEqRgb);
		glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &_blendEqAlpha);
	}

	~CalypsoGlStateGuard()
	{
		glUseProgram(static_cast<GLuint>(_program));
		glBindVertexArray(static_cast<GLuint>(_vao));
		glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(_arrayBuffer));
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(_tex0));
		glActiveTexture(static_cast<GLenum>(_activeTexture));
		if (_blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
		glBlendFuncSeparate(
			static_cast<GLenum>(_blendSrcRgb), static_cast<GLenum>(_blendDstRgb),
			static_cast<GLenum>(_blendSrcAlpha), static_cast<GLenum>(_blendDstAlpha));
		glBlendEquationSeparate(
			static_cast<GLenum>(_blendEqRgb), static_cast<GLenum>(_blendEqAlpha));
	}

	CalypsoGlStateGuard(const CalypsoGlStateGuard&) = delete;
	CalypsoGlStateGuard& operator=(const CalypsoGlStateGuard&) = delete;

private:
	GLint _program = 0;
	GLint _vao = 0;
	GLint _arrayBuffer = 0;
	GLint _activeTexture = GL_TEXTURE0;
	GLint _tex0 = 0;
	GLboolean _blend = GL_FALSE;
	GLint _blendSrcRgb = GL_ONE;
	GLint _blendDstRgb = GL_ZERO;
	GLint _blendSrcAlpha = GL_ONE;
	GLint _blendDstAlpha = GL_ZERO;
	GLint _blendEqRgb = GL_FUNC_ADD;
	GLint _blendEqAlpha = GL_FUNC_ADD;
};

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
