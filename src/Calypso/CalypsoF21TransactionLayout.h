/* F21 Transaction: Abandon shell with table + input */
#pragma once
#include "CalypsoF21LayoutBase.h"
#include "Generated/CalypsoF21Transaction.generated.h"
#include <string>
#include <string_view>
namespace OpenXcom { namespace Calypso {
static_assert(std::string_view(CalypsoF21TransactionGen::kContractVersion) == std::string_view(CalypsoHdThemeGen::kContractVersion), "F21 transaction/theme version mismatch");
struct CalypsoF21TransactionLayout {
 int designWidth=0, designHeight=0;
 CalypsoF21Rect window, status, warning, title, message, footer;
 CalypsoF21Rect cellR1C1, cellR1C2, cellR2C1, cellR2C2, cellR3C1, cellR3C2;
 CalypsoF21Rect columnDivider1, rowDivider1, rowDivider2;
 CalypsoF21Rect inputFrame, inputHint;
 CalypsoF21Rect cancel, create;
};
inline CalypsoF21TransactionLayout calypsoF21TransactionLayout(CalypsoLayoutClass cls){
 CalypsoF21TransactionLayout l;
 const bool wide = cls==CalypsoLayoutClass::Wide;
 const auto* g = &CalypsoF21TransactionGen::kLayouts[wide?0:1];
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
 l.columnDivider1={g->columnDivider1.x,g->columnDivider1.y,g->columnDivider1.w,g->columnDivider1.h};
 l.inputFrame={g->inputFrame.x,g->inputFrame.y,g->inputFrame.w,g->inputFrame.h};
 l.inputHint={g->inputHint.x,g->inputHint.y,g->inputHint.w,g->inputHint.h};
 l.rowDivider1={g->rowDivider1.x,g->rowDivider1.y,g->rowDivider1.w,g->rowDivider1.h};
 l.rowDivider2={g->rowDivider2.x,g->rowDivider2.y,g->rowDivider2.w,g->rowDivider2.h};
 const int idx = wide?0:1;
 for(int k=0;k<CalypsoF21TransactionGen::kButtonCount;++k){
   auto br = CalypsoF21TransactionGen::kButtonRects[idx][k];
   std::string id(br.id);
   if(id=="cancel") l.cancel={br.rect.x,br.rect.y,br.rect.w,br.rect.h};
   if(id=="create") l.create={br.rect.x,br.rect.y,br.rect.w,br.rect.h};
 }
 return l;
}
inline void calypsoF21TransactionApplyHarnessShift(CalypsoF21TransactionLayout& l,bool sideBySide){
 if(!sideBySide || l.designWidth!=1280) return;
 const int dx=40-l.window.x;
 l.window.x+=dx; l.status.x+=dx; l.warning.x+=dx; l.title.x+=dx; l.message.x+=dx; l.footer.x+=dx;
 l.cellR1C1.x+=dx; l.cellR1C2.x+=dx; l.cellR2C1.x+=dx; l.cellR2C2.x+=dx; l.cellR3C1.x+=dx; l.cellR3C2.x+=dx;
 l.columnDivider1.x+=dx; l.rowDivider1.x+=dx; l.rowDivider2.x+=dx; l.inputFrame.x+=dx; l.inputHint.x+=dx; l.cancel.x+=dx; l.create.x+=dx;
}
}}
