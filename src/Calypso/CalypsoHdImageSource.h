#pragma once
// F01 RGBA image source (T05): VFS resolution + PNG decode to RGBA8.
// Whole-file Emscripten guard (Phase 36): needs FileMap + SDL_image.
#ifdef __EMSCRIPTEN__

#include <cstdint>
#include <string>
#include <vector>

namespace OpenXcom
{
namespace Calypso
{

struct CalypsoHdImageRgba
{
	int w = 0;
	int h = 0;
	std::vector<std::uint8_t> px;
};

// Decode a FileMap-relative image to RGBA8 memory order (R,G,B,A).
// Returns false for missing files, broken data, and oversize images;
// never throws, never quantizes to a palette. Details in the .cpp.
bool calypsoHdImageDecode(const std::string &vfsPath, CalypsoHdImageRgba &out);

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
