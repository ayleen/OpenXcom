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
#include "../Geoscape/GeoscapeState.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"

#include "CalypsoF21UiShared.h"
#include "CalypsoHdFontSource.h"
#include "CalypsoHdTheme.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoGeoscapeHdRuntime.h"
#include "CalypsoGeoscapeHdShell.h"
#include "CalypsoViewportRuntime.h"

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

CalypsoHdScreenRenderModel CalypsoHdScreenRenderer::liveGeoscapeModel(const GeoscapeState& state)
{
	CalypsoHdScreenRenderModel empty;
	const auto& metrics = calypsoViewportRuntime().current();
	const bool wide = metrics.layoutClass == CalypsoLayoutClass::Wide;
	const auto* layout = CalypsoGeoscapeCommandShellGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!layout) return empty;

	CalypsoGeoscapeHdRuntimeInput input;
	const auto text = [](const Text* value) {
		return value ? value->getText() : std::string();
	};
	input.copy.emplace_back("time", text(state._txtHour) + ":" + text(state._txtMin));
	input.copy.emplace_back("date", text(state._txtDay) + " "
		+ text(state._txtMonth) + " " + text(state._txtYear));
	input.copy.emplace_back("funds", text(state._txtFunds));
	if (state._timeSpeed == state._btn5Secs) input.selectedActionId = "time.speed.5sec";
	else if (state._timeSpeed == state._btn5Mins) input.selectedActionId = "time.speed.5min";
	else if (state._timeSpeed == state._btn30Mins) input.selectedActionId = "time.speed.30min";
	else if (state._timeSpeed == state._btn1Hour) input.selectedActionId = "time.speed.1hour";
	else if (state._timeSpeed == state._btn1Day) input.selectedActionId = "time.speed.1day";
	else input.selectedActionId = "time.speed.1min";

	CalypsoGeoscapeHdRuntimeModel model = calypsoGeoscapeHdRuntimeModel(*layout, input);
	for (auto& action : model.actions)
	{
		for (const auto& binding : calypsoGeoscapeHdWidgetBindings())
		{
			if (action.id != binding.actionId) continue;
			if (std::string(binding.role).rfind("widget:", 0) == 0)
				action.widget = CalypsoGeoscapeHdShell::resolveWidget(&state,
					std::string(binding.role).substr(7));
			else
				action.widget = CalypsoGeoscapeHdShell::resolveLiveWidget(&state, action.id);
			if (const auto* button = dynamic_cast<const TextButton*>(
				static_cast<const Surface*>(action.widget)))
			{
				if (!button->getText().empty()) action.label = button->getText();
			}
			break;
		}
	}
	// State-only actions have no authoritative legacy visual/input owner yet.
	// Do not expose a physical button that cannot dispatch the real handler.
	model.actions.erase(std::remove_if(model.actions.begin(), model.actions.end(),
		[](const CalypsoHdScreenActionVisual& action) { return action.widget == nullptr; }),
		model.actions.end());
	return model;
}


CalypsoHdScreenRenderer::CalypsoHdScreenRenderer(
	const void* state, CalypsoHdScreenRenderModel model, CalypsoHdScreenRenderMode mode)
	: _state(state), _model(std::move(model)), _mode(mode)
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

bool CalypsoHdScreenRenderer::suppressLogicalState() const
{
	return _mode == CalypsoHdScreenRenderMode::HarnessFullPhysical;
}

void CalypsoHdScreenRenderer::setModel(CalypsoHdScreenRenderModel model)
{
	_model = std::move(model);
}

bool CalypsoHdScreenRenderer::resolvePhysicalFonts(
	CalypsoTtfSourceDescriptor& heading, CalypsoTtfSourceDescriptor& body,
	CalypsoTtfSourceDescriptor& mono) const
{
	Game* game = getCurrentGame();
	const Mod* mod = game ? game->getMod() : nullptr;
	return calypsoHdResolveFontDescriptor(mod, "FONT_F34_SAIRA_700", heading)
		&& calypsoHdResolveFontDescriptor(mod, "FONT_F33_BODY", body)
		&& calypsoHdResolveFontDescriptor(mod, "FONT_F34_MONO", mono);
}

bool CalypsoHdScreenRenderer::physicalReady() const
{
	if (!_state) return false;
	CalypsoTtfSourceDescriptor heading;
	CalypsoTtfSourceDescriptor body;
	CalypsoTtfSourceDescriptor mono;
	return resolvePhysicalFonts(heading, body, mono);
}

void CalypsoHdScreenRenderer::collect(CalypsoHdFrameBuilder& builder) const
{
	const bool live = _mode == CalypsoHdScreenRenderMode::GeoscapeLiveChrome;
	CalypsoHdScreenRenderModel liveSnapshot;
	const CalypsoHdScreenRenderModel* modelPtr = &_model;
	if (live)
	{
		const auto* geoscape = static_cast<const GeoscapeState*>(_state);
		if (!geoscape) return;
		liveSnapshot = liveGeoscapeModel(*geoscape);
		modelPtr = &liveSnapshot;
	}
	const CalypsoHdScreenRenderModel& model = *modelPtr;
	if (!_state || model.designWidth <= 0 || model.designHeight <= 0) return;
	if (model.archetype != "strategic-command-shell") return;

	CalypsoTtfSourceDescriptor heading;
	CalypsoTtfSourceDescriptor body;
	CalypsoTtfSourceDescriptor mono;
	if (!resolvePhysicalFonts(heading, body, mono)) return;

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
	if (!live)
	{
		painter.styled(painter.winLogical,
			screenPanelStyle(0, CalypsoHdThemeGen::kDialogFillTop,
				CalypsoHdThemeGen::kDialogFillBottom, 0.0f), nullptr, role++);

		// Deterministic sparse starfield and synthetic globe belong only to the
		// full-physical harness fixture. Live Geoscape keeps the real Globe/world.
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
	}

	if (live)
	{
		const GeoscapeState* geoscape = static_cast<const GeoscapeState*>(_state);
		painter.claim(geoscape->_txtFunds, role++);
		painter.claim(geoscape->_txtHour, role++);
		painter.claim(geoscape->_txtHourSep, role++);
		painter.claim(geoscape->_txtMin, role++);
		painter.claim(geoscape->_txtMinSep, role++);
		painter.claim(geoscape->_txtSec, role++);
		painter.claim(geoscape->_txtWeekday, role++);
		painter.claim(geoscape->_txtDay, role++);
		painter.claim(geoscape->_txtMonth, role++);
		painter.claim(geoscape->_txtYear, role++);
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

	if (!live)
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
		if (live && action.widget == nullptr) continue;
		const CalypsoLogicalRect rect = painter.project(designRect(action.visible));
		const bool selected = action.id == model.selectedActionId;
		CalypsoHdPanelStyle style = f21QuietButtonStyle(
			selected ? CalypsoInteractionState::Focus : CalypsoInteractionState::Rest);
		if (action.component == "command-icon-action" || action.slotRole == "time-pause")
			style.radiusPx = action.visible.h / 2.0f;
		if (action.component == "notification-action")
			style.borderColorRgba = CalypsoHdThemeGen::kGold;
		painter.styled(rect, style, live ? action.widget : nullptr, role++);
		painter.textRect(rect, live ? action.widget : nullptr,
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
