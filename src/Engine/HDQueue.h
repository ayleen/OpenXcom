#pragma once
/*
 * Phase 6a.1 — HD overlay queue.
 *
 * Collects ARGB8888 HD surfaces that need to be composited over the final
 * _screen after the 8bpp→ARGB blit in Screen::flip(). Surfaces are pushed
 * during the normal blit traversal and flushed onto _screen just before
 * the streaming-texture upload.
 *
 * Ownership contract: callers guarantee that pushed surfaces outlive the
 * next flush() call. In 6a.1 all pushed surfaces are long-lived mod assets.
 * Phase 6b introduces temporary surfaces; that plan handles arena ownership.
 *
 * Intentionally unused in non-Emscripten builds — native uses OpenGL.
 */
#ifdef __EMSCRIPTEN__

#include <SDL.h>
#include <vector>

namespace OpenXcom
{
namespace HDQueue
{

struct Overlay
{
	SDL_Surface *src;
	SDL_Rect     dst;
};

inline std::vector<Overlay> &get()
{
	static std::vector<Overlay> q;
	return q;
}

inline void push(SDL_Surface *src, SDL_Rect dst)
{
	get().push_back({ src, dst });
}

inline void flush(SDL_Surface *target)
{
	for (auto &ov : get())
	{
		SDL_Rect dst = ov.dst;
		SDL_BlitSurface(ov.src, nullptr, target, &dst);
	}
	get().clear();
}

} /* namespace HDQueue */
} /* namespace OpenXcom */

#endif /* __EMSCRIPTEN__ */
