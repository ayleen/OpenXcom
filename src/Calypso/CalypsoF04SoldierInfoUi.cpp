#ifdef __EMSCRIPTEN__
#include "CalypsoF04SoldierInfoUi.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include "../Engine/Font.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Engine/Surface.h"
#include "../Interface/Bar.h"
#include "../Engine/InteractiveSurface.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextEdit.h"
#include "../Interface/TextList.h"
#include "../Interface/Window.h"
#include "../Basescape/SoldierInfoState.h"
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

namespace
{

// One inspector row source: a native label/value widget pair. Values are
// verbatim native display strings (armor/bonus effects already applied by
// prepareStatsWithBonuses); the adapter never recomputes stat math.
struct F04InspectorPair
{
	const Text* label;
	const Text* value;
};

void f04PushInspectorRow(const Text* single, std::vector<std::string>& out)
{
	if (!single || !single->getVisible()) return;
	const std::string text = CommandCenter::calypsoHdNormalizeTtfDisplayText(
		single->getText());
	if (!text.empty()) out.push_back(text);
}

void f04PushInspectorPair(const Text* label, const Text* value, std::vector<std::string>& out)
{
	if (!value || !value->getVisible()) return;
	const std::string labelText = label && label->getVisible()
		? CommandCenter::calypsoHdNormalizeTtfDisplayText(label->getText())
		: std::string();
	const std::string valueText = CommandCenter::calypsoHdNormalizeTtfDisplayText(
		value->getText());
	if (valueText.empty()) return;
	out.push_back(f04ComposeLabelValue(labelText, valueText));
}

} // namespace

// Live native profile content in stable display order: identity/status block,
// then the twelve attribute pairs in native layout order. Conditional rows
// are absent whenever native hides them (never placeholder values).
std::vector<std::string> CalypsoF04SoldierInfoUi::inspectorRows(const SoldierInfoState& state)
{
	std::vector<std::string> out{};
	f04PushInspectorRow(state._txtRank, out);
	f04PushInspectorRow(state._txtMissions, out);
	f04PushInspectorRow(state._txtKills, out);
	f04PushInspectorRow(state._txtCraft, out);
	f04PushInspectorRow(state._txtRecovery, out);
	f04PushInspectorRow(state._txtStuns, out);
	f04PushInspectorRow(state._txtPsionic, out);
	f04PushInspectorRow(state._txtDead, out);
	const F04InspectorPair stats[] = {
		{state._txtTimeUnits, state._numTimeUnits},
		{state._txtStamina, state._numStamina},
		{state._txtHealth, state._numHealth},
		{state._txtBravery, state._numBravery},
		{state._txtReactions, state._numReactions},
		{state._txtFiring, state._numFiring},
		{state._txtThrowing, state._numThrowing},
		{state._txtMelee, state._numMelee},
		{state._txtStrength, state._numStrength},
		{state._txtMana, state._numMana},
		{state._txtPsiStrength, state._numPsiStrength},
		{state._txtPsiSkill, state._numPsiSkill},
	};
	for (const auto& pair : stats) f04PushInspectorPair(pair.label, pair.value, out);
	return out;
}

CalypsoF04SoldierInfoUi::~CalypsoF04SoldierInfoUi()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

const void* CalypsoF04SoldierInfoUi::topState() const
{
	return _state;
}

