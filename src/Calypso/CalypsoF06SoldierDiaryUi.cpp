#ifdef __EMSCRIPTEN__
#include "CalypsoF06SoldierDiaryUi.h"
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
#include "../Basescape/SoldierDiaryOverviewState.h"
#include "../Mod/Mod.h"
#include "../Savegame/MissionStatistics.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Soldier.h"
#include "../Savegame/SoldierDiary.h"
#include "CalypsoF04SelectionListShell.h"
#include "CalypsoF06SelectionListShell.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoUiMetrics.h"
#include "CommandCenter/CommandCenterRenderer.h"
#include "Generated/CalypsoF06SoldierDiary.generated.h"

namespace OpenXcom
{
namespace Calypso
{

struct CalypsoF06SoldierDiaryUi::DiaryRows
{
	std::vector<std::string> ids;
	std::vector<CalypsoSelectionListRow> rows;
};

// Live native rows only, in native order: conditional dead-personnel death
// block first (KIA/MIA title, death date, killer/weapon — present only when
// native shows them), then one composed row per mission ledger entry
// (mission, rating, day/month/year verbatim). Stable ids are campaign mission
// ids (never translated text); the death block carries adapter-internal
// positional ids and is inert.
CalypsoF06SoldierDiaryUi::DiaryRows CalypsoF06SoldierDiaryUi::diaryRows(const SoldierDiaryOverviewState& state)
{
	DiaryRows out{};
	// Death block: native death texts exist only for dead personnel
	// (SoldierDiaryOverviewState ctor binds them when _base == 0).
	f06PushNativeTextRow(state._txtDeathTitle, out.rows, false);
	f06PushNativeTextRow(state._txtDeathDate, out.rows, false);
	f06PushNativeTextRow(state._txtDeathInfo, out.rows, false);
	const size_t headerRows = out.rows.size();
	for (size_t i = 0; i < headerRows; ++i)
		out.ids.push_back("death-" + std::to_string(i));

	const TextList* list = state._lstDiary;
	if (!list) return out;
	const auto& matrix = list->getCellTextsSnapshot();

	// Stable mission ids in ledger order: the same engine sources the native
	// init() filters (campaign statistics in order, kept when the soldier's
	// diary lists the mission id). Read-only; no recomputation.
	std::vector<int> missionIds;
	if (state._game && state._game->getSavedGame() && state._soldier && state._soldier->getDiary())
	{
		const std::vector<int>& sailed = state._soldier->getDiary()->getMissionIdList();
		for (const MissionStatistics* ms : *state._game->getSavedGame()->getMissionStatistics())
		{
			if (!ms) continue;
			for (int id : sailed)
			{
				if (id == ms->id)
				{
					missionIds.push_back(id);
					break;
				}
			}
		}
	}

	size_t row = 0;
	for (const auto& cells : matrix)
	{
		if (cells.empty() || !cells[0]) continue;
		std::vector<std::string> texts;
		texts.reserve(cells.size());
		for (const Text* cell : cells)
			texts.push_back(cell ? cell->getText() : std::string());
		out.ids.push_back(row < missionIds.size()
			? "mission-" + std::to_string(missionIds[row])
			: "mission-row-" + std::to_string(row));
		CalypsoSelectionListRow entry{};
		entry.text = CommandCenter::calypsoHdNormalizeTtfDisplayText(
			f04ComposeRowText(texts));
		entry.enabled = true;
		out.rows.push_back(entry);
		++row;
	}
	return out;
}

CalypsoF06SoldierDiaryUi::~CalypsoF06SoldierDiaryUi()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

const void* CalypsoF06SoldierDiaryUi::topState() const
{
	return _state;
}

void CalypsoF06SoldierDiaryUi::collectLogicalSuppression(
	CalypsoHdLogicalSuppression& suppression) const
{
	// Whole-state suppression (suppressLogicalState, default true) owns the
	// top-state logical UI; list every native surface explicitly so widget
	// suppression also holds during warmup and under a covered child.
	if (!_state) return;
	suppression.add(_state->_window);
	suppression.add(_state->_txtTitle);
	suppression.add(_state->_txtMission);
	suppression.add(_state->_txtRating);
	suppression.add(_state->_txtDate);
	suppression.add(_state->_txtDeathTitle);
	suppression.add(_state->_txtDeathInfo);
	suppression.add(_state->_txtDeathDate);
	suppression.add(_state->_lstDiary);
	suppression.add(_state->_btnOk);
	suppression.add(_state->_btnPrev);
	suppression.add(_state->_btnNext);
	suppression.add(_state->_btnKills);
	suppression.add(_state->_btnMissions);
	suppression.add(_state->_btnCommendations);
}

void CalypsoF06SoldierDiaryUi::collect(CalypsoHdFrameBuilder& builder) const
{
	// The diary overview is a registered HD route: missing prerequisites or
	// a missing generated layout fails the route instead of silently
	// skipping to an empty collection (the overlay would fail it closed).
	if (!_state || !_state->_hdLayout || !_state->_game)
		CalypsoHdUiOverlay::instance().failHdRoute("F06 diary prerequisites are unavailable");
	const bool wide = _state->_hdWideLayout;
	const auto* generated = f06DiaryLayout(wide);

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
	model.familyId = CalypsoF06SoldierDiaryGen::kFamilyId;
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
	model.listWidget = _state->_lstDiary;
	// Title is the live soldier name (native init() owns it, incl. the
	// return-to-profile soldier id contract on btnOkClick).
	model.titleText = CommandCenter::calypsoHdNormalizeTtfDisplayText(
		_state->_txtTitle ? _state->_txtTitle->getText() : std::string());
	model.protocolText = CalypsoF06SoldierDiaryGen::kProtocol;
	model.rowHeight = generated->rowHeight;
	model.visibleRows = generated->visibleRows;
	model.scrollBarWidth = generated->scrollBarWidth;
	model.minThumbHeight = std::max(1, (int)std::llround(generated->minThumbHeight * uiScale));
	// Shared native input geometry in the same logical space, read exactly
	// once per frame: the painter uses these rects instead of its own formula.
	if (_state->_lstDiary && _state->_lstDiary->isCalypsoHdSelectionList())
	{
		const SDL_Rect track = _state->_lstDiary->getCalypsoHdTrackRect();
		const SDL_Rect thumb = _state->_lstDiary->getCalypsoHdThumbRect();
		model.hasNativeScrollGeometry = track.w > 0 && track.h > 0;
		model.nativeTrack = {track.x, track.y, track.w, track.h};
		model.nativeThumb = {thumb.x, thumb.y, thumb.w, thumb.h};
		model.nativeThumbVisible = thumb.w > 0 && thumb.h > 0;
	}

	const int slotCount = wide
		? CalypsoF06SoldierDiaryGen::kRowSlotWideCount
		: CalypsoF06SoldierDiaryGen::kRowSlotCompactCount;
	const auto* slots = wide
		? CalypsoF06SoldierDiaryGen::kRowSlotsWide
		: CalypsoF06SoldierDiaryGen::kRowSlotsCompact;
	for (int i = 0; i < slotCount; ++i)
		model.rowSlots.push_back(project(slots[i]));

	// Native population/order: every physical row in list order; the native
	// list stays the behavior/input owner (row click opens the mission
	// detail with selection/scroll preserved on return via _doNotReset).
	const DiaryRows bound = diaryRows(*_state);
	model.rows = bound.rows;
	const std::size_t total = model.rows.size();
	if (_state->_lstDiary)
	{
		model.scrollOffset = _state->_lstDiary->getScroll();
		const unsigned int selected = _state->_lstDiary->getSelectedRow();
		model.hasSelection = selected < total;
		model.selectedRow = model.hasSelection ? selected : 0;
	}

	model.cutCornerPx = CalypsoF06SoldierDiaryGen::kCutCornerPx;
	model.protocolTextInsetPx = CalypsoF06SoldierDiaryGen::kProtocolTextInsetPx;
	model.panelFillTop = CalypsoF06SoldierDiaryGen::kPanelFillTop;
	model.panelFillBottom = CalypsoF06SoldierDiaryGen::kPanelFillBottom;
	model.frameColor = CalypsoF06SoldierDiaryGen::kFrame;
	model.protocolColor = CalypsoF06SoldierDiaryGen::kProtocolText;
	model.dividerColor = CalypsoF06SoldierDiaryGen::kDivider;
	model.footerDotColor = CalypsoF06SoldierDiaryGen::kFooterDot;
	model.textColor = CalypsoF06SoldierDiaryGen::kText;
	model.mutedTextColor = CalypsoF06SoldierDiaryGen::kMutedText;
	model.selectionColor = CalypsoF06SoldierDiaryGen::kSelection;
	model.scrollTrackColor = CalypsoF06SoldierDiaryGen::kScrollTrack;
	model.scrollThumbColor = CalypsoF06SoldierDiaryGen::kScrollThumb;
	const CalypsoHdPresentationMetrics& metrics =
		CalypsoHdUiOverlay::instance().frozenMetrics();
	model.uiScale = uiScale;
	model.visualScale = CalypsoF06SoldierDiaryGen::kPresentationScale;
	model.projectionScaleX = uiScale * metrics.scaleX;
	model.projectionScaleY = uiScale * metrics.scaleY;
	model.titleDesignHeight = generated->title.h;
	model.motionDurationMs = CalypsoF06SoldierDiaryGen::kMotionDurationMs;
	model.motionScaleFrom = CalypsoF06SoldierDiaryGen::kMotionScaleFrom;

	const auto& generatedButton = CalypsoF06SoldierDiaryGen::kButtons[0];
	TextButton* widget = _state->_btnOk;
	model.cancel.widget = widget;
	model.cancel.peer = nullptr;
	model.cancel.text = CommandCenter::calypsoHdNormalizeTtfDisplayText(
		widget ? widget->getText() : std::string());
	model.cancel.rect = project(f06DiaryButtonRect(wide, generatedButton.id));
	model.cancel.tone = std::string(generatedButton.tone) == "danger"
		? CalypsoActionTone::Destructive : CalypsoActionTone::Safe;
	model.cancel.restFill = generatedButton.fill;
	model.cancel.restBorder = generatedButton.border;
	model.cancel.textColor = generatedButton.text;

	calypsoCollectSelectionList(builder, model, _motion);
}

void CalypsoF06SoldierDiaryUi::applyGeneratedLayout(SoldierDiaryOverviewState& state, bool wide)
{
	const auto* generated = CalypsoF06SoldierDiaryGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return;
	f04ApplyRect(state._window, generated->window.x, generated->window.y,
		generated->window.w, generated->window.h);
	f04ApplyRect(state._txtTitle, generated->title.x, generated->title.y,
		generated->title.w, generated->title.h);
	f04ApplyRect(state._lstDiary, generated->list.x, generated->list.y,
		generated->list.w, generated->list.h);
	{
		const CalypsoLogicalRect touch = f04TouchRect(f06DiaryButtonRect(wide, "cancel"));
		f04ApplyRect(state._btnOk, touch);
	}
	// Column headers have no painted equivalent in the shared shell; the
	// composed rows carry every native cell verbatim.
	f04ParkOffscreen(state._txtMission);
	f04ParkOffscreen(state._txtRating);
	f04ParkOffscreen(state._txtDate);
	// Death block, tab destinations, and personnel cycling stay live native
	// input/behavior owners (handlers, dead-roster reversal, commendation
	// visibility gate, and keyboard paths untouched) without painted
	// controls or pointer hit areas. The death texts are painted verbatim
	// as leading inert rows from their live values each frame.
	f04ParkOffscreen(state._txtDeathTitle);
	f04ParkOffscreen(state._txtDeathInfo);
	f04ParkOffscreen(state._txtDeathDate);
	f04ParkOffscreen(state._btnPrev);
	f04ParkOffscreen(state._btnNext);
	f04ParkOffscreen(state._btnKills);
	f04ParkOffscreen(state._btnMissions);
	f04ParkOffscreen(state._btnCommendations);
	// The generated rowHeight is the full native stride (row box + font
	// spacing). Derive the minimum row box from the live small-font metrics
	// so the projected slots agree with native hit-testing at every scale.
	if (state._lstDiary && state._game && state._game->getMod())
	{
		const Font* font = state._game->getMod()->getFont("FONT_SMALL");
		const int fontH = font ? font->getHeight() : 0;
		const int spacing = font ? font->getSpacing() : 0;
		state._lstDiary->setMinimumRowHeight(
			std::max(fontH, generated->rowHeight - spacing));
	}
}

void CalypsoF06SoldierDiaryUi::configure(SoldierDiaryOverviewState& state)
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
	const auto* generated = CalypsoF06SoldierDiaryGen::layoutForDesign(
		state._hdWideLayout ? 1280 : 740, state._hdWideLayout ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F06 diary generated layout is missing");
	state.enableUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Native HD selection-list seam AFTER scaling: shares the inset track and
	// native scroll count/reveal with the painted slots.
	f06SeamSelectionList(state._lstDiary, state._window, *generated);
	auto* adapter = new CalypsoF06SoldierDiaryUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter);
}

bool CalypsoF06SoldierDiaryUi::resize(SoldierDiaryOverviewState& state)
{
	if (!state._hdLayout) return false;
	// Capture the selection by stable mission id BEFORE re-layout (pitfall
	// 3), then restore-or-clamp it into the relaid-out rows (never rows).
	const DiaryRows before = diaryRows(state);
	const CalypsoF04ListSelection captured = f04CaptureListSelection(
		state._lstDiary, before.ids);
	const bool wide = f06HdWideLayout();
	state._hdWideLayout = wide;
	applyGeneratedLayout(state, wide);
	const auto* generated = CalypsoF06SoldierDiaryGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return false;
	state.recaptureUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Re-seam after recapture; same values preserve drag capture, real layout
	// changes reset it.
	f06SeamSelectionList(state._lstDiary, state._window, *generated);
	const DiaryRows after = diaryRows(state);
	f04RestoreListSelection(state._lstDiary, captured, after.ids,
		(size_t)generated->visibleRows);
	return true;
}

} // namespace Calypso
} // namespace OpenXcom
#endif
