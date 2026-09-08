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
#include "../Savegame/SavedGame.h"
#include "../Savegame/Base.h"

#include "CalypsoCommandActionStyle.h"
#include "CalypsoBaseVisualCatalog.h"
#include "CalypsoBaseGridInput.h"
#include "CalypsoBasescapeHdLayout.h"
#include "CalypsoBasescapeHdUi.h"
#include "CalypsoUiMetrics.h"
#include "CommandCenter/CommandCenterRenderer.h"
#include "../Basescape/BasescapeState.h"
#include "../Basescape/PlaceFacilityState.h"
#include "../Basescape/PlaceStartFacilityState.h"
#include "CalypsoF21UiShared.h"
#include "CalypsoHdFontSource.h"
#include "CalypsoHdTheme.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoGeoscapeHdRuntime.h"
#include "CalypsoGeoscapeHdShell.h"
#include "CalypsoViewportRuntime.h"
#include "CalypsoTextEdit.h"
#include "../Engine/TTFFont.h"
#include "../Interface/TextEdit.h"
#include "../Basescape/BaseView.h"
#include "../Savegame/BaseFacility.h"
#include "../Mod/RuleBaseFacility.h"

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
	if (action.slotRole == "time-pause") return "II";
	return action.label;
}

void paintTimeSpeedRail(const CalypsoHdScreenRenderModel& model, CalypsoF21Painter& painter,
	const CalypsoTtfSourceDescriptor& mono,
	bool live, std::uint32_t& role)
{
	const CalypsoHdScreenActionVisual* firstSpeed = nullptr;
	const CalypsoHdScreenActionVisual* lastSpeed = nullptr;
	for (const auto& action : model.actions)
	{
		if (action.component == "time-speed-control" && action.id.rfind("time.speed.", 0) == 0)
		{
			if (!firstSpeed) firstSpeed = &action;
			lastSpeed = &action;
		}
	}
	if (!firstSpeed || !lastSpeed) return;

	if (const auto* time = findRegion(model, "timeControl"))
	{
		const CalypsoLogicalRect rail = painter.project(designRect(time->rect));
		painter.styled(rail, f21TimeSpeedRailStyle(), nullptr, role++);
	}
	const CalypsoLogicalRect first = painter.project(designRect(firstSpeed->visible));
	const CalypsoLogicalRect last = painter.project(designRect(lastSpeed->visible));
	painter.decoration({ first.x, first.y + first.h / 2,
		std::max(1, last.x + last.w - first.x), 1 }, CalypsoHdThemeGen::kAccentSoft, role++);

	bool drewSpeed = false;
	for (const auto& action : model.actions)
	{
		if (action.component != "time-speed-control") continue;
		const bool pause = action.slotRole == "time-pause";
		if (!pause && action.id.rfind("time.speed.", 0) != 0) continue;
		if (live && action.widget == nullptr) continue;
		const CalypsoLogicalRect rect = painter.project(designRect(action.visible));
		const bool selected = action.id == model.selectedActionId;
		if (pause)
		{
			// Pause alone is the circular dark control (reference .pause).
			const CalypsoInteractionState state = selected
				? CalypsoInteractionState::Focus : CalypsoInteractionState::Rest;
			CalypsoHdPanelStyle style = f21ButtonStyleFor(CalypsoActionTone::Safe, state);
			style.radiusPx = action.visible.h / 2.0f;
			painter.styled(rect, style, live ? action.widget : nullptr, role++);
		}
		else
		{
			// Speed segments stay panel-free bare text (reference
			// .speed-rail button): thin dividers plus the 2px accent selection
			// underline drawn as decorations; textRect owns the hit claim.
			if (drewSpeed)
				painter.decoration({ rect.x, rect.y + 8, 1, std::max(1, rect.h - 16) },
					kF21DividerRgba, role++);
			drewSpeed = true;
			if (selected)
				painter.decoration({ rect.x + 8, rect.y + rect.h - 3,
					std::max(1, rect.w - 16), 2 }, CalypsoHdThemeGen::kAccent, role++);
		}
		painter.textRect(rect, live ? action.widget : nullptr, mono,
			compactGlyph(action),
			selected ? CalypsoHdThemeGen::kAccent : kF21MutedBodyRgba,
			CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, 1, role++, 0.04,
			model.designHeight > 360 ? 12.0 : 9.0);
	}
}

void paintPlacementColumn(CalypsoF21Painter& painter,
	const CalypsoBasescapeHdDerivedLayout& derived,
	const CalypsoBasescapeHdFitParams& fitParams,
	const CalypsoHdScreenRenderModel& model,
	const CalypsoBasescapeHdSnapshot& snapshot,
	bool placementCursor, bool placementValid,
	const CommandCenter::CommandCenterFonts& ccFonts,
	std::uint32_t& role)
{
	// Placement command column (F01 construction): chosen facility details +
	// guidance + Cancel. Same header/rail/background/title/grid as the base
	// shell; the base cards are not painted and never interactive here.
	const CalypsoBasescapeHdPlacementVisual& placement = snapshot.placement;
	const CalypsoBasescapeHdPlacementColumn column =
		calypsoBasescapeHdPlacementColumn(derived, fitParams);
	const CalypsoHdScreenActionVisual* cancel = nullptr;
	for (const auto& candidate : model.actions)
	{
		if (candidate.id == "base.placement.cancel")
		{
			cancel = &candidate;
			break;
		}
	}
	if (cancel == nullptr)
	{
		CalypsoHdUiOverlay::instance().failHdRoute(
			"base action missing: base.placement.cancel");
	}
	const auto toLogical = [&](const CalypsoBasescapeHdRect& r) {
		return painter.project(CalypsoF21Rect{r.x, r.y, r.w, r.h});
	};
	painter.textRect(toLogical(column.name), nullptr, ccFonts.interSb,
		placement.facilityName,
		CommandCenterTheme::packed(CommandCenterTheme::TextPrimary),
		CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, role++, 0.0,
		fitParams.headingFontSize);
	// Detail/guidance metrics come from the same template parameters as
	// calypsoBasescapeHdPlacementColumn above: the details rect always spans
	// the available right column, so every native line paints (no truncation).
	const int detailFont = calypsoBasescapeHdPlacementDetailFont(fitParams);
	const int detailLineH = calypsoBasescapeHdPlacementDetailLine(fitParams);
	int lineY = column.details.y;
	for (const std::string& line : placement.detailLines)
	{
		painter.textRect(painter.project(CalypsoF21Rect{
				column.details.x, lineY, column.details.w, detailLineH }),
			nullptr, ccFonts.plexM, line,
			CommandCenterTheme::packed(CommandCenterTheme::TextSecondary),
			CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, role++, 0.0,
			static_cast<double>(detailFont));
		lineY += detailLineH;
	}
	const int selectH = calypsoBasescapeHdPlacementSelectH(fitParams);
	const int statusH = calypsoBasescapeHdPlacementStatusH(fitParams);
	painter.textRect(painter.project(CalypsoF21Rect{
			column.guidance.x, column.guidance.y, column.guidance.w, selectH }),
		nullptr, ccFonts.plexM, placement.guidanceSelect,
		CommandCenterTheme::packed(CommandCenterTheme::TextSecondary),
		CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, role++, 0.0,
		static_cast<double>(detailFont));
	if (placementCursor)
	{
		painter.textRect(painter.project(CalypsoF21Rect{
				column.guidance.x, column.guidance.y + selectH + fitParams.cardGap,
				column.guidance.w, statusH }),
			nullptr, ccFonts.interM,
			placementValid ? placement.guidanceValid : placement.guidanceInvalid,
			CommandCenterTheme::packed(placementValid
				? CommandCenterTheme::Accent : CommandCenterTheme::Danger),
			CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, role++, 0.0,
			static_cast<double>(fitParams.smallActionFontSize));
	}
	if (cancel == nullptr)
	{
		// Already failHdRoute'd above: texts paint, no Cancel button.
		return;
	}
	CalypsoInteractionState cancelState = CalypsoInteractionState::Rest;
	if (const TextButton* button = static_cast<const TextButton*>(
		static_cast<const Surface*>(cancel->widget)))
	{
		if (button->isPressed())
		{
			cancelState = CalypsoInteractionState::Pressed;
		}
		else if (button->isHovered())
		{
			cancelState = CalypsoInteractionState::Hover;
		}
	}
	CalypsoHdPanelStyle cancelStyle;
	cancelStyle.styled = true;
	cancelStyle.radiusPx = CommandCenterTheme::RadiusSM;
	cancelStyle.borderWidthPx = 1.0f;
	cancelStyle.borderColorRgba = CommandCenterTheme::packed(CommandCenterTheme::Border);
	cancelStyle.fillTopRgba = cancelStyle.fillBottomRgba = (cancelState == CalypsoInteractionState::Pressed)
		? CommandCenterTheme::packed(CommandCenterTheme::BgActive)
		: (cancelState == CalypsoInteractionState::Hover)
		? CommandCenterTheme::packed(CommandCenterTheme::BgHover)
		: CommandCenterTheme::packed(CommandCenterTheme::BgPanelRaised);
	cancelStyle.gradDirX = 0.0f;
	cancelStyle.gradDirY = 1.0f;
	painter.styled(painter.project(CalypsoF21Rect{
			cancel->visible.x, cancel->visible.y, cancel->visible.w, cancel->visible.h }),
		cancelStyle, nullptr, role++);
	painter.textRect(painter.project(CalypsoF21Rect{
			cancel->visible.x, cancel->visible.y, cancel->visible.w, cancel->visible.h }),
		cancel->widget, ccFonts.interM, cancel->label,
		CommandCenterTheme::packed(CommandCenterTheme::TextPrimary),
		CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, 2, role++, 0.02,
		fitParams.smallActionFontSize);
}

} // namespace

