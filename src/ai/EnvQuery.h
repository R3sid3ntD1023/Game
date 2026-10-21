#pragma once

#include "AIContext.h"
#include <functional>
#include <glm/glm.hpp>
#include <vector>

namespace ai
{
	struct EnvQueryItem
	{
		glm::vec3 location{0.0f};
		void *data = nullptr; // whatever the generator attached (e.g. the plant this location belongs to)
		float score = 0.0f;
	};

	// Environment query in the spirit of UE's EQS: a generator proposes candidate items, filters discard the
	// unsuitable ones, weighted scorers (each 0..1) rank the rest, and Run() picks a winner.
	class EnvQuery
	{
	public:
		using Generator = std::function<void(AIContext &, std::vector<EnvQueryItem> &)>;
		using Filter = std::function<bool(AIContext &, const EnvQueryItem &)>;
		using Scorer = std::function<float(AIContext &, const EnvQueryItem &)>;

		void SetGenerator(Generator generator) { m_generator = std::move(generator); }
		void AddFilter(Filter filter) { m_filters.push_back(std::move(filter)); }
		void AddScorer(Scorer scorer, float weight = 1.0f) { m_scorers.push_back({std::move(scorer), weight}); }

		// Items scoring within this distance of the best are treated as equally good; one is picked at random.
		float tolerance = 0.08f;

		// Returns false when no item survives the filters.
		bool Run(AIContext &ctx, EnvQueryItem &out) const;

	private:
		struct WeightedScorer
		{
			Scorer scorer;
			float weight;
		};

		Generator m_generator;
		std::vector<Filter> m_filters;
		std::vector<WeightedScorer> m_scorers;
	};
} // namespace ai
