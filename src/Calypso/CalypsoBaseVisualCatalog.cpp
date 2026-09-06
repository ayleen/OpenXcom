// F01 visual catalog loader (T14): FileMap read plus strict parse.
// Whole-file Emscripten guard (Phase 36); parsing stays native-testable.
#ifdef __EMSCRIPTEN__

#include "CalypsoBaseVisualCatalog.h"

#include <iterator>

#include "../Engine/FileMap.h"
#include "../Engine/Logger.h"

namespace OpenXcom
{
namespace Calypso
{

bool calypsoLoadBaseVisualCatalog(CalypsoBaseVisualCatalog &out, std::string &error)
{
	out = CalypsoBaseVisualCatalog{};
	auto stream = FileMap::getIStream(kCalypsoBaseCatalogPath);
	if (!stream)
	{
		error = "catalog missing: Resources/basescape/catalog.json";
		return false;
	}
	std::string text((std::istreambuf_iterator<char>(*stream)), std::istreambuf_iterator<char>());
	if (!calypsoParseBaseVisualCatalog(text, out, error))
	{
		Log(LOG_WARNING) << "CalypsoBaseVisualCatalog: invalid catalog: " << error;
		return false;
	}
	return true;
}

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
