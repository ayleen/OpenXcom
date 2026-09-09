#ifdef __EMSCRIPTEN__
#include "CalypsoF05SoldierAvatarUi.h"
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
#include "../Basescape/SoldierAvatarState.h"
#include "../Mod/Mod.h"
#include "../Savegame/SoldierAvatar.h"
#include "CalypsoF04SelectionListShell.h"
#include "CalypsoF05SelectionListShell.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoUiMetrics.h"
#include "CommandCenter/CommandCenterRenderer.h"
#include "Generated/CalypsoF05SoldierAvatar.generated.h"

namespace OpenXcom
{
namespace Calypso
{

struct CalypsoF05SoldierAvatarUi::AvatarRows
{
	std::vector<std::string> ids;
	std::vector<CalypsoSelectionListRow> rows;
};

// Live native rows only, in native order: the localized avatar names exactly
// as the native cells show them. Stable ids are the avatar name keys
// (STR_AVATAR_NAME_##, never translated text); gender/look/variant
// combinations stay mod-defined (maxLookVariant, Alt/Ctrl filters) and
// engine-owned.
CalypsoF05SoldierAvatarUi::AvatarRows CalypsoF05SoldierAvatarUi::avatarRows(const SoldierAvatarState& state)
{
	AvatarRows out{};
	const TextList* list = state._lstAvatar;
	if (!list) return out;
	const auto& matrix = list->getCellTextsSnapshot();
	out.ids.reserve(matrix.size());
	out.rows.reserve(matrix.size());
	for (size_t row = 0; row < matrix.size(); ++row)
	{
		const auto& cells = matrix[row];
		if (cells.empty() || !cells[0]) continue;
		std::vector<std::string> texts;
		texts.reserve(cells.size());
		for (const Text* cell : cells)
			texts.push_back(cell ? cell->getText() : std::string());
		std::string id = texts[0];
		if (row < state._avatars.size())
			id = state._avatars[row].getAvatarName();
		out.ids.push_back(id);
		CalypsoSelectionListRow entry{};
		entry.text = CommandCenter::calypsoHdNormalizeTtfDisplayText(
			f04ComposeRowText(texts));
		entry.enabled = true;
		out.rows.push_back(entry);
	}
	return out;
}

CalypsoF05SoldierAvatarUi::~CalypsoF05SoldierAvatarUi()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

const void* CalypsoF05SoldierAvatarUi::topState() const
{
	return _state;
}

void CalypsoF05SoldierAvatarUi::collectLogicalSuppression(
	CalypsoHdLogicalSuppression& suppression) const
{
	// Whole-state suppression is OFF for the avatar route (the live preview
	// surface stays visible data). Suppress every chrome widget explicitly so
	// suppression also holds during warmup and under a covered child. The
	// preview surface is deliberately absent: its soldier pixels are route
	// data, painted by the unchanged native initPreview/SPK path.
	if (!_state) return;
	suppression.add(_state->_window);
	suppression.add(_state->_btnCancel);
	suppression.add(_state->_btnOk);
	suppression.add(_state->_txtTitle);
	suppression.add(_state->_txtType);
	suppression.add(_state->_lstAvatar);
}

void CalypsoF05SoldierAvatarUi::collect(CalypsoHdFrameBuilder& builder) const
{
	// The avatar route is a registered HD route: missing prerequisites or a
	// missing generated layout fails the route instead of silently skipping
	// to an empty collection (the overlay would fail it closed anyway).
	// Selection is a reversible preview through the unchanged native handler
	// (gender + look + variant together); Apply is the only commit, Cancel
	// restores the original triple atomically through the unchanged handler.
	if (!_state || !_state->_hdLayout || !_state->_game)
		CalypsoHdUiOverlay::instance().failHdRoute("F05 avatar prerequisites are unavailable");
	const bool wide = _state->_hdWideLayout;
	const auto* generated = f05AvatarLayout(wide);

	const CalypsoLogicalRect window = _state->_window
		? CalypsoLogicalRect{_state->_window->getX(), _state->_window->getY(),
			_state->_window->getWidth(), _state->_window->getHeight()}
		: CalypsoLogicalRect{};
	const double uiScale = generated->window.w > 0
		? (double)window.w / generated->window.w : 1.0;
	auto project = [&](const auto& rect) -> CalypsoLogicalRect
	{
		return f05ProjectRect(window, *generated, rect);
	};

	CalypsoSelectionListModel model{};
	model.familyId = CalypsoF05SoldierAvatarGen::kFamilyId;
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
	model.listWidget = _state->_lstAvatar;
	model.titleText = CommandCenter::calypsoHdNormalizeTtfDisplayText(
		_state->_txtTitle ? _state->_txtTitle->getText() : std::string());
	model.protocolText = CalypsoF05SoldierAvatarGen::kProtocol;
	model.rowHeight = generated->rowHeight;
	model.visibleRows = generated->visibleRows;
	model.scrollBarWidth = generated->scrollBarWidth;
	model.minThumbHeight = std::max(1, (int)std::llround(generated->minThumbHeight * uiScale));
	// Shared native input geometry in the same logical space, read exactly
	// once per frame: the painter uses these rects instead of its own formula.
	if (_state->_lstAvatar && _state->_lstAvatar->isCalypsoHdSelectionList())
	{
		const SDL_Rect track = _state->_lstAvatar->getCalypsoHdTrackRect();
		const SDL_Rect thumb = _state->_lstAvatar->getCalypsoHdThumbRect();
		model.hasNativeScrollGeometry = track.w > 0 && track.h > 0;
		model.nativeTrack = {track.x, track.y, track.w, track.h};
		model.nativeThumb = {thumb.x, thumb.y, thumb.w, thumb.h};
		model.nativeThumbVisible = thumb.w > 0 && thumb.h > 0;
	}

	const int slotCount = wide
		? CalypsoF05SoldierAvatarGen::kRowSlotWideCount
		: CalypsoF05SoldierAvatarGen::kRowSlotCompactCount;
	const auto* slots = wide
		? CalypsoF05SoldierAvatarGen::kRowSlotsWide
		: CalypsoF05SoldierAvatarGen::kRowSlotsCompact;
	for (int i = 0; i < slotCount; ++i)
		model.rowSlots.push_back(project(slots[i]));

	// Native population/order: every avatar in native order; the native list
	// stays the behavior/input owner (preview on click). The painted
	// selection mirrors the previewed row.
	const AvatarRows bound = avatarRows(*_state);
	model.rows = bound.rows;
	const std::size_t total = model.rows.size();
	if (_state->_lstAvatar)
	{
		model.scrollOffset = _state->_lstAvatar->getScroll();
		const unsigned int selected = _state->_lstAvatar->getSelectedRow();
		model.hasSelection = selected < total;
		model.selectedRow = model.hasSelection ? selected : 0;
	}

	model.cutCornerPx = CalypsoF05SoldierAvatarGen::kCutCornerPx;
	model.protocolTextInsetPx = CalypsoF05SoldierAvatarGen::kProtocolTextInsetPx;
	model.panelFillTop = CalypsoF05SoldierAvatarGen::kPanelFillTop;
	model.panelFillBottom = CalypsoF05SoldierAvatarGen::kPanelFillBottom;
	model.frameColor = CalypsoF05SoldierAvatarGen::kFrame;
	model.protocolColor = CalypsoF05SoldierAvatarGen::kProtocolText;
	model.dividerColor = CalypsoF05SoldierAvatarGen::kDivider;
	model.footerDotColor = CalypsoF05SoldierAvatarGen::kFooterDot;
	model.textColor = CalypsoF05SoldierAvatarGen::kText;
	model.mutedTextColor = CalypsoF05SoldierAvatarGen::kMutedText;
	model.selectionColor = CalypsoF05SoldierAvatarGen::kSelection;
	model.scrollTrackColor = CalypsoF05SoldierAvatarGen::kScrollTrack;
	model.scrollThumbColor = CalypsoF05SoldierAvatarGen::kScrollThumb;
	const CalypsoHdPresentationMetrics& metrics =
		CalypsoHdUiOverlay::instance().frozenMetrics();
	model.uiScale = uiScale;
	model.visualScale = CalypsoF05SoldierAvatarGen::kPresentationScale;
	model.projectionScaleX = uiScale * metrics.scaleX;
	model.projectionScaleY = uiScale * metrics.scaleY;
	model.titleDesignHeight = generated->title.h;
	model.motionDurationMs = CalypsoF05SoldierAvatarGen::kMotionDurationMs;
	model.motionScaleFrom = CalypsoF05SoldierAvatarGen::kMotionScaleFrom;

	const auto& generatedButton = CalypsoF05SoldierAvatarGen::kButtons[0];
	TextButton* widget = _state->_btnOk;
	model.cancel.widget = widget;
	model.cancel.peer = nullptr;
	model.cancel.text = CommandCenter::calypsoHdNormalizeTtfDisplayText(
		widget ? widget->getText() : std::string());
	model.cancel.rect = project(f05AvatarButtonRect(wide, generatedButton.id));
	model.cancel.tone = std::string(generatedButton.tone) == "danger"
		? CalypsoActionTone::Destructive : CalypsoActionTone::Safe;
	model.cancel.restFill = generatedButton.fill;
	model.cancel.restBorder = generatedButton.border;
	model.cancel.textColor = generatedButton.text;

	calypsoCollectSelectionList(builder, model, _motion);
}

void CalypsoF05SoldierAvatarUi::applyGeneratedLayout(SoldierAvatarState& state, bool wide)
{
	const auto* generated = CalypsoF05SoldierAvatarGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return;
	f04ApplyRect(state._window, generated->window.x, generated->window.y,
		generated->window.w, generated->window.h);
	f04ApplyRect(state._txtTitle, generated->title.x, generated->title.y,
		generated->title.w, generated->title.h);
	f04ApplyRect(state._lstAvatar, generated->list.x, generated->list.y,
		generated->list.w, generated->list.h);
	{
		const CalypsoLogicalRect touch = f04TouchRect(f05AvatarButtonRect(wide, "apply"));
		f04ApplyRect(state._btnOk, touch);
	}
	// The type header has no painted equivalent in the shared shell; the
	// composed rows carry the native avatar names verbatim.
	f04ParkOffscreen(state._txtType);
	// Cancel stays a live native input owner (atomic triple-restore handler
	// and its keyCancel path untouched) without painted controls or pointer
	// hit areas; the single shared action slot hosts Apply, the only commit.
	f04ParkOffscreen(state._btnCancel);
	// The preview surface keeps its native fullscreen-backdrop geometry under
	// uiScaling: no invented geometry, live SPK/armor-layer data only.
	// The generated rowHeight is the full native stride (row box + font
	// spacing). Derive the minimum row box from the live small-font metrics
	// so the projected slots agree with native hit-testing at every scale.
	if (state._lstAvatar && state._game && state._game->getMod())
	{
		const Font* font = state._game->getMod()->getFont("FONT_SMALL");
		const int fontH = font ? font->getHeight() : 0;
		const int spacing = font ? font->getSpacing() : 0;
		state._lstAvatar->setMinimumRowHeight(
			std::max(fontH, generated->rowHeight - spacing));
	}
}

void CalypsoF05SoldierAvatarUi::configure(SoldierAvatarState& state)
{
	// Gate-off (family id absent) returns to the legacy path untouched.
	if (!state._game || !state._game->getMod()
		|| !state._game->getMod()->isHdUiFamilyEnabled("F05"))
	{
		state._hdLayout = false;
		return;
	}
	state._hdLayout = true;
	state._hdWideLayout = f05HdWideLayout();
	applyGeneratedLayout(state, state._hdWideLayout);
	const auto* generated = CalypsoF05SoldierAvatarGen::layoutForDesign(
		state._hdWideLayout ? 1280 : 740, state._hdWideLayout ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F05 avatar generated layout is missing");
	state.enableUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Native HD selection-list seam AFTER scaling: shares the inset track and
	// native scroll count/reveal with the painted slots.
	f05SeamSelectionList(state._lstAvatar, state._window, *generated);
	auto* adapter = new CalypsoF05SoldierAvatarUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter);
}

bool CalypsoF05SoldierAvatarUi::resize(SoldierAvatarState& state)
{
	if (!state._hdLayout) return false;
	// Capture the selection by stable id BEFORE re-layout (pitfall 3), then
	// restore-or-clamp it into the relaid-out rows (never row numbers).
	const AvatarRows before = avatarRows(state);
	const CalypsoF04ListSelection captured = f04CaptureListSelection(
		state._lstAvatar, before.ids);
	const bool wide = f05HdWideLayout();
	state._hdWideLayout = wide;
	applyGeneratedLayout(state, wide);
	const auto* generated = CalypsoF05SoldierAvatarGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return false;
	state.recaptureUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Re-seam after recapture; same values preserve drag capture, real layout
	// changes reset it.
	f05SeamSelectionList(state._lstAvatar, state._window, *generated);
	const AvatarRows after = avatarRows(state);
	f04RestoreListSelection(state._lstAvatar, captured, after.ids,
		(size_t)generated->visibleRows);
	return true;
}

} // namespace Calypso
} // namespace OpenXcom
#endif
