#ifdef __EMSCRIPTEN__
/*
 * F33 (Calypso): pause-menu DOM overlay bridge — see CalypsoPauseMenu.h.
 */
#include "CalypsoPauseMenu.h"
#include "../Engine/Game.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Menu/PauseState.h"
#include <emscripten.h>

namespace OpenXcom
{
namespace Calypso
{

bool pauseMenuDomShow(
	int origin,
	bool showLoad, bool showSave, bool showAbandon, bool showOptions, bool showCancel,
	const std::string &title,
	const std::string &loadLabel, const std::string &saveLabel,
	const std::string &abandonLabel, const std::string &optionsLabel,
	const std::string &cancelLabel)
{
	return EM_ASM_INT({
		if (!globalThis.__calypsoPauseShow)
			return 0;
		try {
			return globalThis.__calypsoPauseShow({
				origin: $0,
				buttons: {
					load:    { show: $1, label: UTF8ToString($6) },
					save:    { show: $2, label: UTF8ToString($7) },
					abandon: { show: $3, label: UTF8ToString($8) },
					options: { show: $4, label: UTF8ToString($9) },
					cancel:  { show: $5, label: UTF8ToString($10) }
				},
				title: UTF8ToString($11)
			}) === true ? 1 : 0;
		} catch (_) {
			return 0;
		}
	}, origin,
	   showLoad ? 1 : 0, showSave ? 1 : 0, showAbandon ? 1 : 0,
	   showOptions ? 1 : 0, showCancel ? 1 : 0,
	   loadLabel.c_str(), saveLabel.c_str(), abandonLabel.c_str(),
	   optionsLabel.c_str(), cancelLabel.c_str(), title.c_str());
}

void pauseMenuDomHide()
{
	EM_ASM_({
		if (globalThis.__calypsoPauseHide)
			globalThis.__calypsoPauseHide();
	});
}

void calypsoReportHdRouteError(const std::string &route, const std::string &detail)
{
	EM_ASM_({
		const route = UTF8ToString($0);
		const detail = UTF8ToString($1);
		const message = 'Calypso HD error [' + route + ']: ' + detail;
		const previous = globalThis.__calypsoHdRouteError;
		let delegated = false;
		try {
			if (typeof globalThis.calypsoReportHdRouteError === 'function') {
				globalThis.calypsoReportHdRouteError(route, detail);
				const marker = globalThis.__calypsoHdRouteError;
				delegated = !!marker && marker !== previous
					&& marker.route === route && marker.detail === detail;
			}
		} catch (_) {
			delegated = false;
		}
		if (!delegated) {
			if (typeof document !== 'undefined') {
				const log = document.getElementById('log');
				if (log) {
					const current = String(log.textContent || '');
					if (!current.includes(message)) log.textContent = current + '\n' + message;
					if (log.classList) log.classList.add('visible');
				}
			}
			if (globalThis.console && typeof globalThis.console.error === 'function')
				globalThis.console.error(message);
		}
	}, route.c_str(), detail.c_str());
}

void CalypsoPauseMenu::configure(PauseState& state)
{
	auto setNativePresentation = [&state](bool visible)
	{
		state._window->setVisible(visible ? state._calypsoShowWindow : false);
		state._txtTitle->setVisible(visible ? state._calypsoShowTitle : false);
		state._txtVersion->setVisible(visible ? state._calypsoShowVersion : false);
		state._btnLoad->setVisible(visible ? state._calypsoShowLoad : false);
		state._btnSave->setVisible(visible ? state._calypsoShowSave : false);
		state._btnAbandon->setVisible(visible ? state._calypsoShowAbandon : false);
		state._btnOptions->setVisible(visible ? state._calypsoShowOptions : false);
		state._btnCancel->setVisible(visible ? state._calypsoShowCancel : false);
	};

	// Capture the native widget presentation once. The DOM overlay mirrors these
	// widgets; rereading their visibility after Abandon is cancelled would turn
	// every action off on the next think() pass.
	if (!state._calypsoPresentationCaptured)
	{
		state._calypsoShowWindow = state._window->getVisible();
		state._calypsoShowTitle = state._txtTitle->getVisible();
		state._calypsoShowVersion = state._txtVersion->getVisible();
		state._calypsoShowLoad = state._btnLoad->getVisible();
		state._calypsoShowSave = state._btnSave->getVisible();
		state._calypsoShowAbandon = state._btnAbandon->getVisible();
		state._calypsoShowOptions = state._btnOptions->getVisible();
		state._calypsoShowCancel = state._btnCancel->getVisible();
		state._calypsoLoadLabel = state._btnLoad->getText();
		state._calypsoSaveLabel = state._btnSave->getText();
		state._calypsoAbandonLabel = state._btnAbandon->getText();
		state._calypsoOptionsLabel = state._btnOptions->getText();
		state._calypsoCancelLabel = state._btnCancel->getText();
		state._calypsoPresentationCaptured = true;
	}
	// Registered HD routes are fail-closed: suppress native pixels before trying
	// the DOM owner, and never restore them as a fallback after a failed attempt.
	setNativePresentation(false);
	// Friend access to the state's immutable presentation snapshot (no event
	// ownership): same data the native widget layout would have drawn.
	const bool domPresented = pauseMenuDomShow(
		(int)state._origin,
		state._calypsoShowLoad, state._calypsoShowSave,
		state._calypsoShowAbandon, state._calypsoShowOptions,
		state._calypsoShowCancel,
		std::string(state.tr("STR_OPTIONS_UC")),
		state._calypsoLoadLabel, state._calypsoSaveLabel,
		state._calypsoAbandonLabel, state._calypsoOptionsLabel,
		state._calypsoCancelLabel);
	if (!domPresented)
	{
		pauseMenuDomHide();
		calypsoReportHdRouteError("pause", "DOM pause overlay unavailable");
	}
}

void CalypsoPauseMenu::think(PauseState& state, Game& game)
{
	if (game.isState(&state))
	{
		configure(state);
	}
}

} // namespace Calypso
} // namespace OpenXcom

/* ── JS callable exports ──────────────────────────────────────────────── */

namespace
{

/// The active PauseState (top of the stack), or nullptr.
OpenXcom::PauseState *activePauseState()
{
	OpenXcom::Game *g = OpenXcom::getCurrentGame();
	if (!g) return nullptr;
	return dynamic_cast<OpenXcom::PauseState *>(g->getTopState());
}

} // namespace

extern "C" {

EMSCRIPTEN_KEEPALIVE
void calypso_pause_load()
{
	if (auto *p = activePauseState()) p->btnLoadClick(nullptr);
}

EMSCRIPTEN_KEEPALIVE
void calypso_pause_save()
{
	if (auto *p = activePauseState()) p->btnSaveClick(nullptr);
}

EMSCRIPTEN_KEEPALIVE
void calypso_pause_abandon()
{
	if (auto *p = activePauseState()) p->btnAbandonClick(nullptr);
}

EMSCRIPTEN_KEEPALIVE
void calypso_pause_options()
{
	if (auto *p = activePauseState()) p->btnOptionsClick(nullptr);
}

EMSCRIPTEN_KEEPALIVE
void calypso_pause_cancel()
{
	if (auto *p = activePauseState()) p->btnCancelClick(nullptr);
}

} /* extern "C" */

#endif // __EMSCRIPTEN__
