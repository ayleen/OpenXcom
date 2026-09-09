#ifdef __EMSCRIPTEN__
#include "CalypsoF04SoldiersUi.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include "../Engine/Font.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Engine/Surface.h"
#include "../Interface/ComboBox.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextList.h"
#include "../Interface/Window.h"
#include "../Basescape/SoldiersState.h"
#include "../Mod/Mod.h"
#include "CalypsoF04SelectionListShell.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoUiMetrics.h"
#include "CommandCenter/CommandCenterRenderer.h"
#include "Generated/CalypsoF04Soldiers.generated.h"

namespace OpenXcom
{
namespace Calypso
{

struct CalypsoF04SoldiersUi::SoldierRows
{
	std::vector<std::string> ids;
	std::vector<CalypsoSelectionListRow> rows;
};

// Live native rows only: name/rank/craft cells (plus the dynamic stat cell
// in combo-sort mode) composed verbatim; stable ids are soldier names
// (proper nouns, never translated; immutable while the HD route owns the
// screen since the name editor is parked). Duplicate names resolve to the
// first match on restore; native row-number selection has the same class of
// ambiguity after mutations.
CalypsoF04SoldiersUi::SoldierRows CalypsoF04SoldiersUi::soldierRows(const SoldiersState& state)
{
	SoldierRows out{};
	const TextList* list = state._lstSoldiers;
	if (!list) return out;
	const auto& matrix = list->getCellTextsSnapshot();
	out.ids.reserve(matrix.size());
	out.rows.reserve(matrix.size());
	for (const auto& cells : matrix)
	{
		if (cells.empty() || !cells[0]) continue;
		std::vector<std::string> texts;
		texts.reserve(cells.size());
		for (const Text* cell : cells)
			texts.push_back(cell ? cell->getText() : std::string());
		out.ids.push_back(texts[0]);
		CalypsoSelectionListRow entry{};
		entry.text = CommandCenter::calypsoHdNormalizeTtfDisplayText(
			f04ComposeRowText(texts));
		entry.enabled = true;
		out.rows.push_back(entry);
	}
	return out;
}

CalypsoF04SoldiersUi::~CalypsoF04SoldiersUi()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

const void* CalypsoF04SoldiersUi::topState() const
{
	return _state;
}

void CalypsoF04SoldiersUi::collectLogicalSuppression(
	CalypsoHdLogicalSuppression& suppression) const
{
	// Whole-state suppression (suppressLogicalState, default true) owns the
	// top-state logical UI; list every native surface explicitly so widget
	// suppression also holds during warmup and under a covered child.
	if (!_state) return;
	suppression.add(_state->_window);
	suppression.add(_state->_txtTitle);
	suppression.add(_state->_txtName);
	suppression.add(_state->_txtRank);
	suppression.add(_state->_txtCraft);
	suppression.add(_state->_lstSoldiers);
	suppression.add(_state->_btnOk);
	suppression.add(_state->_btnMemorial);
	suppression.add(_state->_btnPsiTraining);
	suppression.add(_state->_btnTraining);
	suppression.add(_state->_cbxSortBy);
	suppression.add(_state->_cbxScreenActions);
}

void CalypsoF04SoldiersUi::collect(CalypsoHdFrameBuilder& builder) const
{
	// The roster is a registered HD route: missing prerequisites or a
	// missing generated layout fails the route instead of silently skipping
	// to an empty collection (the overlay would fail it closed anyway).
	if (!_state || !_state->_hdLayout || !_state->_game)
		CalypsoHdUiOverlay::instance().failHdRoute("F04 roster prerequisites are unavailable");
	const bool wide = _state->_hdWideLayout;
	const auto* generated = CalypsoF04SoldiersGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F04 roster generated layout is missing");

	const CalypsoLogicalRect window = _state->_window
		? CalypsoLogicalRect{_state->_window->getX(), _state->_window->getY(),
			_state->_window->getWidth(), _state->_window->getHeight()}
		: CalypsoLogicalRect{};
	const double uiScale = generated->window.w > 0
		? (double)window.w / generated->window.w : 1.0;
	auto project = [&](const auto& rect) -> CalypsoLogicalRect
	{
		return f04ProjectRect(window, *generated, rect);
	};

	CalypsoSelectionListModel model{};
	model.familyId = CalypsoF04SoldiersGen::kFamilyId;
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
	model.listWidget = _state->_lstSoldiers;
	model.titleText = CommandCenter::calypsoHdNormalizeTtfDisplayText(
		_state->_txtTitle ? _state->_txtTitle->getText() : std::string());
	model.protocolText = CalypsoF04SoldiersGen::kProtocol;
	model.rowHeight = generated->rowHeight;
	model.visibleRows = generated->visibleRows;
	model.scrollBarWidth = generated->scrollBarWidth;
	model.minThumbHeight = std::max(1, (int)std::llround(generated->minThumbHeight * uiScale));
	// Shared native input geometry in the same logical space, read exactly
	// once per frame: the painter uses these rects instead of its own formula.
	if (_state->_lstSoldiers && _state->_lstSoldiers->isCalypsoHdSelectionList())
	{
		const SDL_Rect track = _state->_lstSoldiers->getCalypsoHdTrackRect();
		const SDL_Rect thumb = _state->_lstSoldiers->getCalypsoHdThumbRect();
		model.hasNativeScrollGeometry = track.w > 0 && track.h > 0;
		model.nativeTrack = {track.x, track.y, track.w, track.h};
		model.nativeThumb = {thumb.x, thumb.y, thumb.w, thumb.h};
		model.nativeThumbVisible = thumb.w > 0 && thumb.h > 0;
	}

	const int slotCount = wide
		? CalypsoF04SoldiersGen::kRowSlotWideCount
		: CalypsoF04SoldiersGen::kRowSlotCompactCount;
	const auto* slots = wide
		? CalypsoF04SoldiersGen::kRowSlotsWide
		: CalypsoF04SoldiersGen::kRowSlotsCompact;
	for (int i = 0; i < slotCount; ++i)
		model.rowSlots.push_back(project(slots[i]));

	// Native population/order: every physical row in list order; the native
	// list stays the behavior/input owner (row click, arrows, wheel).
	const SoldierRows bound = soldierRows(*_state);
	model.rows = bound.rows;
	const std::size_t total = model.rows.size();
	if (_state->_lstSoldiers)
	{
		model.scrollOffset = _state->_lstSoldiers->getScroll();
		const unsigned int selected = _state->_lstSoldiers->getSelectedRow();
		model.hasSelection = selected < total;
		model.selectedRow = model.hasSelection ? selected : 0;
	}

	model.cutCornerPx = CalypsoF04SoldiersGen::kCutCornerPx;
	model.protocolTextInsetPx = CalypsoF04SoldiersGen::kProtocolTextInsetPx;
	model.panelFillTop = CalypsoF04SoldiersGen::kPanelFillTop;
	model.panelFillBottom = CalypsoF04SoldiersGen::kPanelFillBottom;
	model.frameColor = CalypsoF04SoldiersGen::kFrame;
	model.protocolColor = CalypsoF04SoldiersGen::kProtocolText;
	model.dividerColor = CalypsoF04SoldiersGen::kDivider;
	model.footerDotColor = CalypsoF04SoldiersGen::kFooterDot;
	model.textColor = CalypsoF04SoldiersGen::kText;
	model.mutedTextColor = CalypsoF04SoldiersGen::kMutedText;
	model.selectionColor = CalypsoF04SoldiersGen::kSelection;
	model.scrollTrackColor = CalypsoF04SoldiersGen::kScrollTrack;
	model.scrollThumbColor = CalypsoF04SoldiersGen::kScrollThumb;
	const CalypsoHdPresentationMetrics& metrics =
		CalypsoHdUiOverlay::instance().frozenMetrics();
	model.uiScale = uiScale;
	model.visualScale = CalypsoF04SoldiersGen::kPresentationScale;
	model.projectionScaleX = uiScale * metrics.scaleX;
	model.projectionScaleY = uiScale * metrics.scaleY;
	model.titleDesignHeight = generated->title.h;
	model.motionDurationMs = CalypsoF04SoldiersGen::kMotionDurationMs;
	model.motionScaleFrom = CalypsoF04SoldiersGen::kMotionScaleFrom;

	const auto& generatedButton = CalypsoF04SoldiersGen::kButtons[0];
	TextButton* widget = _state->_btnOk;
	model.cancel.widget = widget;
	model.cancel.peer = nullptr;
	model.cancel.text = CommandCenter::calypsoHdNormalizeTtfDisplayText(
		widget ? widget->getText() : std::string());
	model.cancel.rect = project(f04GeneratedButtonRect(wide, generatedButton.id));
	model.cancel.tone = std::string(generatedButton.tone) == "danger"
		? CalypsoActionTone::Destructive : CalypsoActionTone::Safe;
	model.cancel.restFill = generatedButton.fill;
	model.cancel.restBorder = generatedButton.border;
	model.cancel.textColor = generatedButton.text;

	calypsoCollectSelectionList(builder, model, _motion);
}

void CalypsoF04SoldiersUi::applyGeneratedLayout(SoldiersState& state, bool wide)
{
	const auto* generated = CalypsoF04SoldiersGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return;
	f04ApplyRect(state._window, generated->window.x, generated->window.y,
		generated->window.w, generated->window.h);
	f04ApplyRect(state._txtTitle, generated->title.x, generated->title.y,
		generated->title.w, generated->title.h);
	f04ApplyRect(state._lstSoldiers, generated->list.x, generated->list.y,
		generated->list.w, generated->list.h);
	{
		const CalypsoLogicalRect touch = f04TouchRect(f04GeneratedButtonRect(wide, "cancel"));
		f04ApplyRect(state._btnOk, touch);
	}
	// Column headers have no painted equivalent in the shared shell; the
	// composed rows carry every native cell verbatim.
	f04ParkOffscreen(state._txtName);
	f04ParkOffscreen(state._txtRank);
	f04ParkOffscreen(state._txtCraft);
	// Secondary actions stay live native input owners (handlers, gates, and
	// keyboard paths untouched) without painted controls or pointer hit areas.
	f04ParkOffscreen(state._btnMemorial);
	f04ParkOffscreen(state._btnPsiTraining);
	f04ParkOffscreen(state._btnTraining);
	f04ParkOffscreen(state._cbxSortBy);
	f04ParkOffscreen(state._cbxScreenActions);
	// The generated rowHeight is the full native stride (row box + font
	// spacing). Derive the minimum row box from the live small-font metrics
	// so the projected slots agree with native hit-testing at every scale.
	if (state._lstSoldiers && state._game && state._game->getMod())
	{
		const Font* font = state._game->getMod()->getFont("FONT_SMALL");
		const int fontH = font ? font->getHeight() : 0;
		const int spacing = font ? font->getSpacing() : 0;
		state._lstSoldiers->setMinimumRowHeight(
			std::max(fontH, generated->rowHeight - spacing));
	}
}

void CalypsoF04SoldiersUi::configure(SoldiersState& state)
{
	// Gate-off (family id absent) returns to the legacy path untouched.
	if (!state._game || !state._game->getMod()
		|| !state._game->getMod()->isHdUiFamilyEnabled("F04"))
	{
		state._hdLayout = false;
		return;
	}
	state._hdLayout = true;
	state._hdWideLayout = f04HdWideLayout();
	applyGeneratedLayout(state, state._hdWideLayout);
	const auto* generated = CalypsoF04SoldiersGen::layoutForDesign(
		state._hdWideLayout ? 1280 : 740, state._hdWideLayout ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F04 roster generated layout is missing");
	state.enableUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Native HD selection-list seam AFTER scaling: shares the inset track and
	// native scroll count/reveal with the painted slots.
	f04SeamSelectionList(state._lstSoldiers, state._window, *generated);
	auto* adapter = new CalypsoF04SoldiersUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter);
}

bool CalypsoF04SoldiersUi::resize(SoldiersState& state)
{
	if (!state._hdLayout) return false;
	// Capture the selection by stable id BEFORE re-layout (pitfall 3), then
	// restore-or-clamp it into the relaid-out rows (never row numbers).
	const SoldierRows before = soldierRows(state);
	const CalypsoF04ListSelection captured = f04CaptureListSelection(
		state._lstSoldiers, before.ids);
	const bool wide = f04HdWideLayout();
	state._hdWideLayout = wide;
	applyGeneratedLayout(state, wide);
	const auto* generated = CalypsoF04SoldiersGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return false;
	state.recaptureUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Re-seam after recapture; same values preserve drag capture, real layout
	// changes reset it.
	f04SeamSelectionList(state._lstSoldiers, state._window, *generated);
	const SoldierRows after = soldierRows(state);
	f04RestoreListSelection(state._lstSoldiers, captured, after.ids,
		(size_t)generated->visibleRows);
	return true;
}

} // namespace Calypso
} // namespace OpenXcom
#endif
