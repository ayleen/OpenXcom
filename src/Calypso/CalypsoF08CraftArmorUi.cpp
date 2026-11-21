#ifdef __EMSCRIPTEN__
#include "CalypsoF08CraftArmorUi.h"
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
#include "../Basescape/CraftArmorState.h"
#include "../Engine/LocalizedText.h"
#include "../Mod/Mod.h"
#include "../Savegame/Base.h"
#include "../Savegame/Craft.h"
#include "../Savegame/Soldier.h"
#include "CalypsoF06SelectionListShell.h"
#include "CalypsoF08SelectionListShell.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoUiMetrics.h"
#include "CommandCenter/CommandCenterRenderer.h"
#include "Generated/CalypsoF08CraftArmor.generated.h"

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

} // namespace

// Live native rows only: name/craft/armor cells (plus the dynamic stat cell
// in combo-sort mode) composed verbatim in native base-diver order; stable
// ids are soldier names (proper nouns, never translated). The enabled flag
// is a readout only: the native list stays the behavior/input owner, and
// its click is already a native no-op for divers aboard a STR_OUT craft.
// Every other row keeps every native click path (armor route, Ctrl
// assign/remove, quick armor, article).
CalypsoF08CraftArmorUi::SoldierRows CalypsoF08CraftArmorUi::soldierRows(const CraftArmorState& state)
{
	SoldierRows out{};
	const TextList* list = state._lstSoldiers;
	if (!list) return out;
	const auto& matrix = list->getCellTextsSnapshot();
	const std::vector<Soldier*>* soldiers = state._base ? state._base->getSoldiers() : nullptr;
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
		out.ids.push_back(texts[0]);
		CalypsoTabbedRow entry{};
		entry.text = CommandCenter::calypsoHdNormalizeTtfDisplayText(
			f04ComposeRowText(texts));
		entry.enabled = true;
		if (soldiers && row < soldiers->size() && (*soldiers)[row])
		{
			Soldier* soldier = (*soldiers)[row];
			Craft* assignment = soldier->getCraft();
			if (assignment && assignment->getStatus() == "STR_OUT")
				entry.enabled = false;
		}
		out.rows.push_back(entry);
	}
	return out;
}

CalypsoF08CraftArmorUi::~CalypsoF08CraftArmorUi()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

const void* CalypsoF08CraftArmorUi::topState() const
{
	return _state;
}

void CalypsoF08CraftArmorUi::collectLogicalSuppression(
	CalypsoHdLogicalSuppression& suppression) const
{
	// Whole-state suppression (suppressLogicalState, default true) owns the
	// top-state logical UI; list every native surface explicitly so widget
	// suppression also holds during warmup and under a covered child.
	// Parked controls (_cbxSortBy, headers) stay live input/behavior owners:
	// only their blit is suppressed and only their hit area moves
	// (visibility flags and handler bindings are never modified).
	if (!_state) return;
	suppression.add(_state->_window);
	suppression.add(_state->_txtTitle);
	suppression.add(_state->_txtName);
	suppression.add(_state->_txtCraft);
	suppression.add(_state->_txtArmor);
	suppression.add(_state->_lstSoldiers);
	suppression.add(_state->_cbxSortBy);
	suppression.add(_state->_btnOk);
}

