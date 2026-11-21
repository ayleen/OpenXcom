#ifdef __EMSCRIPTEN__
#include "CalypsoF06DiaryPerformanceUi.h"
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
#include "../Basescape/SoldierDiaryPerformanceState.h"
#include "../Mod/Mod.h"
#include "CalypsoF04SelectionListShell.h"
#include "CalypsoF06SelectionListShell.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoUiMetrics.h"
#include "CommandCenter/CommandCenterRenderer.h"
#include "Generated/CalypsoF06DiaryPerformance.generated.h"

namespace OpenXcom
{
namespace Calypso
{

namespace
{

// Compose every cell row of one native list verbatim. Section-header rows
// (single column, e.g. STR_NEUTRALIZATIONS_BY_WEAPON) and totals rows are
// inert views; commendation rows (from _lstCommendations) stay activatable
// through the unchanged native Ufopaedia handler. Free and pure: it takes
// explicit outputs, never private state.
void f06AppendListCells(const TextList* list, bool activatable, std::vector<std::string>& ids, std::vector<CalypsoSelectionListRow>& rows, const std::string& prefix)
{
	if (!list) return;
	const auto& matrix = list->getCellTextsSnapshot();
	for (size_t row = 0; row < matrix.size(); ++row)
	{
		const auto& cells = matrix[row];
		if (cells.empty() || !cells[0]) continue;
		std::vector<std::string> texts;
		texts.reserve(cells.size());
		for (const Text* cell : cells)
			texts.push_back(cell ? cell->getText() : std::string());
		ids.push_back(prefix + std::to_string(row));
		CalypsoSelectionListRow entry{};
		entry.text = CommandCenter::calypsoHdNormalizeTtfDisplayText(
			f04ComposeRowText(texts));
		entry.enabled = activatable;
		rows.push_back(entry);
	}
}

} // namespace

struct CalypsoF06DiaryPerformanceUi::PerformanceRows
{
	std::vector<std::string> ids;
	std::vector<CalypsoSelectionListRow> rows;
	TextList* activeList = nullptr;
};

// Live native grouped view only: the totals header for the Kills/Missions
// views (kills/stuns/accuracy plus conditional mind-controls; missions/
// wins/score/days-wounded), the grouped breakdown rows verbatim, or the
// commendation view (medal headers, activatable commendation rows, live
// medal detail). Mind-control totals appear only when native shows them;
// NO_UFO exclusion stays engine-side. All data is read, never recomputed.
CalypsoF06DiaryPerformanceUi::PerformanceRows CalypsoF06DiaryPerformanceUi::performanceRows(const SoldierDiaryPerformanceState& state)
{
	PerformanceRows out{};
	if (state._display == DIARY_COMMENDATIONS)
	{
		out.activeList = state._lstCommendations;
		f06PushNativeTextRow(state._txtMedalName, out.rows, false);
		f06PushNativeTextRow(state._txtMedalLevel, out.rows, false);
		// Stable positional ids for the header rows actually painted.
		const size_t headers = out.rows.size();
		for (size_t i = 0; i < headers; ++i)
			out.ids.push_back("medal-header-" + std::to_string(i));
		f06AppendListCells(state._lstCommendations, true, out.ids, out.rows, "commendation-");
		f06PushNativeTextRow(state._txtMedalInfo, out.rows, false);
		if (!out.rows.empty() && out.ids.size() < out.rows.size())
			out.ids.push_back("medal-detail");
		return out;
	}
	out.activeList = state._lstPerformance;
	if (state._display == DIARY_KILLS)
		f06AppendListCells(state._lstKillTotals, false, out.ids, out.rows, "kill-totals-");
	else
		f06AppendListCells(state._lstMissionTotals, false, out.ids, out.rows, "mission-totals-");
	f06AppendListCells(state._lstPerformance, false, out.ids, out.rows, "group-");
	return out;
}

CalypsoF06DiaryPerformanceUi::~CalypsoF06DiaryPerformanceUi()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

const void* CalypsoF06DiaryPerformanceUi::topState() const
{
	return _state;
}

void CalypsoF06DiaryPerformanceUi::collectLogicalSuppression(
	CalypsoHdLogicalSuppression& suppression) const
{
	// Explicit chrome list (whole-state suppression is off so the live
	// commendation sprites keep painting as native data). Covers every
	// chrome widget so no vanilla pixel shows while sprites stay live.
	if (!_state) return;
	suppression.add(_state->_window);
	suppression.add(_state->_txtTitle);
	suppression.add(_state->_txtMedalName);
	suppression.add(_state->_txtMedalLevel);
	suppression.add(_state->_txtMedalInfo);
	suppression.add(_state->_lstPerformance);
	suppression.add(_state->_lstKillTotals);
	suppression.add(_state->_lstMissionTotals);
	suppression.add(_state->_lstCommendations);
	suppression.add(_state->_btnOk);
	suppression.add(_state->_btnPrev);
	suppression.add(_state->_btnNext);
	suppression.add(_state->_btnKills);
	suppression.add(_state->_btnMissions);
	suppression.add(_state->_btnCommendations);
}

void CalypsoF06DiaryPerformanceUi::collect(CalypsoHdFrameBuilder& builder) const
{
	// The performance route is a registered HD route: missing prerequisites
	// or a missing generated layout fails the route instead of silently
	// skipping to an empty collection (the overlay would fail it closed).
	if (!_state || !_state->_hdLayout || !_state->_game)
		CalypsoHdUiOverlay::instance().failHdRoute("F06 performance prerequisites are unavailable");
	const bool wide = _state->_hdWideLayout;
	const auto* generated = f06DiaryPerformanceLayout(wide);

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
	model.familyId = CalypsoF06DiaryPerformanceGen::kFamilyId;
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
	const PerformanceRows bound = performanceRows(*_state);
	model.listWidget = bound.activeList ? bound.activeList : _state->_lstPerformance;
	// Title is the live soldier name (native init() owns it, incl. the
	// return-to-overview soldier id contract on btnOkClick).
	model.titleText = CommandCenter::calypsoHdNormalizeTtfDisplayText(
		_state->_txtTitle ? _state->_txtTitle->getText() : std::string());
	model.protocolText = CalypsoF06DiaryPerformanceGen::kProtocol;
	model.rowHeight = generated->rowHeight;
	model.visibleRows = generated->visibleRows;
	model.scrollBarWidth = generated->scrollBarWidth;
	model.minThumbHeight = std::max(1, (int)std::llround(generated->minThumbHeight * uiScale));
	// Shared native input geometry in the same logical space, read exactly
	// once per frame: the painter uses these rects instead of its own formula.
	if (model.listWidget && model.listWidget->isCalypsoHdSelectionList())
	{
		const SDL_Rect track = model.listWidget->getCalypsoHdTrackRect();
		const SDL_Rect thumb = model.listWidget->getCalypsoHdThumbRect();
		model.hasNativeScrollGeometry = track.w > 0 && track.h > 0;
		model.nativeTrack = {track.x, track.y, track.w, track.h};
		model.nativeThumb = {thumb.x, thumb.y, thumb.w, thumb.h};
		model.nativeThumbVisible = thumb.w > 0 && thumb.h > 0;
	}

	const int slotCount = wide
		? CalypsoF06DiaryPerformanceGen::kRowSlotWideCount
		: CalypsoF06DiaryPerformanceGen::kRowSlotCompactCount;
	const auto* slots = wide
		? CalypsoF06DiaryPerformanceGen::kRowSlotsWide
		: CalypsoF06DiaryPerformanceGen::kRowSlotsCompact;
	for (int i = 0; i < slotCount; ++i)
		model.rowSlots.push_back(project(slots[i]));

	// Native population/order of the active grouped view; the visible native
	// list stays the behavior/input owner (scroll, commendation Ufopaedia
	// activation, personnel cycling with tab + selection stable on return).
	model.rows = bound.rows;
	const std::size_t total = model.rows.size();
	if (model.listWidget)
	{
		model.scrollOffset = model.listWidget->getScroll();
		const unsigned int selected = model.listWidget->getSelectedRow();
		model.hasSelection = selected < total;
		model.selectedRow = model.hasSelection ? selected : 0;
	}

	model.cutCornerPx = CalypsoF06DiaryPerformanceGen::kCutCornerPx;
	model.protocolTextInsetPx = CalypsoF06DiaryPerformanceGen::kProtocolTextInsetPx;
	model.panelFillTop = CalypsoF06DiaryPerformanceGen::kPanelFillTop;
	model.panelFillBottom = CalypsoF06DiaryPerformanceGen::kPanelFillBottom;
	model.frameColor = CalypsoF06DiaryPerformanceGen::kFrame;
	model.protocolColor = CalypsoF06DiaryPerformanceGen::kProtocolText;
	model.dividerColor = CalypsoF06DiaryPerformanceGen::kDivider;
	model.footerDotColor = CalypsoF06DiaryPerformanceGen::kFooterDot;
	model.textColor = CalypsoF06DiaryPerformanceGen::kText;
	model.mutedTextColor = CalypsoF06DiaryPerformanceGen::kMutedText;
	model.selectionColor = CalypsoF06DiaryPerformanceGen::kSelection;
	model.scrollTrackColor = CalypsoF06DiaryPerformanceGen::kScrollTrack;
	model.scrollThumbColor = CalypsoF06DiaryPerformanceGen::kScrollThumb;
	const CalypsoHdPresentationMetrics& metrics =
		CalypsoHdUiOverlay::instance().frozenMetrics();
	model.uiScale = uiScale;
	model.visualScale = CalypsoF06DiaryPerformanceGen::kPresentationScale;
	model.projectionScaleX = uiScale * metrics.scaleX;
	model.projectionScaleY = uiScale * metrics.scaleY;
	model.titleDesignHeight = generated->title.h;
	model.motionDurationMs = CalypsoF06DiaryPerformanceGen::kMotionDurationMs;
	model.motionScaleFrom = CalypsoF06DiaryPerformanceGen::kMotionScaleFrom;

	const auto& generatedButton = CalypsoF06DiaryPerformanceGen::kButtons[0];
	TextButton* widget = _state->_btnOk;
	model.cancel.widget = widget;
	model.cancel.peer = nullptr;
	model.cancel.text = CommandCenter::calypsoHdNormalizeTtfDisplayText(
		widget ? widget->getText() : std::string());
	model.cancel.rect = project(f06DiaryPerformanceButtonRect(wide, generatedButton.id));
	model.cancel.tone = std::string(generatedButton.tone) == "danger"
		? CalypsoActionTone::Destructive : CalypsoActionTone::Safe;
	model.cancel.restFill = generatedButton.fill;
	model.cancel.restBorder = generatedButton.border;
	model.cancel.textColor = generatedButton.text;

	calypsoCollectSelectionList(builder, model, _motion);
}

void CalypsoF06DiaryPerformanceUi::applyGeneratedLayout(SoldierDiaryPerformanceState& state, bool wide)
{
	const auto* generated = CalypsoF06DiaryPerformanceGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return;
	f04ApplyRect(state._window, generated->window.x, generated->window.y,
		generated->window.w, generated->window.h);
	f04ApplyRect(state._txtTitle, generated->title.x, generated->title.y,
		generated->title.w, generated->title.h);
	// All grouped views share the list slot; the toggles swap native
	// visibility and the adapter mirrors whichever list is visible.
	f04ApplyRect(state._lstPerformance, generated->list.x, generated->list.y,
		generated->list.w, generated->list.h);
	f04ApplyRect(state._lstCommendations, generated->list.x, generated->list.y,
		generated->list.w, generated->list.h);
	{
		const CalypsoLogicalRect touch = f04TouchRect(f06DiaryPerformanceButtonRect(wide, "cancel"));
		f04ApplyRect(state._btnOk, touch);
	}
	// Commendation sprites are live native data, not chrome: pin each
	// visible-slot surface onto its painted row slot so sprite content
	// (refreshed natively per scroll offset in drawSprites) lands on the
	// matching painted row. Positions refresh on every layout/resize; in
	// non-commendation views the surfaces are natively cleared.
	{
		const int slotCount = wide
			? CalypsoF06DiaryPerformanceGen::kRowSlotWideCount
			: CalypsoF06DiaryPerformanceGen::kRowSlotCompactCount;
		const auto* slots = wide
			? CalypsoF06DiaryPerformanceGen::kRowSlotsWide
			: CalypsoF06DiaryPerformanceGen::kRowSlotsCompact;
		const size_t surfaces = state._commendations.size();
		for (size_t i = 0; i < surfaces && (int)i < slotCount; ++i)
		{
			if (!state._commendations[i] || !state._commendationDecorations[i]) continue;
			const auto& slot = slots[i];
			f04ApplyRect(state._commendations[i], slot.x, slot.y, 31, 8);
			f04ApplyRect(state._commendationDecorations[i], slot.x, slot.y, 31, 8);
		}
	}
	// Totals headers and medal texts are painted verbatim as inert rows from
	// their live values each frame. View toggles and personnel cycling stay
	// live native input owners (handlers, dead-roster reversal, tab state,
	// and keyboard paths untouched) without painted controls.
	f04ParkOffscreen(state._lstKillTotals);
	f04ParkOffscreen(state._lstMissionTotals);
	f04ParkOffscreen(state._txtMedalName);
	f04ParkOffscreen(state._txtMedalLevel);
	f04ParkOffscreen(state._txtMedalInfo);
	f04ParkOffscreen(state._btnPrev);
	f04ParkOffscreen(state._btnNext);
	f04ParkOffscreen(state._btnKills);
	f04ParkOffscreen(state._btnMissions);
	f04ParkOffscreen(state._btnCommendations);
	// The generated rowHeight is the full native stride (row box + font
	// spacing). Derive the minimum row box from the live small-font metrics
	// so the projected slots agree with native hit-testing at every scale.
	if (state._game && state._game->getMod())
	{
		const Font* font = state._game->getMod()->getFont("FONT_SMALL");
		const int fontH = font ? font->getHeight() : 0;
		const int spacing = font ? font->getSpacing() : 0;
		const int minimum = std::max(fontH, generated->rowHeight - spacing);
		if (state._lstPerformance) state._lstPerformance->setMinimumRowHeight(minimum);
		if (state._lstCommendations) state._lstCommendations->setMinimumRowHeight(minimum);
	}
}

void CalypsoF06DiaryPerformanceUi::configure(SoldierDiaryPerformanceState& state)
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
	const auto* generated = CalypsoF06DiaryPerformanceGen::layoutForDesign(
		state._hdWideLayout ? 1280 : 740, state._hdWideLayout ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F06 performance generated layout is missing");
	state.enableUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Native HD selection-list seam AFTER scaling: shares the inset track and
	// native scroll count/reveal with the painted slots.
	f06SeamSelectionList(state._lstPerformance, state._window, *generated);
	f06SeamSelectionList(state._lstCommendations, state._window, *generated);
	auto* adapter = new CalypsoF06DiaryPerformanceUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter);
}

bool CalypsoF06DiaryPerformanceUi::resize(SoldierDiaryPerformanceState& state)
{
	if (!state._hdLayout) return false;
	// Capture the selection by stable positional id BEFORE re-layout
	// (pitfall 3), then restore-or-clamp it into the relaid-out rows.
	const PerformanceRows before = performanceRows(state);
	const CalypsoF04ListSelection captured = f04CaptureListSelection(
		before.activeList, before.ids);
	const bool wide = f06HdWideLayout();
	state._hdWideLayout = wide;
	applyGeneratedLayout(state, wide);
	const auto* generated = CalypsoF06DiaryPerformanceGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return false;
	state.recaptureUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Re-seam after recapture; same values preserve drag capture, real layout
	// changes reset it.
	f06SeamSelectionList(state._lstPerformance, state._window, *generated);
	f06SeamSelectionList(state._lstCommendations, state._window, *generated);
	const PerformanceRows after = performanceRows(state);
	f04RestoreListSelection(after.activeList, captured, after.ids,
		(size_t)generated->visibleRows);
	return true;
}

} // namespace Calypso
} // namespace OpenXcom
#endif
