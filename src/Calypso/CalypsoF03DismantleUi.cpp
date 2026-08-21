#ifdef __EMSCRIPTEN__
#include "CalypsoF03DismantleUi.h"
#include <algorithm>
#include <cmath>
#include <string>
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Engine/Surface.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Basescape/DismantleFacilityState.h"
#include "../Savegame/BaseFacility.h"
#include "../Savegame/SavedGame.h"
#include "../Mod/Mod.h"
#include "Generated/CalypsoF03Dismantle.generated.h"
#include "CalypsoHdHarnessHostState.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoUiFamilies.h"
#include "CalypsoUiMetrics.h"
#include "CalypsoViewportRuntime.h"

namespace OpenXcom
{
namespace Calypso
{

namespace
{

CalypsoLayoutClass currentLayoutClass()
{
	CalypsoBaseSafeRect safe{0, 0, Options::baseXResolution, Options::baseYResolution};
	(void)calypsoProjectedSafeRectForLayout(
		Options::baseXResolution, Options::baseYResolution, safe);
	return calypsoHarnessEffectiveLayout(calypsoHarnessSession(), safe);
}

CalypsoLogicalRect shiftedRect(
	const CalypsoF03DismantleGen::CalypsoF03DismantleGenRect& rect, int dx)
{
	return {rect.x + dx, rect.y, rect.w, rect.h};
}

void applyRect(Surface* surface, const CalypsoLogicalRect& rect)
{
	if (!surface) return;
	surface->setX(rect.x);
	surface->setY(rect.y);
	surface->setWidth(rect.w);
	surface->setHeight(rect.h);
}

int presentationShiftX(
	const CalypsoF03DismantleGen::CalypsoF03DismantleGenLayout& layout, bool wide)
{
	return calypsoHarnessSession().sideBySide && wide ? 40 - layout.window.x : 0;
}

CalypsoLogicalRect generatedButtonRect(bool wide, const char* id, int dx)
{
	const auto& buttons = CalypsoF03DismantleGen::kButtonRects[wide ? 0 : 1];
	for (int i = 0; i < CalypsoF03DismantleGen::kButtonCount; ++i)
	{
		if (std::string(buttons[i].id) == id) return shiftedRect(buttons[i].rect, dx);
	}
	return {};
}

CalypsoLogicalRect touchRect(CalypsoLogicalRect visual)
{
	const int width = std::max(visual.w, CALYPSO_MIN_TOUCH_TARGET);
	const int height = std::max(visual.h, CALYPSO_MIN_TOUCH_TARGET);
	visual.x -= width - visual.w;
	visual.y -= (height - visual.h) / 2;
	visual.w = width;
	visual.h = height;
	return visual;
}

} // namespace

CalypsoF03DismantleUi::~CalypsoF03DismantleUi()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

const void* CalypsoF03DismantleUi::topState() const
{
	return _state;
}

void CalypsoF03DismantleUi::collect(CalypsoHdFrameBuilder& builder) const
{
	if (!_state || !_state->_hdLayout || !_state->_game) return;
	const bool wide = _state->_hdWideLayout;
	const auto* generated = CalypsoF03DismantleGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return;

	const CalypsoLogicalRect window = _state->_window
		? CalypsoLogicalRect{_state->_window->getX(), _state->_window->getY(),
			_state->_window->getWidth(), _state->_window->getHeight()}
		: CalypsoLogicalRect{};
	const double uiScale = generated->window.w > 0
		? (double)window.w / generated->window.w : 1.0;
	auto project = [&](const auto& rect) -> CalypsoLogicalRect
	{
		return {
			window.x + (int)std::llround((rect.x - generated->window.x) * uiScale),
			window.y + (int)std::llround((rect.y - generated->window.y) * uiScale),
			std::max(1, (int)std::llround(rect.w * uiScale)),
			std::max(1, (int)std::llround(rect.h * uiScale))};
	};

	CalypsoSmallConfirmationModel model{};
	model.familyId = CalypsoF03DismantleGen::kFamilyId;
	model.instance = _state;
	model.mod = _state->_game->getMod();
	model.wide = wide;
	model.designWidth = generated->designWidth;
	model.designHeight = generated->designHeight;
	model.window = window;
	model.status = project(generated->status);
	model.warning = project(generated->warning);
	model.title = project(generated->title);
	model.message = project(generated->message);
	model.footer = project(generated->footer);
	model.windowWidget = _state->_window;
	model.titleWidget = _state->_txtTitle;
	model.titleText = _state->_txtTitle ? _state->_txtTitle->getText() : std::string();
	model.messageWidget = _state->_txtFacility;
	{
		std::string facility = _state->_hdFacilityText;
		std::string refund = _state->_hdRefundVisible ? _state->_hdRefundText : "";
		std::string combined = facility;
		if (!refund.empty()) {
			if (!combined.empty()) combined += "\n";
			combined += refund;
		}
		model.messageText = combined;
	}
	model.protocolText = std::string(_state->tr("STR_CAL_F03_PROTOCOL_DISMANTLE"));
	model.warningGlyph = "!";
	model.cutCornerPx = CalypsoF03DismantleGen::kCutCornerPx;
	model.protocolTextInsetPx = CalypsoF03DismantleGen::kProtocolTextInsetPx;
	model.panelFillTop = CalypsoF03DismantleGen::kPanelFillTop;
	model.panelFillBottom = CalypsoF03DismantleGen::kPanelFillBottom;
	model.frameColor = CalypsoF03DismantleGen::kFrame;
	model.protocolColor = CalypsoF03DismantleGen::kProtocolText;
	model.dividerColor = CalypsoF03DismantleGen::kDivider;
	model.footerDotColor = CalypsoF03DismantleGen::kFooterDot;
	model.warningColor = CalypsoF03DismantleGen::kWarning;
	const CalypsoHdPresentationMetrics& metrics =
		CalypsoHdUiOverlay::instance().frozenMetrics();
	model.uiScale = uiScale;
	model.visualScale = CalypsoF03DismantleGen::kPresentationScale;
	model.projectionScaleX = uiScale * metrics.scaleX;
	model.projectionScaleY = uiScale * metrics.scaleY;
	model.messageDesignWidth = generated->message.w;
	model.titleDesignHeight = generated->title.h;
	model.motionDurationMs = CalypsoF03DismantleGen::kMotionDurationMs;
	model.motionScaleFrom = CalypsoF03DismantleGen::kMotionScaleFrom;

	const auto& buttonRects = CalypsoF03DismantleGen::kButtonRects[wide ? 0 : 1];
	for (int i = 0; i < CalypsoF03DismantleGen::kButtonCount; ++i)
	{
		const auto& generatedButton = CalypsoF03DismantleGen::kButtons[i];
		TextButton* widget = std::string(generatedButton.id) == "cancel"
			? _state->_btnCancel : _state->_btnOk;
		// Only expose visible actions; native hides Confirm when unaffordable
		if (widget && !widget->getVisible()) continue;
		TextButton* peer = widget == _state->_btnCancel ? _state->_btnOk : _state->_btnCancel;
		// Peer must also be visible to be considered for focus
		if (peer && !peer->getVisible()) peer = nullptr;
		model.buttons.push_back({
			widget,
			peer,
			widget ? widget->getText() : std::string(),
			project(buttonRects[i].rect),
			std::string(generatedButton.tone) == "danger"
				? CalypsoActionTone::Destructive : CalypsoActionTone::Safe,
			generatedButton.fill,
			generatedButton.border,
			generatedButton.text});
	}

	calypsoCollectSmallConfirmation(builder, model, _motion);
}

void CalypsoF03DismantleUi::applyGeneratedLayout(
	DismantleFacilityState& state, bool wide)
{
	const auto* generated = CalypsoF03DismantleGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return;
	const int dx = presentationShiftX(*generated, wide);
	applyRect(state._window, shiftedRect(generated->window, dx));
	applyRect(state._txtTitle, shiftedRect(generated->title, dx));
	applyRect(state._txtFacility, shiftedRect(generated->message, dx));
	applyRect(state._txtRefundValue, shiftedRect(generated->message, dx));
	applyRect(state._btnCancel, touchRect(generatedButtonRect(wide, "cancel", dx)));
	applyRect(state._btnOk, touchRect(generatedButtonRect(wide, "confirm", dx)));
}

void CalypsoF03DismantleUi::configure(DismantleFacilityState& state, bool allow)
{
	const bool english = Options::language == "en-US"
		|| Options::language == "en-GB" || Options::language == "en";
	state._hdLayout = allow && english && state._game && state._game->getMod()
		&& state._game->getLanguage()
		&& state._game->getMod()->isHdUiFamilyEnabled("F03");
	if (!state._hdLayout) return;

	state._hdWideLayout = currentLayoutClass() == CalypsoLayoutClass::Wide;
	// data-hiding P1: the HD adapter must surface the actual selected facility type
	// (from the rule) instead of the generic canonical body lines.
	const std::string facilityType = state._fac ? state._fac->getRules()->getType() : std::string();
	state._hdFacilityText = facilityType.empty()
		? std::string(state._txtFacility ? state._txtFacility->getText() : std::string())
		: std::string(state.tr(facilityType));
	// data-hiding P1: surface the computed refund value (STR_REFUND_VALUE).
	state._hdRefundText = state._txtRefundValue
		? std::string(state._txtRefundValue->getText())
		: std::string(state.tr("STR_REFUND_VALUE"));
	state._hdRefundVisible = state._txtRefundValue ? state._txtRefundValue->getVisible() : false;
	state._hdHarnessGeneration = Calypso::calypsoHarnessSession().generation;
	state._txtTitle->setText(state.tr("STR_CAL_F03_DISMANTLE_TITLE"));
	state._txtFacility->setText(
		std::string(state.tr("STR_CAL_F03_DISMANTLE_LINE_1")) + "\n"
		+ std::string(state.tr("STR_CAL_F03_DISMANTLE_LINE_2")));
	state._txtRefundValue->setVisible(false);
	state._btnOk->setText(state.tr("STR_CAL_F03_DISMANTLE_ACTION"));
	applyGeneratedLayout(state, state._hdWideLayout);
	const auto* generated = CalypsoF03DismantleGen::layoutForDesign(
		state._hdWideLayout ? 1280 : 740, state._hdWideLayout ? 720 : 360);
	if (!generated) { state._hdLayout = false; return; }
	state.enableUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);

	auto* adapter = new CalypsoF03DismantleUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter);
	if (calypsoHarnessHostUp(calypsoHarnessSession())) calypsoHdHarnessDomShow();
}

bool CalypsoF03DismantleUi::resize(DismantleFacilityState& state)
{
	if (!state._hdLayout) return false;
	const bool wide = currentLayoutClass() == CalypsoLayoutClass::Wide;
	state._hdWideLayout = wide;
	applyGeneratedLayout(state, wide);
	const auto* generated = CalypsoF03DismantleGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return false;
	state.recaptureUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	return true;
}

} // namespace Calypso
} // namespace OpenXcom
#endif
