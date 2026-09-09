#ifdef __EMSCRIPTEN__
#include "CalypsoF08CraftWeaponsUi.h"
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
#include "../Basescape/CraftWeaponsState.h"
#include "../Engine/LocalizedText.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleCraft.h"
#include "../Mod/RuleCraftWeapon.h"
#include "../Savegame/Base.h"
#include "../Savegame/Craft.h"
#include "CalypsoF06SelectionListShell.h"
#include "CalypsoF08SelectionListShell.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoUiMetrics.h"
#include "CommandCenter/CommandCenterRenderer.h"
#include "Generated/CalypsoF08CraftWeapons.generated.h"

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

// Live native rows only: type plus launcher-stock plus clip-stock (or the
// localized not-available marker) cells composed verbatim in native
// candidate order; stable ids are the ruleset weapon types (never
// translated text; the leading None row uses its stable empty-mount id).
// Every row paints enabled: candidacy is already delegated to the native
// state, and the capacity pre-check errors are native UX that must stay
// reachable through the unchanged click handler.
CalypsoF08CraftWeaponsUi::WeaponRows CalypsoF08CraftWeaponsUi::weaponRows(const CraftWeaponsState& state)
{
	WeaponRows out{};
	const TextList* list = state._lstWeapons;
	if (!list) return out;
	const auto& matrix = list->getCellTextsSnapshot();
	out.ids.reserve(matrix.size());
	out.rows.reserve(matrix.size() + 1);
	for (std::size_t row = 0; row < matrix.size(); ++row)
	{
		const auto& cells = matrix[row];
		if (cells.empty() || !cells[0]) continue;
		std::vector<std::string> texts;
		texts.reserve(cells.size());
		for (const Text* cell : cells)
			texts.push_back(cell ? cell->getText() : std::string());
		if (row < state._weapons.size() && state._weapons[row])
			out.ids.push_back(state._weapons[row]->getType());
		else
			out.ids.push_back("none");
		CalypsoTabbedRow entry{};
		entry.text = CommandCenter::calypsoHdNormalizeTtfDisplayText(
			f04ComposeRowText(texts));
		entry.enabled = true;
		out.rows.push_back(entry);
	}
	return out;
}

CalypsoF08CraftWeaponsUi::~CalypsoF08CraftWeaponsUi()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

const void* CalypsoF08CraftWeaponsUi::topState() const
{
	return _state;
}

void CalypsoF08CraftWeaponsUi::collectLogicalSuppression(
	CalypsoHdLogicalSuppression& suppression) const
{
	// Whole-state suppression (suppressLogicalState, default true) owns the
	// top-state logical UI; list every native surface explicitly so widget
	// suppression also holds during warmup and under a covered child.
	if (!_state) return;
	suppression.add(_state->_window);
	suppression.add(_state->_txtTitle);
	suppression.add(_state->_txtArmament);
	suppression.add(_state->_txtQuantity);
	suppression.add(_state->_txtAmmunition);
	suppression.add(_state->_txtCurrentWeapon);
	suppression.add(_state->_lstWeapons);
	suppression.add(_state->_btnCancel);
}

