#ifdef __EMSCRIPTEN__
/*
 * F33 (Calypso): pause-menu DOM overlay bridge — see CalypsoPauseMenu.h.
 */
#include "CalypsoPauseMenu.h"
#include "../Engine/Game.h"
#include "../Menu/PauseState.h"
#include <emscripten.h>

namespace OpenXcom
{
namespace Calypso
{

void pauseMenuDomShow(
	int origin,
	bool showLoad, bool showSave, bool showAbandon, bool showOptions, bool showCancel,
	const std::string &title,
	const std::string &loadLabel, const std::string &saveLabel,
	const std::string &abandonLabel, const std::string &optionsLabel,
	const std::string &cancelLabel)
{
	EM_ASM_({
		if (globalThis.__calypsoPauseShow)
			globalThis.__calypsoPauseShow({
				origin: $0,
				buttons: {
					load:    { show: $1, label: UTF8ToString($6) },
					save:    { show: $2, label: UTF8ToString($7) },
					abandon: { show: $3, label: UTF8ToString($8) },
					options: { show: $4, label: UTF8ToString($9) },
					cancel:  { show: $5, label: UTF8ToString($10) }
				},
				title: UTF8ToString($11)
			});
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
