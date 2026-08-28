#ifdef __EMSCRIPTEN__

#include "GpuTextureValidation.h"
#include "../Engine/Logger.h"
#include "../Engine/GpuInit.h"

#include <GLES3/gl3.h>
#include <cstddef>
#include <iomanip>
#include <limits>

namespace OpenXcom
{
namespace CalypsoGpuTextureValidation
{

void drainPriorGlErrors(const char* operation)
{
	for (GLenum error = glGetError(); error != GL_NO_ERROR; error = glGetError())
		Log(LOG_WARNING) << "GpuTexture::" << operation
		                 << ": clearing pre-existing GL error 0x"
		                 << std::hex << (unsigned)error << std::dec;
}

unsigned int takeGlError()
{
	const GLenum first = glGetError();
	for (GLenum error = glGetError(); error != GL_NO_ERROR; error = glGetError()) {}
	return (unsigned int)first;
}

bool dimensionsFitRuntime(const char* operation, const std::uint8_t* data,
	                      int w, int h, int bytesPerPixel)
{
	const std::size_t maxSize = std::numeric_limits<std::size_t>::max();
	if (!data || w <= 0 || h <= 0 || bytesPerPixel <= 0
	 || (std::size_t)w > maxSize / (std::size_t)h
	 || (std::size_t)w * (std::size_t)h > maxSize / (std::size_t)bytesPerPixel)
	{
		Log(LOG_ERROR) << "GpuTexture::" << operation
		               << ": invalid upload dimensions/data " << w << "x" << h;
		return false;
	}
    const GLint maxTextureSize = GpuInit::maxTextureSize();
    if (maxTextureSize <= 0
        || w > maxTextureSize || h > maxTextureSize)
	{
		Log(LOG_ERROR) << "GpuTexture::" << operation << ": " << w << "x" << h
		               << " exceeds/failed runtime GL_MAX_TEXTURE_SIZE="
                       << maxTextureSize << " (validated reset-boundary limit)";
		return false;
	}
	return true;
}

}
}
#endif
