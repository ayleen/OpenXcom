#pragma once

#include <cctype>
#include <cstddef>
#include <string>

namespace OpenXcom
{
namespace Calypso
{

// Kept independent of SDL and Emscripten so the pacing contract can be
// covered by the native unit-test target too.  The formula is deliberately
// conservative: a short line is visible for at least three seconds, then gets
// a 1.2 second lead-in plus reading time at 180 words/minute.
inline std::size_t radioWordCount(const std::string &text)
{
	std::size_t words = 0;
	bool inWord = false;
	for (unsigned char c : text)
	{
		if (std::isspace(c)) inWord = false;
		else if (!inWord) { ++words; inWord = true; }
	}
	return words;
}

inline unsigned radioNarrativeDurationMs(const std::string &text)
{
	const unsigned readingMs = static_cast<unsigned>((radioWordCount(text) * 60000u + 179u) / 180u);
	const unsigned paced = 1200u + readingMs;
	return paced < 3000u ? 3000u : paced;
}

} // namespace Calypso
} // namespace OpenXcom