void CalypsoF08CraftWeaponsUi::collect(CalypsoHdFrameBuilder& builder) const
{
	// The weapons picker is a registered HD route: missing prerequisites or
	// a missing generated layout fails the route instead of silently
	// skipping to an empty collection (the overlay would fail it closed
	// anyway).
	if (!_state || !_state->_hdLayout || !_state->_game)
		CalypsoHdUiOverlay::instance().failHdRoute("F08 weapons prerequisites are unavailable");
	const bool wide = _state->_hdWideLayout;
	const auto* generated = f08CraftWeaponsLayout(wide);

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
	model.familyId = CalypsoF08CraftWeaponsGen::kFamilyId;
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
	model.listWidget = _state->_lstWeapons;
	model.titleText = normalize(
		_state->_txtTitle ? _state->_txtTitle->getText() : std::string());
	model.rowHeight = generated->rowHeight;
	model.visibleRows = generated->visibleRows;
	model.scrollBarWidth = generated->scrollBarWidth;
	model.minThumbHeight = std::max(1, (int)std::llround(CALYPSO_MIN_TOUCH_TARGET * uiScale));
	// Shared native input geometry in the same logical space, read exactly
	// once per frame: the painter uses these rects instead of its own formula.
	if (_state->_lstWeapons && _state->_lstWeapons->isCalypsoHdSelectionList())
	{
		const SDL_Rect track = _state->_lstWeapons->getCalypsoHdTrackRect();
		const SDL_Rect thumb = _state->_lstWeapons->getCalypsoHdThumbRect();
		model.hasNativeScrollGeometry = track.w > 0 && track.h > 0;
		model.nativeTrack = {track.x, track.y, track.w, track.h};
		model.nativeThumb = {thumb.x, thumb.y, thumb.w, thumb.h};
		model.nativeThumbVisible = thumb.w > 0 && thumb.h > 0;
	}

	for (int i = 0; i < generated->visibleRows; ++i)
		model.rowSlots.push_back(project(CalypsoF08CraftWeaponsGen::kRowSlots[wide ? 0 : 1][i].rect));

	// Section tab: the weapons picker is a leaf route with no native tab
	// controls, so only the current weapons tab paints (selected,
	// display-only) with its localized section name.
	{
		CalypsoTabbedTab weapons{};
		weapons.id = "weapons";
		weapons.label = normalize(_state->tr("STR_CAL_F08_TITLE_CRAFT_WEAPONS"));
		weapons.selected = true;
		weapons.enabled = true;
		weapons.widget = nullptr;
		weapons.rect = project(f08CraftWeaponsTabRect(wide, "weapons"));
		if (!weapons.label.empty()) model.tabs.push_back(weapons);
	}

	// Summary: live mount-slot count from the craft rules. Ammo has no
	// exact native owner (per-candidate stocks paint in the rows below)
	// and stays unpainted rather than stale.
	{
		CalypsoTabbedSummaryField mounts{};
		mounts.text = _state->_craft && _state->_craft->getRules()
			? std::to_string(_state->_craft->getRules()->getWeapons())
			: std::string();
		mounts.rect = project(calypsoTabbedFindRect(
			CalypsoF08CraftWeaponsGen::kSummaryRects[wide ? 0 : 1],
			CalypsoF08CraftWeaponsGen::kSummaryCount, "mounts"));
		if (!mounts.text.empty()) model.summary.push_back(mounts);
	}

	// Collection columns from the live native header texts.
	for (int i = 0; i < CalypsoF08CraftWeaponsGen::kColumnCount; ++i)
	{
		const std::string columnId = CalypsoF08CraftWeaponsGen::kColumnIds[i]
			? CalypsoF08CraftWeaponsGen::kColumnIds[i] : "";
		const Text* header = nullptr;
		if (columnId == "type") header = _state->_txtArmament;
		else if (columnId == "launchers") header = _state->_txtQuantity;
		else if (columnId == "clips") header = _state->_txtAmmunition;
		CalypsoTabbedColumn column{};
		column.label = header ? normalize(header->getText()) : std::string();
		column.rect = project(calypsoTabbedFindRect(
			CalypsoF08CraftWeaponsGen::kColumnHeaders[wide ? 0 : 1],
			CalypsoF08CraftWeaponsGen::kColumnCount, columnId.c_str()));
		if (!column.label.empty()) model.columns.push_back(column);
	}

	// Native population/order: every candidate in native order. The native
	// list stays the behavior/input owner (row click exchanges,
	// middle-click opens the reference).
	const WeaponRows bound = weaponRows(*_state);
	model.rows = bound.rows;
	const std::size_t candidates = bound.ids.size();
	if (_state->_lstWeapons)
	{
		model.scrollOffset = _state->_lstWeapons->getScroll();
		const unsigned int selected = _state->_lstWeapons->getSelectedRow();
		model.hasSelection = selected < candidates;
		model.selectedRow = model.hasSelection ? selected : 0;
	}

	// Selected-mount detail. Title composes the selected candidate row's
	// verbatim type cell, subtitle mirrors the live mount-context line,
	// and ammo/stock metrics mirror the selected row's verbatim stock
	// cells — the same numbers the picker rows show. The contract's
	// Change/More entries resolve to no native widget (exchange is the
	// row tap, reference is the middle-click), so they stay unpainted
	// instead of dead-end controls.
	{
		model.detail.present = true;
		model.detail.panel = project(generated->detailPanel);
		model.detail.titleRect = project(CalypsoF08CraftWeaponsGen::kDetailTitleRects[wide ? 0 : 1]);
		model.detail.subtitleRect = project(CalypsoF08CraftWeaponsGen::kDetailSubtitleRects[wide ? 0 : 1]);
		model.detail.subtitleText = normalize(
			_state->_txtCurrentWeapon ? _state->_txtCurrentWeapon->getText() : std::string());
		if (_state->_lstWeapons)
		{
			const auto& matrix = _state->_lstWeapons->getCellTextsSnapshot();
			const unsigned int row = _state->_lstWeapons->getSelectedRow();
			if (row < matrix.size() && !matrix[row].empty() && matrix[row][0])
			{
				model.detail.titleText = normalize(matrix[row][0]->getText());
				// Native cells run type/launchers/clips: launchers feed
				// the stock slot, clips feed the ammo slot.
				const char* const metricIds[2] = {"stock", "ammo"};
				for (int i = 0; i < 2; ++i)
				{
					const std::size_t cell = (std::size_t)(i + 1);
					if (cell >= matrix[row].size() || !matrix[row][cell]) continue;
					const std::string value = normalize(matrix[row][cell]->getText());
					if (value.empty()) continue;
					CalypsoTabbedMetric metric{};
					metric.text = value;
					metric.rect = project(f08CraftWeaponsDetailMetricRect(wide, metricIds[i]));
					model.detail.metrics.push_back(metric);
				}
			}
		}
	}

	// Footer: Cancel only.
	for (int i = 0; i < CalypsoF08CraftWeaponsGen::kActionCount; ++i)
	{
		const auto& generatedAction = CalypsoF08CraftWeaponsGen::kActions[i];
		const std::string actionId =
			generatedAction.id ? generatedAction.id : "";
		TextButton* widget = nullptr;
		if (actionId == "cancel") widget = _state->_btnCancel;
		if (!widget) continue;
		CalypsoTabbedAction cancel{};
		cancel.widget = widget;
		cancel.peer = nullptr;
		cancel.text = normalize(widget->getText());
		cancel.rect = project(f08CraftWeaponsActionRect(wide, generatedAction.id));
		cancel.tone = calypsoTabbedToneForContract(
			generatedAction.tone ? generatedAction.tone : "safe");
		cancel.restFill = generatedAction.fill;
		cancel.restBorder = generatedAction.border;
		cancel.textColor = generatedAction.text;
		model.actions.push_back(cancel);
	}

	model.cutCornerPx = CalypsoF08CraftWeaponsGen::kCutCornerPx;
	model.panelFillTop = CalypsoF08CraftWeaponsGen::kPanelFillTop;
	model.panelFillBottom = CalypsoF08CraftWeaponsGen::kPanelFillBottom;
	model.frameColor = CalypsoF08CraftWeaponsGen::kFrame;
	model.selectedTabColor = CalypsoF08CraftWeaponsGen::kSelectedTab;
	model.dividerColor = CalypsoF08CraftWeaponsGen::kDivider;
	// The tabbed contract carries only the template style words: selection
	// reuses the identical selected-tab word, and scroll/footer-dot chrome
	// keeps the approved cross-family values (visually identical to the
	// retired selection-list contracts).
	model.footerDotColor = kF08ChromeFooterDot;
	model.textColor = CalypsoF08CraftWeaponsGen::kText;
	model.mutedTextColor = CalypsoF08CraftWeaponsGen::kMutedText;
	model.selectionColor = CalypsoF08CraftWeaponsGen::kSelectedTab;
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
	model.motionDurationMs = CalypsoF08CraftWeaponsGen::kMotionDurationMs;
	model.motionScaleFrom = CalypsoF08CraftWeaponsGen::kMotionScaleFrom;

	calypsoCollectTabbedManagement(builder, model, _motion);
}

