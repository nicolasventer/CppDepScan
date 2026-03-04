#pragma once

#include "../Glob.hpp"
#include <vector>

namespace utils::glob
{
	inline const Glob* getGlobThatMatchesSegmentList(
		const std::vector<std::string>& segmentList, const std::vector<Glob>& globList)
	{
		for (const auto& glob : globList)
			if (glob.bMatch(segmentList)) return &glob;
		return nullptr;
	}

} // namespace utils::glob