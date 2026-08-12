/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * Phase 46.2-HD (Calypso) -- see CalypsoHdFontSource.h.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoHdFontSource.h"

#include "../Engine/TTFFont.h"
#include "../Mod/Mod.h"

namespace OpenXcom
{
namespace Calypso
{

namespace
{
std::uint64_t s_generation = 0;
} // namespace

std::uint64_t calypsoHdFontResourceGeneration()
{
	return s_generation;
}

void calypsoHdBumpFontResourceGeneration()
{
	++s_generation;
}

bool calypsoHdResolveFontDescriptor(const Mod* mod, const std::string& fontId,
	CalypsoTtfSourceDescriptor& out)
{
	if (!mod)
	{
		return false;
	}
	TTFFont* f = mod->getTTFFont(fontId, false);
	if (!f)
	{
		return false;
	}
	out.canonicalVfsPath = f->vfsPath();
	out.faceIndex = 0;
	out.logicalDesignSize = f->pixelSize();
	out.resourceGeneration = calypsoHdFontResourceGeneration();
	return true;
}

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
