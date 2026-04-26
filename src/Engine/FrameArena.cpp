/*
 * Phase 6b — Per-frame surface arena implementation.
 */
#ifdef __EMSCRIPTEN__

#include "FrameArena.h"
#include "Exception.h"

namespace OpenXcom
{

SDL_Surface *FrameArena::alloc(int w, int h)
{
	// SDL_CreateRGBSurfaceWithFormat allocates pixel memory internally.
	// SDL_FreeSurface (called by UniqueSurfaceDeleter) frees both.
	SDL_Surface *raw = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
	if (!raw)
		throw Exception(SDL_GetError());

	SDL_SetSurfaceBlendMode(raw, SDL_BLENDMODE_BLEND);
	SDL_FillRect(raw, nullptr, 0); // transparent black
	_owned.push_back(Surface::NewSdlSurface(raw));
	return raw;
}

void FrameArena::reset()
{
	_owned.clear();
}

FrameArena &frameArena()
{
	static FrameArena instance;
	return instance;
}

} /* namespace OpenXcom */

#endif /* __EMSCRIPTEN__ */
