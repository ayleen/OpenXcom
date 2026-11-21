#pragma once
#ifdef __EMSCRIPTEN__
#include "../Menu/ErrorMessageState.h"
namespace OpenXcom { namespace Calypso {
inline ErrorMessageHdForm calypsoRdErrorForm(const void* owner,
 const std::string& title, const std::string& reason, const std::string& remedy, const std::string& ok)
{
 ErrorMessageHdForm form;
 form.protocol = title; form.title = title;
 form.bodyLines.push_back(reason); form.bodyLines.push_back(remedy); form.actionLabel = ok;
 form.hdUnderlayState = owner;
 form.allowStrategicNavigation = true;
 return form;
}
} }
#endif
