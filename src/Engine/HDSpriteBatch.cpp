/*
 * Phase 6b — HD-only sprite batch implementation.
 */
#ifdef __EMSCRIPTEN__

#include "HDSpriteBatch.h"
#include "HDQueue.h"
#include <algorithm>

namespace OpenXcom
{
namespace HDSpriteBatch
{

std::vector<Entry> &get()
{
	static std::vector<Entry> batch;
	return batch;
}

void push(Entry e)
{
	get().push_back(e);
}

void sortAndFlushIntoQueue()
{
	auto &batch = get();
	std::stable_sort(batch.begin(), batch.end(),
		[](const Entry &a, const Entry &b) { return a.depth < b.depth; });
	for (auto &e : batch)
	{
		HDQueue::push(e.src, e.dst);
	}
	batch.clear();
}

} /* namespace HDSpriteBatch */
} /* namespace OpenXcom */

#endif /* __EMSCRIPTEN__ */
