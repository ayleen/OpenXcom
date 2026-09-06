// F01 RGBA image source (T05): FileMap + SDL_image PNG decode to RGBA8.
// No palette path anywhere: HD art must never be quantized to 8-bit.
#ifdef __EMSCRIPTEN__

#include "CalypsoHdImageSource.h"
#include "CalypsoHdImageModel.h"

#include "../Engine/FileMap.h"
#include "../Engine/Logger.h"

#include <cstring>
#include <memory>

#include <SDL.h>
#include <SDL_image.h>

namespace OpenXcom
{
namespace Calypso
{

namespace
{
constexpr long long kMaxFileBytes = 32LL * 1024LL * 1024LL;

struct SdlSurfaceDeleter
{
	void operator()(SDL_Surface *s) const { if (s) SDL_FreeSurface(s); }
};

} // namespace

bool calypsoHdImageDecode(const std::string &vfsPath, CalypsoHdImageRgba &out)
{
	out = CalypsoHdImageRgba{};
	if (vfsPath.empty())
	{
		return false;
	}
	SDL_RWops *rw = FileMap::getRWops(vfsPath);
	if (!rw)
	{
		Log(LOG_WARNING) << "CalypsoHdImageSource: missing VFS image " << vfsPath;
		return false;
	}
	const long long fileBytes = SDL_RWsize(rw);
	if (fileBytes > kMaxFileBytes)
	{
		SDL_RWclose(rw);
		Log(LOG_WARNING) << "CalypsoHdImageSource: image too large " << vfsPath;
		return false;
	}
	// IMG_Load_RW with SDL_TRUE takes ownership of rw and closes it.
	std::unique_ptr<SDL_Surface, SdlSurfaceDeleter> raw(IMG_Load_RW(rw, SDL_TRUE));
	if (!raw || !raw->pixels || raw->w <= 0 || raw->h <= 0)
	{
		Log(LOG_WARNING) << "CalypsoHdImageSource: broken image " << vfsPath
			<< ": " << IMG_GetError();
		return false;
	}
	if (raw->w > kCalypsoHdImageMaxExtent || raw->h > kCalypsoHdImageMaxExtent)
	{
		Log(LOG_WARNING) << "CalypsoHdImageSource: decoded extent too large " << vfsPath;
		return false;
	}
	const long long bytes = static_cast<long long>(raw->w) * raw->h * 4LL;
	if (bytes > kCalypsoHdImageMaxBytes)
	{
		Log(LOG_WARNING) << "CalypsoHdImageSource: decoded bytes too large " << vfsPath;
		return false;
	}
	// ABGR8888 is R,G,B,A in memory order on little-endian: exactly uploadRGBA.
	std::unique_ptr<SDL_Surface, SdlSurfaceDeleter> rgba(
		SDL_ConvertSurfaceFormat(raw.get(), SDL_PIXELFORMAT_ABGR8888, 0));
	if (!rgba || !rgba->pixels)
	{
		Log(LOG_WARNING) << "CalypsoHdImageSource: convert failed " << vfsPath
			<< ": " << SDL_GetError();
		return false;
	}
	out.w = rgba->w;
	out.h = rgba->h;
	out.px.resize(static_cast<std::size_t>(bytes));
	const std::uint8_t *src = static_cast<const std::uint8_t *>(rgba->pixels);
	for (int y = 0; y < out.h; ++y)
	{
		memcpy(out.px.data() + static_cast<std::size_t>(y) * out.w * 4,
			src + static_cast<std::size_t>(y) * rgba->pitch, static_cast<std::size_t>(out.w) * 4);
	}
	return true;
}

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
