#pragma once

#ifdef __EMSCRIPTEN__
#include <cstdint>

namespace OpenXcom
{
namespace CalypsoGpuTextureValidation
{
void drainPriorGlErrors(const char* operation);
unsigned int takeGlError();
bool dimensionsFitRuntime(const char* operation, const std::uint8_t* data,
                          int w, int h, int bytesPerPixel);
}
}
#endif
