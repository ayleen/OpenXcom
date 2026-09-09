#ifdef __EMSCRIPTEN__
#include "CalypsoF08CraftEquipmentSaveUi.h"
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
#include "../Basescape/CraftEquipmentSaveState.h"
#include "../Engine/LocalizedText.h"
#include "../Mod/Mod.h"
#include "../Savegame/ItemContainer.h"
#include "../Savegame/SavedGame.h"
#include "CalypsoF06SelectionListShell.h"
#include "CalypsoF08SelectionListShell.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoUiMetrics.h"
#include "CommandCenter/CommandCenterRenderer.h"
#include "Generated/CalypsoF08CraftEquipmentSave.generated.h"

namespace OpenXcom
{
namespace Calypso
{

namespace
{

// Tabbed contracts carry only the template style words, so selection,
// scroll, and footer-dot chrome reuse the approved cross-family values
// (identical to the retired selection-list contracts).
constexpr std::uint32_t kF08ChromeFooterDot = 0x74FFB01Fu;
constexpr std::uint32_t kF08ChromeScrollTrack = 0x061B1CD6u;
constexpr std::uint32_t kF08ChromeScrollThumb = 0x74FFB099u;

// Empty-preset predicate shared with the load adapter's guard semantics:
// unknown or out-of-range slots count as empty (fail closed — an unknown
// slot never commits).
bool f08SaveSlotEmpty(Game* game, int row)
{
	if (!game || !game->getSavedGame()
		|| row < 0 || row >= SavedGame::MAX_CRAFT_LOADOUT_TEMPLATES)
		return true;
	ItemContainer* slot = game->getSavedGame()->getGlobalCraftLoadout(row);
	return !slot || slot->getContents()->empty();
}

} // namespace

// Live native slot rows only: empty, unnamed, or custom names composed
// verbatim in slot order; stable ids are slot-1..10 (never translated
// text). The row under an active inline rename paints the live editor
// text (the native cell is blanked while editing) — a verbatim mirror,
// zero invented copy. Every row paints enabled: overwrites are gated by
// the explicit two-press review on the Save button, never by dimming.
CalypsoF08CraftEquipmentSaveUi::SlotRows CalypsoF08CraftEquipmentSaveUi::slotRows(const CraftEquipmentSaveState& state)
{
	SlotRows out{};
	const TextList* list = state._lstLoadout;
	if (!list) return out;
	const auto& matrix = list->getCellTextsSnapshot();
	const TextEdit* editor = state._edtSave;
	const bool editing = editor && editor->getVisible();
	const std::string editText = editing
		? CommandCenter::calypsoHdNormalizeTtfDisplayText(editor->getText())
		: std::string();
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
		out.ids.push_back("slot-" + std::to_string(row + 1));
		std::string composed = CommandCenter::calypsoHdNormalizeTtfDisplayText(
			f04ComposeRowText(texts));
		if (composed.empty() && !editText.empty())
			composed = editText;
		CalypsoTabbedRow entry{};
		entry.text = composed;
		entry.enabled = true;
		out.rows.push_back(entry);
	}
	return out;
}

CalypsoF08CraftEquipmentSaveUi::~CalypsoF08CraftEquipmentSaveUi()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

const void* CalypsoF08CraftEquipmentSaveUi::topState() const
{
	return _state;
}

void CalypsoF08CraftEquipmentSaveUi::collectLogicalSuppression(
	CalypsoHdLogicalSuppression& suppression) const
{
	// Whole-state suppression (suppressLogicalState, default true) owns the
	// top-state logical UI; list every native surface explicitly so widget
	// suppression also holds during warmup and under a covered child. The
	// inline name editor stays a live input owner at its native position
	// (focus, Return commit, and right-click cancel untouched); only its
	// blit is suppressed while its live text mirrors into the painted row.
	if (!_state) return;
	suppression.add(_state->_window);
	suppression.add(_state->_txtTitle);
	suppression.add(_state->_lstLoadout);
	suppression.add(_state->_edtSave);
	suppression.add(_state->_btnCancel);
	suppression.add(_state->_btnSave);
}

void CalypsoF08CraftEquipmentSaveUi::collect(CalypsoHdFrameBuilder& builder) const
{
	// The preset picker is a registered HD route: missing prerequisites or
	// a missing generated layout fails the route instead of silently
	// skipping to an empty collection (the overlay would fail it closed
	// anyway).
	if (!_state || !_state->_hdLayout || !_state->_game)
		CalypsoHdUiOverlay::instance().failHdRoute("F08 preset-save prerequisites are unavailable");
	const bool wide = _state->_hdWideLayout;
	const auto* generated = f08CraftEquipmentSaveLayout(wide);

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
	model.familyId = CalypsoF08CraftEquipmentSaveGen::kFamilyId;
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
	model.listWidget = _state->_lstLoadout;
	model.titleText = normalize(
		_state->_txtTitle ? _state->_txtTitle->getText() : std::string());
	model.rowHeight = generated->rowHeight;
	model.visibleRows = generated->visibleRows;
	model.scrollBarWidth = generated->scrollBarWidth;
	model.minThumbHeight = std::max(1, (int)std::llround(CALYPSO_MIN_TOUCH_TARGET * uiScale));
	// Shared native input geometry in the same logical space, read exactly
	// once per frame: the painter uses these rects instead of its own formula.
	if (_state->_lstLoadout && _state->_lstLoadout->isCalypsoHdSelectionList())
	{
		const SDL_Rect track = _state->_lstLoadout->getCalypsoHdTrackRect();
		const SDL_Rect thumb = _state->_lstLoadout->getCalypsoHdThumbRect();
		model.hasNativeScrollGeometry = track.w > 0 && track.h > 0;
		model.nativeTrack = {track.x, track.y, track.w, track.h};
		model.nativeThumb = {thumb.x, thumb.y, thumb.w, thumb.h};
		model.nativeThumbVisible = thumb.w > 0 && thumb.h > 0;
	}

	for (int i = 0; i < generated->visibleRows; ++i)
		model.rowSlots.push_back(project(CalypsoF08CraftEquipmentSaveGen::kRowSlots[wide ? 0 : 1][i].rect));

	// Section tab: the picker is a leaf route with no native tab controls,
	// so only the current equipment tab paints (selected, display-only)
	// with its localized section name.
	{
		CalypsoTabbedTab equipment{};
		equipment.id = "equipment";
		equipment.label = normalize(_state->tr("STR_CAL_F08_TITLE_CRAFT_EQUIPMENT"));
		equipment.selected = true;
		equipment.enabled = true;
		equipment.widget = nullptr;
		equipment.rect = project(f08CraftEquipmentSaveTabRect(wide, "equipment"));
		if (!equipment.label.empty()) model.tabs.push_back(equipment);
	}

	// Summary: live slot count plus the live named (non-empty) count,
	// both language-neutral and matching the approved slot semantics.
	{
		const SlotRows bound = slotRows(*_state);
		std::size_t named = 0;
		for (std::size_t row = 0; row < bound.ids.size(); ++row)
		{
			if (!f08SaveSlotEmpty(_state->_game, (int)row)) ++named;
		}
		CalypsoTabbedSummaryField slots{};
		slots.text = std::to_string(bound.ids.size());
		slots.rect = project(calypsoTabbedFindRect(
			CalypsoF08CraftEquipmentSaveGen::kSummaryRects[wide ? 0 : 1],
			CalypsoF08CraftEquipmentSaveGen::kSummaryCount, "slots"));
		model.summary.push_back(slots);
		CalypsoTabbedSummaryField namedField{};
		namedField.text = std::to_string(named);
		namedField.rect = project(calypsoTabbedFindRect(
			CalypsoF08CraftEquipmentSaveGen::kSummaryRects[wide ? 0 : 1],
			CalypsoF08CraftEquipmentSaveGen::kSummaryCount, "named"));
		model.summary.push_back(namedField);
	}

	// Name control: the inline editor keeps its native position, focus,
	// commit, and cancel behavior (re-seated by the native select handler
	// per tap); its live text mirrors into the generated slot paint.
	for (int i = 0; i < CalypsoF08CraftEquipmentSaveGen::kControlCount; ++i)
	{
		const auto& generatedControl = CalypsoF08CraftEquipmentSaveGen::kControls[i];
		const std::string controlId =
			generatedControl.id ? generatedControl.id : "";
		Surface* widget = nullptr;
		std::string text;
		if (controlId == "name" && _state->_edtSave)
		{
			widget = _state->_edtSave;
			if (_state->_edtSave->getVisible())
				text = normalize(_state->_edtSave->getText());
		}
		if (!widget) continue;
		CalypsoTabbedControl control{};
		control.id = controlId;
		control.kind = generatedControl.kind ? generatedControl.kind : "";
		control.text = text;
		control.widget = widget;
		control.rect = project(f08CraftEquipmentSaveControlRect(wide, generatedControl.id));
		if (!control.text.empty()) model.controls.push_back(control);
	}

	// Native slot order; the native list stays the behavior/input owner
	// (select, inline rename, Save via the two-press gate, Cancel).
	const SlotRows bound = slotRows(*_state);
	model.rows = bound.rows;
	const std::size_t presets = bound.ids.size();
	if (_state->_lstLoadout)
	{
		model.scrollOffset = _state->_lstLoadout->getScroll();
		const unsigned int selected = _state->_lstLoadout->getSelectedRow();
		model.hasSelection = selected < presets;
		model.selectedRow = model.hasSelection ? selected : 0;
	}

	// Footer: the commit plus Cancel, both with live labels. The Save label
	// tracks the two-press review (Save vs the armed confirm), so the
	// review is always visible, never shortcut-only.
	for (int i = 0; i < CalypsoF08CraftEquipmentSaveGen::kActionCount; ++i)
	{
		const auto& generatedAction = CalypsoF08CraftEquipmentSaveGen::kActions[i];
		const std::string actionId =
			generatedAction.id ? generatedAction.id : "";
		TextButton* widget = nullptr;
		if (actionId == "save") widget = _state->_btnSave;
		else if (actionId == "cancel") widget = _state->_btnCancel;
		if (!widget) continue;
		CalypsoTabbedAction action{};
		action.widget = widget;
		action.peer = nullptr;
		action.text = normalize(widget->getText());
		action.rect = project(f08CraftEquipmentSaveActionRect(wide, generatedAction.id));
		action.tone = calypsoTabbedToneForContract(
			generatedAction.tone ? generatedAction.tone : "safe");
		action.restFill = generatedAction.fill;
		action.restBorder = generatedAction.border;
		action.textColor = generatedAction.text;
		model.actions.push_back(action);
	}

	model.cutCornerPx = CalypsoF08CraftEquipmentSaveGen::kCutCornerPx;
	model.panelFillTop = CalypsoF08CraftEquipmentSaveGen::kPanelFillTop;
	model.panelFillBottom = CalypsoF08CraftEquipmentSaveGen::kPanelFillBottom;
	model.frameColor = CalypsoF08CraftEquipmentSaveGen::kFrame;
	model.selectedTabColor = CalypsoF08CraftEquipmentSaveGen::kSelectedTab;
	model.dividerColor = CalypsoF08CraftEquipmentSaveGen::kDivider;
	// The tabbed contract carries only the template style words: selection
	// reuses the identical selected-tab word, and scroll/footer-dot chrome
	// keeps the approved cross-family values (visually identical to the
	// retired selection-list contracts).
	model.footerDotColor = kF08ChromeFooterDot;
	model.textColor = CalypsoF08CraftEquipmentSaveGen::kText;
	model.mutedTextColor = CalypsoF08CraftEquipmentSaveGen::kMutedText;
	model.selectionColor = CalypsoF08CraftEquipmentSaveGen::kSelectedTab;
	model.scrollTrackColor = kF08ChromeScrollTrack;
	model.scrollThumbColor = kF08ChromeScrollThumb;
	const CalypsoHdPresentationMetrics& metrics =
		CalypsoHdUiOverlay::instance().frozenMetrics();
	model.uiScale = uiScale;
	// Standard density (no presentation scale in tabbed contracts).
	model.visualScale = 1.0;
	model.projectionScaleX = uiScale * metrics.scaleX;
	model.projectionScaleY = uiScale * metrics.scaleY;
	model.titleDesignHeight = generated->title.h;
	model.motionDurationMs = CalypsoF08CraftEquipmentSaveGen::kMotionDurationMs;
	model.motionScaleFrom = CalypsoF08CraftEquipmentSaveGen::kMotionScaleFrom;

	calypsoCollectTabbedManagement(builder, model, _motion);
}

void CalypsoF08CraftEquipmentSaveUi::applyGeneratedLayout(CraftEquipmentSaveState& state, bool wide)
{
	const auto* generated = CalypsoF08CraftEquipmentSaveGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return;
	f04ApplyRect(state._window, generated->window.x, generated->window.y,
		generated->window.w, generated->window.h);
	f04ApplyRect(state._txtTitle, generated->title.x, generated->title.y,
		generated->title.w, generated->title.h);
	f04ApplyRect(state._lstLoadout, generated->collectionViewport.x,
		generated->collectionViewport.y, generated->collectionViewport.w,
		generated->collectionViewport.h);
	{
		const CalypsoLogicalRect touch = f04TouchRect(f08CraftEquipmentSaveActionRect(wide, "save"));
		f04ApplyRect(state._btnSave, touch);
	}
	// Cancel binds its own generated footer slot with its live label
	// (handler and Esc path untouched). The inline editor keeps its native
	// focus, commit, and cancel behavior; it starts on the generated name
	// slot and the native select handler re-seats it per tap, so only its
	// blit is suppressed while its live text mirrors paint-side.
	{
		const CalypsoLogicalRect touch = f04TouchRect(f08CraftEquipmentSaveActionRect(wide, "cancel"));
		f04ApplyRect(state._btnCancel, touch);
	}
	f04ApplyRect(state._edtSave, f08CraftEquipmentSaveControlRect(wide, "name"));
	// The generated rowHeight is the full native stride (row box + font
	// spacing). Derive the minimum row box from the live small-font metrics
	// so the projected slots agree with native hit-testing at every scale.
	if (state._lstLoadout && state._game && state._game->getMod())
	{
		const Font* font = state._game->getMod()->getFont("FONT_SMALL");
		const int fontH = font ? font->getHeight() : 0;
		const int spacing = font ? font->getSpacing() : 0;
		state._lstLoadout->setMinimumRowHeight(
			std::max(fontH, generated->rowHeight - spacing));
	}
}

void CalypsoF08CraftEquipmentSaveUi::configure(CraftEquipmentSaveState& state)
{
	// Gate-off (family id absent) returns to the legacy path untouched.
	if (!state._game || !state._game->getMod()
		|| !state._game->getMod()->isHdUiFamilyEnabled("F08"))
	{
		state._hdLayout = false;
		return;
	}
	state._hdLayout = true;
	state._hdWideLayout = f08HdWideLayout();
	applyGeneratedLayout(state, state._hdWideLayout);
	const auto* generated = CalypsoF08CraftEquipmentSaveGen::layoutForDesign(
		state._hdWideLayout ? 1280 : 740, state._hdWideLayout ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F08 preset-save generated layout is missing");
	state.enableUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Tabbed list seam AFTER scaling: shares the inset track and native
	// scroll count/reveal with the painted slots.
	calypsoTabbedSeamList(state._lstLoadout, state._window, *generated);
	// Route the commit through the explicit two-press overwrite review
	// (F05 Start-gate precedent): empty slots still commit through the
	// unchanged path, non-empty slots arm first. Gate-off states never
	// reach this rebinding.
	state._btnSave->onMouseClick((ActionHandler)&CraftEquipmentSaveState::hdSaveClickGate);
	auto* adapter = new CalypsoF08CraftEquipmentSaveUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter);
}

bool CalypsoF08CraftEquipmentSaveUi::resize(CraftEquipmentSaveState& state)
{
	if (!state._hdLayout) return false;
	// Capture the selection by stable slot id BEFORE re-layout (pitfall 3),
	// then restore-or-clamp it into the relaid-out rows (never row numbers).
	const SlotRows before = slotRows(state);
	const CalypsoF04ListSelection captured = f04CaptureListSelection(
		state._lstLoadout, before.ids);
	const bool wide = f08HdWideLayout();
	state._hdWideLayout = wide;
	applyGeneratedLayout(state, wide);
	const auto* generated = CalypsoF08CraftEquipmentSaveGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return false;
	state.recaptureUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Re-seam after recapture; same values preserve drag capture, real layout
	// changes reset it.
	calypsoTabbedSeamList(state._lstLoadout, state._window, *generated);
	const SlotRows after = slotRows(state);
	f04RestoreListSelection(state._lstLoadout, captured, after.ids,
		(size_t)generated->visibleRows);
	return true;
}

} // namespace Calypso
} // namespace OpenXcom
#endif
