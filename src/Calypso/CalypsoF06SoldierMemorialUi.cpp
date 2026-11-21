#ifdef __EMSCRIPTEN__
#include "CalypsoF06SoldierMemorialUi.h"
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
#include "../Interface/TextEdit.h"
#include "../Interface/TextList.h"
#include "../Interface/Window.h"
#include "../Basescape/SoldierMemorialState.h"
#include "../Mod/Mod.h"
#include "CalypsoF04SelectionListShell.h"
#include "CalypsoF06SelectionListShell.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoUiMetrics.h"
#include "CommandCenter/CommandCenterRenderer.h"
#include "Generated/CalypsoF06SoldierMemorial.generated.h"

namespace OpenXcom
{
namespace Calypso
{

struct CalypsoF06SoldierMemorialUi::MemorialRows
{
	std::vector<std::string> ids;
	std::vector<CalypsoSelectionListRow> rows;
};

// Live native rows only, in native newest-first order: the recruited/lost
// totals lead as inert rows (verbatim native texts), then one composed row
// per dead soldier (name, rank, date verbatim). Stable ids are native
// dead-list indices (never translated text). An empty campaign loss record
// paints only the totals rows: honest, with no fake rows and no selection.
CalypsoF06SoldierMemorialUi::MemorialRows CalypsoF06SoldierMemorialUi::memorialRows(const SoldierMemorialState& state)
{
	MemorialRows out{};
	f06PushNativeTextRow(state._txtRecruited, out.rows, false);
	f06PushNativeTextRow(state._txtLost, out.rows, false);
	const size_t headerRows = out.rows.size();
	for (size_t i = 0; i < headerRows; ++i)
		out.ids.push_back("totals-" + std::to_string(i));

	const TextList* list = state._lstSoldiers;
	if (!list) return out;
	const auto& matrix = list->getCellTextsSnapshot();
	for (size_t row = 0; row < matrix.size(); ++row)
	{
		const auto& cells = matrix[row];
		if (cells.empty() || !cells[0]) continue;
		std::vector<std::string> texts;
		texts.reserve(cells.size());
		for (const Text* cell : cells)
			texts.push_back(cell ? cell->getText() : std::string());
		out.ids.push_back(row < state._indices.size()
			? "dead-" + std::to_string(state._indices[row])
			: "dead-row-" + std::to_string(row));
		CalypsoSelectionListRow entry{};
		entry.text = CommandCenter::calypsoHdNormalizeTtfDisplayText(
			f04ComposeRowText(texts));
		entry.enabled = true;
		out.rows.push_back(entry);
	}
	return out;
}

CalypsoF06SoldierMemorialUi::~CalypsoF06SoldierMemorialUi()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

const void* CalypsoF06SoldierMemorialUi::topState() const
{
	return _state;
}

void CalypsoF06SoldierMemorialUi::collectLogicalSuppression(
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
	suppression.add(_state->_txtDate);
	suppression.add(_state->_txtRecruited);
	suppression.add(_state->_txtLost);
	suppression.add(_state->_lstSoldiers);
	suppression.add(_state->_btnOk);
	suppression.add(_state->_btnStatistics);
	suppression.add(_state->_btnQuickSearch);
}

void CalypsoF06SoldierMemorialUi::collect(CalypsoHdFrameBuilder& builder) const
{
	// The memorial is a registered HD route: missing prerequisites or a
	// missing generated layout fails the route instead of silently skipping
	// to an empty collection (the overlay would fail it closed anyway).
	if (!_state || !_state->_hdLayout || !_state->_game)
		CalypsoHdUiOverlay::instance().failHdRoute("F06 memorial prerequisites are unavailable");
	const bool wide = _state->_hdWideLayout;
	const auto* generated = f06MemorialLayout(wide);

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
	model.familyId = CalypsoF06SoldierMemorialGen::kFamilyId;
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
	model.protocolText = CalypsoF06SoldierMemorialGen::kProtocol;
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
		? CalypsoF06SoldierMemorialGen::kRowSlotWideCount
		: CalypsoF06SoldierMemorialGen::kRowSlotCompactCount;
	const auto* slots = wide
		? CalypsoF06SoldierMemorialGen::kRowSlotsWide
		: CalypsoF06SoldierMemorialGen::kRowSlotsCompact;
	for (int i = 0; i < slotCount; ++i)
		model.rowSlots.push_back(project(slots[i]));

	// Native population/order: every physical row in list order; the native
	// list stays the behavior/input owner (row click opens the read-only
	// dead profile + diary through the dead-list index; the OK handler
	// keeps the Geoscape music transition).
	const MemorialRows bound = memorialRows(*_state);
	model.rows = bound.rows;
	const std::size_t total = model.rows.size();
	if (_state->_lstSoldiers)
	{
		model.scrollOffset = _state->_lstSoldiers->getScroll();
		const unsigned int selected = _state->_lstSoldiers->getSelectedRow();
		model.hasSelection = selected < total;
		model.selectedRow = model.hasSelection ? selected : 0;
	}

	model.cutCornerPx = CalypsoF06SoldierMemorialGen::kCutCornerPx;
	model.protocolTextInsetPx = CalypsoF06SoldierMemorialGen::kProtocolTextInsetPx;
	model.panelFillTop = CalypsoF06SoldierMemorialGen::kPanelFillTop;
	model.panelFillBottom = CalypsoF06SoldierMemorialGen::kPanelFillBottom;
	model.frameColor = CalypsoF06SoldierMemorialGen::kFrame;
	model.protocolColor = CalypsoF06SoldierMemorialGen::kProtocolText;
	model.dividerColor = CalypsoF06SoldierMemorialGen::kDivider;
	model.footerDotColor = CalypsoF06SoldierMemorialGen::kFooterDot;
	model.textColor = CalypsoF06SoldierMemorialGen::kText;
	model.mutedTextColor = CalypsoF06SoldierMemorialGen::kMutedText;
	model.selectionColor = CalypsoF06SoldierMemorialGen::kSelection;
	model.scrollTrackColor = CalypsoF06SoldierMemorialGen::kScrollTrack;
	model.scrollThumbColor = CalypsoF06SoldierMemorialGen::kScrollThumb;
	const CalypsoHdPresentationMetrics& metrics =
		CalypsoHdUiOverlay::instance().frozenMetrics();
	model.uiScale = uiScale;
	model.visualScale = CalypsoF06SoldierMemorialGen::kPresentationScale;
	model.projectionScaleX = uiScale * metrics.scaleX;
	model.projectionScaleY = uiScale * metrics.scaleY;
	model.titleDesignHeight = generated->title.h;
	model.motionDurationMs = CalypsoF06SoldierMemorialGen::kMotionDurationMs;
	model.motionScaleFrom = CalypsoF06SoldierMemorialGen::kMotionScaleFrom;

	const auto& generatedButton = CalypsoF06SoldierMemorialGen::kButtons[0];
	TextButton* widget = _state->_btnOk;
	model.cancel.widget = widget;
	model.cancel.peer = nullptr;
	model.cancel.text = CommandCenter::calypsoHdNormalizeTtfDisplayText(
		widget ? widget->getText() : std::string());
	model.cancel.rect = project(f06MemorialButtonRect(wide, generatedButton.id));
	model.cancel.tone = std::string(generatedButton.tone) == "danger"
		? CalypsoActionTone::Destructive : CalypsoActionTone::Safe;
	model.cancel.restFill = generatedButton.fill;
	model.cancel.restBorder = generatedButton.border;
	model.cancel.textColor = generatedButton.text;

	calypsoCollectSelectionList(builder, model, _motion);
}

void CalypsoF06SoldierMemorialUi::applyGeneratedLayout(SoldierMemorialState& state, bool wide)
{
	const auto* generated = CalypsoF06SoldierMemorialGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return;
	f04ApplyRect(state._window, generated->window.x, generated->window.y,
		generated->window.w, generated->window.h);
	f04ApplyRect(state._txtTitle, generated->title.x, generated->title.y,
		generated->title.w, generated->title.h);
	f04ApplyRect(state._lstSoldiers, generated->list.x, generated->list.y,
		generated->list.w, generated->list.h);
	{
		const CalypsoLogicalRect touch = f04TouchRect(f06MemorialButtonRect(wide, "cancel"));
		f04ApplyRect(state._btnOk, touch);
	}
	// Column headers have no painted equivalent in the shared shell; the
	// composed rows carry every native cell verbatim. Totals are painted
	// verbatim as leading inert rows from their live values each frame.
	f04ParkOffscreen(state._txtName);
	f04ParkOffscreen(state._txtRank);
	f04ParkOffscreen(state._txtDate);
	f04ParkOffscreen(state._txtRecruited);
	f04ParkOffscreen(state._txtLost);
	// Statistics and quick search stay live native input owners (push,
	// toggle/apply handlers, and the visibility gate untouched) without
	// painted controls or pointer hit areas.
	f04ParkOffscreen(state._btnStatistics);
	f04ParkOffscreen(state._btnQuickSearch);
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

void CalypsoF06SoldierMemorialUi::configure(SoldierMemorialState& state)
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
	const auto* generated = CalypsoF06SoldierMemorialGen::layoutForDesign(
		state._hdWideLayout ? 1280 : 740, state._hdWideLayout ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F06 memorial generated layout is missing");
	state.enableUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Native HD selection-list seam AFTER scaling: shares the inset track and
	// native scroll count/reveal with the painted slots.
	f06SeamSelectionList(state._lstSoldiers, state._window, *generated);
	auto* adapter = new CalypsoF06SoldierMemorialUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter);
}

bool CalypsoF06SoldierMemorialUi::resize(SoldierMemorialState& state)
{
	if (!state._hdLayout) return false;
	// Capture the selection by stable dead-list index BEFORE re-layout
	// (pitfall 3), then restore-or-clamp it into the relaid-out rows
	// (never row numbers).
	const MemorialRows before = memorialRows(state);
	const CalypsoF04ListSelection captured = f04CaptureListSelection(
		state._lstSoldiers, before.ids);
	const bool wide = f06HdWideLayout();
	state._hdWideLayout = wide;
	applyGeneratedLayout(state, wide);
	const auto* generated = CalypsoF06SoldierMemorialGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return false;
	state.recaptureUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Re-seam after recapture; same values preserve drag capture, real layout
	// changes reset it.
	f06SeamSelectionList(state._lstSoldiers, state._window, *generated);
	const MemorialRows after = memorialRows(state);
	f04RestoreListSelection(state._lstSoldiers, captured, after.ids,
		(size_t)generated->visibleRows);
	return true;
}

} // namespace Calypso
} // namespace OpenXcom
#endif
