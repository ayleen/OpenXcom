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
inline void calypsoAppendReportText(TextList &list, Mod &mod, const std::string &label, const std::string &text, int width, int bodySize, int minimumRows = 0)
{
	if (text.empty()) return;
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
	for (int i = static_cast<int>(lines.lines.size()); i < minimumRows; ++i)
		list.addRow(2, "", "");
	for (const auto &line : lines.lines)
		list.addRow(2, line.start == 0 ? label.c_str() : "",
			Unicode::convUtf32ToUtf8(value.substr(line.start, line.end - line.start)).c_str());
}
} }
#endif