CalypsoGeoscapeHdRuntimeModel CalypsoHdScreenRenderer::liveGeoscapeModel(const GeoscapeState& state)
{
	CalypsoGeoscapeHdRuntimeModel empty;
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
	const char* runningSpeed = "time.speed.1min";
	if (state._timeSpeed == state._btn5Secs) runningSpeed = "time.speed.5sec";
	else if (state._timeSpeed == state._btn5Mins) runningSpeed = "time.speed.5min";
	else if (state._timeSpeed == state._btn30Mins) runningSpeed = "time.speed.30min";
	else if (state._timeSpeed == state._btn1Hour) runningSpeed = "time.speed.1hour";
	else if (state._timeSpeed == state._btn1Day) runningSpeed = "time.speed.1day";
	input.selectedActionId = runningSpeed;

	CalypsoGeoscapeHdRuntimeModel model = calypsoGeoscapeHdRuntimeModel(*layout, input);
	for (auto& action : model.actions)
	{
		bool available = false;
		std::string role;
		for (const auto& binding : calypsoGeoscapeHdWidgetBindings())
		{
			if (action.id != binding.actionId) continue;
			role = binding.role;
			if (std::string(binding.role).rfind("widget:", 0) == 0)
			{
				action.widget = CalypsoGeoscapeHdShell::resolveWidget(&state,
					std::string(binding.role).substr(7));
			}
			else
			{
				available = CalypsoGeoscapeHdShell::isLiveActionVisible(&state, action.id);
				action.widget = CalypsoGeoscapeHdShell::resolveLiveWidget(&state, action.id);
			}
			if (const auto* button = dynamic_cast<const TextButton*>(
				static_cast<const Surface*>(action.widget)))
			{
				if (!button->getText().empty()) action.label = button->getText();
			}
			if (calypsoGeoscapeHdActionExpected(action.id, role, available))
				model.expectedActionIds.push_back(action.id);
			break;
		}
	}
	// Recenter/contact are intentionally omitted until an audited owner exists;
	// every other canonical expected action remains in the model even when its
	// owner is absent, so readiness fails closed instead of drawing a partial UI.
	model.actions.erase(std::remove_if(model.actions.begin(), model.actions.end(),
		[&model](const CalypsoHdScreenActionVisual& action) {
			return std::find(model.expectedActionIds.begin(), model.expectedActionIds.end(), action.id)
				== model.expectedActionIds.end();
		}),
		model.actions.end());
	return model;
}

// --- Stage 8/9 closure: one generation-invalidated snapshot -----------------

/// Cheap allocation-free fingerprint of everything the live model depends on.
/// Text fields are monotonic content generations from the owning Text widgets
/// (bumped only when setText() stores different content), so steady-state
/// frames compare plain integers; actual text is read only inside rebuild().
CalypsoGeoscapeHdSnapshotKey CalypsoHdScreenRenderer::liveGeoscapeKey(const GeoscapeState& state)
{
	CalypsoGeoscapeHdSnapshotKey key;
	key.viewportGeneration = calypsoViewportRuntime().generation();
	key.contextGeneration = CalypsoHdUiOverlay::instance().contextGeneration();
	key.fundsVisible = state._txtFunds && state._txtFunds->getVisible();
	key.drawerOpen = CalypsoGeoscapeHdShell::isDrawerOpen(&state);
	key.extendedLinks = Options::oxceLinks;
	key.debugOption = Options::debug;
	const SavedGame* save = state._game ? state._game->getSavedGame() : nullptr;
	key.ironman = save != nullptr && save->isIronman();
	key.selectedSpeed = state._timeSpeed;
	key.hourTextGeneration = state._txtHour ? state._txtHour->calypsoTextGeneration() : 0;
	key.minuteTextGeneration = state._txtMin ? state._txtMin->calypsoTextGeneration() : 0;
	key.dayTextGeneration = state._txtDay ? state._txtDay->calypsoTextGeneration() : 0;
	key.monthTextGeneration = state._txtMonth ? state._txtMonth->calypsoTextGeneration() : 0;
	key.yearTextGeneration = state._txtYear ? state._txtYear->calypsoTextGeneration() : 0;
	key.fundsTextGeneration = state._txtFunds ? state._txtFunds->calypsoTextGeneration() : 0;
	return key;
}

const CalypsoGeoscapeHdRuntimeModel& CalypsoHdScreenRenderer::liveGeoscapeSnapshot(
	const GeoscapeState& state) const
{
	return _liveSnapshot.current(liveGeoscapeKey(state),
		[&state]() { return liveGeoscapeModel(state); });
}


namespace
{
constexpr std::uint32_t kBaseScreenFamilyId = 62;
} // namespace

