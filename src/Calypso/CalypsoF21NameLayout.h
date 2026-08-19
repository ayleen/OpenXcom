#pragma once
#include "CalypsoF21LayoutBase.h"
#include "Generated/CalypsoF21Name.generated.h"
#include <string>
#include <string_view>
namespace OpenXcom { namespace Calypso {
static_assert(std::string_view(CalypsoF21NameGen::kContractVersion)==std::string_view(CalypsoHdThemeGen::kContractVersion),"F21 name/theme mismatch");
struct CalypsoF21NameLayout{ int designWidth=0,designHeight=0; CalypsoF21Rect window,status,warning,title,message,footer; CalypsoF21Rect inputFrame,inputHint; CalypsoF21Rect ok; };
inline CalypsoF21NameLayout calypsoF21NameLayout(CalypsoLayoutClass cls){
 CalypsoF21NameLayout l; const bool wide=cls==CalypsoLayoutClass::Wide;
 const auto* g = &CalypsoF21NameGen::kLayouts[wide?0:1];
 l.designWidth=g->designWidth; l.designHeight=g->designHeight;
 l.window={g->window.x,g->window.y,g->window.w,g->window.h};
 l.status={g->status.x,g->status.y,g->status.w,g->status.h};
 l.warning={g->warning.x,g->warning.y,g->warning.w,g->warning.h};
 l.title={g->title.x,g->title.y,g->title.w,g->title.h};
 l.message={g->message.x,g->message.y,g->message.w,g->message.h};
 l.footer={g->footer.x,g->footer.y,g->footer.w,g->footer.h};
 l.inputFrame={g->inputFrame.x,g->inputFrame.y,g->inputFrame.w,g->inputFrame.h};
 l.inputHint={g->inputHint.x,g->inputHint.y,g->inputHint.w,g->inputHint.h};
 const int idx=wide?0:1;
 for(int k=0;k<CalypsoF21NameGen::kButtonCount;++k){ auto br=CalypsoF21NameGen::kButtonRects[idx][k]; if(std::string(br.id)=="ok") l.ok={br.rect.x,br.rect.y,br.rect.w,br.rect.h}; }
 return l;
}
inline void calypsoF21NameApplyHarnessShift(CalypsoF21NameLayout& l,bool sideBySide){ if(!sideBySide||l.designWidth!=1280) return; int dx=40-l.window.x; l.window.x+=dx; l.status.x+=dx; l.warning.x+=dx; l.title.x+=dx; l.message.x+=dx; l.footer.x+=dx; l.inputFrame.x+=dx; l.inputHint.x+=dx; l.ok.x+=dx; }
}}
