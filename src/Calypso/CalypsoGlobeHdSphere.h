#pragma once
#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#include <iomanip>
#include "../Engine/Logger.h"

namespace OpenXcom
{
class Globe;
class GpuTexture;
struct Cord;

struct GlobeSphereGlSave
{
	GLfloat lineWidth = 1.0f;
	GLint activeTexture = GL_TEXTURE0;
	const char *errorOperation = nullptr;
	const char *restoreErrorOperation = nullptr;
	GLenum restoreError = GL_NO_ERROR;
	bool saved = false;
	GLenum save()
	{
		glGetFloatv(GL_LINE_WIDTH, &lineWidth);
		glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
		const GLenum error = glGetError();
		if (error != GL_NO_ERROR) errorOperation = "glGetIntegerv(GL_ACTIVE_TEXTURE)";
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
		glActiveTexture(activeTexture);
		check("glActiveTexture");
		glActiveTexture(GL_TEXTURE0);
		check("glActiveTexture(GL_TEXTURE0)");
		glBindTexture(GL_TEXTURE_2D, 0);
		check("glBindTexture(GL_TEXTURE_2D, 0)");
		glLineWidth(lineWidth);
		check("glLineWidth");
		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		check("glBlendFuncSeparate");
	}
};

namespace Calypso
{
struct GeoscapeQaVec3;
GpuTexture* calypsoGlobeQaHiddenCloudsTexture();
float calypsoGlobeQaEffectiveMs(float liveMs);
Cord calypsoGlobeQaCord(const GeoscapeQaVec3& value);
bool calypsoGlobeInitSphereGPU(Globe& globe);
Cord calypsoGlobeSunDirectionWorld(const Globe& globe);
void calypsoGlobeDrawHDStarfield(Globe& globe);
void calypsoGlobeDrawSphereGPU(Globe& globe);
void calypsoGlobeDrawHoverCircles(Globe& globe);
void calypsoGlobeHoverOverlayFrame(Globe& globe);
} // namespace Calypso
} // namespace OpenXcom
#endif
