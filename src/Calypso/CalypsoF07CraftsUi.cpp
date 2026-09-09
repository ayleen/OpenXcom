#ifdef __EMSCRIPTEN__
#include "CalypsoF07CraftsUi.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include "../Engine/Font.h"
#include "../Engine/Game.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Options.h"
#include "../Engine/Surface.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextList.h"
#include "../Interface/Window.h"
#include "../Basescape/CraftsState.h"
#include "../Mod/Mod.h"
#include "../Savegame/Base.h"
#include "../Savegame/Craft.h"
#include "CalypsoF07SelectionListShell.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoTabbedManagementRenderer.h"
#include "CalypsoUiMetrics.h"
#include "CommandCenter/CommandCenterRenderer.h"
#include "Generated/CalypsoF07Crafts.generated.h"

namespace OpenXcom
{
namespace Calypso
{


// Live native rows only: name/status/weapon/crew/HWP cells composed verbatim
// in native base-craft order; stable ids are craft names (proper nouns, never
// translated). A craft whose live status is STR_OUT paints disabled: the row
// click is a native no-op there, so the disabled readout mirrors existing
// behavior instead of implying a future action. Row/craft alignment holds
// because initList populates the list in base-craft order.
CalypsoF07CraftsUi::CraftRows CalypsoF07CraftsUi::craftRows(const CraftsState& state)
{
	CraftRows out{};
	const TextList* list = state._lstCrafts;
	if (!list) return out;
	const auto& matrix = list->getCellTextsSnapshot();
	const std::vector<Craft*>* crafts = state._base ? state._base->getCrafts() : nullptr;
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
		// Per-column values aligned to the contract columns
		// (submarine/status/weapons-crew/open); the open cell stays empty
		// here because the renderer paints the availability marker.
		std::string weapon = texts.size() > 2 ? texts[2] : std::string();
		std::string crew = texts.size() > 3 ? texts[3] : std::string();
		std::string weaponsCrew = weapon;
		if (!crew.empty())
			weaponsCrew = weaponsCrew.empty() ? crew : weaponsCrew + " \xc2\xb7 " + crew;
		entry.cells.push_back(CommandCenter::calypsoHdNormalizeTtfDisplayText(texts[0]));
		entry.cells.push_back(CommandCenter::calypsoHdNormalizeTtfDisplayText(
			texts.size() > 1 ? texts[1] : std::string()));
		entry.cells.push_back(CommandCenter::calypsoHdNormalizeTtfDisplayText(weaponsCrew));
		entry.cells.push_back(std::string());
		// Openability is engine-owned: only STR_OUT blocks lstCraftsClick.
		entry.enabled = true;
		if (crafts && row < crafts->size() && (*crafts)[row]
			&& (*crafts)[row]->getStatus() == "STR_OUT")
			entry.enabled = false;
		out.rows.push_back(entry);
	}
	return out;
}

CalypsoF07CraftsUi::~CalypsoF07CraftsUi()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

const void* CalypsoF07CraftsUi::topState() const
{
	return _state;
}

void CalypsoF07CraftsUi::collectLogicalSuppression(
	CalypsoHdLogicalSuppression& suppression) const
{
	// Whole-state suppression (suppressLogicalState, default true) owns the
	// top-state logical UI; list every native surface explicitly so widget
	// suppression also holds during warmup and under a covered child.
	if (!_state) return;
	suppression.add(_state->_window);
	suppression.add(_state->_txtTitle);
	suppression.add(_state->_txtBase);
	suppression.add(_state->_txtName);
	suppression.add(_state->_txtStatus);
	suppression.add(_state->_txtWeapon);
	suppression.add(_state->_txtCrew);
	suppression.add(_state->_txtHwp);
	suppression.add(_state->_lstCrafts);
	suppression.add(_state->_btnOk);
}

void CalypsoF07CraftsUi::collect(CalypsoHdFrameBuilder& builder) const
{
	// The roster is a registered HD route: missing prerequisites or a
	// missing generated layout fails the route instead of silently skipping
	// to an empty collection (the overlay would fail it closed anyway).
	if (!_state || !_state->_hdLayout || !_state->_game)
		CalypsoHdUiOverlay::instance().failHdRoute("F07 roster prerequisites are unavailable");
	const bool wide = _state->_hdWideLayout;
	const auto* generated = f07CraftsLayout(wide);

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
	model.familyId = CalypsoF07CraftsGen::kFamilyId;
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
	model.listWidget = _state->_lstCrafts;
	model.titleText = normalize(
		_state->_txtTitle ? _state->_txtTitle->getText() : std::string());
	model.rowHeight = generated->rowHeight;
	model.visibleRows = generated->visibleRows;
	model.scrollBarWidth = generated->scrollBarWidth;
	model.minThumbHeight = std::max(1, (int)std::llround(CALYPSO_MIN_TOUCH_TARGET * uiScale));
	// Shared native input geometry in the same logical space, read exactly
	// once per frame: the painter uses these rects instead of its own formula.
	if (_state->_lstCrafts && _state->_lstCrafts->isCalypsoHdSelectionList())
	{
		const SDL_Rect track = _state->_lstCrafts->getCalypsoHdTrackRect();
		const SDL_Rect thumb = _state->_lstCrafts->getCalypsoHdThumbRect();
		model.hasNativeScrollGeometry = track.w > 0 && track.h > 0;
		model.nativeTrack = {track.x, track.y, track.w, track.h};
		model.nativeThumb = {thumb.x, thumb.y, thumb.w, thumb.h};
		model.nativeThumbVisible = thumb.w > 0 && thumb.h > 0;
	}

	for (int i = 0; i < generated->visibleRows; ++i)
		model.rowSlots.push_back(project(CalypsoF07CraftsGen::kRowSlots[wide ? 0 : 1][i].rect));

	// Forces section tabs. No native section-tab widget exists on either
	// roster state, so both tabs paint display-only with the live
	// submarines selection; cross-section navigation is a documented
	// follow-up, never an invented push. Labels resolve from localization.
	CalypsoTabbedTab divers{};
	divers.id = "divers";
	divers.label = normalize(_state->tr("STR_CAL_F07_TAB_DIVERS"));
	divers.selected = false;
	divers.enabled = true;
	divers.widget = nullptr;
	divers.rect = project(f07CraftsTabRect(wide, "divers"));
	CalypsoTabbedTab submarines{};
	submarines.id = "submarines";
	submarines.label = normalize(_state->tr("STR_CAL_F07_TAB_SUBMARINES"));
	submarines.selected = true;
	submarines.enabled = true;
	submarines.widget = nullptr;
	submarines.rect = project(f07CraftsTabRect(wide, "submarines"));
	model.tabs.push_back(divers);
	model.tabs.push_back(submarines);

	// Summary facts from live base state: the native base line verbatim
	// plus the operational-strength counts through localization. Funds
	// have no native text on this screen and stay unpainted rather than
	// invented.
	{
		if (_state->_txtBase)
		{
			CalypsoTabbedSummaryField base{};
			base.text = normalize(_state->_txtBase->getText());
			base.rect = project(f07CraftsSummaryRect(wide, "active-base"));
			if (!base.text.empty()) model.summary.push_back(base);
		}
		std::size_t ready = 0;
		std::size_t away = 0;
		if (_state->_base && _state->_base->getCrafts())
		{
			for (const Craft* craft : *_state->_base->getCrafts())
			{
				if (!craft) continue;
				if (craft->getStatus() == "STR_OUT") ++away;
				else ++ready;
			}
		}
		if (_state->_base && _state->_base->getCrafts()
			&& !_state->_base->getCrafts()->empty())
		{
			CalypsoTabbedSummaryField strength{};
			strength.text = normalize(
				_state->tr("STR_CAL_F07_SUMMARY_STRENGTH").arg(ready).arg(away));
			strength.rect = project(f07CraftsSummaryRect(wide, "strength"));
			if (!strength.text.empty()) model.summary.push_back(strength);
		}
	}

	// Column headers from the live native header texts (localized verbatim).
	// The weapons-crew column joins the two native headers exactly like its
	// rows do; the open column carries no text because the renderer paints
	// the ›/— availability marker from the row verdict instead.
	{
		const char* const ids[4] = {"submarine", "status", "weapons-crew", "open"};
		for (int i = 0; i < CalypsoF07CraftsGen::kColumnCount; ++i)
		{
			CalypsoTabbedColumn column{};
			column.rect = project(CalypsoF07CraftsGen::kColumnHeaders[wide ? 0 : 1][i].rect);
			const std::string id = CalypsoF07CraftsGen::kColumnIds[i];
			column.id = id;
			if (id == ids[0] && _state->_txtName)
				column.label = normalize(_state->_txtName->getText());
			else if (id == ids[1] && _state->_txtStatus)
				column.label = normalize(_state->_txtStatus->getText());
			else if (id == ids[2] && _state->_txtWeapon && _state->_txtCrew)
				column.label = normalize(_state->_txtWeapon->getText())
					+ " \xc2\xb7 " + normalize(_state->_txtCrew->getText());
			// The open column keeps its id with an empty label: the header
			// paints nothing while the renderer still addresses the marker
			// cells below it. All four entries are pushed so row cells keep
			// a one-to-one column mapping.
			model.columns.push_back(column);
		}
	}

	// Native population/order: every physical row in list order; the native
	// list stays the behavior/input owner (row click opens the overview,
	// Shift+right-click reorders, middle-click opens the craft article).
	// Rows carry per-column cells aligned to the contract columns; the Out
	// readout stays text + disabled, never color-only.
	const CraftRows bound = craftRows(*_state);
	for (const auto& entry : bound.rows)
	{
		CalypsoTabbedRow painted{};
		painted.text = entry.text;
		painted.cells = entry.cells;
		painted.enabled = entry.enabled;
		model.rows.push_back(painted);
	}
	const std::size_t rowsTotal = model.rows.size();
	if (_state->_lstCrafts)
	{
		model.scrollOffset = _state->_lstCrafts->getScroll();
		const unsigned int selected = _state->_lstCrafts->getSelectedRow();
		model.hasSelection = selected < rowsTotal;
		model.selectedRow = model.hasSelection ? selected : 0;
	}

	// No selected-submarine detail on the roster: the approved board gives
	// the collection the full workspace and row activation stays
	// native-owned, so model.detail stays absent and nothing paints here.

	// Footer: exactly the Base Overview close action bound to the existing
	// Done handler. The live label comes from the relabeled native button
	// (see configure), so no hardcoded copy ships; the Open route stays
	// with the native row chevron.
	for (int i = 0; i < CalypsoF07CraftsGen::kActionCount; ++i)
	{
		const auto& generatedAction = CalypsoF07CraftsGen::kActions[i];
		const std::string actionId =
			generatedAction.id ? generatedAction.id : "";
		if (actionId != "overview") continue;
		TextButton* widget = _state->_btnOk;
		if (!widget) continue;
		CalypsoTabbedAction done{};
		done.widget = widget;
		done.peer = nullptr;
		done.text = normalize(widget->getText());
		done.rect = project(f07CraftsActionRect(wide, generatedAction.id));
		done.tone = calypsoTabbedToneForContract(
			generatedAction.tone ? generatedAction.tone : "safe");
		done.restFill = generatedAction.fill;
		done.restBorder = generatedAction.border;
		done.textColor = generatedAction.text;
		model.actions.push_back(done);
	}

	model.cutCornerPx = CalypsoF07CraftsGen::kCutCornerPx;
	model.panelFillTop = CalypsoF07CraftsGen::kPanelFillTop;
	model.panelFillBottom = CalypsoF07CraftsGen::kPanelFillBottom;
	model.frameColor = CalypsoF07CraftsGen::kFrame;
	model.selectedTabColor = CalypsoF07CraftsGen::kSelectedTab;
	model.dividerColor = CalypsoF07CraftsGen::kDivider;
	model.footerDotColor = CalypsoF07CraftsGen::kFooterDot;
	model.textColor = CalypsoF07CraftsGen::kText;
	model.mutedTextColor = CalypsoF07CraftsGen::kMutedText;
	model.selectionColor = CalypsoF07CraftsGen::kSelection;
	model.scrollTrackColor = CalypsoF07CraftsGen::kScrollTrack;
	model.scrollThumbColor = CalypsoF07CraftsGen::kScrollThumb;
	const CalypsoHdPresentationMetrics& metrics =
		CalypsoHdUiOverlay::instance().frozenMetrics();
	model.uiScale = uiScale;
	// Standard density (no presentation scale in tabbed contracts).
	model.visualScale = 1.0;
	model.projectionScaleX = uiScale * metrics.scaleX;
	model.projectionScaleY = uiScale * metrics.scaleY;
	model.titleDesignHeight = generated->title.h;
	model.motionDurationMs = CalypsoF07CraftsGen::kMotionDurationMs;
	model.motionScaleFrom = CalypsoF07CraftsGen::kMotionScaleFrom;

	calypsoCollectTabbedManagement(builder, model, _motion);
}

void CalypsoF07CraftsUi::applyGeneratedLayout(CraftsState& state, bool wide)
{
	const auto* generated = CalypsoF07CraftsGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return;
	f04ApplyRect(state._window, generated->window.x, generated->window.y,
		generated->window.w, generated->window.h);
	f04ApplyRect(state._txtTitle, generated->title.x, generated->title.y,
		generated->title.w, generated->title.h);
	f04ApplyRect(state._lstCrafts, generated->collectionViewport.x,
		generated->collectionViewport.y, generated->collectionViewport.w,
		generated->collectionViewport.h);
	{
		const CalypsoLogicalRect touch = f04TouchRect(f07CraftsActionRect(wide, "cancel"));
		f04ApplyRect(state._btnOk, touch);
	}
	// Column headers and the base line have no painted equivalent in the
	// shared shell; the composed rows carry every native cell verbatim.
	f04ParkOffscreen(state._txtBase);
	f04ParkOffscreen(state._txtName);
	f04ParkOffscreen(state._txtStatus);
	f04ParkOffscreen(state._txtWeapon);
	f04ParkOffscreen(state._txtCrew);
	f04ParkOffscreen(state._txtHwp);
	// The generated rowHeight is the full native stride (row box + font
	// spacing). Derive the minimum row box from the live small-font metrics
	// so the projected slots agree with native hit-testing at every scale.
	if (state._lstCrafts && state._game && state._game->getMod())
	{
		const Font* font = state._game->getMod()->getFont("FONT_SMALL");
		const int fontH = font ? font->getHeight() : 0;
		const int spacing = font ? font->getSpacing() : 0;
		state._lstCrafts->setMinimumRowHeight(
			std::max(fontH, generated->rowHeight - spacing));
	}
}

void CalypsoF07CraftsUi::configure(CraftsState& state)
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
	// The footer close action reads Base Overview per the approved board:
	// relabel the live Done button (handler, gates, and keyboard paths
	// untouched; gate-off returns before this line, so vanilla keeps OK).
	state._btnOk->setText(state.tr("STR_CAL_F07_CLOSE_BASE_OVERVIEW"));
	applyGeneratedLayout(state, state._hdWideLayout);
	const auto* generated = CalypsoF07CraftsGen::layoutForDesign(
		state._hdWideLayout ? 1280 : 740, state._hdWideLayout ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F07 roster generated layout is missing");
	state.enableUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Tabbed list seam AFTER scaling: shares the inset track and native
	// scroll count/reveal with the painted slots.
	calypsoTabbedSeamList(state._lstCrafts, state._window, *generated);
	auto* adapter = new CalypsoF07CraftsUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter);
}

bool CalypsoF07CraftsUi::resize(CraftsState& state)
{
	if (!state._hdLayout) return false;
	// Capture the selection by stable craft name BEFORE re-layout (pitfall
	// 3), then restore-or-clamp it into the relaid-out rows (never row
	// numbers).
	const CraftRows before = craftRows(state);
	const CalypsoF04ListSelection captured = f04CaptureListSelection(
		state._lstCrafts, before.ids);
	const bool wide = f07HdWideLayout();
	state._hdWideLayout = wide;
	applyGeneratedLayout(state, wide);
	const auto* generated = CalypsoF07CraftsGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return false;
	state.recaptureUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Re-seam after recapture; same values preserve drag capture, real layout
	// changes reset it.
	calypsoTabbedSeamList(state._lstCrafts, state._window, *generated);
	const CraftRows after = craftRows(state);
	f04RestoreListSelection(state._lstCrafts, captured, after.ids,
		(size_t)generated->visibleRows);
	return true;
}

} // namespace Calypso
} // namespace OpenXcom
#endif
