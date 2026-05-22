#pragma once

#include <algorithm>
#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

namespace utils::segment_list
{
	inline size_t commonLength(const std::vector<std::string>& segmentListA, const std::vector<std::string>& segmentListB)
	{
		const size_t minLength = std::min(segmentListA.size(), segmentListB.size());
		for (size_t i = 0; i < minLength; ++i)
			if (segmentListA[i] != segmentListB[i]) return i;
		return minLength;
	}

	inline std::string toDotted(const std::vector<std::string>& segmentList, size_t length)
	{
		std::ostringstream oss;

		for (size_t i = 0; i < length; ++i)
		{
			const auto& segment = segmentList[i];
			if (segment.find('.') != std::string::npos) oss << '"' << segment << '"' << ".";
			else oss << segment << ".";
		}

		const std::string result = oss.str();
		return result.substr(0, result.size() - 1);
	}

} // namespace utils::segment_list
