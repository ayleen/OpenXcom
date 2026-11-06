#pragma once

#ifdef __EMSCRIPTEN__
#include <functional>
#include <vector>

namespace OpenXcom
{
class Base;
class Game;
class RuleResearch;
class RuleManufacture;
class Production;
namespace Calypso
{
/// Harness-owned fixture result. Factories allocate a base but must not add it
/// to SavedGame; the host owns registration, teardown and option restoration.
struct RdAcceptanceFixture
{
	Base* base = nullptr;
	// Global F14 fixtures may need more than one native base. The harness must
	// register and track every entry before constructing the target state.
	std::vector<Base*> additionalBases;
	RuleResearch* research = nullptr;
	RuleManufacture* manufacture = nullptr;
	Production* production = nullptr;
	std::function<void()> restoreOptions;
};
}
}
#endif
