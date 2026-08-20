#pragma once

#include "CalypsoUiMetrics.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace OpenXcom
{
namespace Calypso
{

struct GeoscapeHdSize
{
	int width = 0;
	int height = 0;
};

struct GeoscapeHdRect
{
	int x = 0;
	int y = 0;
	int width = 0;
	int height = 0;

	bool operator==(const GeoscapeHdRect& other) const
	{
		return x == other.x && y == other.y && width == other.width && height == other.height;
	}

	bool intersects(const GeoscapeHdRect& other) const
	{
		return x < other.x + other.width && other.x < x + width
			&& y < other.y + other.height && other.y < y + height;
	}
};

enum class GeoscapeHdComponent : std::size_t
{
	WorldBackground,
	Session,
	StatusPrimary,
	WorldInteraction,
	MarkerContact,
	MarkerBase,
	WorldTools,
	ZoomIn,
	Recenter,
	ZoomOut,
	ActionBases,
	ActionGraphs,
	ActionFundingOrExtended,
	ActionIntercept,
	ActionUfopaedia,
	ActionOptions,
	NotificationContact,
	NotificationOpen,
	TimeControl,
	TimePause,
	Speed5Seconds,
	Speed1Minute,
	Speed5Minutes,
	Speed30Minutes,
	Speed1Hour,
	Speed1Day,
	Count
};

constexpr std::size_t GEOSCAPE_HD_COMPONENT_COUNT = static_cast<std::size_t>(GeoscapeHdComponent::Count);

constexpr std::array<GeoscapeHdRect, GEOSCAPE_HD_COMPONENT_COUNT> GEOSCAPE_HD_DESKTOP_RECTS{{
	{0, 0, 1280, 720}, {18, 16, 122, 46}, {400, 16, 480, 46}, {172, 70, 936, 552},
	{450, 225, 44, 44}, {812, 382, 44, 44}, {176, 474, 48, 132}, {177, 475, 46, 44},
	{177, 519, 46, 44}, {177, 563, 46, 44}, {24, 138, 120, 84}, {24, 242, 120, 84},
	{24, 346, 120, 84}, {1136, 138, 120, 84}, {1136, 242, 120, 84}, {1136, 346, 120, 84},
	{936, 552, 320, 76}, {1211, 568, 44, 44}, {280, 634, 720, 72}, {280, 638, 64, 64},
	{362, 650, 105, 54}, {467, 650, 105, 54}, {572, 650, 105, 54}, {677, 650, 105, 54},
	{782, 650, 105, 54}, {887, 650, 105, 54}
}};

constexpr std::array<GeoscapeHdRect, GEOSCAPE_HD_COMPONENT_COUNT> GEOSCAPE_HD_COMPACT_RECTS{{
	{0, 0, 740, 360}, {8, 8, 44, 44}, {171, 8, 350, 38}, {0, 0, 520, 296},
	{232, 114, 44, 44}, {376, 196, 44, 44}, {464, 156, 44, 126}, {465, 157, 44, 44},
	{465, 201, 44, 44}, {465, 245, 44, 44}, {524, 54, 96, 66}, {524, 128, 96, 66},
	{524, 202, 96, 66}, {628, 54, 96, 66}, {628, 128, 96, 66}, {628, 202, 96, 66},
	{12, 62, 194, 60}, {161, 70, 44, 44}, {12, 296, 716, 56}, {12, 299, 50, 50},
	{74, 303, 109, 48}, {183, 303, 109, 48}, {292, 303, 109, 48}, {401, 303, 109, 48},
	{510, 303, 109, 48}, {619, 303, 109, 48}
}};

class GeoscapeHdLayout
{
public:
	GeoscapeHdLayout(int viewportWidth, int viewportHeight, GeoscapeHdRect safe,
		CalypsoLayoutClass layoutClass,
		std::array<GeoscapeHdRect, GEOSCAPE_HD_COMPONENT_COUNT> rects)
		: _viewportWidth(viewportWidth), _viewportHeight(viewportHeight), _safe(safe),
		  _layoutClass(layoutClass), _rects(rects) {}

	const GeoscapeHdRect& rect(GeoscapeHdComponent id) const
	{
		return _rects[static_cast<std::size_t>(id)];
	}

	const GeoscapeHdRect& safeRect() const { return _safe; }
	CalypsoLayoutClass layoutClass() const { return _layoutClass; }

	bool insideViewport() const
	{
		for (const auto& r : _rects)
		{
			if (r.x < 0 || r.y < 0 || r.width < 0 || r.height < 0) return false;
			if (r.x + r.width > _viewportWidth || r.y + r.height > _viewportHeight) return false;
		}
		return true;
	}

	int minimumInteractiveExtent() const
	{
		constexpr std::array<GeoscapeHdComponent, 21> interactive{{
			GeoscapeHdComponent::Session, GeoscapeHdComponent::WorldInteraction,
			GeoscapeHdComponent::MarkerContact, GeoscapeHdComponent::MarkerBase,
			GeoscapeHdComponent::ZoomIn, GeoscapeHdComponent::Recenter, GeoscapeHdComponent::ZoomOut,
			GeoscapeHdComponent::ActionBases, GeoscapeHdComponent::ActionGraphs,
			GeoscapeHdComponent::ActionFundingOrExtended, GeoscapeHdComponent::ActionIntercept,
			GeoscapeHdComponent::ActionUfopaedia, GeoscapeHdComponent::ActionOptions,
			GeoscapeHdComponent::NotificationOpen, GeoscapeHdComponent::TimePause,
			GeoscapeHdComponent::Speed5Seconds, GeoscapeHdComponent::Speed1Minute,
			GeoscapeHdComponent::Speed5Minutes, GeoscapeHdComponent::Speed30Minutes,
			GeoscapeHdComponent::Speed1Hour, GeoscapeHdComponent::Speed1Day
		}};
		int result = 0x7fffffff;
		for (const auto id : interactive)
		{
			const auto& r = rect(id);
			result = std::min(result, std::min(r.width, r.height));
		}
		return result;
	}

private:
	int _viewportWidth;
	int _viewportHeight;
	GeoscapeHdRect _safe;
	CalypsoLayoutClass _layoutClass;
	std::array<GeoscapeHdRect, GEOSCAPE_HD_COMPONENT_COUNT> _rects;
};

inline GeoscapeHdRect calypsoGeoscapeScaleRect(
	const GeoscapeHdRect& source, double scale, int offsetX, int offsetY)
{
	const auto round = [](double value) { return static_cast<int>(value + 0.5); };
	return {offsetX + round(source.x * scale), offsetY + round(source.y * scale),
		round(source.width * scale), round(source.height * scale)};
}

inline GeoscapeHdLayout calypsoGeoscapeHdLayout(
	int viewportWidth, int viewportHeight, CalypsoSafeInsets insets)
{
	const auto metrics = calypsoComputeMetrics(
		viewportWidth, viewportHeight, insets, CalypsoVisualContext::Strategic);
	const GeoscapeHdRect safe{metrics.safeX, metrics.safeY, metrics.safeWidth, metrics.safeHeight};
	std::array<GeoscapeHdRect, GEOSCAPE_HD_COMPONENT_COUNT> result{};
	const auto set = [&result](GeoscapeHdComponent id, GeoscapeHdRect rect)
	{
		result[static_cast<std::size_t>(id)] = rect;
	};

	if (metrics.layoutClass == CalypsoLayoutClass::Wide)
	{
		const int x = metrics.safeX;
		const int y = metrics.safeY;
		const int w = metrics.safeWidth;
		const int h = metrics.safeHeight;
		set(GeoscapeHdComponent::WorldBackground, safe);
		set(GeoscapeHdComponent::Session, {x + 18, y + 16, 122, 46});
		set(GeoscapeHdComponent::StatusPrimary, {x + (w - 480) / 2, y + 16, 480, 46});

		const GeoscapeHdRect world{x + 172, y + 70, w - 344, h - 168};
		set(GeoscapeHdComponent::WorldInteraction, world);
		const auto projectMarker = [&world](const GeoscapeHdRect& authored)
		{
			const auto round = [](double value) { return static_cast<int>(value + 0.5); };
			return GeoscapeHdRect{
				world.x + round((authored.x - 172) * static_cast<double>(world.width) / 936),
				world.y + round((authored.y - 70) * static_cast<double>(world.height) / 552),
				authored.width, authored.height};
		};
		set(GeoscapeHdComponent::MarkerContact,
			projectMarker(GEOSCAPE_HD_DESKTOP_RECTS[static_cast<std::size_t>(GeoscapeHdComponent::MarkerContact)]));
		set(GeoscapeHdComponent::MarkerBase,
			projectMarker(GEOSCAPE_HD_DESKTOP_RECTS[static_cast<std::size_t>(GeoscapeHdComponent::MarkerBase)]));

		const int toolsY = y + h - 246;
		set(GeoscapeHdComponent::WorldTools, {x + 176, toolsY, 48, 132});
		set(GeoscapeHdComponent::ZoomIn, {x + 177, toolsY + 1, 46, 44});
		set(GeoscapeHdComponent::Recenter, {x + 177, toolsY + 45, 46, 44});
		set(GeoscapeHdComponent::ZoomOut, {x + 177, toolsY + 89, 46, 44});

		set(GeoscapeHdComponent::ActionBases, {x + 24, y + 138, 120, 84});
		set(GeoscapeHdComponent::ActionGraphs, {x + 24, y + 242, 120, 84});
		set(GeoscapeHdComponent::ActionFundingOrExtended, {x + 24, y + 346, 120, 84});
		const int rightActionX = x + w - 144;
		set(GeoscapeHdComponent::ActionIntercept, {rightActionX, y + 138, 120, 84});
		set(GeoscapeHdComponent::ActionUfopaedia, {rightActionX, y + 242, 120, 84});
		set(GeoscapeHdComponent::ActionOptions, {rightActionX, y + 346, 120, 84});

		set(GeoscapeHdComponent::NotificationContact, {x + w - 344, y + h - 168, 320, 76});
		set(GeoscapeHdComponent::NotificationOpen, {x + w - 69, y + h - 152, 44, 44});
		const int timeX = x + (w - 720) / 2;
		const int timeY = y + h - 86;
		set(GeoscapeHdComponent::TimeControl, {timeX, timeY, 720, 72});
		set(GeoscapeHdComponent::TimePause, {timeX, timeY + 4, 64, 64});
		constexpr std::array<GeoscapeHdComponent, 6> speedIds{{
			GeoscapeHdComponent::Speed5Seconds, GeoscapeHdComponent::Speed1Minute,
			GeoscapeHdComponent::Speed5Minutes, GeoscapeHdComponent::Speed30Minutes,
			GeoscapeHdComponent::Speed1Hour, GeoscapeHdComponent::Speed1Day
		}};
		for (std::size_t i = 0; i < speedIds.size(); ++i)
			set(speedIds[i], {timeX + 82 + static_cast<int>(i) * 105, timeY + 16, 105, 54});
	}
	else
	{
		const double scale = std::min(
			static_cast<double>(metrics.safeWidth) / 740,
			static_cast<double>(metrics.safeHeight) / 360);
		const int renderedWidth = static_cast<int>(740 * scale + 0.5);
		const int renderedHeight = static_cast<int>(360 * scale + 0.5);
		const int offsetX = metrics.safeX + (metrics.safeWidth - renderedWidth) / 2;
		const int offsetY = metrics.safeY + (metrics.safeHeight - renderedHeight) / 2;

		for (std::size_t i = 0; i < result.size(); ++i)
			result[i] = calypsoGeoscapeScaleRect(GEOSCAPE_HD_COMPACT_RECTS[i], scale, offsetX, offsetY);
	}

	return GeoscapeHdLayout(viewportWidth, viewportHeight, safe, metrics.layoutClass, result);
}

} // namespace Calypso
} // namespace OpenXcom
