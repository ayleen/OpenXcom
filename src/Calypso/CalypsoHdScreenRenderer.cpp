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

#include "CalypsoCommandActionStyle.h"
#include "CommandCenter/CommandCenterRenderer.h"
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
	input.selectedActionId = calypsoGeoscapeHdSelectedTimeAction(
		CalypsoGeoscapeHdShell::effectivePause(&state), runningSpeed);

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
	key.paused = CalypsoGeoscapeHdShell::effectivePause(&state);
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
	CalypsoTtfSourceDescriptor heading;
	CalypsoTtfSourceDescriptor body;
	CalypsoTtfSourceDescriptor mono;
	return resolvePhysicalFonts(heading, body, mono);
}

bool CalypsoHdScreenRenderer::completeFrameReady() const
{
	if (!physicalReady()) return false;
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

	// Command Center gate (normative spec 2026-08-28): the wide canvas
	// renders through the Command Center renderer instead of the strategic
	// shell. Compact keeps the F16 shell until the CC mobile wave lands.
	if (CommandCenter::calypsoCcEnabled() && model.designWidth >= 1024)
	{
		// The CC design space maps 1:1 onto the FULL display canvas (spec
		// s.4): the compact grid is laid out in display pixels, the globe
		// pass scissors to the same rect (stage 7), and the DPR is absorbed
		// by the backing-store size itself.
		painter.winLogical = { 0, 0, Options::displayWidth, Options::displayHeight };
		painter.windowDesign = { 0, 0, model.designWidth, model.designHeight };
		painter.uiScale = static_cast<double>(Options::displayWidth) / model.designWidth;
		painter.sx = 1.0;
		painter.sy = 1.0;
		const CommandCenter::CommandCenterFonts ccFonts =
			CommandCenter::calypsoCcResolveFonts(
				getCurrentGame() ? getCurrentGame()->getMod() : nullptr);
		CommandCenter::CommandCenterSnapshot snap;
		snap.selectedTimeStep = 1; // canonical reference: 1 MIN active
		auto* geoscapeState = live ? static_cast<GeoscapeState*>(const_cast<void*>(_state)) : nullptr;
		if (geoscapeState != nullptr)
		{
			const auto txt = [](const Text* value) { return value ? value->getText() : std::string(); };
			snap.displayTime = txt(geoscapeState->_txtHour) + ":" + txt(geoscapeState->_txtMin);
			snap.displayDate = txt(geoscapeState->_txtDay) + " " + txt(geoscapeState->_txtMonth)
				+ " " + txt(geoscapeState->_txtYear);
			snap.simulationPlaying = !geoscapeState->_pause;
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
		}
		// The desktop grid's fixed constants (rail 88 + stage 960 + inspector
		// 320) presuppose the 1440-wide reference; our production canvas is
		// 1280 wide, which is the spec's compact-desktop band (1024..1279
		// composition: rail 72, inspector 300 overlaying the stage). Use it
		// whenever the full desktop grid cannot fit without overflow.
		// Lay the CC grid out inside the FITTED canvas the overlay composites
		// (winLogical), not the full design width — the margins around it are
		// painted by the world pass clear (stage 7).
		const float ccWidth = static_cast<float>(Options::displayWidth);
		const float ccHeight = static_cast<float>(Options::displayHeight);
		// The inspector carries the Intercept block, which ships separately:
		// keep it closed so the stage expands (spec s.74 no-selection layout).
		const auto ccLayout = (ccWidth >= 112.0f + 960.0f + 24.0f + 320.0f + 24.0f)
			? CommandCenter::computeDesktopLayout(CommandCenter::Size2{ccWidth, ccHeight}, false)
			: CommandCenter::computeCompactDesktopLayout(CommandCenter::Size2{ccWidth, ccHeight}, false);
		CommandCenter::calypsoCcRender(painter, ccLayout, snap, ccFonts, live,
			geoscapeState, role);
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
