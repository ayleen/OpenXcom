#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>

namespace OpenXcom
{
struct ProductionOutputSale
{
	int quantity = 0;
	int64_t unitPrice = 0;

	int64_t proceeds() const { return quantity * unitPrice; }

	static ProductionOutputSale quote(int produced, int stock, int demand, int64_t price, int64_t funds)
	{
		const int count = std::min({produced, stock, demand});
		const auto max = std::numeric_limits<int64_t>::max();
		if (count <= 0 || price <= 0 || price > max / count || funds > max - price * count)
			return {};
		return {count, price};
	}

	bool canConfirm(const ProductionOutputSale &latest) const
	{
		return quantity > 0 && quantity == latest.quantity && unitPrice == latest.unitPrice;
	}
};
}