void CalypsoF08CraftWeaponsUi::applyGeneratedLayout(CraftWeaponsState& state, bool wide)
{
	const auto* generated = CalypsoF08CraftWeaponsGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return;
	f04ApplyRect(state._window, generated->window.x, generated->window.y,
		generated->window.w, generated->window.h);
	f04ApplyRect(state._txtTitle, generated->title.x, generated->title.y,
		generated->title.w, generated->title.h);
	f04ApplyRect(state._lstWeapons, generated->collectionViewport.x,
		generated->collectionViewport.y, generated->collectionViewport.w,
		generated->collectionViewport.h);
	{
		const CalypsoLogicalRect touch = f04TouchRect(f08CraftWeaponsActionRect(wide, "cancel"));
		f04ApplyRect(state._btnCancel, touch);
	}
	// Column headers and the mount-context line have no painted equivalent
	// outside the tabbed slots; the candidate rows carry every native cell
	// verbatim and the mount context paints in the detail subtitle.
	f04ParkOffscreen(state._txtArmament);
	f04ParkOffscreen(state._txtQuantity);
	f04ParkOffscreen(state._txtAmmunition);
	f04ParkOffscreen(state._txtCurrentWeapon);
	// The generated rowHeight is the full native stride (row box + font
	// spacing). Derive the minimum row box from the live small-font metrics
	// so the projected slots agree with native hit-testing at every scale.
	if (state._lstWeapons && state._game && state._game->getMod())
	{
		const Font* font = state._game->getMod()->getFont("FONT_SMALL");
		const int fontH = font ? font->getHeight() : 0;
		const int spacing = font ? font->getSpacing() : 0;
		state._lstWeapons->setMinimumRowHeight(
			std::max(fontH, generated->rowHeight - spacing));
	}
}

