#ifdef __EMSCRIPTEN__
#include "CalypsoF06DiaryMissionUi.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include "../Engine/Font.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Engine/Surface.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextList.h"
#include "../Interface/Window.h"
#include "../Basescape/SoldierDiaryMissionState.h"
#include "../Mod/Mod.h"
#include "CalypsoF04SelectionListShell.h"
#include "CalypsoF06SelectionListShell.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoUiMetrics.h"
#include "CommandCenter/CommandCenterRenderer.h"
#include "Generated/CalypsoF06DiaryMission.generated.h"

namespace OpenXcom
{
namespace Calypso
{

struct CalypsoF06DiaryMissionUi::MissionRows
{
	std::vector<std::string> ids;
	std::vector<CalypsoSelectionListRow> rows;
};

// Live native record only, in native display order: the conditional mission
// context (UFO, score, kills, location, race, daylight, days wounded — each
// painted only when native shows it, so unknown fields stay absent), then
// one composed row per kill entry (status, unit, weapon verbatim — the
// unit-vs-race distinction is preserved verbatim), then the explicit
// No-record branch when native shows it. All rows are inert views: the
// native state owns no row activation and every field is reinitialized per
// mission by the native init().
CalypsoF06DiaryMissionUi::MissionRows CalypsoF06DiaryMissionUi::missionRows(const SoldierDiaryMissionState& state)
{
	MissionRows out{};
	auto push = [&](const Text* single)
	{
		const size_t before = out.rows.size();
		f06PushNativeTextRow(single, out.rows, false);
		for (size_t i = before; i < out.rows.size(); ++i)
			out.ids.push_back("context-" + std::to_string(i));
	};
	push(state._txtUFO);
	push(state._txtScore);
	push(state._txtKills);
	push(state._txtLocation);
	push(state._txtRace);
	push(state._txtDaylight);
	push(state._txtDaysWounded);

	const TextList* list = state._lstKills;
	if (list)
	{
		const auto& matrix = list->getCellTextsSnapshot();
		for (size_t row = 0; row < matrix.size(); ++row)
		{
			const auto& cells = matrix[row];
			if (cells.empty() || !cells[0]) continue;
			std::vector<std::string> texts;
			texts.reserve(cells.size());
			for (const Text* cell : cells)
				texts.push_back(cell ? cell->getText() : std::string());
			out.ids.push_back("kill-" + std::to_string(row));
			CalypsoSelectionListRow entry{};
			entry.text = CommandCenter::calypsoHdNormalizeTtfDisplayText(
				f04ComposeRowText(texts));
			entry.enabled = false;
			out.rows.push_back(entry);
		}
	}
	push(state._txtNoRecord);
	return out;
}

CalypsoF06DiaryMissionUi::~CalypsoF06DiaryMissionUi()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

const void* CalypsoF06DiaryMissionUi::topState() const
{
	return _state;
}

void CalypsoF06DiaryMissionUi::collectLogicalSuppression(
	CalypsoHdLogicalSuppression& suppression) const
{
	// Whole-state suppression (suppressLogicalState, default true) owns the
	// top-state logical UI; list every native surface explicitly so widget
	// suppression also holds during warmup and under a covered child.
	if (!_state) return;
	suppression.add(_state->_window);
	suppression.add(_state->_txtTitle);
	suppression.add(_state->_txtUFO);
	suppression.add(_state->_txtScore);
	suppression.add(_state->_txtKills);
	suppression.add(_state->_txtLocation);
	suppression.add(_state->_txtRace);
	suppression.add(_state->_txtDaylight);
	suppression.add(_state->_txtDaysWounded);
	suppression.add(_state->_txtNoRecord);
	suppression.add(_state->_lstKills);
	suppression.add(_state->_btnOk);
	suppression.add(_state->_btnPrev);
	suppression.add(_state->_btnNext);
}

void CalypsoF06DiaryMissionUi::collect(CalypsoHdFrameBuilder& builder) const
{
	// The mission detail is a registered HD route: missing prerequisites or
	// a missing generated layout fails the route instead of silently
	// skipping to an empty collection (the overlay would fail it closed).
	if (!_state || !_state->_hdLayout || !_state->_game)
		CalypsoHdUiOverlay::instance().failHdRoute("F06 mission prerequisites are unavailable");
	const bool wide = _state->_hdWideLayout;
	const auto* generated = f06DiaryMissionLayout(wide);

	const CalypsoLogicalRect window = _state->_window
		? CalypsoLogicalRect{_state->_window->getX(), _state->_window->getY(),
			_state->_window->getWidth(), _state->_window->getHeight()}
		: CalypsoLogicalRect{};
	const double uiScale = generated->window.w > 0
		? (double)window.w / generated->window.w : 1.0;
	auto project = [&](const auto& rect) -> CalypsoLogicalRect
	{
		return f06ProjectRect(window, *generated, rect);
	};

	CalypsoSelectionListModel model{};
	model.familyId = CalypsoF06DiaryMissionGen::kFamilyId;
	model.instance = _state;
	model.mod = _state->_game->getMod();
	model.wide = wide;
	model.designWidth = generated->designWidth;
	model.designHeight = generated->designHeight;
	model.window = window;
	model.status = project(generated->status);
	model.title = project(generated->title);
	model.list = project(generated->list);
	model.footer = project(generated->footer);
	model.windowWidget = _state->_window;
	model.titleWidget = _state->_txtTitle;
	model.listWidget = _state->_lstKills;
	// Title is the live mission type text (native init() owns it per
	// mission, along with every conditional field below).
	model.titleText = CommandCenter::calypsoHdNormalizeTtfDisplayText(
		_state->_txtTitle ? _state->_txtTitle->getText() : std::string());
	model.protocolText = CalypsoF06DiaryMissionGen::kProtocol;
	model.rowHeight = generated->rowHeight;
	model.visibleRows = generated->visibleRows;
	model.scrollBarWidth = generated->scrollBarWidth;
	model.minThumbHeight = std::max(1, (int)std::llround(generated->minThumbHeight * uiScale));
	// Shared native input geometry in the same logical space, read exactly
	// once per frame: the painter uses these rects instead of its own formula.
	if (_state->_lstKills && _state->_lstKills->isCalypsoHdSelectionList())
	{
		const SDL_Rect track = _state->_lstKills->getCalypsoHdTrackRect();
		const SDL_Rect thumb = _state->_lstKills->getCalypsoHdThumbRect();
		model.hasNativeScrollGeometry = track.w > 0 && track.h > 0;
		model.nativeTrack = {track.x, track.y, track.w, track.h};
		model.nativeThumb = {thumb.x, thumb.y, thumb.w, thumb.h};
		model.nativeThumbVisible = thumb.w > 0 && thumb.h > 0;
	}

	const int slotCount = wide
		? CalypsoF06DiaryMissionGen::kRowSlotWideCount
		: CalypsoF06DiaryMissionGen::kRowSlotCompactCount;
	const auto* slots = wide
		? CalypsoF06DiaryMissionGen::kRowSlotsWide
		: CalypsoF06DiaryMissionGen::kRowSlotsCompact;
	for (int i = 0; i < slotCount; ++i)
		model.rowSlots.push_back(project(slots[i]));

	// Inert views of the live mission record; wrapping mission navigation
	// stays with the parked native Prev/Next controls (handlers, keyboard
	// paths, and per-mission reinitialization untouched). No selection
	// applies to the mission record.
	const MissionRows bound = missionRows(*_state);
	model.rows = bound.rows;
	if (_state->_lstKills)
		model.scrollOffset = _state->_lstKills->getScroll();
	model.hasSelection = false;
	model.selectedRow = 0;

	model.cutCornerPx = CalypsoF06DiaryMissionGen::kCutCornerPx;
	model.protocolTextInsetPx = CalypsoF06DiaryMissionGen::kProtocolTextInsetPx;
	model.panelFillTop = CalypsoF06DiaryMissionGen::kPanelFillTop;
	model.panelFillBottom = CalypsoF06DiaryMissionGen::kPanelFillBottom;
	model.frameColor = CalypsoF06DiaryMissionGen::kFrame;
	model.protocolColor = CalypsoF06DiaryMissionGen::kProtocolText;
	model.dividerColor = CalypsoF06DiaryMissionGen::kDivider;
	model.footerDotColor = CalypsoF06DiaryMissionGen::kFooterDot;
	model.textColor = CalypsoF06DiaryMissionGen::kText;
	model.mutedTextColor = CalypsoF06DiaryMissionGen::kMutedText;
	model.selectionColor = CalypsoF06DiaryMissionGen::kSelection;
	model.scrollTrackColor = CalypsoF06DiaryMissionGen::kScrollTrack;
	model.scrollThumbColor = CalypsoF06DiaryMissionGen::kScrollThumb;
	const CalypsoHdPresentationMetrics& metrics =
		CalypsoHdUiOverlay::instance().frozenMetrics();
	model.uiScale = uiScale;
	model.visualScale = CalypsoF06DiaryMissionGen::kPresentationScale;
	model.projectionScaleX = uiScale * metrics.scaleX;
	model.projectionScaleY = uiScale * metrics.scaleY;
	model.titleDesignHeight = generated->title.h;
	model.motionDurationMs = CalypsoF06DiaryMissionGen::kMotionDurationMs;
	model.motionScaleFrom = CalypsoF06DiaryMissionGen::kMotionScaleFrom;

	const auto& generatedButton = CalypsoF06DiaryMissionGen::kButtons[0];
	TextButton* widget = _state->_btnOk;
	model.cancel.widget = widget;
	model.cancel.peer = nullptr;
	model.cancel.text = CommandCenter::calypsoHdNormalizeTtfDisplayText(
		widget ? widget->getText() : std::string());
	model.cancel.rect = project(f06DiaryMissionButtonRect(wide, generatedButton.id));
	model.cancel.tone = std::string(generatedButton.tone) == "danger"
		? CalypsoActionTone::Destructive : CalypsoActionTone::Safe;
	model.cancel.restFill = generatedButton.fill;
	model.cancel.restBorder = generatedButton.border;
	model.cancel.textColor = generatedButton.text;

	calypsoCollectSelectionList(builder, model, _motion);
}

void CalypsoF06DiaryMissionUi::applyGeneratedLayout(SoldierDiaryMissionState& state, bool wide)
{
	const auto* generated = CalypsoF06DiaryMissionGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return;
	f04ApplyRect(state._window, generated->window.x, generated->window.y,
		generated->window.w, generated->window.h);
	f04ApplyRect(state._txtTitle, generated->title.x, generated->title.y,
		generated->title.w, generated->title.h);
	f04ApplyRect(state._lstKills, generated->list.x, generated->list.y,
		generated->list.w, generated->list.h);
	{
		const CalypsoLogicalRect touch = f04TouchRect(f06DiaryMissionButtonRect(wide, "cancel"));
		f04ApplyRect(state._btnOk, touch);
	}
	// Conditional context fields have no painted equivalent beyond their
	// verbatim leading rows; the parked mission-navigation controls stay
	// live native input owners (wrapping handlers and keyboard paths
	// untouched) without painted controls or pointer hit areas.
	f04ParkOffscreen(state._txtUFO);
	f04ParkOffscreen(state._txtScore);
	f04ParkOffscreen(state._txtKills);
	f04ParkOffscreen(state._txtLocation);
	f04ParkOffscreen(state._txtRace);
	f04ParkOffscreen(state._txtDaylight);
	f04ParkOffscreen(state._txtDaysWounded);
	f04ParkOffscreen(state._txtNoRecord);
	f04ParkOffscreen(state._btnPrev);
	f04ParkOffscreen(state._btnNext);
	// The generated rowHeight is the full native stride (row box + font
	// spacing). Derive the minimum row box from the live small-font metrics
	// so the projected slots agree with native hit-testing at every scale.
	if (state._lstKills && state._game && state._game->getMod())
	{
		const Font* font = state._game->getMod()->getFont("FONT_SMALL");
		const int fontH = font ? font->getHeight() : 0;
		const int spacing = font ? font->getSpacing() : 0;
		state._lstKills->setMinimumRowHeight(
			std::max(fontH, generated->rowHeight - spacing));
	}
}

void CalypsoF06DiaryMissionUi::configure(SoldierDiaryMissionState& state)
{
	// Gate-off (family id absent) returns to the legacy path untouched.
	if (!state._game || !state._game->getMod()
		|| !state._game->getMod()->isHdUiFamilyEnabled("F06"))
	{
		state._hdLayout = false;
		return;
	}
	state._hdLayout = true;
	state._hdWideLayout = f06HdWideLayout();
	applyGeneratedLayout(state, state._hdWideLayout);
	const auto* generated = CalypsoF06DiaryMissionGen::layoutForDesign(
		state._hdWideLayout ? 1280 : 740, state._hdWideLayout ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F06 mission generated layout is missing");
	state.enableUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Native HD selection-list seam AFTER scaling: shares the inset track and
	// native scroll count/reveal with the painted slots.
	f06SeamSelectionList(state._lstKills, state._window, *generated);
	auto* adapter = new CalypsoF06DiaryMissionUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter);
}

bool CalypsoF06DiaryMissionUi::resize(SoldierDiaryMissionState& state)
{
	if (!state._hdLayout) return false;
	const bool wide = f06HdWideLayout();
	state._hdWideLayout = wide;
	applyGeneratedLayout(state, wide);
	const auto* generated = CalypsoF06DiaryMissionGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return false;
	state.recaptureUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Re-seam after recapture; same values preserve drag capture, real layout
	// changes reset it. The mission record carries no selection; conditional
	// fields refresh through the native init() on mission change.
	f06SeamSelectionList(state._lstKills, state._window, *generated);
	return true;
}

} // namespace Calypso
} // namespace OpenXcom
#endif
