/* Shared full-screen HD renderer; see CalypsoHdScreenRenderer.h. */
#ifdef __EMSCRIPTEN__

#include "CalypsoHdScreenRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Mod/Mod.h"

#include "CalypsoF21UiShared.h"
#include "CalypsoHdFontSource.h"
#include "CalypsoHdTheme.h"
#include "CalypsoHdUiOverlay.h"

namespace OpenXcom
{
namespace Calypso
{

namespace
{

constexpr std::uint32_t kScreenFamilyId = 61;
constexpr int kCompactNotificationBodyHeightPx = 31;

const CalypsoHdScreenRegionVisual* findRegion(
	const CalypsoHdScreenRenderModel& model, const char* id)
{
	for (const auto& region : model.regions)
		if (region.id == id) return &region;
	return nullptr;
}

std::string copyValue(const CalypsoHdScreenRenderModel& model, const char* key)
{
	for (const auto& item : model.copy)
		if (item.key == key) return item.value;
	return std::string();
}

CalypsoF21Rect designRect(const CalypsoHdScreenRect& rect)
{
	return { rect.x, rect.y, rect.w, rect.h };
}

CalypsoHdPanelStyle screenPanelStyle(std::uint32_t border, std::uint32_t top,
	std::uint32_t bottom, float radius, float glow = 0.0f)
{
	CalypsoHdPanelStyle style;
	style.styled = true;
	style.radiusPx = radius;
	style.borderWidthPx = border ? 1.0f : 0.0f;
	style.borderColorRgba = border;
	style.fillTopRgba = top;
	style.fillBottomRgba = bottom;
	style.glowRgba = CalypsoHdThemeGen::kAccentSoft;
	style.glowRadiusPx = glow;
	return style;
}

std::string compactGlyph(const CalypsoHdScreenActionVisual& action)
{
	if (action.slotRole == "world-zoom-in") return "+";
	if (action.slotRole == "world-recenter") return "O";
	if (action.slotRole == "world-zoom-out") return "-";
	if (action.slotRole == "notification-open") return ">";
	if (action.slotRole == "session-action") return "MENU";
	if (action.slotRole == "time-pause") return "II";
	return action.label;
}

} // namespace

CalypsoHdScreenRenderer::CalypsoHdScreenRenderer(
	const void* state, CalypsoHdScreenRenderModel model)
	: _state(state), _model(std::move(model))
{
}

CalypsoHdScreenRenderer::~CalypsoHdScreenRenderer()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

const void* CalypsoHdScreenRenderer::topState() const
{
	return _state;
}

void CalypsoHdScreenRenderer::setModel(CalypsoHdScreenRenderModel model)
{
	_model = std::move(model);
}

void CalypsoHdScreenRenderer::collect(CalypsoHdFrameBuilder& builder) const
{
	const CalypsoHdScreenRenderModel& model = _model;
	if (!_state || model.designWidth <= 0 || model.designHeight <= 0) return;
	if (model.archetype != "strategic-command-shell") return;

	Game* game = getCurrentGame();
	Mod* mod = game ? game->getMod() : nullptr;
	CalypsoTtfSourceDescriptor heading;
	CalypsoTtfSourceDescriptor body;
	CalypsoTtfSourceDescriptor mono;
	if (!calypsoHdResolveFontDescriptor(mod, "FONT_F34_SAIRA_700", heading)) return;
	if (!calypsoHdResolveFontDescriptor(mod, "FONT_F33_BODY", body)) return;
	if (!calypsoHdResolveFontDescriptor(mod, "FONT_F34_MONO", mono)) return;

	const int availableWidth = model.sideBySidePreview
		? Options::baseXResolution / 2
		: Options::baseXResolution;
	const double scale = std::min(
		(double)availableWidth / model.designWidth,
		(double)Options::baseYResolution / model.designHeight);
	const int logicalWidth = std::max(1, (int)std::llround(model.designWidth * scale));
	const int logicalHeight = std::max(1, (int)std::llround(model.designHeight * scale));
	const int originX = (availableWidth - logicalWidth) / 2;
	const int originY = (Options::baseYResolution - logicalHeight) / 2;
	const CalypsoHdPresentationMetrics& metrics = CalypsoHdUiOverlay::instance().frozenMetrics();

	builder.beginSubgroup();
	CalypsoF21Painter painter{ builder, kScreenFamilyId,
		reinterpret_cast<std::uintptr_t>(_state), 0, 1.0f, 1.0,
		CalypsoF21Rect{ originX, originY, logicalWidth, logicalHeight },
		metrics.scaleX, metrics.scaleY };
	painter.winLogical = { originX, originY, logicalWidth, logicalHeight };
	painter.windowDesign = { 0, 0, model.designWidth, model.designHeight };
	painter.uiScale = scale;

	std::uint32_t role = 1;
	painter.styled(painter.winLogical,
		screenPanelStyle(0, CalypsoHdThemeGen::kDialogFillTop,
			CalypsoHdThemeGen::kDialogFillBottom, 0.0f), nullptr, role++);

	// Deterministic sparse starfield. Positions are archetype-owned fractions,
	// projected through the generated design canvas rather than screen pixels.
	for (int index = 0; index < 42; ++index)
	{
		const int x = (index * 211 + 37) % model.designWidth;
		const int y = (index * 137 + 19) % model.designHeight;
		painter.decoration(painter.project({ x, y, index % 5 == 0 ? 2 : 1, index % 7 == 0 ? 2 : 1 }),
			index % 4 == 0 ? CalypsoHdThemeGen::kAccent : CalypsoHdThemeGen::kNearWhite, role++);
	}

	if (const auto* world = findRegion(model, "world"))
	{
		const int diameter = std::min(world->rect.w, world->rect.h);
		const CalypsoF21Rect globe{
			world->rect.x + (world->rect.w - diameter) / 2,
			world->rect.y + (world->rect.h - diameter) / 2,
			diameter, diameter };
		painter.styled(painter.project(globe),
			screenPanelStyle(CalypsoHdThemeGen::kAccent,
				CalypsoHdThemeGen::kSafeRestFill, CalypsoHdThemeGen::kDialogFillBottom,
				diameter / 2.0f, 18.0f), nullptr, role++);
		for (int ring = 1; ring <= 4; ++ring)
		{
			const int inset = diameter * ring / 10;
			painter.styled(painter.project({ globe.x + inset, globe.y + inset,
				globe.width - inset * 2, globe.height - inset * 2 }),
				screenPanelStyle(CalypsoHdThemeGen::kAccentSoft,
					calypsoRgba(0, 0, 0, 0), calypsoRgba(0, 0, 0, 0),
					(globe.width - inset * 2) / 2.0f), nullptr, role++);
		}
	}

	if (const auto* status = findRegion(model, "status"))
	{
		const CalypsoLogicalRect rect = painter.project(designRect(status->rect));
		painter.styled(rect, screenPanelStyle(CalypsoHdThemeGen::kAccentSoft,
			CalypsoHdThemeGen::kDialogFillTop, CalypsoHdThemeGen::kDialogFillBottom,
			status->rect.h / 2.0f), nullptr, role++);
		const std::string statusText = copyValue(model, "time") + "  |  "
			+ copyValue(model, "date") + "  |  " + copyValue(model, "funds");
		painter.textRect(rect, nullptr, mono, statusText, CalypsoHdThemeGen::kNearWhite,
			CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, 1, role++, 0.04,
			model.designHeight > 360 ? 13.0 : 11.0);
	}

	if (const auto* notification = findRegion(model, "notification"))
	{
		const CalypsoLogicalRect rect = painter.project(designRect(notification->rect));
		painter.styled(rect, screenPanelStyle(CalypsoHdThemeGen::kGold,
			CalypsoHdThemeGen::kDialogFillTop, CalypsoHdThemeGen::kDialogFillBottom,
			CalypsoHdTheme::kButtonRadiusPx), nullptr, role++);
		const int inset = model.designHeight > 360 ? 12 : 8;
		const int bodyHeight = model.designHeight > 360
			? notification->rect.h / 2 - inset
			: kCompactNotificationBodyHeightPx;
		painter.textRect(painter.project({ notification->rect.x + inset,
			notification->rect.y + inset, notification->rect.w - 58,
			notification->rect.h / 3 }), nullptr, mono,
			copyValue(model, "notificationTitle"), CalypsoHdThemeGen::kGold,
			CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, role++, 0.08,
			model.designHeight > 360 ? 12.0 : 9.0);
		painter.textRect(painter.project({ notification->rect.x + inset,
			notification->rect.y + notification->rect.h / 2 - 2,
			notification->rect.w - 58, bodyHeight }), nullptr,
			body, copyValue(model, "notificationBody"), kF21MutedBodyRgba,
			CalypsoHdHAlign::Left, CalypsoHdVAlign::Top, 2, role++, 0.0,
			model.designHeight > 360 ? 10.0 : 8.0);
	}

	if (const auto* time = findRegion(model, "timeControl"))
	{
		painter.styled(painter.project(designRect(time->rect)),
			screenPanelStyle(CalypsoHdThemeGen::kAccentSoft,
				calypsoRgba(0x05, 0x0F, 0x14, 0x70), calypsoRgba(0x05, 0x0F, 0x14, 0x18),
				time->rect.h / 2.0f), nullptr, role++);
	}

	for (const auto& action : model.actions)
	{
		const CalypsoLogicalRect rect = painter.project(designRect(action.visible));
		const bool selected = action.id == model.selectedActionId;
		CalypsoHdPanelStyle style = f21QuietButtonStyle(
			selected ? CalypsoInteractionState::Focus : CalypsoInteractionState::Rest);
		if (action.component == "command-icon-action" || action.slotRole == "time-pause")
			style.radiusPx = action.visible.h / 2.0f;
		if (action.component == "notification-action")
			style.borderColorRgba = CalypsoHdThemeGen::kGold;
		painter.styled(rect, style, nullptr, role++);
		painter.textRect(rect, nullptr,
			action.component == "command-icon-action" ? heading : mono,
			compactGlyph(action), CalypsoHdThemeGen::kNearWhite,
			CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, 1, role++,
			action.component == "command-icon-action" ? CalypsoHdTheme::kLabelTrackingEm : 0.04,
			model.designHeight > 360 ? 12.0 : 9.0);
	}
}

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