void CalypsoHdScreenRenderer::collectBasescape(CalypsoHdFrameBuilder& builder) const
{
	const CalypsoHdScreenRenderModel& model = _model;
	if (model.archetype != "base-command-shell") return;
	// Fixture (harness) mode has no live state: paint from the model only and
	// claim nothing. Live modes additionally bind the existing input owners.
	// Placement is the same archetype on the same geometry, hosted by a
	// PlaceFacilityState: identical shell paint, repurposed command column.
	const bool placementMode = _mode == CalypsoHdScreenRenderMode::BasescapePlacementChrome;
	const bool live = _mode == CalypsoHdScreenRenderMode::BasescapeLiveChrome || placementMode;
	const BasescapeState* base = !placementMode && live ? static_cast<const BasescapeState*>(_state) : nullptr;
	const PlaceFacilityState* placing = placementMode ? static_cast<const PlaceFacilityState*>(_state) : nullptr;
	if (live && base == nullptr && placing == nullptr) return;
	Game* game = getCurrentGame();
	const Mod* mod = game ? game->getMod() : nullptr;
	const CommandCenter::CommandCenterFonts ccFonts =
		CommandCenter::calypsoCcResolveFonts(mod);
	if (!ccFonts.ready) return;
	CalypsoBaseVisualCatalog catalog;
	std::string catalogError;
	if (!calypsoLoadBaseVisualCatalog(catalog, catalogError))
	{
		CalypsoHdUiOverlay::instance().failHdRoute("base visual catalog: " + catalogError);
	}
	// Contract item 1: the base shell shares the Geoscape CSS-viewport
	// projection (frozen metrics density per axis, offset inversion) and the
	// CommandCenter layout at the real CSS viewport -- never the legacy
	// fixed-1280 uniform fit. Widget placement in CalypsoBasescapeHdUi uses
	// the same CalypsoBasescapeHdLayout helpers, so paint and input agree on
	// every axis, including resize and fractional DPR. Nothing here mutates
	// widgets; paint only claims the existing input owners.
	const CalypsoHdPresentationMetrics& metrics = CalypsoHdUiOverlay::instance().frozenMetrics();
	const auto& viewportMetrics = calypsoViewportRuntime().current();
	const int ccCssWidth = std::max(1, viewportMetrics.logicalWidth);
	const int ccCssHeight = std::max(1, viewportMetrics.logicalHeight);
	if (!metrics.valid() || metrics.scaleX <= 0.0 || metrics.scaleY <= 0.0)
	{
		CalypsoHdUiOverlay::instance().failHdRoute(
			"Basescape HD requires valid presentation metrics");
		return;
	}
	const CommandCenter::CommandCenterLayout ccLayout = CommandCenter::computeLayout(
		CommandCenter::Size2{static_cast<float>(ccCssWidth), static_cast<float>(ccCssHeight)},
		false, CommandCenter::InsetsF{
			static_cast<float>(viewportMetrics.safeX),
			static_cast<float>(viewportMetrics.safeY),
			static_cast<float>(ccCssWidth - viewportMetrics.safeX - viewportMetrics.safeWidth),
			static_cast<float>(ccCssHeight - viewportMetrics.safeY - viewportMetrics.safeHeight)});
	const CalypsoBasescapeHdFitParams fitParams;
	// Live frames derive the composition at the authored CSS size; the
	// harness fixture keeps its canonical model design size.
	const CalypsoBasescapeHdAuthoredSize authored = live
		? calypsoBasescapeHdAuthoredSize(ccCssWidth, ccCssHeight, fitParams)
		: CalypsoBasescapeHdAuthoredSize{1.0f,
			std::max(1, model.designWidth), std::max(1, model.designHeight)};
	const CalypsoBasescapeHdDerivedLayout derived =
		calypsoBasescapeHdDerivedLayout(authored.w, authored.h, fitParams);
	const double densityX = static_cast<double>(metrics.physicalWidth) / ccCssWidth;
	const double densityY = static_cast<double>(metrics.physicalHeight) / ccCssHeight;
	const double logicalPerCssX = densityX / metrics.scaleX;
	const double logicalPerCssY = densityY / metrics.scaleY;
	builder.beginSubgroup();
	CalypsoF21Painter painter{ builder, kBaseScreenFamilyId,
		reinterpret_cast<std::uintptr_t>(_state), 0, 1.0f, 1.0,
		CalypsoF21Rect{
			static_cast<int>(std::llround(-(metrics.contentOffsetX / metrics.scaleX))),
			static_cast<int>(std::llround(-(metrics.contentOffsetY / metrics.scaleY))),
			static_cast<int>(std::llround(ccCssWidth * logicalPerCssX)),
			static_cast<int>(std::llround(ccCssHeight * logicalPerCssY)) },
		metrics.scaleX, metrics.scaleY };
	painter.winLogical = {
		static_cast<int>(std::llround(-(metrics.contentOffsetX / metrics.scaleX))),
		static_cast<int>(std::llround(-(metrics.contentOffsetY / metrics.scaleY))),
		static_cast<int>(std::llround(ccCssWidth * logicalPerCssX)),
		static_cast<int>(std::llround(ccCssHeight * logicalPerCssY)) };
	painter.windowDesign = { 0, 0, ccCssWidth, ccCssHeight };
	painter.uiScale = logicalPerCssX * ccLayout.scale;
	painter.uiAspectY = logicalPerCssY / logicalPerCssX;
	std::uint32_t role = 1;
	const auto project = [&](const CalypsoHdScreenRect& r) {
		return painter.project(CalypsoF21Rect{ r.x, r.y, r.w, r.h });
	};
	// Design rects below are authored CSS pixels at the COMMANDED fit scale:
	// the painter carries the same layoutScale factor, so one project() call
	// maps them exactly like the shared widget placement helper.
	const auto projectAuthored = [&](const CalypsoBasescapeHdRect& r) {
		return painter.project(CalypsoF21Rect{ r.x, r.y, r.w, r.h });
	};
	const CalypsoBasescapeHdSnapshot& snapshot = model.baseSnapshot;

	painter.panel(projectAuthored({0, 0, authored.w, authored.h}),
		CommandCenterTheme::packed(CommandCenterTheme::BgRoot), nullptr, role++);
	auto background = calypsoBaseCatalogImage(catalog, catalog.background);
	background.cover = true;
	painter.image(projectAuthored(CalypsoBasescapeHdRect{
		fitParams.railWidth, fitParams.headerHeight,
		authored.w - fitParams.railWidth, authored.h - fitParams.headerHeight }),
		background, nullptr, role++);
	// Quiet cinematic interior: a light dim over the underwater plate keeps
	// the workspace calm behind the deck and command panels. Header and rail
	// chrome paint above it, unaffected.
	painter.panel(projectAuthored(CalypsoBasescapeHdRect{
		fitParams.railWidth, fitParams.headerHeight,
		authored.w - fitParams.railWidth, authored.h - fitParams.headerHeight }),
		0x0000002Eu, nullptr, role++);
	CalypsoHdPanelStyle panel;
	panel.styled = true;
	panel.radiusPx = CommandCenterTheme::RadiusMD;
	panel.borderWidthPx = 1.0f;
	panel.borderColorRgba = CommandCenterTheme::packed(CommandCenterTheme::Border);
	panel.fillTopRgba = panel.fillBottomRgba =
		CommandCenterTheme::packed(CommandCenterTheme::BgPanelGlass);
	painter.styled(projectAuthored(derived.deckPanel), panel, nullptr, role++);
	painter.styled(projectAuthored(CalypsoBasescapeHdRect{
		derived.commandX - fitParams.cardPad, derived.deckPanel.y,
		derived.commandW + 2 * fitParams.cardPad, derived.deckPanel.h }),
		panel, nullptr, role++);

	// Centered square deck from the canonical derivation (contract item 2):
	// always six square cells; the native BaseView keeps the same footprint
	// and input mapping at the derived extent.
	const int deckSide = derived.deckGrid.w;
	const int deckX = derived.deckGrid.x;
	const int deckY = derived.deckGrid.y;
	for (int cy = 0; cy < 6; ++cy)
	{
		for (int cx = 0; cx < 6; ++cx)
		{
			const BaseGridCellRect cell = calypsoBaseDeckCellRect(
				deckX, deckY, deckSide, cx, cy, 1, 1);
			painter.image(project(CalypsoHdScreenRect{ cell.x, cell.y, cell.w, cell.h }),
				calypsoBaseCatalogImage(catalog, catalog.emptyCell), nullptr, role++);
		}
	}
	if (base != nullptr)
	{
		painter.claim(base->_view, role++);
	}
	else if (placing != nullptr)
	{
		painter.claim(placing->_view, role++);
	}
	const CalypsoBasescapeHdFacilityVisual* grid[6][6] = {};
	for (const auto& fac : snapshot.facilities)
	{
		for (int yy = 0; yy < fac.sizeY; ++yy)
		{
			for (int xx = 0; xx < fac.sizeX; ++xx)
			{
				if (fac.x + xx >= 0 && fac.x + xx < 6 && fac.y + yy >= 0 && fac.y + yy < 6)
				{
					grid[fac.x + xx][fac.y + yy] = &fac;
				}
			}
		}
	}
	const auto builtOrPrevious = [](const CalypsoBasescapeHdFacilityVisual& fac) {
		return fac.buildTime == 0 || fac.hadPrevious;
	};
	for (const auto& fac : snapshot.facilities)
	{
		const BaseGridCellRect rect = calypsoBaseDeckCellRect(
			deckX, deckY, deckSide, fac.x, fac.y, fac.sizeX, fac.sizeY);
		const CalypsoHdScreenRect dest{ rect.x, rect.y, rect.w, rect.h };
		std::string art;
		if (fac.buildTime > 0)
		{
			art = catalog.construction;
		}
		else
		{
			bool known = false;
			art = calypsoBaseFacilityImage(catalog, fac.ruleType, known);
		}
		painter.image(project(dest), calypsoBaseCatalogImage(catalog, art), nullptr, role++);
		if (fac.craftDrawn && fac.craftIndex >= 0
			&& (size_t)fac.craftIndex < snapshot.crafts.size())
		{
			const std::string craftArt = calypsoBaseCraftImage(
				catalog, snapshot.crafts[(size_t)fac.craftIndex].artKey);
			if (craftArt.empty())
			{
				CalypsoHdUiOverlay::instance().failHdRoute("base craft art missing: "
					+ snapshot.crafts[(size_t)fac.craftIndex].artKey);
			}
			painter.image(project(dest), calypsoBaseCatalogImage(catalog, craftArt),
				nullptr, role++);
		}
	}
	// Doorway bridges follow native neighbor eligibility, but do not stretch
	// an opaque corridor across whole rooms. Complete all room art first.
	for (const auto& fac : snapshot.facilities)
	{
		if (!builtOrPrevious(fac) || fac.connectorsDisabled) continue;
		const int nx = fac.x + fac.sizeX;
		if (nx < 6)
		{
			for (int yy = fac.y; yy < fac.y + fac.sizeY; ++yy)
			{
				const auto* neighbor = (yy >= 0 && yy < 6) ? grid[nx][yy] : nullptr;
				if (!neighbor || !builtOrPrevious(*neighbor) || neighbor->connectorsDisabled) continue;
				const auto cell = calypsoBaseDeckCellRect(deckX, deckY, deckSide, nx, yy, 1, 1);
				const auto door = calypsoBasescapeHdConnectorRect(
					{cell.x, cell.y, cell.w, cell.h}, true, fitParams);
				painter.image(projectAuthored(door), calypsoBaseCatalogImage(catalog, catalog.connectorH),
					nullptr, role++);
			}
		}
		const int ny = fac.y + fac.sizeY;
		if (ny < 6)
		{
			for (int xx = fac.x; xx < fac.x + fac.sizeX; ++xx)
			{
				const auto* neighbor = (xx >= 0 && xx < 6) ? grid[xx][ny] : nullptr;
				if (!neighbor || !builtOrPrevious(*neighbor) || neighbor->connectorsDisabled) continue;
				const auto cell = calypsoBaseDeckCellRect(deckX, deckY, deckSide, xx, ny, 1, 1);
				const auto door = calypsoBasescapeHdConnectorRect(
					{cell.x, cell.y, cell.w, cell.h}, false, fitParams);
				painter.image(projectAuthored(door), calypsoBaseCatalogImage(catalog, catalog.connectorV),
					nullptr, role++);
			}
		}
	}
	// Status remains above room/craft art and every connecting doorway.
	for (const auto& fac : snapshot.facilities)
	{
		const auto rect = calypsoBaseDeckCellRect(deckX, deckY, deckSide,
			fac.x, fac.y, fac.sizeX, fac.sizeY);
		const CalypsoHdScreenRect dest{rect.x, rect.y, rect.w, rect.h};
		if (fac.buildTime > 0 || fac.disabled)
		{
			std::string marker = fac.disabled ? "X" : std::to_string(fac.buildTime);
			if (fac.hadPrevious) marker += "*";
			painter.textRect(project(dest), nullptr, ccFonts.plexM, marker,
				CommandCenterTheme::packed(CommandCenterTheme::TextPrimary),
				CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, 1, role++, 0.0, 14.0);
		}
		if (fac.buildTime == 0 && fac.ammoMax > 0)
		{
			const auto color = fac.ammo >= fac.ammoMax ? CommandCenterTheme::Success
				: fac.ammo <= fac.ammoMax / 2 ? CommandCenterTheme::Danger : CommandCenterTheme::Warning;
			painter.textRect(project(CalypsoHdScreenRect{rect.x, rect.y, rect.w, 12}),
				nullptr, ccFonts.plexM, std::to_string(fac.ammo) + "/" + std::to_string(fac.ammoMax),
				CommandCenterTheme::packed(color), CalypsoHdHAlign::Left, CalypsoHdVAlign::Top,
				1, role++, 0.0, 11.0);
		}
	}
	const BaseFacility* hovered = base != nullptr ? base->_view->getSelectedFacility() : nullptr;
	// Placement owns cursor feedback itself (footprint preview below); the
	// base hover ring never paints in placement mode.
	if (!placementMode && (live ? hovered != nullptr : snapshot.hasHoverCell))
	{
		const BaseGridCellRect hover = hovered != nullptr
			? calypsoBaseDeckCellRect(deckX, deckY, deckSide, hovered->getX(), hovered->getY(),
				hovered->getRules()->getSizeX(), hovered->getRules()->getSizeY())
			: calypsoBaseDeckCellRect(deckX, deckY, deckSide,
				snapshot.hoverX, snapshot.hoverY, snapshot.hoverSizeX, snapshot.hoverSizeY);
		CalypsoHdPanelStyle ring;
		ring.styled = true;
		ring.radiusPx = 6.0f;
		ring.borderWidthPx = 2.0f;
		ring.borderColorRgba = CommandCenterTheme::packed(CommandCenterTheme::Accent);
		ring.fillTopRgba = ring.fillBottomRgba = 0x00000000u;
		painter.styled(project(CalypsoHdScreenRect{
			hover.x, hover.y, hover.w, hover.h }), ring, nullptr, role++);
	}
	// Placement preview (F01 construction): the true sizeX/sizeY footprint at
	// the live cursor, clipped to the deck square. Real facility art, a
	// validity ring from the native getPlacementError (text status lives in
	// the command column), no phantom craft or rooms, no duplicate rules.
	bool placementCursor = false;
	bool placementValid = false;
	if (placementMode && placing != nullptr && placing->_view != nullptr
		&& placing->_view->isHovered() && placing->_rule != nullptr && !snapshot.placement.ruleType.empty())
	{
		const int gridX = placing->_view->getGridX();
		const int gridY = placing->_view->getGridY();
		if (gridX >= 0 && gridX < 6 && gridY >= 0 && gridY < 6)
		{
			placementCursor = true;
			const bool isStart = dynamic_cast<const PlaceStartFacilityState*>(placing) != nullptr;
			placementValid = placing->_view->getPlacementError(
				placing->_rule, placing->_origFac, isStart) == BPE_None;
			const BaseGridCellRect cell = calypsoBaseDeckCellRect(deckX, deckY, deckSide,
				gridX, gridY, snapshot.placement.sizeX, snapshot.placement.sizeY);
			const int clipX0 = std::max(cell.x, deckX);
			const int clipY0 = std::max(cell.y, deckY);
			const int clipX1 = std::min(cell.x + cell.w, deckX + deckSide);
			const int clipY1 = std::min(cell.y + cell.h, deckY + deckSide);
			if (clipX1 > clipX0 && clipY1 > clipY0)
			{
				// True footprint scale with proper clipping: the painter has
				// no partial-sprite clip, so the undistorted sprite paints
				// only when the whole footprint sits inside the deck; at an
				// edge the visible clipped outline alone carries validity
				// (never a squashed sprite). Validity still comes from the
				// native getPlacementError above.
				const bool fullyInside = clipX0 == cell.x && clipY0 == cell.y
					&& clipX1 == cell.x + cell.w && clipY1 == cell.y + cell.h;
				const CalypsoHdScreenRect preview = fullyInside
					? CalypsoHdScreenRect{cell.x, cell.y, cell.w, cell.h}
					: CalypsoHdScreenRect{clipX0, clipY0, clipX1 - clipX0, clipY1 - clipY0};
				if (fullyInside)
				{
					bool known = false;
					const std::string previewArt = calypsoBaseFacilityImage(
						catalog, snapshot.placement.ruleType, known);
					painter.image(project(preview),
						calypsoBaseCatalogImage(catalog, previewArt), nullptr, role++);
				}
				CalypsoHdPanelStyle previewRing;
				previewRing.styled = true;
				previewRing.radiusPx = 6.0f;
				previewRing.borderWidthPx = 2.0f;
				previewRing.borderColorRgba = CommandCenterTheme::packed(placementValid
					? CommandCenterTheme::Accent : CommandCenterTheme::Danger);
				previewRing.fillTopRgba = previewRing.fillBottomRgba = 0x00000000u;
				painter.styled(project(preview), previewRing, nullptr, role++);
			}
		}
	}
	// Read the live editor draft; native TextEdit still owns input, caret and IME.
	const std::string baseName = base != nullptr ? base->_edtBase->getText() : snapshot.baseName;
	painter.textRect(projectAuthored(derived.titleName), base != nullptr ? base->_edtBase : nullptr,
		ccFonts.interSb, baseName,
		CommandCenterTheme::packed(CommandCenterTheme::TextPrimary),
		CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, role++, 0.0, fitParams.titleFontSize);
	if (base != nullptr)
	{
		painter.claim(base->_edtBase, role++);
		TTFFont* font = mod->getTTFFont("FONT_CC_INTER_SB", false);
		double advance = 0;
		if (CalypsoTextEdit::caretAdvance(*base->_edtBase, font, advance))
		{
			const int offset = std::min(derived.titleName.w - 2,
				static_cast<int>(std::lround(advance * fitParams.titleFontSize / font->pixelSize())));
			painter.panel(projectAuthored(CalypsoBasescapeHdRect{
				derived.titleName.x + offset, derived.titleName.y + 4, 1, derived.titleName.h - 8 }),
				CommandCenterTheme::packed(CommandCenterTheme::Accent), nullptr, role++);
		}
	}
	painter.textRect(projectAuthored(derived.titleRegion), base != nullptr ? base->_txtLocation : nullptr,
		ccFonts.plexM, snapshot.region,
		CommandCenterTheme::packed(CommandCenterTheme::TextSecondary),
		CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, role++, 0.0, 11.0);
	painter.textRect(projectAuthored(derived.hoverLine), base != nullptr ? base->_txtFacility : nullptr,
		ccFonts.plexM, base != nullptr ? base->_txtFacility->getText() : snapshot.hoverFacility,
		CommandCenterTheme::packed(CommandCenterTheme::TextPrimary),
		CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, role++, 0.0, 12.0);
	if (base != nullptr)
	{
		painter.claim(base->_mini, role++);
	}
	{
		// Cinematic selector: eight variable-width slots from the shared
		// helper (selected wide, others narrow). Selection and reorder stay
		// on the hidden native MiniBaseView, which consumes the same
		// geometry; this is paint only, never a duplicate selected state.
		// Selected slot: live base name, monochrome stroked occupancy plan,
		// mint underline. Occupied inactive slots: quiet monochrome plans.
		// Vacant positions: unfilled muted two-digit numbers.
		for (int i = 0; i < 8; ++i)
		{
			const CalypsoBasescapeHdRect slot = calypsoBasescapeHdSelectorSlot(
				derived.titleSelector, i, snapshot.selectedBase, fitParams);
			const bool occupied = (size_t)i < snapshot.bases.size();
			const bool selected = i == snapshot.selectedBase;
			if (occupied && selected)
			{
				CalypsoHdPanelStyle slotBg;
				slotBg.styled = true;
				slotBg.radiusPx = CommandCenterTheme::RadiusSM;
				slotBg.borderWidthPx = 1.0f;
				slotBg.borderColorRgba = CommandCenterTheme::packed(CommandCenterTheme::Border);
				slotBg.fillTopRgba = slotBg.fillBottomRgba =
					CommandCenterTheme::packed(CommandCenterTheme::BgPanelRaised);
				painter.styled(projectAuthored(slot), slotBg, nullptr, role++);
			}
			if (!occupied)
			{
				char position[3] = {'0', static_cast<char>('1' + i), '\0'};
				painter.textRect(projectAuthored(slot), nullptr,
					ccFonts.plexM, position,
					CommandCenterTheme::packed(CommandCenterTheme::TextMuted),
					CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, 1, role++, 0.0, 12.0);
				continue;
			}
			const auto& entry = snapshot.bases[(size_t)i];
			// Monochrome occupancy plan: stroked outlines only, brighter on
			// the selected slot. The name reserves the top band of the
			// selected slot; the underline reserves its bottom edge.
			const int nameH = selected ? 20 : 0;
			const int underlineH = selected ? 4 : 0;
			if (selected)
			{
				painter.textRect(projectAuthored(CalypsoBasescapeHdRect{
						slot.x + 6, slot.y + 2, slot.w - 12, nameH - 2 }),
					base != nullptr ? base->_edtBase : nullptr,
					ccFonts.interSb, base != nullptr ? baseName : entry.name,
					CommandCenterTheme::packed(CommandCenterTheme::TextPrimary),
					CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, role++, 0.0,
					static_cast<float>(fitParams.smallActionFontSize));
			}
			const int availW = slot.w - 8;
			const int availH = slot.h - nameH - underlineH - 8;
			const int unit = std::max(1, std::min(availW, availH) / 6);
			const int planW = 6 * unit;
			const int planH = 6 * unit;
			const int planX = slot.x + (slot.w - planW) / 2;
			const int planY = slot.y + nameH + (slot.h - nameH - underlineH - planH) / 2;
			const std::uint32_t stroke = CommandCenterTheme::packed(
				selected ? CommandCenterTheme::TextPrimary : CommandCenterTheme::TextMuted);
			for (const auto& cell : entry.cells)
			{
				CalypsoHdPanelStyle outline;
				outline.styled = true;
				outline.radiusPx = 1.0f;
				outline.borderWidthPx = 1.0f;
				outline.borderColorRgba = stroke;
				outline.fillTopRgba = outline.fillBottomRgba = 0x00000000u;
				painter.styled(projectAuthored(CalypsoBasescapeHdRect{
					planX + cell.x * unit, planY + cell.y * unit,
					std::max(1, cell.sizeX * unit), std::max(1, cell.sizeY * unit) }),
					outline, nullptr, role++);
			}
			if (selected)
			{
				painter.panel(projectAuthored(CalypsoBasescapeHdRect{
					slot.x + 6, slot.y + slot.h - 3, slot.w - 12, 2 }),
					CommandCenterTheme::packed(CommandCenterTheme::Accent), nullptr, role++);
			}
		}
	}
	// Shared strategic chrome at the real CSS viewport (contract item 1):
	// 72 CSS header, 88 CSS rail, globe-identical rail items.
	CommandCenter::calypsoCcPaintHeaderBackground(painter, ccLayout.header, role);
	CommandCenter::calypsoCcPaintRailBackground(painter, ccLayout.navigationRail, role);
	{
		const char* railLabels[5] = { CommandCenter::calypsoCcRailLabel(0),
			CommandCenter::calypsoCcRailLabel(1), CommandCenter::calypsoCcRailLabel(2),
			CommandCenter::calypsoCcRailLabel(3), CommandCenter::calypsoCcRailLabel(4) };
		CommandCenter::calypsoCcPaintRailItems(painter, ccLayout.navigationRail,
			CommandCenter::RailAction::Bases, railLabels, ccFonts, role);
	}
	{
		// Contract item 3: the same pure header-content painter as Geoscape
		// shows the real base identity and the real campaign time/date in the
		// same positions. Display-only chip: no chevron, no dropdown -- the
		// native MiniBaseView band in the content owns selection.
		CommandCenter::CommandCenterSnapshot headerContent;
		headerContent.baseCaption = snapshot.baseCaption;
		headerContent.baseName = baseName;
		headerContent.displayTime = snapshot.displayTime;
		headerContent.displayDate = snapshot.displayDate;
		CommandCenter::calypsoCcPaintHeaderContent(painter, ccLayout, headerContent,
			ccFonts, role, false);
	}
	// Deck furniture over the background art: heading caption, coordinate
	// gutters when the centered grid leaves slack, and the quiet funds line
	// in the footer (contract item 7 -- never the header clock).
	{
		const std::string deckHeading = copyValue(model, "heading.deck");
		painter.textRect(projectAuthored(CalypsoBasescapeHdRect{
				derived.deckHeading.x + 12, derived.deckHeading.y + 8,
				derived.deckHeading.w - 24, 24 }),
			nullptr, ccFonts.interSb, deckHeading,
			CommandCenterTheme::packed(CommandCenterTheme::TextSecondary),
			CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, role++, 0.0, fitParams.headingFontSize);
		if (derived.showRowGutters)
		{
			static const char *const rowNames[6] = {"A", "B", "C", "D", "E", "F"};
			const int gutterL = derived.deckGrid.x - fitParams.gutterMinSlack;
			const int gutterW = fitParams.gutterMinSlack - 4;
			for (int row = 0; row < 6; ++row)
			{
				const BaseGridCellRect cell = calypsoBaseDeckCellRect(
					deckX, deckY, deckSide, 0, row, 6, 1);
				painter.textRect(projectAuthored(CalypsoBasescapeHdRect{
						gutterL, cell.y, std::max(1, gutterW), cell.h }),
					nullptr, ccFonts.plexM, rowNames[row],
					CommandCenterTheme::packed(CommandCenterTheme::TextMuted),
					CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, 1, role++, 0.0, 11.0);
			}
		}
		if (derived.showColGutters)
		{
			char colName[2] = {'1', '\0'};
			const int gutterT = derived.deckGrid.y - 2 * fitParams.cardPad - 4;
			const int gutterH = 2 * fitParams.cardPad;
			for (int col = 0; col < 6; ++col)
			{
				colName[0] = static_cast<char>('1' + col);
				const BaseGridCellRect cell = calypsoBaseDeckCellRect(
					deckX, deckY, deckSide, col, 0, 1, 6);
				painter.textRect(projectAuthored(CalypsoBasescapeHdRect{
						cell.x, gutterT, cell.w, std::max(1, gutterH) }),
					nullptr, ccFonts.plexM, colName,
					CommandCenterTheme::packed(CommandCenterTheme::TextMuted),
					CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, 1, role++, 0.0, 11.0);
			}
		}
		// Funds carry the native {ALT} control marker (Language maps it to
		// 0x01 TOK_COLOR_FLIP): strip engine controls through the shared
		// normalizer before TTF drawing instead of special-casing money.
		// The _txtFunds widget stays the (claimed) owner; its text is only
		// read, never rewritten here.
		painter.textRect(projectAuthored(derived.fundsLine),
			base != nullptr ? base->_txtFunds : nullptr,
			ccFonts.plexM,
			CommandCenter::calypsoHdNormalizeTtfDisplayText(snapshot.funds),
			CommandCenterTheme::packed(CommandCenterTheme::TextSecondary),
			CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, role++, 0.0, 12.0);
	}
	// Command column from the canonical derivation (contract item 2): the
	// model carries labels/components, geometry comes from the derived
	// layout so paint and widget placement cannot drift. Resting cards use
	// the selective visual state (contract item 5): native default focus no
	// longer outlines every card mint.
	const auto findModelAction = [&](const std::string& id) -> const CalypsoHdScreenActionVisual* {
		for (const auto& candidate : model.actions)
		{
			if (candidate.id == id)
			{
				return &candidate;
			}
		}
		return nullptr;
	};
	CalypsoBasescapeHdWidgetState cardStates[10];
	for (int i = 0; i < 10; ++i)
	{
		cardStates[i] = CalypsoBasescapeHdWidgetState{};
		if (!live || placementMode)
		{
			continue;
		}
		const CalypsoHdScreenActionVisual* visual = findModelAction(derived.rows[i].actionId);
		const TextButton* button = visual != nullptr
			? static_cast<const TextButton*>(static_cast<const Surface*>(visual->widget))
			: nullptr;
		if (button != nullptr)
		{
			cardStates[i].pressed = button->isPressed();
			cardStates[i].hovered = button->isHovered();
			cardStates[i].focused = base->getCalypsoFocusedTarget() == button;
		}
	}
	if (placementMode)
	{
		paintPlacementColumn(painter, derived, fitParams, model, snapshot,
			placementCursor, placementValid, ccFonts, role);
	}
	else
	{
		const std::string columnHeading = copyValue(model, "heading.column");
		painter.textRect(projectAuthored(CalypsoBasescapeHdRect{
				derived.columnHeading.x, derived.columnHeading.y,
				derived.columnHeading.w, derived.columnHeading.h }),
			nullptr, ccFonts.interSb,
			columnHeading,
			CommandCenterTheme::packed(CommandCenterTheme::TextSecondary),
			CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, role++, 0.0, fitParams.headingFontSize);
	}
	// Pass 1: row fills + input-owner claims. No container hit target: each
	// logistics row keeps its own direct native action widget.
	// Placement paints no base cards: the column above owns details + Cancel.
	for (int i = 0; i < 10 && !placementMode; ++i)
	{
		const auto& row = derived.rows[i];
		const CalypsoHdScreenActionVisual* visual = findModelAction(row.actionId);
		if (visual == nullptr)
		{
			CalypsoHdUiOverlay::instance().failHdRoute(
				std::string("base action missing: ") + row.actionId);
			continue;
		}
		const TextButton* button = live
			? static_cast<const TextButton*>(static_cast<const Surface*>(visual->widget))
			: nullptr;
		if (button != nullptr && !button->getVisible())
		{
			continue;
		}
		const CalypsoInteractionState state = live
			? calypsoBasescapeHdCardVisualState(cardStates, 10, i)
			: CalypsoInteractionState::Rest;
		CalypsoHdPanelStyle card;
		card.styled = true;
		card.radiusPx = CommandCenterTheme::RadiusSM;
		card.borderWidthPx = (state == CalypsoInteractionState::Focus) ? 2.0f : 1.0f;
		card.borderColorRgba = (state == CalypsoInteractionState::Focus)
			? CommandCenterTheme::packed(CommandCenterTheme::Accent)
			: CommandCenterTheme::packed(CommandCenterTheme::Border);
		card.fillTopRgba = card.fillBottomRgba = (state == CalypsoInteractionState::Pressed)
			? CommandCenterTheme::packed(CommandCenterTheme::BgActive)
			: (state == CalypsoInteractionState::Hover)
			? CommandCenterTheme::packed(CommandCenterTheme::BgHover)
			: CommandCenterTheme::packed(CommandCenterTheme::BgPanelRaised);
		card.gradDirX = 0.0f;
		card.gradDirY = 1.0f;
		painter.styled(projectAuthored(row.rect), card, nullptr, role++);
	}
	// Pass 2: illustrations ABOVE the row fills (the logistics group art was
	// hidden behind opaque fills before), then Inter labels above the art.
	// Skipped in placement mode with pass 1 (no base cards there).
	if (!placementMode)
	{
		bool groupIllustrated = false;
		for (int i = 0; i < 10; ++i)
		{
			const auto& row = derived.rows[i];
			const CalypsoHdScreenActionVisual* visual = findModelAction(row.actionId);
			if (visual == nullptr)
			{
				continue;
			}
			const TextButton* button = live
				? static_cast<const TextButton*>(static_cast<const Surface*>(visual->widget))
				: nullptr;
			if (button != nullptr && !button->getVisible())
			{
				continue;
			}
			const bool logistics = std::string(row.slotRole) == "logistics-rows";
			const bool illustratedCard = !logistics
				&& std::string(row.slotRole).rfind("card-", 0) == 0;
			if (logistics && row.rowIndex == 0 && !groupIllustrated)
			{
				groupIllustrated = true;
				const std::string groupArt = calypsoBaseCardImage(catalog, "logistics-group");
				if (groupArt.empty())
				{
					CalypsoHdUiOverlay::instance().failHdRoute("base card art missing: logistics-group");
				}
				const CalypsoBasescapeHdRect art{
					derived.commandX, row.rect.y + fitParams.logisticsH,
					fitParams.logisticsArtW, fitParams.logisticsArtW * 2 / 3 };
				painter.textRect(projectAuthored(CalypsoBasescapeHdRect{
					derived.commandX, row.rect.y + 8, fitParams.logisticsArtW, 24 }),
					nullptr, ccFonts.interM, copyValue(model, "heading.logistics"),
					CommandCenterTheme::packed(CommandCenterTheme::TextSecondary),
					CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, 1, role++, 0.0, 11.0);
				painter.image(projectAuthored(art),
					calypsoBaseCatalogImage(catalog, groupArt), nullptr, role++);
			}
			CalypsoBasescapeHdRect thumb{0, 0, 0, 0};
			bool hasThumb = false;
			if (illustratedCard)
			{
				const std::string art = calypsoBaseCardImage(catalog, row.actionId);
				if (art.empty())
				{
					CalypsoHdUiOverlay::instance().failHdRoute(
						std::string("base card art missing: ") + row.actionId);
				}
				thumb = calypsoBasescapeHdCardThumb(row.rect, fitParams);
				painter.image(projectAuthored(thumb),
					calypsoBaseCatalogImage(catalog, art), nullptr, role++);
				hasThumb = true;
			}
			const int labelX = row.rect.x + (hasThumb
				? thumb.w + 2 * fitParams.cardPad : fitParams.cardPad);
			const int labelW = row.rect.x + row.rect.w - fitParams.cardPad - labelX
				- (illustratedCard ? 2 * fitParams.cardPad : 0);
			const double labelSize = illustratedCard ? fitParams.actionFontSize : fitParams.smallActionFontSize;
			painter.textRect(projectAuthored(CalypsoBasescapeHdRect{
					labelX, row.rect.y + 6, std::max(1, labelW), row.rect.h - 12 }),
				live ? visual->widget : nullptr,
				illustratedCard ? ccFonts.interSb : ccFonts.interM, visual->label,
				CommandCenterTheme::packed(CommandCenterTheme::TextPrimary),
				CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 2, role++, 0.02, labelSize);
			if (illustratedCard)
			{
				painter.textRect(projectAuthored(CalypsoBasescapeHdRect{
					row.rect.x + row.rect.w - 3 * fitParams.cardPad, row.rect.y,
					2 * fitParams.cardPad, row.rect.h }),
					nullptr, ccFonts.interR, "\xE2\x80\xBA",
					CommandCenterTheme::packed(CommandCenterTheme::TextSecondary),
					CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, 1, role++, 0.0,
					fitParams.actionFontSize + 4);
			}
		}
	}
	if (live && !placementMode)
	{
		if (const CalypsoHdScreenActionVisual* world = findModelAction("navigation.world"))
		{
			painter.claim(world->widget, role++);
		}
	}
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

void CalypsoHdScreenRenderer::collectLogicalSuppression(
	CalypsoHdLogicalSuppression& suppression) const
{
	if (_mode == CalypsoHdScreenRenderMode::BasescapeLiveChrome && _state != nullptr)
	{
		// T15: visible input-only rail owners (no native pixels of their own,
		// but their frames must never blit).
		// T16: every other main widget is suppressed with them, so a covered
		// base can never leak legacy pixels around a child state. The name
		// field draws natively while top (caret/IME) and joins the list only
		// when covered.
		const BasescapeState* base = static_cast<const BasescapeState*>(_state);
		if (base != nullptr)
		{
			suppression.add(base->_view);
			suppression.add(base->_mini);
			suppression.add(base->_btnNewBase);
			suppression.add(base->_btnBaseInfo);
			suppression.add(base->_btnSoldiers);
			suppression.add(base->_btnCrafts);
			suppression.add(base->_btnFacilities);
			suppression.add(base->_btnResearch);
			suppression.add(base->_btnManufacture);
			suppression.add(base->_btnTransfer);
			suppression.add(base->_btnPurchase);
			suppression.add(base->_btnSell);
			suppression.add(base->_btnGeoscape);
			suppression.add(base->_txtFacility);
			suppression.add(base->_txtLocation);
			suppression.add(base->_txtFunds);
			if (base->_calypsoHdUi != nullptr)
			{
				for (const TextButton* button : base->_calypsoHdUi->railButtons())
				{
					suppression.add(button);
				}
			}
			Game* game = getCurrentGame();
			if (game != nullptr && game->getTopState() != _state)
			{
				suppression.add(base->_edtBase);
			}
		}
		return;
	}
	if (_mode == CalypsoHdScreenRenderMode::BasescapePlacementChrome && _state != nullptr)
	{
		// Placement hides its whole native window while top: the grid and
		// Cancel stay claimed input owners in collect(), every other widget
		// is suppressed so no vanilla pixels can leak around the HD shell.
		const PlaceFacilityState* placing = static_cast<const PlaceFacilityState*>(_state);
		if (placing != nullptr)
		{
			suppression.add(placing->_view);
			suppression.add(placing->_btnCancel);
			suppression.add(placing->_window);
			suppression.add(placing->_txtFacility);
			suppression.add(placing->_txtCost);
			suppression.add(placing->_numCost);
			suppression.add(placing->_numResources);
			suppression.add(placing->_txtTime);
			suppression.add(placing->_numTime);
			suppression.add(placing->_txtMaintenance);
			suppression.add(placing->_numMaintenance);
		}
		return;
	}
	if (_mode != CalypsoHdScreenRenderMode::GeoscapeLiveChrome || !_state) return;
	const auto* geoscape = static_cast<const GeoscapeState*>(_state);
	if (!geoscape) return;
	// Suppress the same lower shell surfaces that the live physical renderer
	// claims, before fonts/GPU/model readiness is evaluated. They remain hidden
	// input owners until this adapter is destroyed with the state.
	if (geoscape->_txtFunds && geoscape->_txtFunds->getVisible())
		suppression.add(geoscape->_txtFunds);
	suppression.add(geoscape->_txtHour);
	suppression.add(geoscape->_txtHourSep);
	suppression.add(geoscape->_txtMin);
	suppression.add(geoscape->_txtMinSep);
	suppression.add(geoscape->_txtSec);
	suppression.add(geoscape->_txtWeekday);
	suppression.add(geoscape->_txtDay);
	suppression.add(geoscape->_txtMonth);
	suppression.add(geoscape->_txtYear);
	suppression.add(geoscape->_sidebar);
	suppression.add(geoscape->_sideLine);
	suppression.add(geoscape->_sideTop);
	suppression.add(geoscape->_sideBottom);
	// One cached snapshot feeds every live consumer; no per-frame model rebuild.
	const auto& model = liveGeoscapeSnapshot(*geoscape);
	for (const auto& action : model.actions)
		suppression.add(action.widget);
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
	const bool resolved = calypsoHdResolveFontDescriptor(mod, "FONT_F34_SAIRA_700", heading)
		&& calypsoHdResolveFontDescriptor(mod, "FONT_F33_BODY", body)
		&& calypsoHdResolveFontDescriptor(mod, "FONT_F34_MONO", mono);
	return resolved && !heading.canonicalVfsPath.empty() && !body.canonicalVfsPath.empty()
		&& !mono.canonicalVfsPath.empty() && heading.logicalDesignSize > 0
		&& body.logicalDesignSize > 0 && mono.logicalDesignSize > 0;
}

bool CalypsoHdScreenRenderer::physicalReady() const
{
	if (!_state) return false;
	if (_mode == CalypsoHdScreenRenderMode::BasescapeLiveChrome
		|| _mode == CalypsoHdScreenRenderMode::BasescapePlacementChrome)
	{
		// Base chrome shares the Command Center faces (single typography).
		Game* game = getCurrentGame();
		const Mod* mod = game ? game->getMod() : nullptr;
		return CommandCenter::calypsoCcResolveFonts(mod).ready;
	}
	CalypsoTtfSourceDescriptor heading;
	CalypsoTtfSourceDescriptor body;
	CalypsoTtfSourceDescriptor mono;
	return resolvePhysicalFonts(heading, body, mono);
}

bool CalypsoHdScreenRenderer::completeFrameReady() const
{
	if (!physicalReady()) return false;
	if (_mode == CalypsoHdScreenRenderMode::BasescapeLiveChrome
		|| _mode == CalypsoHdScreenRenderMode::BasescapePlacementChrome)
		return calypsoBasescapeHdModelReady(_model);
	if (_mode != CalypsoHdScreenRenderMode::GeoscapeLiveChrome) return true;
	const auto* geoscape = static_cast<const GeoscapeState*>(_state);
	if (!geoscape) return false;
	const auto& model = liveGeoscapeSnapshot(*geoscape);
	const bool fundsVisible = geoscape->_txtFunds && geoscape->_txtFunds->getVisible();
	return calypsoGeoscapeHdFrameReady(model, model.expectedActionIds,
		calypsoGeoscapeHdRequiredCopyKeys(fundsVisible),
		CalypsoHdUiOverlay::instance().resourcesReadyForFrame());
}

bool CalypsoHdScreenRenderer::retryableReadiness() const
{
	if (_mode != CalypsoHdScreenRenderMode::GeoscapeLiveChrome || !_state)
		return false;
	const auto* geoscape = static_cast<const GeoscapeState*>(_state);
	if (!geoscape || !physicalReady()) return false;
	const auto& model = liveGeoscapeSnapshot(*geoscape);
	const bool fundsVisible = geoscape->_txtFunds && geoscape->_txtFunds->getVisible();
	return !calypsoGeoscapeHdModelReady(model, model.expectedActionIds,
		calypsoGeoscapeHdRequiredCopyKeys(fundsVisible));
}

void CalypsoHdScreenRenderer::collect(CalypsoHdFrameBuilder& builder) const
{
	const bool live = _mode == CalypsoHdScreenRenderMode::GeoscapeLiveChrome;
	const CalypsoHdScreenRenderModel* modelPtr = &_model;
	if (live)
	{
		const auto* geoscape = static_cast<const GeoscapeState*>(_state);
		if (!geoscape) return;
		modelPtr = &liveGeoscapeSnapshot(*geoscape);
	}
	const CalypsoHdScreenRenderModel& model = *modelPtr;
	if (!_state || model.designWidth <= 0 || model.designHeight <= 0) return;
	if (model.archetype == "base-command-shell")
	{
		collectBasescape(builder);
		return;
	}
	if (model.archetype != "strategic-command-shell") return;

	CalypsoTtfSourceDescriptor heading;
	CalypsoTtfSourceDescriptor body;
	CalypsoTtfSourceDescriptor mono;
	if (!resolvePhysicalFonts(heading, body, mono)) return;
	// Visual contract s.10.1 rule 8: the wide command rail draws Phosphor
	// line icons from the registered FONT_HD_ICONS face. Optional by design:
	// a missing icon face fails closed to circle + label, never blocks
	// readiness or the frame.
	CalypsoTtfSourceDescriptor icon;
	const bool iconsResolved = calypsoHdResolveFontDescriptor(
		getCurrentGame() ? getCurrentGame()->getMod() : nullptr, "FONT_HD_ICONS", icon)
		&& !icon.canonicalVfsPath.empty() && icon.logicalDesignSize > 0;

	if (live && !completeFrameReady()) return;
	const auto* projectionLayout = CalypsoGeoscapeCommandShellGen::layoutForDesign(
		model.designWidth, model.designHeight);
	if (live && projectionLayout == nullptr) return;
	const auto& viewportMetrics = calypsoViewportRuntime().current();
	const CalypsoGeoscapeHdProjection projection = projectionLayout != nullptr
		? calypsoGeoscapeHdProjection(*projectionLayout, viewportMetrics,
			Options::baseXResolution, Options::baseYResolution)
		: CalypsoGeoscapeHdProjection(*CalypsoGeoscapeCommandShellGen::layoutForDesign(1280, 720),
			viewportMetrics);
	int availableWidth = Options::baseXResolution;
	double scale = 1.0;
	int logicalWidth = model.designWidth;
	int logicalHeight = model.designHeight;
	int originX = 0;
	int originY = 0;
	if (live)
	{
		scale = projection.uiScale();
		logicalWidth = projection.canvasWidth();
		logicalHeight = projection.canvasHeight();
		originX = projection.canvasX();
		originY = projection.canvasY();
	}
	else
	{
		availableWidth = model.sideBySidePreview
			? Options::baseXResolution / 2 : Options::baseXResolution;
		scale = std::min((double)availableWidth / model.designWidth,
			(double)Options::baseYResolution / model.designHeight);
		logicalWidth = std::max(1, (int)std::llround(model.designWidth * scale));
		logicalHeight = std::max(1, (int)std::llround(model.designHeight * scale));
		originX = (availableWidth - logicalWidth) / 2;
		originY = (Options::baseYResolution - logicalHeight) / 2;
	}
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

	// Command Center gate (normative spec 2026-08-29): both wide and compact
	// landscape render through the current physical Command Center surface.
	if (CommandCenter::calypsoCcEnabled())
	{
		// CSS-authored geometry must invert each frozen presentation axis.
		// The engine canvas is stretched independently in X/Y; requiring a
		// uniform transform crashes valid desktop and fractional-DPR layouts.
		if (!metrics.valid() || metrics.scaleX <= 0.0 || metrics.scaleY <= 0.0)
			CalypsoHdUiOverlay::instance().failHdRoute(
				"Command Center requires valid presentation metrics");
		const int ccCssWidth = std::max(1, viewportMetrics.logicalWidth);
		const int ccCssHeight = std::max(1, viewportMetrics.logicalHeight);
		const double densityX = static_cast<double>(metrics.physicalWidth) / ccCssWidth;
		const double densityY = static_cast<double>(metrics.physicalHeight) / ccCssHeight;
		const double logicalPerCssX = densityX / metrics.scaleX;
		const double logicalPerCssY = densityY / metrics.scaleY;
		painter.winLogical = {
			-(int)std::llround(metrics.contentOffsetX / metrics.scaleX),
			-(int)std::llround(metrics.contentOffsetY / metrics.scaleY),
			(int)std::llround(ccCssWidth * logicalPerCssX),
			(int)std::llround(ccCssHeight * logicalPerCssY) };
		painter.windowDesign = { 0, 0, ccCssWidth, ccCssHeight };
		painter.uiScale = logicalPerCssX;
		painter.uiAspectY = logicalPerCssY / logicalPerCssX;
		const CommandCenter::CommandCenterFonts ccFonts =
			CommandCenter::calypsoCcResolveFonts(
				getCurrentGame() ? getCurrentGame()->getMod() : nullptr);
		CommandCenter::CommandCenterSnapshot snap;
		snap.selectedTimeStep = 1; // canonical reference: 1 MIN active
		auto* geoscapeState = live ? static_cast<GeoscapeState*>(const_cast<void*>(_state)) : nullptr;
		if (geoscapeState != nullptr)
		{
			snap.baseCaption = geoscapeState->tr("STR_BASES");
			const auto txt = [](const Text* value) { return value ? value->getText() : std::string(); };
			snap.displayTime = txt(geoscapeState->_txtHour) + ":" + txt(geoscapeState->_txtMin);
			snap.displayDate = txt(geoscapeState->_txtDay) + " " + txt(geoscapeState->_txtMonth)
				+ " " + txt(geoscapeState->_txtYear);
			const SavedGame* save = geoscapeState->_game
				? geoscapeState->_game->getSavedGame() : nullptr;
			if (save != nullptr && save->getBases() != nullptr)
			{
				for (const Base* base : *save->getBases())
					snap.baseNames.push_back(base->getName());
				snap.selectedBaseIndex =
					CalypsoGeoscapeHdShell::selectedBaseIndex(geoscapeState);
				if (snap.selectedBaseIndex >= snap.baseNames.size())
					snap.selectedBaseIndex = 0;
				if (!snap.baseNames.empty())
					snap.baseName = snap.baseNames[snap.selectedBaseIndex];
				snap.baseSelectorOpen =
					CalypsoGeoscapeHdShell::isBaseSelectorOpen(geoscapeState);
			}
			if (geoscapeState->_timeSpeed == geoscapeState->_btn5Secs) snap.selectedTimeStep = 0;
			else if (geoscapeState->_timeSpeed == geoscapeState->_btn1Min) snap.selectedTimeStep = 1;
			else if (geoscapeState->_timeSpeed == geoscapeState->_btn5Mins) snap.selectedTimeStep = 2;
			else if (geoscapeState->_timeSpeed == geoscapeState->_btn30Mins) snap.selectedTimeStep = 3;
			else if (geoscapeState->_timeSpeed == geoscapeState->_btn1Hour) snap.selectedTimeStep = 4;
			else if (geoscapeState->_timeSpeed == geoscapeState->_btn1Day) snap.selectedTimeStep = 5;
		}
		else
		{
			snap.displayTime = "14:18 UTC"; // reference state (spec s.61)
			snap.displayDate = "1 JAN 2040";
			snap.baseNames.push_back(snap.baseName);
		}
		// Author one desktop composition and fit it uniformly to small windows.
		const float ccWidth = static_cast<float>(ccCssWidth);
		const float ccHeight = static_cast<float>(ccCssHeight);
		const auto ccLayout = CommandCenter::computeLayout(
			CommandCenter::Size2{ccWidth, ccHeight}, false,
			CommandCenter::InsetsF{
				static_cast<float>(viewportMetrics.safeX),
				static_cast<float>(viewportMetrics.safeY),
				static_cast<float>(ccCssWidth - viewportMetrics.safeX - viewportMetrics.safeWidth),
				static_cast<float>(ccCssHeight - viewportMetrics.safeY - viewportMetrics.safeHeight)});
		painter.uiScale *= ccLayout.scale;
		CommandCenter::calypsoCcRender(painter, ccLayout, snap, ccFonts,
			metrics, live, geoscapeState, role);
		(void)projectionLayout;
		return;
	}

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
		const bool fundsVisible = geoscape->_txtFunds && geoscape->_txtFunds->getVisible();
		if (fundsVisible)
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
		painter.claim(geoscape->_sidebar, role++);
		painter.claim(geoscape->_sideLine, role++);
		painter.claim(geoscape->_sideTop, role++);
		painter.claim(geoscape->_sideBottom, role++);
	}

	if (const auto* status = findRegion(model, "status"))
	{
		const CalypsoLogicalRect rect = painter.project(designRect(status->rect));
		painter.styled(rect, screenPanelStyle(CalypsoHdThemeGen::kAccentSoft,
			CalypsoHdThemeGen::kDialogFillTop, CalypsoHdThemeGen::kDialogFillBottom,
			status->rect.h / 2.0f), nullptr, role++);
		const auto* liveGeoscape = live ? static_cast<const GeoscapeState*>(_state) : nullptr;
		const bool fundsVisible = !live || (liveGeoscape && liveGeoscape->_txtFunds
			&& liveGeoscape->_txtFunds->getVisible());
		std::string statusText = copyValue(model, "time") + "  |  "
			+ copyValue(model, "date");
		if (fundsVisible) statusText += "  |  " + copyValue(model, "funds");
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

	paintTimeSpeedRail(model, painter, mono, live, role);

	for (const auto& action : model.actions)
	{
		if (action.component == "time-speed-control") continue;
		if (live && action.widget == nullptr) continue;
		const CalypsoLogicalRect rect = painter.project(designRect(action.visible));
		const bool selected = action.id == model.selectedActionId;
		// Visual contract s.10.1: live buttons read the widget's real
		// interaction state every frame; the deterministic fixture renders
		// rest, and a selected action keeps the focus-ring semantics.
		CalypsoInteractionState state = CalypsoInteractionState::Rest;
		if (live && action.widget != nullptr)
			state = f21ButtonVisualState(static_cast<const TextButton*>(action.widget));
		if (selected && state == CalypsoInteractionState::Rest)
			state = CalypsoInteractionState::Focus;
		const bool commandAction = action.component == "command-icon-action"
			|| action.component == "compact-command-action";
		const auto tone = action.id == "action.session"
			? CalypsoCommandActionTone::Primary : CalypsoCommandActionTone::Normal;
		// Wide command rail (s.10.1 rule 8): circular icon button with the
		// label below, exactly as the canonical desktop mockup. Only the tall
		// rail slots qualify; zoom/time glyphs and every compact card keep the
		// single-surface presentation.
		const char32_t iconGlyph = calypsoCommandActionIconGlyph(action.id);
		const bool wideIconRail = commandAction && iconGlyph != 0
			&& action.visible.h >= kCommandIconCirclePx + kCommandIconLabelGapPx + 12
			&& action.visible.w >= kCommandIconCirclePx;
		if (wideIconRail)
		{
			const auto slot = calypsoCommandIconSlotLayout(designRect(action.visible));
			const CalypsoLogicalRect circleRect = painter.project(slot.circle);
			const CalypsoLogicalRect labelRect = painter.project(slot.label);
			CalypsoHdPanelStyle circleStyle = calypsoCommandActionStyle(state, tone);
			// A circle is the one legitimate full-radius surface (s.10.1 rule 8).
			circleStyle.radiusPx = slot.circle.height / 2.0f;
			painter.styled(circleRect, circleStyle, live ? action.widget : nullptr, role++);
			if (iconsResolved)
			{
				// Phosphor PUA codepoints are 3-byte UTF-8 (0xE000..0xF8FF).
				const char32_t cp = iconGlyph;
				const std::string glyph{
					static_cast<char>(0xE0 | (cp >> 12)),
					static_cast<char>(0x80 | ((cp >> 6) & 0x3F)),
					static_cast<char>(0x80 | (cp & 0x3F)) };
				painter.textRect(circleRect, live ? action.widget : nullptr, icon,
					glyph, CalypsoHdThemeGen::kNearWhite,
					CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, 1, role++,
					0.0, 30.0);
			}
			painter.textRect(labelRect, live ? action.widget : nullptr, mono,
				compactGlyph(action), state == CalypsoInteractionState::Focus
					? CalypsoHdThemeGen::kNearWhite : kF21MutedBodyRgba,
				CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, 1, role++, 0.08,
				model.designHeight > 360 ? 11.0 : 9.0);
			continue;
		}
		// Fixed canonical radius; the height-derived stadium is retired.
		CalypsoHdPanelStyle style = commandAction
			? calypsoCommandActionStyle(state, tone)
			: f21QuietButtonStyle(state);
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