void CalypsoF04SoldierInfoUi::collectLogicalSuppression(
	CalypsoHdLogicalSuppression& suppression) const
{
	// Whole-state suppression (suppressLogicalState, default true) owns the
	// top-state logical UI; list every native surface explicitly so widget
	// suppression also holds during warmup and under a covered child.
	if (!_state) return;
	suppression.add(_state->_bg);
	suppression.add(_state->_rank);
	suppression.add(_state->_flag);
	suppression.add(_state->_btnOk);
	suppression.add(_state->_btnPrev);
	suppression.add(_state->_btnNext);
	suppression.add(_state->_btnArmor);
	suppression.add(_state->_btnSack);
	suppression.add(_state->_btnDiary);
	suppression.add(_state->_btnBonuses);
	suppression.add(_state->_btnTransformations);
	suppression.add(_state->_txtRank);
	suppression.add(_state->_txtMissions);
	suppression.add(_state->_txtKills);
	suppression.add(_state->_txtCraft);
	suppression.add(_state->_txtRecovery);
	suppression.add(_state->_txtPsionic);
	suppression.add(_state->_txtDead);
	suppression.add(_state->_txtStuns);
	suppression.add(_state->_edtSoldier);
	suppression.add(_state->_txtTimeUnits);
	suppression.add(_state->_txtStamina);
	suppression.add(_state->_txtHealth);
	suppression.add(_state->_txtBravery);
	suppression.add(_state->_txtReactions);
	suppression.add(_state->_txtFiring);
	suppression.add(_state->_txtThrowing);
	suppression.add(_state->_txtMelee);
	suppression.add(_state->_txtStrength);
	suppression.add(_state->_txtPsiStrength);
	suppression.add(_state->_txtPsiSkill);
	suppression.add(_state->_txtMana);
	suppression.add(_state->_numTimeUnits);
	suppression.add(_state->_numStamina);
	suppression.add(_state->_numHealth);
	suppression.add(_state->_numBravery);
	suppression.add(_state->_numReactions);
	suppression.add(_state->_numFiring);
	suppression.add(_state->_numThrowing);
	suppression.add(_state->_numMelee);
	suppression.add(_state->_numStrength);
	suppression.add(_state->_numPsiStrength);
	suppression.add(_state->_numPsiSkill);
	suppression.add(_state->_numMana);
	suppression.add(_state->_barTimeUnits);
	suppression.add(_state->_barStamina);
	suppression.add(_state->_barHealth);
	suppression.add(_state->_barBravery);
	suppression.add(_state->_barReactions);
	suppression.add(_state->_barFiring);
	suppression.add(_state->_barThrowing);
	suppression.add(_state->_barMelee);
	suppression.add(_state->_barStrength);
	suppression.add(_state->_barPsiStrength);
	suppression.add(_state->_barPsiSkill);
	suppression.add(_state->_barMana);
	suppression.add(_state->_hdInspectorList);
}

