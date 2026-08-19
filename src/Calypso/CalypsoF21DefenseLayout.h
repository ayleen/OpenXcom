#pragma once
#include "CalypsoF21LayoutBase.h"
#include <string>
#include <string_view>
#include "Generated/CalypsoF21Defense.generated.h"
namespace OpenXcom { namespace Calypso {
static_assert(std::string_view(CalypsoF21DefenseGen::kContractVersion)==std::string_view(CalypsoHdThemeGen::kContractVersion),"F21 defense/theme mismatch");
struct CalypsoF21DefenseLayout{ int designWidth=0,designHeight=0; CalypsoF21Rect window,status,warning,title,message,footer; CalypsoF21Rect skip,start,ok; };
inline CalypsoF21DefenseLayout calypsoF21DefenseLayout(CalypsoLayoutClass cls){
 CalypsoF21DefenseLayout l; const bool wide=cls==CalypsoLayoutClass::Wide;
 const auto* g = &CalypsoF21DefenseGen::kLayouts[wide?0:1];
 l.designWidth=g->designWidth; l.designHeight=g->designHeight;
 l.window={g->window.x,g->window.y,g->window.w,g->window.h};
 l.status={g->status.x,g->status.y,g->status.w,g->status.h};
 l.warning={g->warning.x,g->warning.y,g->warning.w,g->warning.h};
 l.title={g->title.x,g->title.y,g->title.w,g->title.h};
 l.message={g->message.x,g->message.y,g->message.w,g->message.h};
 l.footer={g->footer.x,g->footer.y,g->footer.w,g->footer.h};
 const int idx=wide?0:1;
 for(int k=0;k<CalypsoF21DefenseGen::kButtonCount;++k){ auto br=CalypsoF21DefenseGen::kButtonRects[idx][k]; std::string id(br.id); if(id=="skip") l.skip={br.rect.x,br.rect.y,br.rect.w,br.rect.h}; if(id=="start") l.start={br.rect.x,br.rect.y,br.rect.w,br.rect.h}; if(id=="ok") l.ok={br.rect.x,br.rect.y,br.rect.w,br.rect.h}; }
 return l;
}
inline void calypsoF21DefenseApplyHarnessShift(CalypsoF21DefenseLayout& l,bool sideBySide){ if(!sideBySide||l.designWidth!=1280) return; int dx=40-l.window.x; l.window.x+=dx; l.status.x+=dx; l.warning.x+=dx; l.title.x+=dx; l.message.x+=dx; l.footer.x+=dx; l.skip.x+=dx; l.start.x+=dx; l.ok.x+=dx; }
}}
