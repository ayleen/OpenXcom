/* F21 Destruction: Abandon shell expanded for table */
#pragma once
#include "CalypsoF21LayoutBase.h"
#include "Generated/CalypsoF21Destruction.generated.h"
#include <string>
namespace OpenXcom { namespace Calypso {
static_assert(std::string_view(CalypsoF21DestructionGen::kContractVersion)==std::string_view(CalypsoHdThemeGen::kContractVersion),"F21 destruction/theme mismatch");
struct CalypsoF21DestructionLayout{ int designWidth=0,designHeight=0; CalypsoF21Rect window,status,warning,title,message,footer,acknowledge; CalypsoF21Rect cellR1C1,cellR1C2,cellR2C1,cellR2C2,cellR3C1,cellR3C2,cellR4C1,cellR4C2,columnDivider1,rowDivider1,rowDivider2,rowDivider3; };
inline CalypsoF21DestructionLayout calypsoF21DestructionLayout(CalypsoLayoutClass cls){
 CalypsoF21DestructionLayout l; const bool wide=cls==CalypsoLayoutClass::Wide;
 const auto* g = &CalypsoF21DestructionGen::kLayouts[wide?0:1];
 l.designWidth=g->designWidth; l.designHeight=g->designHeight;
 l.window={g->window.x,g->window.y,g->window.w,g->window.h};
 l.status={g->status.x,g->status.y,g->status.w,g->status.h};
 l.warning={g->warning.x,g->warning.y,g->warning.w,g->warning.h};
 l.title={g->title.x,g->title.y,g->title.w,g->title.h};
 l.message={g->message.x,g->message.y,g->message.w,g->message.h};
 l.footer={g->footer.x,g->footer.y,g->footer.w,g->footer.h};
 l.cellR1C1={g->cellR1C1.x,g->cellR1C1.y,g->cellR1C1.w,g->cellR1C1.h};
 l.cellR1C2={g->cellR1C2.x,g->cellR1C2.y,g->cellR1C2.w,g->cellR1C2.h};
 l.cellR2C1={g->cellR2C1.x,g->cellR2C1.y,g->cellR2C1.w,g->cellR2C1.h};
 l.cellR2C2={g->cellR2C2.x,g->cellR2C2.y,g->cellR2C2.w,g->cellR2C2.h};
 l.cellR3C1={g->cellR3C1.x,g->cellR3C1.y,g->cellR3C1.w,g->cellR3C1.h};
 l.cellR3C2={g->cellR3C2.x,g->cellR3C2.y,g->cellR3C2.w,g->cellR3C2.h};
 l.cellR4C1={g->cellR4C1.x,g->cellR4C1.y,g->cellR4C1.w,g->cellR4C1.h};
 l.cellR4C2={g->cellR4C2.x,g->cellR4C2.y,g->cellR4C2.w,g->cellR4C2.h};
 l.columnDivider1={g->columnDivider1.x,g->columnDivider1.y,g->columnDivider1.w,g->columnDivider1.h};
 l.rowDivider1={g->rowDivider1.x,g->rowDivider1.y,g->rowDivider1.w,g->rowDivider1.h};
 l.rowDivider2={g->rowDivider2.x,g->rowDivider2.y,g->rowDivider2.w,g->rowDivider2.h};
 l.rowDivider3={g->rowDivider3.x,g->rowDivider3.y,g->rowDivider3.w,g->rowDivider3.h};
 const int idx=wide?0:1;
 for(int k=0;k<CalypsoF21DestructionGen::kButtonCount;++k){ auto br=CalypsoF21DestructionGen::kButtonRects[idx][k]; if(std::string(br.id)=="acknowledge") l.acknowledge={br.rect.x,br.rect.y,br.rect.w,br.rect.h}; }
 return l;
}
inline void calypsoF21DestructionApplyHarnessShift(CalypsoF21DestructionLayout& l,bool sideBySide){ if(!sideBySide||l.designWidth!=1280) return; int dx=40-l.window.x; l.window.x+=dx; l.status.x+=dx; l.warning.x+=dx; l.title.x+=dx; l.message.x+=dx; l.footer.x+=dx; l.cellR1C1.x+=dx; l.cellR1C2.x+=dx; l.cellR2C1.x+=dx; l.cellR2C2.x+=dx; l.cellR3C1.x+=dx; l.cellR3C2.x+=dx; l.cellR4C1.x+=dx; l.cellR4C2.x+=dx; l.columnDivider1.x+=dx; l.rowDivider1.x+=dx; l.rowDivider2.x+=dx; l.rowDivider3.x+=dx; l.acknowledge.x+=dx; }
}}
