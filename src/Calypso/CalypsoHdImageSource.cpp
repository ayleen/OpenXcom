// F01 RGBA image source (T05): FileMap + SDL_image PNG decode to RGBA8.
// No palette path anywhere: HD art must never be quantized to 8-bit.
#ifdef __EMSCRIPTEN__

#include "CalypsoHdImageSource.h"
#include "CalypsoHdImageModel.h"

#include "../Engine/FileMap.h"
#include "../Engine/Game.h"
#include "../Engine/State.h"
#include "../Engine/Surface.h"
#include "../Mod/Mod.h"
#include "../Ufopaedia/ArticleState.h"
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
	// Scripted events name a mod-owned illustration (often a .SCR sprite).
	// Read only the artwork; the HD renderer owns every label and control.
	const std::string prefix = "ruleset-art:";
	const bool articleSprite = vfsPath.rfind("article-sprite:", 0) == 0;
	if (articleSprite || vfsPath.compare(0, prefix.size(), prefix) == 0)
	{
		auto *game = getCurrentGame();
		auto *state = game ? game->getTopState() : nullptr;
		if (!state) return false;
		Surface *art = nullptr;
		if (articleSprite)
		{
			auto *article = dynamic_cast<ArticleState *>(state);
			if (article) art = article->articleSpriteFor(vfsPath);
		}
		else art = game->getMod()->getSurface(vfsPath.substr(prefix.size()), false);
		if (!art || !art->getSurface()) return false;
		const auto *src = art->getSurface();
		if (src->w <= 0 || src->h <= 0 || src->w > kCalypsoHdImageMaxExtent || src->h > kCalypsoHdImageMaxExtent
			|| static_cast<int64_t>(src->w) * src->h * 4 > kCalypsoHdImageMaxBytes) return false;
		out.w = src->w;
		out.h = src->h;
		out.px.resize(static_cast<size_t>(src->w) * src->h * 4);
		if (src->format->BytesPerPixel == 1)
		{
			const auto *palette = state->getPalette();
			if (!palette) return false;
			for (int y = 0; y < src->h; ++y)
				for (int x = 0; x < src->w; ++x)
				{
					const auto index = art->getPixel(x, y);
					const auto &color = palette[index];
					const size_t i = (static_cast<size_t>(y) * src->w + x) * 4;
					out.px[i] = color.r;
					out.px[i + 1] = color.g;
					out.px[i + 2] = color.b;
					out.px[i + 3] = articleSprite && index == 0 ? 0 : 255;
				}
			return true;
		}
		return SDL_ConvertPixels(src->w, src->h, src->format->format, src->pixels, src->pitch,
			SDL_PIXELFORMAT_ABGR8888, out.px.data(), out.w * 4) == 0;
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
