#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoTextEditLayout.h"
#include "CalypsoHdUiOverlay.h"
#include "CommandCenter/CommandCenterRenderer.h"
#include "../Engine/TTFFont.h"
#include "../Engine/Unicode.h"
#include "../Interface/TextList.h"
#include "../Mod/Mod.h"

namespace OpenXcom { namespace Calypso {
/// Wrap a translated report at the same font size used by the physical table.
/// Native list rows own scrolling, even for long words or scripts without spaces.
inline std::vector<std::string> calypsoWrapReportText(Mod &mod, const std::string &text, int width, int bodySize)
{
	if (text.empty()) return {};
	auto *font = mod.getTTFFont("FONT_F33_BODY", false);
	if (!font || font->pixelSize() <= 0)
		CalypsoHdUiOverlay::instance().failHdRoute("Report body font is unavailable");
	const auto value = calypsoNormalizeTextEditNewlines(Unicode::convUtf8ToUtf32(
		CommandCenter::calypsoHdNormalizeTtfDisplayText(text)));
	CalypsoTextEditMetrics metrics;
	if (!font->measureGlyphs(value, metrics.advances, metrics.kerningBefore))
		CalypsoHdUiOverlay::instance().failHdRoute("Report body could not be measured");
	const auto lines = calypsoLayoutTextEdit(value, std::max(1, width - 24), metrics,
		static_cast<float>(bodySize) / font->pixelSize());
	std::vector<std::string> result;
	for (const auto &line : lines.lines)
		result.push_back(Unicode::convUtf32ToUtf8(value.substr(line.start, line.end - line.start)));
	return result;
}

inline void calypsoAppendReportText(TextList &list, Mod &mod, const std::string &label, const std::string &text, int width, int bodySize, int minimumRows = 0)
{
	const auto lines = calypsoWrapReportText(mod, text, width, bodySize);
	if (lines.empty()) return;
	for (int pad = int(lines.size()); pad < minimumRows; ++pad) list.addRow(2, "", "");
	for (size_t i = 0; i < lines.size(); ++i)
		list.addRow(2, i == 0 ? label.c_str() : "", lines[i].c_str());
}
} }
#endif
