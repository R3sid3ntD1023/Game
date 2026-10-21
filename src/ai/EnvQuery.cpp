#include "EnvQuery.h"
#include <algorithm>
#include <random>

namespace ai
{
	bool EnvQuery::Run(AIContext &ctx, EnvQueryItem &out) const
	{
		if (!m_generator)
			return false;

		std::vector<EnvQueryItem> items;
		m_generator(ctx, items);

		std::erase_if(items, [&](const EnvQueryItem &item) { return !std::ranges::all_of(m_filters, [&](const Filter &f) { return f(ctx, item); }); });
		if (items.empty())
			return false;

		float totalWeight = 0.0f;
		for (const auto &s : m_scorers)
			totalWeight += s.weight;

		float best = -1.0f;
		for (auto &item : items)
		{
			float sum = 0.0f;
			for (const auto &s : m_scorers)
				sum += s.weight * std::clamp(s.scorer(ctx, item), 0.0f, 1.0f);
			item.score = totalWeight > 0.0f ? sum / totalWeight : 0.0f;
			best = std::max(best, item.score);
		}

		std::erase_if(items, [&](const EnvQueryItem &item) { return item.score < best - tolerance; });

		static thread_local std::mt19937 rng{std::random_device{}()};
		out = items[std::uniform_int_distribution<size_t>(0, items.size() - 1)(rng)];
		return true;
	}
} // namespace ai