void CalypsoF08CraftArmorUi::collect(CalypsoHdFrameBuilder& builder) const
{
	// The armor screen is a registered HD route: missing prerequisites or a
	// missing generated layout fails the route instead of silently skipping
	// to an empty collection (the overlay would fail it closed anyway).
	if (!_state || !_state->_hdLayout || !_state->_game)
		CalypsoHdUiOverlay::instance().failHdRoute("F08 armor prerequisites are unavailable");
	const bool wide = _state->_hdWideLayout;
	const auto* generated = f08CraftArmorLayout(wide);

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
	model.familyId = CalypsoF08CraftArmorGen::kFamilyId;
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
	model.listWidget = _state->_lstSoldiers;
	model.titleText = normalize(
		_state->_txtTitle ? _state->_txtTitle->getText() : std::string());
	model.rowHeight = generated->rowHeight;
	model.visibleRows = generated->visibleRows;
	model.scrollBarWidth = generated->scrollBarWidth;
	model.minThumbHeight = std::max(1, (int)std::llround(CALYPSO_MIN_TOUCH_TARGET * uiScale));
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

	for (int i = 0; i < generated->visibleRows; ++i)
		model.rowSlots.push_back(project(CalypsoF08CraftArmorGen::kRowSlots[wide ? 0 : 1][i].rect));

	// Section tab: the armor screen is a leaf route with no native tab
	// controls, so only the current armor tab paints (selected,
	// display-only) with its localized section name.
	{
		CalypsoTabbedTab armor{};
		armor.id = "armor";
		armor.label = normalize(_state->tr("STR_CAL_F08_TITLE_CRAFT_ARMOR"));
		armor.selected = true;
		armor.enabled = true;
		armor.widget = nullptr;
		armor.rect = project(f08CraftArmorTabRect(wide, "armor"));
		if (!armor.label.empty()) model.tabs.push_back(armor);
	}

	// Summary: live diver count plus the base name (proper noun, never
	// translated). Counts are language-neutral and match the approved
	// slot semantics.
	{
		CalypsoTabbedSummaryField divers{};
		divers.text = _state->_base && _state->_base->getSoldiers()
			? std::to_string(_state->_base->getSoldiers()->size())
			: std::string();
		divers.rect = project(calypsoTabbedFindRect(
			CalypsoF08CraftArmorGen::kSummaryRects[wide ? 0 : 1],
			CalypsoF08CraftArmorGen::kSummaryCount, "divers"));
		if (!divers.text.empty()) model.summary.push_back(divers);
		CalypsoTabbedSummaryField base{};
		base.text = normalize(
			_state->_base ? _state->_base->getName() : std::string());
		base.rect = project(calypsoTabbedFindRect(
			CalypsoF08CraftArmorGen::kSummaryRects[wide ? 0 : 1],
			CalypsoF08CraftArmorGen::kSummaryCount, "base"));
		if (!base.text.empty()) model.summary.push_back(base);
	}

	// Sort control: the native combobox stays the behavior/input owner on
	// its generated slot (handler, options, and original-order restore
	// untouched). The selected option text is not readable back from the
	// widget, so the slot paints the combo's own live label; the live
	// options show in the native dropdown on tap.
	for (int i = 0; i < CalypsoF08CraftArmorGen::kControlCount; ++i)
	{
		const auto& generatedControl = CalypsoF08CraftArmorGen::kControls[i];
		const std::string controlId =
			generatedControl.id ? generatedControl.id : "";
		Surface* widget = nullptr;
		std::string text;
		if (controlId == "sort")
		{
			widget = _state->_cbxSortBy;
			text = normalize(_state->tr("STR_SORT_BY"));
		}
		if (!widget) continue;
		CalypsoTabbedControl control{};
		control.id = controlId;
		control.kind = generatedControl.kind ? generatedControl.kind : "";
		control.text = text;
		control.widget = widget;
		control.rect = project(f08CraftArmorControlRect(wide, generatedControl.id));
		if (!control.text.empty()) model.controls.push_back(control);
	}

	// Collection columns from the live native header texts. The native
	// list carries three columns (name/craft/armor); the contract declares
	// two, and the composed rows below carry every native cell verbatim.
	for (int i = 0; i < CalypsoF08CraftArmorGen::kColumnCount; ++i)
	{
		const std::string columnId = CalypsoF08CraftArmorGen::kColumnIds[i]
			? CalypsoF08CraftArmorGen::kColumnIds[i] : "";
		const Text* header = nullptr;
		if (columnId == "diver") header = _state->_txtName;
		else if (columnId == "armor") header = _state->_txtArmor;
		CalypsoTabbedColumn column{};
		column.label = header ? normalize(header->getText()) : std::string();
		column.rect = project(calypsoTabbedFindRect(
			CalypsoF08CraftArmorGen::kColumnHeaders[wide ? 0 : 1],
			CalypsoF08CraftArmorGen::kColumnCount, columnId.c_str()));
		if (!column.label.empty()) model.columns.push_back(column);
	}

	// Native population/order: every physical diver row in list order; the
	// native list stays the behavior/input owner (armor route, Ctrl
	// assign/remove, quick armor, article, arrows, wheel, sort).
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

	// Footer: Done only.
	for (int i = 0; i < CalypsoF08CraftArmorGen::kActionCount; ++i)
	{
		const auto& generatedAction = CalypsoF08CraftArmorGen::kActions[i];
		const std::string actionId =
			generatedAction.id ? generatedAction.id : "";
		TextButton* widget = nullptr;
		if (actionId == "cancel") widget = _state->_btnOk;
		if (!widget) continue;
		CalypsoTabbedAction done{};
		done.widget = widget;
		done.peer = nullptr;
		done.text = normalize(widget->getText());
		done.rect = project(f08CraftArmorActionRect(wide, generatedAction.id));
		done.tone = calypsoTabbedToneForContract(
			generatedAction.tone ? generatedAction.tone : "safe");
		done.restFill = generatedAction.fill;
		done.restBorder = generatedAction.border;
		done.textColor = generatedAction.text;
		model.actions.push_back(done);
	}

	model.cutCornerPx = CalypsoF08CraftArmorGen::kCutCornerPx;
	model.panelFillTop = CalypsoF08CraftArmorGen::kPanelFillTop;
	model.panelFillBottom = CalypsoF08CraftArmorGen::kPanelFillBottom;
	model.frameColor = CalypsoF08CraftArmorGen::kFrame;
	model.selectedTabColor = CalypsoF08CraftArmorGen::kSelectedTab;
	model.dividerColor = CalypsoF08CraftArmorGen::kDivider;
	// The tabbed contract carries only the template style words: selection
	// reuses the identical selected-tab word, and scroll/footer-dot chrome
	// keeps the approved cross-family values (visually identical to the
	// retired selection-list contracts).
	model.footerDotColor = kF08ChromeFooterDot;
	model.textColor = CalypsoF08CraftArmorGen::kText;
	model.mutedTextColor = CalypsoF08CraftArmorGen::kMutedText;
	model.selectionColor = CalypsoF08CraftArmorGen::kSelectedTab;
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
	model.motionDurationMs = CalypsoF08CraftArmorGen::kMotionDurationMs;
	model.motionScaleFrom = CalypsoF08CraftArmorGen::kMotionScaleFrom;

	calypsoCollectTabbedManagement(builder, model, _motion);
}