void CalypsoF04SoldierInfoUi::collect(CalypsoHdFrameBuilder& builder) const
{
	// The profile is a registered HD route: missing prerequisites or a
	// missing generated layout fails the route instead of silently skipping
	// to an empty collection (the overlay would fail it closed anyway).
	if (!_state || !_state->_hdLayout || !_state->_game)
		CalypsoHdUiOverlay::instance().failHdRoute("F04 profile prerequisites are unavailable");
	const bool wide = _state->_hdWideLayout;
	const auto* generated = CalypsoF04SoldiersGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F04 profile generated layout is missing");

	const CalypsoLogicalRect window = _state->_bg
		? CalypsoLogicalRect{_state->_bg->getX(), _state->_bg->getY(),
			_state->_bg->getWidth(), _state->_bg->getHeight()}
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
	model.windowWidget = _state->_bg;
	model.titleWidget = nullptr; // no native title Text exists; the painted
	// title reads the live name editor (a TextEdit, suppressed via its own
	// entry), so no widget claim applies here.
	model.listWidget = _state->_hdInspectorList;
	model.titleText = CommandCenter::calypsoHdNormalizeTtfDisplayText(
		_state->_edtSoldier ? _state->_edtSoldier->getText() : std::string());
	model.protocolText = CalypsoF04SoldiersGen::kProtocol;
	model.rowHeight = generated->rowHeight;
	model.visibleRows = generated->visibleRows;
	model.scrollBarWidth = generated->scrollBarWidth;
	model.minThumbHeight = std::max(1, (int)std::llround(generated->minThumbHeight * uiScale));
	// Shared native input geometry in the same logical space, read exactly
	// once per frame: the painter uses these rects instead of its own formula.
	if (_state->_hdInspectorList && _state->_hdInspectorList->isCalypsoHdSelectionList())
	{
		const SDL_Rect track = _state->_hdInspectorList->getCalypsoHdTrackRect();
		const SDL_Rect thumb = _state->_hdInspectorList->getCalypsoHdThumbRect();
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

	// Inspector rows are rebuilt from live native texts outside collection
	// (refreshInspectorRows on init/configure); the native list owns scroll
	// position while the painted window follows it.
	if (_state->_hdInspectorList)
	{
		const auto& matrix = _state->_hdInspectorList->getCellTextsSnapshot();
		for (const auto& cells : matrix)
		{
			if (cells.empty() || !cells[0]) continue;
			CalypsoSelectionListRow entry{};
			entry.text = CommandCenter::calypsoHdNormalizeTtfDisplayText(
				cells[0]->getText());
			entry.enabled = true;
			model.rows.push_back(entry);
		}
		model.scrollOffset = _state->_hdInspectorList->getScroll();
	}
	model.hasSelection = false;
	model.selectedRow = 0;

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

void CalypsoF04SoldierInfoUi::refreshInspectorRows(SoldierInfoState& state)
{
	TextList* const list = state._hdInspectorList;
	if (!list) return;
	list->clearList();
	for (const auto& text : inspectorRows(state))
		list->addRow(1, text.c_str());
	list->scrollTo(0);
}

void CalypsoF04SoldierInfoUi::applyGeneratedLayout(SoldierInfoState& state, bool wide)
{
	const auto* generated = CalypsoF04SoldiersGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return;
	// SoldierInfoState owns no native window: reuse the generated shell
	// window as the projection origin while native chrome is parked.
	f04ApplyRect(state._bg, generated->window.x, generated->window.y,
		generated->window.w, generated->window.h);
	if (state._hdInspectorList)
	{
		f04ApplyRect(state._hdInspectorList, generated->list.x, generated->list.y,
			generated->list.w, generated->list.h);
		// Column geometry follows the HD design list size BEFORE any row is
		// added (statistics rebase precedent); later layouts keep rows.
		if (state._hdInspectorList->getRowsDoNotUse() == 0)
		{
			state._hdInspectorList->rebaseNativeSize(generated->list.w, generated->list.h);
			state._hdInspectorList->setColumns(1, generated->list.w);
		}
	}
	{
		const CalypsoLogicalRect touch = f04TouchRect(f04GeneratedButtonRect(wide, "cancel"));
		f04ApplyRect(state._btnOk, touch);
	}
	// Every remaining native control stays a live input/behavior owner
	// (handlers, gates, and keyboard paths untouched) without painted
	// controls or pointer hit areas.
	f04ParkOffscreen(state._rank);
	f04ParkOffscreen(state._flag);
	f04ParkOffscreen(state._btnPrev);
	f04ParkOffscreen(state._btnNext);
	f04ParkOffscreen(state._btnArmor);
	f04ParkOffscreen(state._btnSack);
	f04ParkOffscreen(state._btnDiary);
	f04ParkOffscreen(state._btnBonuses);
	f04ParkOffscreen(state._btnTransformations);
	f04ParkOffscreen(state._txtRank);
	f04ParkOffscreen(state._txtMissions);
	f04ParkOffscreen(state._txtKills);
	f04ParkOffscreen(state._txtCraft);
	f04ParkOffscreen(state._txtRecovery);
	f04ParkOffscreen(state._txtPsionic);
	f04ParkOffscreen(state._txtDead);
	f04ParkOffscreen(state._txtStuns);
	f04ParkOffscreen(state._edtSoldier);
	f04ParkOffscreen(state._txtTimeUnits);
	f04ParkOffscreen(state._txtStamina);
	f04ParkOffscreen(state._txtHealth);
	f04ParkOffscreen(state._txtBravery);
	f04ParkOffscreen(state._txtReactions);
	f04ParkOffscreen(state._txtFiring);
	f04ParkOffscreen(state._txtThrowing);
	f04ParkOffscreen(state._txtMelee);
	f04ParkOffscreen(state._txtStrength);
	f04ParkOffscreen(state._txtPsiStrength);
	f04ParkOffscreen(state._txtPsiSkill);
	f04ParkOffscreen(state._txtMana);
	f04ParkOffscreen(state._numTimeUnits);
	f04ParkOffscreen(state._numStamina);
	f04ParkOffscreen(state._numHealth);
	f04ParkOffscreen(state._numBravery);
	f04ParkOffscreen(state._numReactions);
	f04ParkOffscreen(state._numFiring);
	f04ParkOffscreen(state._numThrowing);
	f04ParkOffscreen(state._numMelee);
	f04ParkOffscreen(state._numStrength);
	f04ParkOffscreen(state._numPsiStrength);
	f04ParkOffscreen(state._numPsiSkill);
	f04ParkOffscreen(state._numMana);
	f04ParkOffscreen(state._barTimeUnits);
	f04ParkOffscreen(state._barStamina);
	f04ParkOffscreen(state._barHealth);
	f04ParkOffscreen(state._barBravery);
	f04ParkOffscreen(state._barReactions);
	f04ParkOffscreen(state._barFiring);
	f04ParkOffscreen(state._barThrowing);
	f04ParkOffscreen(state._barMelee);
	f04ParkOffscreen(state._barStrength);
	f04ParkOffscreen(state._barPsiStrength);
	f04ParkOffscreen(state._barPsiSkill);
	f04ParkOffscreen(state._barMana);
	if (state._hdInspectorList && state._game && state._game->getMod())
	{
		const Font* font = state._game->getMod()->getFont("FONT_SMALL");
		const int fontH = font ? font->getHeight() : 0;
		const int spacing = font ? font->getSpacing() : 0;
		state._hdInspectorList->setMinimumRowHeight(
			std::max(fontH, generated->rowHeight - spacing));
	}
}

void CalypsoF04SoldierInfoUi::configure(SoldierInfoState& state)
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
	// Adapter-owned inspector list: carries the painted stat rows and owns
	// scroll position; it binds no click handler and commits nothing.
	if (!state._hdInspectorList)
	{
		state._hdInspectorList = new TextList(8, 8, -4096, -4096);
		state._hdInspectorList->setSelectable(false);
		state._hdInspectorList->setScrolling(true, 0);
		state.add(state._hdInspectorList, "list", "soldierInfo");
	}
	applyGeneratedLayout(state, state._hdWideLayout);
	refreshInspectorRows(state);
	const auto* generated = CalypsoF04SoldiersGen::layoutForDesign(
		state._hdWideLayout ? 1280 : 740, state._hdWideLayout ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F04 profile generated layout is missing");
	state.enableUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Native HD selection-list seam AFTER scaling: shares the inset track and
	// native scroll count/reveal with the painted slots.
	f04SeamSelectionList(state._hdInspectorList, state._bg, *generated);
	auto* adapter = new CalypsoF04SoldierInfoUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter);
}

bool CalypsoF04SoldierInfoUi::resize(SoldierInfoState& state)
{
	if (!state._hdLayout) return false;
	const bool wide = f04HdWideLayout();
	state._hdWideLayout = wide;
	applyGeneratedLayout(state, wide);
	const auto* generated = CalypsoF04SoldiersGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return false;
	state.recaptureUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Re-seam after recapture; same values preserve drag capture, real layout
	// changes reset it. Inspector rows are stable per state instance (values
	// refresh through init()); selection never applies to the inspector.
	f04SeamSelectionList(state._hdInspectorList, state._bg, *generated);
	return true;
}

} // namespace Calypso
} // namespace OpenXcom
#endif
