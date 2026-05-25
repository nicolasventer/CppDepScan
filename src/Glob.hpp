#pragma once

#include "utils/str.hpp"
#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

class Glob
{
public:
	explicit Glob(const std::string& pattern) : pattern(pattern)
	{
		size_t lastPos = 0;
		for (size_t i = 0; i < pattern.size(); ++i)
		{
			const bool isLast = i == pattern.size() - 1;
			if (pattern[i] == '/' || pattern[i] == '\\' || isLast)
			{
				auto& globSegmentList = bSuffix ? suffixGlobSegmentList : prefixGlobSegmentList;
				auto segment = pattern.substr(lastPos, i - lastPos + (isLast ? 1 : 0));
				globSegmentList.emplace_back(segment);
				segmentList.push_back(segment);
				lastPos = i + 1;
				if (!isLast && pattern[i + 1] == '\\') ++i;
			}
			else if (pattern[i] == '*' && !isLast && pattern[i + 1] == '*')
			{
				segmentList.emplace_back("\"**\"");
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

	[[nodiscard]] const std::string& getPattern() const { return pattern; }

	[[nodiscard]] bool bMatch(const std::vector<std::string>& candidateSegments) const
	{
		if (prefixGlobSegmentList.size() + suffixGlobSegmentList.size() > candidateSegments.size()) return false;
		if (!bSuffix && prefixGlobSegmentList.size() != candidateSegments.size()) return false;
		for (size_t i = 0; i < prefixGlobSegmentList.size(); ++i)
			if (!prefixGlobSegmentList[i].bMatch(candidateSegments[i])) return false;
		for (size_t i = 0; i < suffixGlobSegmentList.size(); ++i)
			if (!suffixGlobSegmentList[i].bMatch(candidateSegments[candidateSegments.size() - suffixGlobSegmentList.size() + i]))
				return false;
		return true;
	}

	[[nodiscard]] std::string toDotted() const
	{
		std::ostringstream oss;

		for (const auto& segment : prefixGlobSegmentList)
			if (!segment.isDot()) oss << segment.toDotted() << ".";
		oss << (bSuffix ? "\"**\"." : "");
		for (const auto& segment : suffixGlobSegmentList)
			if (!segment.isDot()) oss << segment.toDotted() << ".";

		std::string result = oss.str();
		result.pop_back();
		static constexpr std::string_view DOUBLE_STAR_SUFFIX = ".\"**\"";
		if (utils::str::bEndWith(result, DOUBLE_STAR_SUFFIX))
			result = result.substr(0, result.size() - DOUBLE_STAR_SUFFIX.size());
		return result;
	}

	[[nodiscard]] std::filesystem::path toRootPath() const
	{
		std::filesystem::path result;
		for (const auto& segment : prefixGlobSegmentList)
		{
			if (segment.getLiteral().empty()) break;
			result /= segment.getLiteral();
			if (!segment.isLiteral()) break;
		}
		return result;
	}

	[[nodiscard]] const std::vector<std::string>& getSegmentList() const { return segmentList; }

	friend bool operator<(const Glob& lhs, const Glob& rhs)
	{
		auto minSize = std::min(lhs.prefixGlobSegmentList.size(), rhs.prefixGlobSegmentList.size());
		for (size_t i = 0; i < minSize; ++i)
		{
			if (lhs.prefixGlobSegmentList[i] < rhs.prefixGlobSegmentList[i]) return true;
			if (rhs.prefixGlobSegmentList[i] < lhs.prefixGlobSegmentList[i]) return false;
		}
		return lhs.prefixGlobSegmentList.size() < rhs.prefixGlobSegmentList.size();
	}

	[[nodiscard]] static const Glob* getGlobThatMatchesSegmentList(
		const std::vector<std::string>& segmentList, const std::vector<Glob>& globList)
	{
		for (const auto& glob : globList)
			if (glob.bMatch(segmentList)) return &glob;
		return nullptr;
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

		[[nodiscard]] bool bMatch(const std::string& segment) const
		{
			std::string_view segmentWithoutPrefix;
			if (!utils::str::bStartWith(segment, prefix, &segmentWithoutPrefix)) return false;
			return utils::str::bEndWith(segmentWithoutPrefix, suffix);
		}

		[[nodiscard]] bool isDot() const { return suffix == "."; }

		[[nodiscard]] std::string toDotted() const
		{
			const bool hasSpecials
				= prefix.find_first_of(".@") != std::string::npos || suffix.find_first_of(".@") != std::string::npos;
			std::ostringstream oss;
			if (hasSpecials || bSuffix) oss << '"';
			oss << prefix << (bSuffix ? "*" : "") << suffix;
			if (hasSpecials || bSuffix) oss << '"';
			return oss.str();
		}

		[[nodiscard]] bool isLiteral() const { return !bSuffix; }

		[[nodiscard]] const std::string& getLiteral() const { return bSuffix ? prefix : suffix; }

		friend bool operator<(const GlobSegment& lhs, const GlobSegment& rhs)
		{
			return lhs.prefix < rhs.prefix || lhs.suffix < rhs.suffix;
		}

	private:
		std::string prefix;
		std::string suffix;
		bool bSuffix = false;
	};

	std::string pattern;
	std::vector<GlobSegment> prefixGlobSegmentList;
	std::vector<GlobSegment> suffixGlobSegmentList;
	std::vector<std::string> segmentList;
	bool bSuffix = false; // if false, the match should be exact
};
