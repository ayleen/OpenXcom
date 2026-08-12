#pragma once
/* Phase 46.1.5 -- pure browser HD resolution-floor calculation. */
#include <array>
#include <string>

#include "../Engine/Options.h"

namespace OpenXcom
{
namespace Calypso
{

static const int CALYPSO_HD_MIN_WIDTH = 740;
static const int CALYPSO_HD_MIN_HEIGHT = 360;
static const int CALYPSO_BROWSER_SCALE_COUNT = 5;
static const int CALYPSO_SCALE_TYPE_COUNT = SCALE_SCREEN_3_4 + 1;

struct CalypsoScaleResult
{
	int scaleType = 0;
	int width = 0;
	int height = 0;
	bool eligible = false;
};

struct CalypsoScaleOptionModel
{
	std::array<CalypsoScaleResult, CALYPSO_BROWSER_SCALE_COUNT> options;
	std::array<int, CALYPSO_SCALE_TYPE_COUNT> scaleToOption;
	int geoscapeSelection = 0;
	int battlescapeSelection = 0;
};

/// The browser canvas already has square CSS/backing-store pixels. The native
/// 320x200 correction is meaningless there, but old options.cfg files may
/// still carry it from another build. Normalize it away before any browser
/// scale calculation so a saved setting cannot shrink the logical height.
inline bool calypsoNormalizeBrowserNonSquarePixels(bool /*storedValue*/)
{
	return false;
}

inline void calypsoScaleFraction(int scaleType, int& numerator, int& denominator)
{
	switch (scaleType)
	{
	case SCALE_SCREEN:        numerator = 1; denominator = 1; break;
	case SCALE_SCREEN_3_4:    numerator = 3; denominator = 4; break;
	case SCALE_SCREEN_DIV_2:  numerator = 1; denominator = 2; break;
	case SCALE_SCREEN_DIV_3:  numerator = 1; denominator = 3; break;
	case SCALE_SCREEN_DIV_4:  numerator = 1; denominator = 4; break;
	case SCALE_SCREEN_DIV_5:  numerator = 1; denominator = 5; break;
	case SCALE_SCREEN_DIV_6:  numerator = 1; denominator = 6; break;
	case SCALE_SCREEN_DIV_8:  numerator = 1; denominator = 8; break;
	case SCALE_SCREEN_DIV_10: numerator = 1; denominator = 10; break;
	default: numerator = 1; denominator = 2; break;
	}
}

inline CalypsoScaleResult calypsoEvaluateScale(int displayWidth, int displayHeight,
	                                            bool nonSquarePixels, int scaleType)
{
	int numerator = 1, denominator = 1;
	calypsoScaleFraction(scaleType, numerator, denominator);
	CalypsoScaleResult result;
	result.scaleType = scaleType;
	result.width = displayWidth > 0 ? displayWidth * numerator / denominator : 0;
	// Browser backing-store pixels are square. Ignore a stale native-only
	// nonSquarePixelRatio value even before Options has persisted its normalized
	// false value; every browser caller therefore receives the same safe result.
	const double pixelRatioY = calypsoNormalizeBrowserNonSquarePixels(nonSquarePixels)
		? 1.2 : 1.0;
	result.height = displayHeight > 0
		? static_cast<int>(displayHeight / pixelRatioY * numerator / denominator) : 0;
	result.eligible = result.width >= CALYPSO_HD_MIN_WIDTH
	               && result.height >= CALYPSO_HD_MIN_HEIGHT;
	return result;
}

/// Maps every persisted ScaleType onto the browser's supported proportional
/// ladder. Keep browser runtime migration and the Video Options combobox
/// grounded in this one mapping so legacy values cannot disagree between the
/// two surfaces.
inline int calypsoSupportedScaleType(int scaleType)
{
	switch (scaleType)
	{
	case SCALE_SCREEN:       return SCALE_SCREEN;
	case SCALE_SCREEN_3_4:   return SCALE_SCREEN_3_4;
	case SCALE_SCREEN_DIV_2: return SCALE_SCREEN_DIV_2;
	case SCALE_SCREEN_DIV_3: return SCALE_SCREEN_DIV_3;
	case SCALE_SCREEN_DIV_4:
	case SCALE_SCREEN_DIV_5:
	case SCALE_SCREEN_DIV_6:
	case SCALE_SCREEN_DIV_8:
	case SCALE_SCREEN_DIV_10:
		return SCALE_SCREEN_DIV_4;
	default:
		return SCALE_SCREEN_DIV_2;
	}
}

inline int calypsoScaleLadderIndex(int scaleType)
{
	switch (calypsoSupportedScaleType(scaleType))
	{
	case SCALE_SCREEN:       return 0;
	case SCALE_SCREEN_3_4:   return 1;
	case SCALE_SCREEN_DIV_2: return 2;
	case SCALE_SCREEN_DIV_3: return 3;
	case SCALE_SCREEN_DIV_4: return 4;
	default:                 return 2;
	}
}

inline int calypsoScaleAtLadderIndex(int index)
{
	static const int ladder[5] = {SCALE_SCREEN, SCALE_SCREEN_3_4, SCALE_SCREEN_DIV_2,
	                              SCALE_SCREEN_DIV_3, SCALE_SCREEN_DIV_4};
	return index >= 0 && index < 5 ? ladder[index] : SCALE_SCREEN;
}

inline CalypsoScaleResult calypsoPromoteScale(int displayWidth, int displayHeight,
	                                           bool nonSquarePixels, int scaleType)
{
	const int selected = calypsoScaleLadderIndex(scaleType);
	for (int i = selected; i >= 0; --i)
	{
		CalypsoScaleResult result = calypsoEvaluateScale(
			displayWidth, displayHeight, nonSquarePixels, calypsoScaleAtLadderIndex(i));
		if (result.eligible) return result;
	}
	// Full is the truthful proportional result even when the CSS gate must block.
	return calypsoEvaluateScale(displayWidth, displayHeight, nonSquarePixels, SCALE_SCREEN);
}

inline void calypsoNormalizeBrowserScaleSnapshot(
	int displayWidth, int displayHeight, bool nonSquarePixels,
	int& geoscapeLive, int& geoscapePending,
	int& battlescapeLive, int& battlescapePending)
{
	geoscapeLive = calypsoPromoteScale(
		displayWidth, displayHeight, nonSquarePixels, geoscapeLive).scaleType;
	geoscapePending = calypsoPromoteScale(
		displayWidth, displayHeight, nonSquarePixels, geoscapePending).scaleType;
	battlescapeLive = calypsoPromoteScale(
		displayWidth, displayHeight, nonSquarePixels, battlescapeLive).scaleType;
	battlescapePending = calypsoPromoteScale(
		displayWidth, displayHeight, nonSquarePixels, battlescapePending).scaleType;
}

inline CalypsoScaleOptionModel calypsoBuildScaleOptionModel(
	int displayWidth, int displayHeight, bool nonSquarePixels,
	int geoscapePending, int battlescapePending)
{
	CalypsoScaleOptionModel model;
	for (int i = 0; i < CALYPSO_BROWSER_SCALE_COUNT; ++i)
	{
		model.options[i] = calypsoEvaluateScale(
			displayWidth, displayHeight, nonSquarePixels, calypsoScaleAtLadderIndex(i));
	}
	for (int scaleType = SCALE_ORIGINAL; scaleType < CALYPSO_SCALE_TYPE_COUNT; ++scaleType)
	{
		model.scaleToOption[scaleType] = calypsoScaleLadderIndex(scaleType);
	}
	model.geoscapeSelection = calypsoScaleLadderIndex(calypsoPromoteScale(
		displayWidth, displayHeight, nonSquarePixels, geoscapePending).scaleType);
	model.battlescapeSelection = calypsoScaleLadderIndex(calypsoPromoteScale(
		displayWidth, displayHeight, nonSquarePixels, battlescapePending).scaleType);
	return model;
}

inline std::string calypsoScaleOptionLabel(
	const CalypsoScaleResult& option, const std::string& minimumLabel)
{
	std::string label = std::to_string(option.width) + "x" + std::to_string(option.height);
	if (!option.eligible && !minimumLabel.empty()) label += " — " + minimumLabel;
	return label;
}

inline bool calypsoUsableViewportSupported(int logicalWidth, int logicalHeight,
	                                        int top, int right, int bottom, int left)
{
	const long long usableWidth = static_cast<long long>(logicalWidth)
		- (left > 0 ? left : 0) - (right > 0 ? right : 0);
	const long long usableHeight = static_cast<long long>(logicalHeight)
		- (top > 0 ? top : 0) - (bottom > 0 ? bottom : 0);
	return usableWidth >= CALYPSO_HD_MIN_WIDTH && usableHeight >= CALYPSO_HD_MIN_HEIGHT;
}

} // namespace Calypso
} // namespace OpenXcom
