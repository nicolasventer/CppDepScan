#pragma once

#include "utils/str.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

class Glob
{
public:
	explicit Glob(const std::string& pattern)
	{
		size_t lastPos = 0;
		for (size_t i = 0; i < pattern.size(); ++i)
		{
			bool isLast = i == pattern.size() - 1;
			if (pattern[i] == '/' || pattern[i] == '\\' || isLast)
			{
				auto& segmentList = bSuffix ? suffixSegmentList : prefixSegmentList;
				segmentList.emplace_back(pattern.substr(lastPos, i - lastPos + (isLast ? 1 : 0)));
				lastPos = i + 1;
				if (!isLast && pattern[i + 1] == '\\') ++i;
			}
			else if (pattern[i] == '*' && !isLast && pattern[i + 1] == '*')
			{
				if (bSuffix)
					throw std::runtime_error("Invalid glob pattern: '" + pattern + "'. Multiple '**' wildcards are not allowed.");
				++i;
				bSuffix = true;
				if (i + 1 < pattern.size())
				{
					if (pattern[i + 1] != '/' && pattern[i + 1] != '\\')
						throw std::runtime_error("Invalid glob pattern: '" + pattern
												 + "'. '**' wildcard must be followed by '/' or '\\' or be the last character.");
					++i;
					lastPos = i + 1;
				}
			}
		}
		bSuffix |= pattern.back() != '*'; // make the match not exact if the last character is not '*'
	}

	bool bMatch(const std::vector<std::string>& segmentList) const
	{
		if (prefixSegmentList.size() + suffixSegmentList.size() > segmentList.size()) return false;
		if (!bSuffix && prefixSegmentList.size() != segmentList.size()) return false;
		for (size_t i = 0; i < prefixSegmentList.size(); ++i)
			if (!prefixSegmentList[i].bMatch(segmentList[i])) return false;
		for (size_t i = 0; i < suffixSegmentList.size(); ++i)
			if (!suffixSegmentList[i].bMatch(segmentList[segmentList.size() - suffixSegmentList.size() + i])) return false;
		return true;
	}

	std::string toDotted() const
	{
		std::ostringstream oss;

		for (const auto& segment : prefixSegmentList)
			if (!segment.isDot()) oss << segment.toDotted() << ".";
		oss << (bSuffix ? "\"**\"." : "");
		for (const auto& segment : suffixSegmentList)
			if (!segment.isDot()) oss << segment.toDotted() << ".";

		std::string result = oss.str();
		result.pop_back();
		if (utils::str::bEndWith(result, ".\"**\"")) result = result.substr(0, result.size() - 5);
		return result;
	}

	friend bool operator<(const Glob& lhs, const Glob& rhs)
	{
		auto minSize = std::min(lhs.prefixSegmentList.size(), rhs.prefixSegmentList.size());
		for (size_t i = 0; i < minSize; ++i)
		{
			if (lhs.prefixSegmentList[i] < rhs.prefixSegmentList[i]) return true;
			if (rhs.prefixSegmentList[i] < lhs.prefixSegmentList[i]) return false;
		}
		return lhs.prefixSegmentList.size() < rhs.prefixSegmentList.size();
	}

private:
	class GlobSegment
	{
	public:
		explicit GlobSegment(const std::string& segment)
		{
			size_t lastPos = 0;
			for (size_t i = 0; i < segment.size(); ++i)
			{
				if (segment[i] == '*')
				{
					if (bSuffix)
						throw std::runtime_error(
							"Invalid glob pattern: '" + segment + "'. Multiple '*' wildcards are not allowed.");
					bSuffix = true;
					prefix = segment.substr(lastPos, i - lastPos);
					lastPos = i + 1;
				}
			}
			suffix = segment.substr(lastPos);
		}

		bool bMatch(const std::string& segment) const
		{
			std::string_view segmentWithoutPrefix;
			if (!utils::str::bStartWith(segment, prefix, &segmentWithoutPrefix)) return false;
			return utils::str::bEndWith(segmentWithoutPrefix, suffix);
		}

		bool isDot() const { return suffix == "."; }

		std::string toDotted() const
		{
			bool hasDot = prefix.find('.') != std::string::npos || suffix.find('.') != std::string::npos;
			std::ostringstream oss;
			if (hasDot || bSuffix) oss << '"';
			oss << prefix << (bSuffix ? "*" : "") << suffix;
			if (hasDot || bSuffix) oss << '"';
			return oss.str();
		}

		friend bool operator<(const GlobSegment& lhs, const GlobSegment& rhs)
		{
			return lhs.prefix < rhs.prefix || lhs.suffix < rhs.suffix;
		}

	private:
		std::string prefix;
		std::string suffix;
		bool bSuffix = false;
	};

	std::vector<GlobSegment> prefixSegmentList;
	std::vector<GlobSegment> suffixSegmentList;
	bool bSuffix = false; // if false, the match should be exact
};