void CalypsoF08CraftWeaponsUi::configure(CraftWeaponsState& state)
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
	const auto* generated = CalypsoF08CraftWeaponsGen::layoutForDesign(
		state._hdWideLayout ? 1280 : 740, state._hdWideLayout ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F08 weapons generated layout is missing");
	state.enableUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Tabbed list seam AFTER scaling: shares the inset track and native
	// scroll count/reveal with the painted slots.
	calypsoTabbedSeamList(state._lstWeapons, state._window, *generated);
	auto* adapter = new CalypsoF08CraftWeaponsUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter);
}

bool CalypsoF08CraftWeaponsUi::resize(CraftWeaponsState& state)
{
	if (!state._hdLayout) return false;
	// Capture the selection by stable weapon type BEFORE re-layout (pitfall
	// 3), then restore-or-clamp it into the relaid-out rows (never row
	// numbers).
	const WeaponRows before = weaponRows(state);
	const CalypsoF04ListSelection captured = f04CaptureListSelection(
		state._lstWeapons, before.ids);
	const bool wide = f08HdWideLayout();
	state._hdWideLayout = wide;
	applyGeneratedLayout(state, wide);
	const auto* generated = CalypsoF08CraftWeaponsGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return false;
	state.recaptureUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Re-seam after recapture; same values preserve drag capture, real layout
	// changes reset it.
	calypsoTabbedSeamList(state._lstWeapons, state._window, *generated);
	const WeaponRows after = weaponRows(state);
	f04RestoreListSelection(state._lstWeapons, captured, after.ids,
		(size_t)generated->visibleRows);
	return true;
}

} // namespace Calypso
} // namespace OpenXcom
#endif
