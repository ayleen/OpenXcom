#pragma once
#ifdef __EMSCRIPTEN__
#include <string>

namespace OpenXcom
{
namespace Calypso
{
void calypsoBuildNewBaseNoteHover(int x, int y);
void calypsoBuildNewBaseNoteClick(int x, int y);
void calypsoBuildNewBaseNoteOutcome(int outcome);
} // namespace Calypso
} // namespace OpenXcom

extern "C" {
int calypso_qa_buildnewbase_hover_count();
int calypso_qa_buildnewbase_click_count();
int calypso_qa_buildnewbase_last_hover_x();
int calypso_qa_buildnewbase_last_hover_y();
int calypso_qa_buildnewbase_last_click_x();
int calypso_qa_buildnewbase_last_click_y();
int calypso_qa_buildnewbase_last_outcome();
void calypso_qa_buildnewbase_reset();
} // extern "C"
#endif /* __EMSCRIPTEN__ */
