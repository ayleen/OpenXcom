#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <numeric>
#include <utility>
#include <vector>

namespace OpenXcom
{

inline std::uint32_t calypsoVoiceNextRandom(std::uint32_t &state)
{
	state ^= state << 13;
	state ^= state >> 17;
	state ^= state << 5;
	return state;
}

/**
 * Weighted shuffle bag for one speaker/event pair.
 *
 * Every line is returned exactly once per cycle. Weights affect its position
 * inside the cycle, not its frequency, so a high-weight line cannot starve a
 * quieter alternative. Equal weights deliberately retain the G0.5 Fisher-
 * Yates sequence.
 */
class CalypsoVoiceLineBag
{
private:
	std::vector<std::size_t> _order;
	std::vector<int> _weights;
	std::size_t _cursor = 0;
	std::size_t _last = noLine();
	unsigned int _cycle = 0;

	static bool equalWeights(const std::vector<int> &weights)
	{
		return std::adjacent_find(weights.begin(), weights.end(),
			std::not_equal_to<int>()) == weights.end();
	}

	void refill(const std::vector<int> &weights, std::uint32_t seedBase)
	{
		_order.resize(weights.size());
		std::iota(_order.begin(), _order.end(), 0u);
		_cursor = 0;

		std::uint32_t state = seedBase ^ (++_cycle * 0x27d4eb2du);
		if (equalWeights(weights))
		{
			for (std::size_t i = _order.size() - 1; i > 0; --i)
			{
				const std::size_t j = calypsoVoiceNextRandom(state) % (i + 1);
				std::swap(_order[i], _order[j]);
			}
		}
		else
		{
			std::vector<std::size_t> remaining = _order;
			_order.clear();
			_order.reserve(weights.size());
			while (!remaining.empty())
			{
				std::uint64_t total = 0;
				for (std::size_t index : remaining)
				{
					total += static_cast<std::uint64_t>(weights[index]);
				}
				const std::uint64_t draw = calypsoVoiceNextRandom(state) % total;
				std::uint64_t cumulative = 0;
				std::size_t selected = 0;
				for (; selected + 1 < remaining.size(); ++selected)
				{
					cumulative += static_cast<std::uint64_t>(weights[remaining[selected]]);
					if (draw < cumulative)
					{
						break;
					}
				}
				_order.push_back(remaining[selected]);
				remaining.erase(remaining.begin() + selected);
			}
		}

		if (_last != noLine() && _order.size() > 1 && _order.front() == _last)
		{
			std::swap(_order[0], _order[1]);
		}
	}

public:
	static constexpr std::size_t noLine()
	{
		return std::numeric_limits<std::size_t>::max();
	}

	std::size_t next(const std::vector<int> &weights, std::uint32_t seedBase)
	{
		if (weights.empty()
			|| std::any_of(weights.begin(), weights.end(), [](int weight) { return weight <= 0; }))
		{
			return noLine();
		}
		if (_weights != weights)
		{
			_order.clear();
			_weights = weights;
			_cursor = 0;
			_last = noLine();
			_cycle = 0;
		}
		if (_cursor >= _order.size())
		{
			refill(weights, seedBase);
		}

		_last = _order[_cursor++];
		return _last;
	}
};

}
