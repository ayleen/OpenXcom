#pragma once
#ifdef __EMSCRIPTEN__
#include <cstdint>
namespace OpenXcom { namespace Calypso {
struct CalypsoHdRuntimeDiagnostics
{
 std::uint64_t surfaceAllocations = 0;
 std::uint64_t registeredVanillaBlits = 0;
 std::uint64_t renderedFrames = 0;
 std::uint64_t lastDrawCount = 0;
 std::uint64_t routeFailures = 0;
 bool textAuditEnabled = false;
 int harnessFault = 0;
};
inline CalypsoHdRuntimeDiagnostics& calypsoHdRuntimeDiagnostics()
{ static CalypsoHdRuntimeDiagnostics value; return value; }
} }
#endif
