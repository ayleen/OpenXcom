#ifdef __EMSCRIPTEN__
#include "CalypsoBuildNewBaseQa.h"
#include <emscripten.h>

extern "C" int g_calypsoGeoscapeHdPreview;

namespace {
static int s_calypsoBuildNewBaseHoverCount = 0;
static int s_calypsoBuildNewBaseClickCount = 0;
static int s_calypsoBuildNewBaseLastHoverX = 0;
static int s_calypsoBuildNewBaseLastHoverY = 0;
static int s_calypsoBuildNewBaseLastClickX = 0;
static int s_calypsoBuildNewBaseLastClickY = 0;
static int s_calypsoBuildNewBaseLastOutcome = 0;
}

namespace OpenXcom
{
namespace Calypso
{
void calypsoBuildNewBaseNoteHover(int x, int y)
{
	if (g_calypsoGeoscapeHdPreview == 0) return;
	++s_calypsoBuildNewBaseHoverCount;
	s_calypsoBuildNewBaseLastHoverX = x;
	s_calypsoBuildNewBaseLastHoverY = y;
}
void calypsoBuildNewBaseNoteClick(int x, int y)
{
	if (g_calypsoGeoscapeHdPreview == 0) return;
	++s_calypsoBuildNewBaseClickCount;
	s_calypsoBuildNewBaseLastClickX = x;
	s_calypsoBuildNewBaseLastClickY = y;
}
void calypsoBuildNewBaseNoteOutcome(int outcome)
{
	if (g_calypsoGeoscapeHdPreview == 0) return;
	s_calypsoBuildNewBaseLastOutcome = outcome;
}
} // namespace Calypso
} // namespace OpenXcom

extern "C" {
EMSCRIPTEN_KEEPALIVE int calypso_qa_buildnewbase_hover_count()
{
	return s_calypsoBuildNewBaseHoverCount;
}
EMSCRIPTEN_KEEPALIVE int calypso_qa_buildnewbase_click_count()
{
	return s_calypsoBuildNewBaseClickCount;
}
EMSCRIPTEN_KEEPALIVE int calypso_qa_buildnewbase_last_hover_x()
{
	return s_calypsoBuildNewBaseLastHoverX;
}
EMSCRIPTEN_KEEPALIVE int calypso_qa_buildnewbase_last_hover_y()
{
	return s_calypsoBuildNewBaseLastHoverY;
}
EMSCRIPTEN_KEEPALIVE int calypso_qa_buildnewbase_last_click_x()
{
	return s_calypsoBuildNewBaseLastClickX;
}
EMSCRIPTEN_KEEPALIVE int calypso_qa_buildnewbase_last_click_y()
{
	return s_calypsoBuildNewBaseLastClickY;
}
EMSCRIPTEN_KEEPALIVE int calypso_qa_buildnewbase_last_outcome()
{
	return s_calypsoBuildNewBaseLastOutcome;
}
EMSCRIPTEN_KEEPALIVE void calypso_qa_buildnewbase_reset()
{
	s_calypsoBuildNewBaseHoverCount = 0;
	s_calypsoBuildNewBaseClickCount = 0;
	s_calypsoBuildNewBaseLastHoverX = 0;
	s_calypsoBuildNewBaseLastHoverY = 0;
	s_calypsoBuildNewBaseLastClickX = 0;
	s_calypsoBuildNewBaseLastClickY = 0;
	s_calypsoBuildNewBaseLastOutcome = 0;
}
} // extern "C"
#endif /* __EMSCRIPTEN__ */
