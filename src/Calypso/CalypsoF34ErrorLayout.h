#pragma once
/*
 * F34.ErrorPopup compatibility view over the canonical small-confirmation
 * contract. Geometry is generated from FormConfigs/f34-error.json; this file
 * only adapts the generated names to the existing native layout consumers.
 */
#include <initializer_list>

#include "CalypsoUiMetrics.h"
#include "Generated/CalypsoF34Error.generated.h"

namespace OpenXcom
{
namespace Calypso
{

struct CalypsoF34Rect
{
	int x = 0;
	int y = 0;
	int width = 0;
	int height = 0;
};

inline CalypsoF34Rect calypsoF34Rect(const CalypsoF34ErrorGen::CalypsoF34ErrorGenRect& rect)
{
	return { rect.x, rect.y, rect.w, rect.h };
}

struct CalypsoF34ErrorLayout
{
	int designWidth = 0;
	int designHeight = 0;
	CalypsoF34Rect window;
	CalypsoF34Rect status;
	CalypsoF34Rect iconPanel;
	CalypsoF34Rect icon;
	CalypsoF34Rect warning;
	CalypsoF34Rect message;
	CalypsoF34Rect messageDetail;
	CalypsoF34Rect footer;
	CalypsoF34Rect acknowledge;
};

inline CalypsoF34ErrorLayout calypsoF34ErrorLayout(CalypsoLayoutClass layoutClass)
{
	const int designWidth = layoutClass == CalypsoLayoutClass::Wide ? 1280 : 740;
	const int designHeight = layoutClass == CalypsoLayoutClass::Wide ? 720 : 360;
	const auto* generated = CalypsoF34ErrorGen::layoutForDesign(designWidth, designHeight);
	const int index = layoutClass == CalypsoLayoutClass::Wide ? 0 : 1;
	const auto& button = CalypsoF34ErrorGen::kButtonRects[index][0].rect;
	if (!generated)
		return {};
	return {
		generated->designWidth,
		generated->designHeight,
		calypsoF34Rect(generated->window),
		calypsoF34Rect(generated->status),
		calypsoF34Rect(generated->warning),
		calypsoF34Rect(generated->warning),
		calypsoF34Rect(generated->title),
		calypsoF34Rect(generated->message),
		{},
		calypsoF34Rect(generated->footer),
		calypsoF34Rect(button)};
}

inline void calypsoF34ErrorApplyHarnessShift(
	CalypsoF34ErrorLayout& layout, bool sideBySide)
{
	if (!sideBySide || layout.designWidth != 1280) return;
	const int dx = 40 - layout.window.x;
	for (CalypsoF34Rect* rect : {
		&layout.window, &layout.status, &layout.iconPanel, &layout.icon,
		&layout.warning, &layout.message, &layout.messageDetail,
		&layout.footer, &layout.acknowledge})
	{
		rect->x += dx;
	}
}

} // namespace Calypso
} // namespace OpenXcom
