#ifdef __EMSCRIPTEN__
#include "CalypsoF07PilotSelectUi.h"
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
#include "../Basescape/CraftPilotSelectState.h"
#include "../Engine/LocalizedText.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleCraft.h"
#include "../Savegame/Base.h"
#include "../Savegame/Craft.h"
#include "CalypsoF07SelectionListShell.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoUiMetrics.h"
#include "CommandCenter/CommandCenterRenderer.h"
#include "Generated/CalypsoF07PilotSelect.generated.h"

namespace OpenXcom
{
namespace Calypso
{


// Live native candidate rows only, in native order: name + firing,
// reactions, bravery from getStatsWithSoldierBonusesOnly(), composed
// verbatim. Stable ids are native soldier ids (never translated text);
// the fallback row index applies only if the native id vector ever
// desynchronizes from the painted rows.
CalypsoF07PilotSelectUi::PilotSelectRows CalypsoF07PilotSelectUi::pilotSelectRows(const CraftPilotSelectState& state)
{
	PilotSelectRows out{};
	const TextList* list = state._lstPilot;
	if (!list) return out;
	const auto& matrix = list->getCellTextsSnapshot();
	out.ids.reserve(matrix.size());
	out.rows.reserve(matrix.size());
	for (std::size_t row = 0; row < matrix.size(); ++row)
	{
		const auto& cells = matrix[row];
		if (cells.empty() || !cells[0]) continue;
		std::vector<std::string> texts;
		texts.reserve(cells.size());
		for (const Text* cell : cells)
			texts.push_back(cell ? cell->getText() : std::string());
		out.ids.push_back(row < state._pilot.size()
			? "pilot-" + std::to_string(state._pilot[row])
			: "pilot-row-" + std::to_string(row));
		CalypsoTabbedRow entry{};
		entry.cells.reserve(2);
		for (int i = 0; i < 2; ++i)
		{
			entry.cells.push_back(CommandCenter::calypsoHdNormalizeTtfDisplayText(
				i < (int)cells.size() && cells[i] ? cells[i]->getText() : std::string()));
		}
		entry.text = CommandCenter::calypsoHdNormalizeTtfDisplayText(
			f04ComposeRowText(texts));
		entry.enabled = true;
		out.rows.push_back(entry);
	}
	return out;
}

CalypsoF07PilotSelectUi::~CalypsoF07PilotSelectUi()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

const void* CalypsoF07PilotSelectUi::topState() const
{
	return _state;
}

void CalypsoF07PilotSelectUi::collectLogicalSuppression(
	CalypsoHdLogicalSuppression& suppression) const
{
	// Whole-state suppression (suppressLogicalState, default true) owns the
	// top-state logical UI; list every native surface explicitly so widget
	// suppression also holds during warmup and under a covered child.
	// The selector is pushed over the pilots screen: unlike the roster and
	// overview adapters this one does NOT suppress when covered (no
	// persistent lower shell of its own to own).
	if (!_state) return;
	suppression.add(_state->_window);
	suppression.add(_state->_txtTitle);
	suppression.add(_state->_txtName);
	suppression.add(_state->_txtFiringAcc);
	suppression.add(_state->_txtReactions);
	suppression.add(_state->_txtBravery);
	suppression.add(_state->_lstPilot);
	suppression.add(_state->_btnCancel);
}

void CalypsoF07PilotSelectUi::collect(CalypsoHdFrameBuilder& builder) const
{
	// The selector is a registered HD route: missing prerequisites or a
	// missing generated layout fails the route instead of silently skipping
	// to an empty collection (the overlay would fail it closed anyway).
	if (!_state || !_state->_hdLayout || !_state->_game)
		CalypsoHdUiOverlay::instance().failHdRoute("F07 pilot selector prerequisites are unavailable");
	const bool wide = _state->_hdWideLayout;
	const auto* generated = f07PilotSelectLayout(wide);

	const CalypsoLogicalRect window = _state->_window
		? CalypsoLogicalRect{_state->_window->getX(), _state->_window->getY(),
			_state->_window->getWidth(), _state->_window->getHeight()}
		: CalypsoLogicalRect{};
	const double uiScale = generated->window.w > 0
		? (double)window.w / generated->window.w : 1.0;
	auto project = [&](const auto& rect) -> CalypsoLogicalRect
	{
		return calypsoTabbedProjectRect(window, *generated, rect);
	};
	auto normalize = [](const std::string& text) -> std::string
	{
		return CommandCenter::calypsoHdNormalizeTtfDisplayText(text);
	};

	CalypsoTabbedModel model{};
	model.familyId = CalypsoF07PilotSelectGen::kFamilyId;
	model.instance = _state;
	model.mod = _state->_game->getMod();
	model.wide = wide;
	model.designWidth = generated->designWidth;
	model.designHeight = generated->designHeight;
	model.window = window;
	model.title = project(generated->title);
	model.summaryBar = project(generated->summaryBar);
	model.tabBar = project(generated->tabBar);
	model.toolbarBar = project(generated->toolbarBar);
	model.collectionViewport = project(generated->collectionViewport);
	model.detailPanel = project(generated->detailPanel);
	model.footer = project(generated->footer);
	model.windowWidget = _state->_window;
	model.titleWidget = _state->_txtTitle;
	model.listWidget = _state->_lstPilot;
	model.titleText = normalize(
		_state->_txtTitle ? _state->_txtTitle->getText() : std::string());
	model.rowHeight = generated->rowHeight;
	model.visibleRows = generated->visibleRows;
	model.scrollBarWidth = generated->scrollBarWidth;
	model.minThumbHeight = std::max(1, (int)std::llround(CALYPSO_MIN_TOUCH_TARGET * uiScale));
	// Shared native input geometry in the same logical space, read exactly
	// once per frame: the painter uses these rects instead of its own formula.
	if (_state->_lstPilot && _state->_lstPilot->isCalypsoHdSelectionList())
	{
		const SDL_Rect track = _state->_lstPilot->getCalypsoHdTrackRect();
		const SDL_Rect thumb = _state->_lstPilot->getCalypsoHdThumbRect();
		model.hasNativeScrollGeometry = track.w > 0 && track.h > 0;
		model.nativeTrack = {track.x, track.y, track.w, track.h};
		model.nativeThumb = {thumb.x, thumb.y, thumb.w, thumb.h};
		model.nativeThumbVisible = thumb.w > 0 && thumb.h > 0;
	}

	for (int i = 0; i < generated->visibleRows; ++i)
		model.rowSlots.push_back(project(CalypsoF07PilotSelectGen::kRowSlots[wide ? 0 : 1][i].rect));

	// Section tab: the selector is a leaf route with no native tab
	// controls, so only the current pilots tab paints (selected,
	// display-only) with its localized section name.
	{
		CalypsoTabbedTab pilots{};
		pilots.id = "pilots";
		pilots.label = normalize(_state->tr("STR_PILOTS"));
		pilots.selected = true;
		pilots.enabled = true;
		pilots.widget = nullptr;
		pilots.rect = project(f07PilotSelectTabRect(wide, "pilots"));
		if (!pilots.label.empty()) model.tabs.push_back(pilots);
	}

	// Summary: the live craft name plus the live required-seat count
	// (language-neutral, matching the approved slot semantics).
	{
		const std::vector<Craft*>* crafts =
			_state->_base ? _state->_base->getCrafts() : nullptr;
		const Craft* craft = (crafts && _state->_craft < crafts->size())
			? (*crafts)[_state->_craft] : nullptr;
		CalypsoTabbedSummaryField craftField{};
		craftField.text = normalize(craft && _state->_game
			? craft->getName(_state->_game->getLanguage()) : std::string());
		craftField.rect = project(calypsoTabbedFindRect(
			CalypsoF07PilotSelectGen::kSummaryRects[wide ? 0 : 1],
			CalypsoF07PilotSelectGen::kSummaryCount, "craft"));
		if (!craftField.text.empty()) model.summary.push_back(craftField);
		const int seats = (craft && craft->getRules())
			? craft->getRules()->getPilots() : 0;
		if (seats > 0)
		{
			CalypsoTabbedSummaryField seatsField{};
			seatsField.text = std::to_string(seats);
			seatsField.rect = project(calypsoTabbedFindRect(
				CalypsoF07PilotSelectGen::kSummaryRects[wide ? 0 : 1],
				CalypsoF07PilotSelectGen::kSummaryCount, "seats"));
			model.summary.push_back(seatsField);
		}
	}

	// Collection columns from the live native header texts. The native list
	// carries four columns (name + three abbreviated stats); the contract
	// declares two, and the composed rows below carry every native cell
	// verbatim.
	for (int i = 0; i < CalypsoF07PilotSelectGen::kColumnCount; ++i)
	{
		const std::string columnId = CalypsoF07PilotSelectGen::kColumnIds[i]
			? CalypsoF07PilotSelectGen::kColumnIds[i] : "";
		const Text* header = nullptr;
		if (columnId == "pilot") header = _state->_txtName;
		else if (columnId == "firing") header = _state->_txtFiringAcc;
		CalypsoTabbedColumn column{};
		column.label = header ? normalize(header->getText()) : std::string();
		column.rect = project(calypsoTabbedFindRect(
			CalypsoF07PilotSelectGen::kColumnHeaders[wide ? 0 : 1],
			CalypsoF07PilotSelectGen::kColumnCount, columnId.c_str()));
		if (!column.label.empty()) model.columns.push_back(column);
	}

	// Native population/order: every candidate in list order; the native
	// list stays the behavior/input owner (row click assigns immediately).
	const PilotSelectRows bound = pilotSelectRows(*_state);
	model.rows = bound.rows;
	const std::size_t total = model.rows.size();
	if (_state->_lstPilot)
	{
		model.scrollOffset = _state->_lstPilot->getScroll();
		const unsigned int selected = _state->_lstPilot->getSelectedRow();
		model.hasSelection = selected < total;
		model.selectedRow = model.hasSelection ? selected : 0;
	}

	// Footer: Cancel only. The fixture Assign entry resolves to no native
	// widget (assignment is the row tap through lstPilotClick), so it stays
	// unpainted instead of a dead-end control.
	for (int i = 0; i < CalypsoF07PilotSelectGen::kActionCount; ++i)
	{
		const auto& generatedAction = CalypsoF07PilotSelectGen::kActions[i];
		const std::string actionId =
			generatedAction.id ? generatedAction.id : "";
		if (actionId != "cancel") continue;
		TextButton* widget = _state->_btnCancel;
		if (!widget) continue;
		CalypsoTabbedAction cancel{};
		cancel.widget = widget;
		cancel.peer = nullptr;
		cancel.text = normalize(widget->getText());
		cancel.rect = project(f07PilotSelectActionRect(wide, generatedAction.id));
		cancel.tone = calypsoTabbedToneForContract(
			generatedAction.tone ? generatedAction.tone : "safe");
		cancel.restFill = generatedAction.fill;
		cancel.restBorder = generatedAction.border;
		cancel.textColor = generatedAction.text;
		model.actions.push_back(cancel);
	}

	model.cutCornerPx = CalypsoF07PilotSelectGen::kCutCornerPx;
	model.panelFillTop = CalypsoF07PilotSelectGen::kPanelFillTop;
	model.panelFillBottom = CalypsoF07PilotSelectGen::kPanelFillBottom;
	model.frameColor = CalypsoF07PilotSelectGen::kFrame;
	model.selectedTabColor = CalypsoF07PilotSelectGen::kSelectedTab;
	model.dividerColor = CalypsoF07PilotSelectGen::kDivider;
	model.footerDotColor = CalypsoF07PilotSelectGen::kFooterDot;
	model.textColor = CalypsoF07PilotSelectGen::kText;
	model.mutedTextColor = CalypsoF07PilotSelectGen::kMutedText;
	model.selectionColor = CalypsoF07PilotSelectGen::kSelection;
	model.scrollTrackColor = CalypsoF07PilotSelectGen::kScrollTrack;
	model.scrollThumbColor = CalypsoF07PilotSelectGen::kScrollThumb;
	const CalypsoHdPresentationMetrics& metrics =
		CalypsoHdUiOverlay::instance().frozenMetrics();
	model.uiScale = uiScale;
	// Standard density (no presentation scale in tabbed contracts).
	model.visualScale = 1.0;
	model.projectionScaleX = uiScale * metrics.scaleX;
	model.projectionScaleY = uiScale * metrics.scaleY;
	model.titleDesignHeight = generated->title.h;
	model.motionDurationMs = CalypsoF07PilotSelectGen::kMotionDurationMs;
	model.motionScaleFrom = CalypsoF07PilotSelectGen::kMotionScaleFrom;

	calypsoCollectTabbedManagement(builder, model, _motion);
}

void CalypsoF07PilotSelectUi::applyGeneratedLayout(CraftPilotSelectState& state, bool wide)
{
	const auto* generated = CalypsoF07PilotSelectGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return;
	f04ApplyRect(state._window, generated->window.x, generated->window.y,
		generated->window.w, generated->window.h);
	f04ApplyRect(state._txtTitle, generated->title.x, generated->title.y,
		generated->title.w, generated->title.h);
	f04ApplyRect(state._lstPilot, generated->collectionViewport.x,
		generated->collectionViewport.y, generated->collectionViewport.w,
		generated->collectionViewport.h);
	{
		const CalypsoLogicalRect touch = f04TouchRect(f07PilotSelectActionRect(wide, "cancel"));
		f04ApplyRect(state._btnCancel, touch);
	}
	// Column headers have no painted equivalent in the shared shell; the
	// composed rows carry every native cell verbatim.
	f04ParkOffscreen(state._txtName);
	f04ParkOffscreen(state._txtFiringAcc);
	f04ParkOffscreen(state._txtReactions);
	f04ParkOffscreen(state._txtBravery);
	// The generated rowHeight is the full native stride (row box + font
	// spacing). Derive the minimum row box from the live small-font metrics
	// so the projected slots agree with native hit-testing at every scale.
	if (state._lstPilot && state._game && state._game->getMod())
	{
		const Font* font = state._game->getMod()->getFont("FONT_SMALL");
		const int fontH = font ? font->getHeight() : 0;
		const int spacing = font ? font->getSpacing() : 0;
		state._lstPilot->setMinimumRowHeight(
			std::max(fontH, generated->rowHeight - spacing));
	}
}

void CalypsoF07PilotSelectUi::configure(CraftPilotSelectState& state)
{
	// Gate-off (family id absent) returns to the legacy path untouched.
	if (!state._game || !state._game->getMod()
		|| !state._game->getMod()->isHdUiFamilyEnabled("F07"))
	{
		state._hdLayout = false;
		return;
	}
	state._hdLayout = true;
	state._hdWideLayout = f07HdWideLayout();
	applyGeneratedLayout(state, state._hdWideLayout);
	const auto* generated = CalypsoF07PilotSelectGen::layoutForDesign(
		state._hdWideLayout ? 1280 : 740, state._hdWideLayout ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F07 pilot selector generated layout is missing");
	state.enableUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Tabbed list seam AFTER scaling: shares the inset track and native
	// scroll count/reveal with the painted slots.
	calypsoTabbedSeamList(state._lstPilot, state._window, *generated);
	auto* adapter = new CalypsoF07PilotSelectUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter);
}

bool CalypsoF07PilotSelectUi::resize(CraftPilotSelectState& state)
{
	if (!state._hdLayout) return false;
	// Capture the selection by stable soldier id BEFORE re-layout (pitfall
	// 3), then restore-or-clamp it into the relaid-out rows (never row
	// numbers).
	const PilotSelectRows before = pilotSelectRows(state);
	const CalypsoF04ListSelection captured = f04CaptureListSelection(
		state._lstPilot, before.ids);
	const bool wide = f07HdWideLayout();
	state._hdWideLayout = wide;
	applyGeneratedLayout(state, wide);
	const auto* generated = CalypsoF07PilotSelectGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return false;
	state.recaptureUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Re-seam after recapture; same values preserve drag capture, real layout
	// changes reset it.
	calypsoTabbedSeamList(state._lstPilot, state._window, *generated);
	const PilotSelectRows after = pilotSelectRows(state);
	f04RestoreListSelection(state._lstPilot, captured, after.ids,
		(size_t)generated->visibleRows);
	return true;
}

} // namespace Calypso
} // namespace OpenXcom
#endif
