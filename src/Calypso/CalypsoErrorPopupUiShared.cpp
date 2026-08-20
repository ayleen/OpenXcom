/*
 * Generated small-confirmation adapter for ErrorMessageState.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoErrorPopupUi.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "../Engine/Game.h"
#include "../Engine/Language.h"
#include "../Engine/Options.h"
#include "../Engine/Surface.h"
#include "../Geoscape/BuildNewBaseState.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Menu/ErrorMessageState.h"
#include "../Mod/Mod.h"

#include "CalypsoBevelPanel.h"
#include "CalypsoF34ErrorLayout.h"
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

CalypsoF34ErrorLayout currentPresentationLayout(bool wide)
{
	CalypsoF34ErrorLayout layout = calypsoF34ErrorLayout(
		wide ? CalypsoLayoutClass::Wide : CalypsoLayoutClass::Compact);
	calypsoF34ErrorApplyHarnessShift(
		layout, calypsoHarnessSession().sideBySide && wide);
	return layout;
}

void applyRect(Surface* surface, const CalypsoF34Rect& rect)
{
	if (!surface) return;
	surface->setX(rect.x);
	surface->setY(rect.y);
	surface->setWidth(rect.width);
	surface->setHeight(rect.height);
}

CalypsoLogicalRect widgetRect(const Surface* surface)
{
	return surface
		? CalypsoLogicalRect{surface->getX(), surface->getY(), surface->getWidth(), surface->getHeight()}
		: CalypsoLogicalRect{};
}

std::string joinLines(const std::vector<std::string>& lines)
{
	std::string text;
	for (const std::string& line : lines)
	{
		if (!text.empty()) text += '\n';
		text += line;
	}
	return text;
}

} // namespace

CalypsoErrorPopupUi::~CalypsoErrorPopupUi()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

const void* CalypsoErrorPopupUi::topState() const
{
	return _state;
}

void CalypsoErrorPopupUi::collectLogicalSuppression(
	CalypsoHdLogicalSuppression& suppression) const
{
	if (!_state || !_state->_coveredState) return;
	const auto* site = dynamic_cast<const BuildNewBaseState*>(_state->_coveredState);
	if (!site) return;
	suppression.add(site->_window);
	suppression.add(site->_txtTitle);
	suppression.add(site->_btnCancel);
	suppression.add(site->_hdProtocol);
	suppression.add(site->_hdSlot);
	suppression.add(site->_hdFunds);
	suppression.add(site->_hdCost);
	suppression.add(site->_hdCard);
	suppression.add(site->_hdCoords);
	suppression.add(site->_hdRegion);
	suppression.add(site->_hdLegality);
	suppression.add(site->_hdPreview);
}

void CalypsoErrorPopupUi::collect(CalypsoHdFrameBuilder& builder) const
{
	if (!_state || !_state->_hdLayout || !_state->_game) return;

	const bool wide = _state->_hdWideLayout;
	const auto* generated = CalypsoF34ErrorGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return;

	const CalypsoLogicalRect window = widgetRect(_state->_window);
	const double uiScale = generated->window.w > 0
		? (double)window.w / generated->window.w : 1.0;
	const CalypsoHdPresentationMetrics& metrics =
		CalypsoHdUiOverlay::instance().frozenMetrics();
	auto project = [&](const CalypsoF34ErrorGen::CalypsoF34ErrorGenRect& rect)
		-> CalypsoLogicalRect
	{
		return {
			window.x + (int)std::llround((rect.x - generated->window.x) * uiScale),
			window.y + (int)std::llround((rect.y - generated->window.y) * uiScale),
			std::max(1, (int)std::llround(rect.w * uiScale)),
			std::max(1, (int)std::llround(rect.h * uiScale))};
	};

	CalypsoSmallConfirmationModel model;
	model.familyId = CalypsoF34ErrorGen::kFamilyId;
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
	model.warningWidget = _state->_hdIconPanel;
	model.protocolWidget = _state->_hdProtocol;
	model.titleWidget = _state->_hdWarning;
	model.messageWidget = _state->_txtMessage;
	model.protocolText = _state->_hdProtocol ? _state->_hdProtocol->getText() : std::string();
	model.titleText = _state->_hdWarning ? _state->_hdWarning->getText() : std::string();
	model.messageText = _state->_txtMessage ? _state->_txtMessage->getText() : std::string();
	model.cutCornerPx = CalypsoF34ErrorGen::kCutCornerPx;
	model.protocolTextInsetPx = CalypsoF34ErrorGen::kProtocolTextInsetPx;
	model.panelFillTop = CalypsoF34ErrorGen::kPanelFillTop;
	model.panelFillBottom = CalypsoF34ErrorGen::kPanelFillBottom;
	model.frameColor = CalypsoF34ErrorGen::kFrame;
	model.protocolColor = CalypsoF34ErrorGen::kProtocolText;
	model.dividerColor = CalypsoF34ErrorGen::kDivider;
	model.footerDotColor = CalypsoF34ErrorGen::kFooterDot;
	model.warningColor = CalypsoF34ErrorGen::kWarning;
	model.uiScale = uiScale;
	model.visualScale = CalypsoF34ErrorGen::kPresentationScale;
	model.projectionScaleX = uiScale * metrics.scaleX;
	model.projectionScaleY = uiScale * metrics.scaleY;
	model.messageDesignWidth = generated->message.w;
	model.titleDesignHeight = generated->title.h;
	model.motionDurationMs = CalypsoF34ErrorGen::kMotionDurationMs;
	model.motionScaleFrom = CalypsoF34ErrorGen::kMotionScaleFrom;
	model.buttons.push_back({
		_state->_btnOk, nullptr,
		_state->_btnOk ? _state->_btnOk->getText() : std::string(),
		widgetRect(_state->_btnOk), CalypsoActionTone::Safe,
		CalypsoF34ErrorGen::kButtons[0].fill,
		CalypsoF34ErrorGen::kButtons[0].border,
		CalypsoF34ErrorGen::kButtons[0].text});

	calypsoCollectSmallConfirmation(builder, model, _motion);
}

void CalypsoErrorPopupUi::applyRects(
	ErrorMessageState& state, const CalypsoF34ErrorLayout& layout)
{
	applyRect(state._window, layout.window);
	applyRect(state._hdProtocol, layout.status);
	applyRect(state._hdIconPanel, layout.iconPanel);
	applyRect(state._hdIcon, layout.icon);
	applyRect(state._hdWarning, layout.warning);
	applyRect(state._txtMessage, layout.message);
	applyRect(state._btnOk, layout.acknowledge);
}

void CalypsoErrorPopupUi::configure(
	ErrorMessageState& state, bool allowPhysicalOverlay)
{
	state._hdLayout = !state._hdForm.empty()
		&& state._game && state._game->getMod() && state._game->getLanguage()
		&& isF34PhysicalRouteEligible(
			state._game->getMod()->isHdUiFamilyEnabled("F34"),
			state._game->getLanguage()->getTextDirection() == DIRECTION_RTL,
			!allowPhysicalOverlay);
	if (!state._hdLayout) return;

	state._hdWideLayout = currentLayoutClass() == CalypsoLayoutClass::Wide;
	const CalypsoF34ErrorLayout layout = currentPresentationLayout(state._hdWideLayout);
	const Uint8 themeColor = state._txtMessage->getColor();

	CalypsoBevelPanel* badge = new CalypsoBevelPanel();
	badge->setTheme(themeColor, themeColor);
	state._hdIconPanel = badge;
	state._hdIcon = new Text(1, 1, 0, 0);
	state._hdProtocol = new Text(1, 1, 0, 0);
	state._hdWarning = new Text(1, 1, 0, 0);
	state.add(state._hdIconPanel);
	state.add(state._hdIcon);
	state.add(state._hdProtocol);
	state.add(state._hdWarning);

	applyRects(state, layout);
	state._hdIcon->setSmall();
	state._hdIcon->setColor(themeColor);
	state._hdIcon->setAlign(ALIGN_CENTER);
	state._hdIcon->setVerticalAlign(ALIGN_MIDDLE);
	state._hdIcon->setText("!");
	state._hdProtocol->setSmall();
	state._hdProtocol->setColor(themeColor);
	state._hdProtocol->setAlign(ALIGN_LEFT);
	state._hdProtocol->setVerticalAlign(ALIGN_MIDDLE);
	state._hdProtocol->setText(state._hdForm.protocol);
	state._hdWarning->setSmall();
	state._hdWarning->setColor(themeColor);
	state._hdWarning->setAlign(ALIGN_LEFT);
	state._hdWarning->setVerticalAlign(ALIGN_MIDDLE);
	state._hdWarning->setText(state._hdForm.title);
	state._txtMessage->setAlign(ALIGN_LEFT);
	state._txtMessage->setVerticalAlign(ALIGN_TOP);
	state._txtMessage->setWordWrap(true);
	state._txtMessage->setText(joinLines(state._hdForm.bodyLines));
	state._btnOk->setText(state._hdForm.actionLabel);

	state.enableUiScaling(layout.designWidth, layout.designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	CalypsoErrorPopupUi* adapter = new CalypsoErrorPopupUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter);
	if (calypsoHarnessHostUp(calypsoHarnessSession()))
	{
		calypsoHdHarnessDomShow();
	}
}

bool CalypsoErrorPopupUi::resize(ErrorMessageState& state)
{
	if (!state._hdLayout) return false;
	const bool wide = currentLayoutClass() == CalypsoLayoutClass::Wide;
	if (wide != state._hdWideLayout)
	{
		state._hdWideLayout = wide;
		const CalypsoF34ErrorLayout layout = currentPresentationLayout(wide);
		applyRects(state, layout);
		state.recaptureUiScaling(layout.designWidth, layout.designHeight, 1.0f,
			/*subtractVanillaCenter=*/false);
	}
	else
	{
		applyRects(state, currentPresentationLayout(wide));
		state.applyUiScaling();
	}
	return true;
}

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