void CalypsoF08CraftArmorUi::applyGeneratedLayout(CraftArmorState& state, bool wide)
{
	const auto* generated = CalypsoF08CraftArmorGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return;
	f04ApplyRect(state._window, generated->window.x, generated->window.y,
		generated->window.w, generated->window.h);
	f04ApplyRect(state._txtTitle, generated->title.x, generated->title.y,
		generated->title.w, generated->title.h);
	f04ApplyRect(state._lstSoldiers, generated->collectionViewport.x,
		generated->collectionViewport.y, generated->collectionViewport.w,
		generated->collectionViewport.h);
	{
		const CalypsoLogicalRect touch = f04TouchRect(f08CraftArmorActionRect(wide, "cancel"));
		f04ApplyRect(state._btnOk, touch);
	}
	// Column headers have no painted equivalent outside the tabbed slots;
	// the composed rows carry every native cell verbatim.
	f04ParkOffscreen(state._txtName);
	f04ParkOffscreen(state._txtCraft);
	f04ParkOffscreen(state._txtArmor);
	// The sort combobox stays the live behavior owner (handler, options, and
	// original-order restore untouched) on its generated control slot.
	f04ApplyRect(state._cbxSortBy, f08CraftArmorControlRect(wide, "sort"));
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

void CalypsoF08CraftArmorUi::configure(CraftArmorState& state)
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
	const auto* generated = CalypsoF08CraftArmorGen::layoutForDesign(
		state._hdWideLayout ? 1280 : 740, state._hdWideLayout ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F08 armor generated layout is missing");
	state.enableUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Tabbed list seam AFTER scaling: shares the inset track and native
	// scroll count/reveal with the painted slots.
	calypsoTabbedSeamList(state._lstSoldiers, state._window, *generated);
	auto* adapter = new CalypsoF08CraftArmorUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter);
}

bool CalypsoF08CraftArmorUi::resize(CraftArmorState& state)
{
	if (!state._hdLayout) return false;
	// Capture the selection by stable soldier name BEFORE re-layout (pitfall
	// 3), then restore-or-clamp it into the relaid-out rows (never row
	// numbers).
	const SoldierRows before = soldierRows(state);
	const CalypsoF04ListSelection captured = f04CaptureListSelection(
		state._lstSoldiers, before.ids);
	const bool wide = f08HdWideLayout();
	state._hdWideLayout = wide;
	applyGeneratedLayout(state, wide);
	const auto* generated = CalypsoF08CraftArmorGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return false;
	state.recaptureUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Re-seam after recapture; same values preserve drag capture, real layout
	// changes reset it.
	calypsoTabbedSeamList(state._lstSoldiers, state._window, *generated);
	const SoldierRows after = soldierRows(state);
	f04RestoreListSelection(state._lstSoldiers, captured, after.ids,
		(size_t)generated->visibleRows);
	return true;
}

} // namespace Calypso
} // namespace OpenXcom
#endif
