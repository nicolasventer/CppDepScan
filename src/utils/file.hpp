#pragma once

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

namespace utils::file
{
	namespace fs = std::filesystem;

	inline std::vector<std::string> toSegmentList(const fs::path& path)
	{
		std::vector<std::string> segmentList;
		for (const auto& segment : path.lexically_normal().relative_path()) segmentList.push_back(segment.string());
		return segmentList;
	}

	inline bool isHeaderFile(const fs::path& path)
	{
		static constexpr auto HEADER_EXTENSION_LIST = {".h", ".hpp", ".hxx", ".hh"};
		for (const auto* headerExtension : HEADER_EXTENSION_LIST)
			if (path.extension() == headerExtension) return true;
		return false;
	}

	inline bool isSourceFile(const fs::path& path)
	{
		static constexpr auto SOURCE_EXTENSION_LIST = {".c", ".cc", ".cpp", ".cxx"};
		for (const auto* sourceExtension : SOURCE_EXTENSION_LIST)
			if (path.extension() == sourceExtension) return true;
		return false;
	}

	inline bool isCppFile(const fs::path& path) { return isSourceFile(path) || isHeaderFile(path); }

	inline std::string pathToDotted(const fs::path& p)
	{
		std::ostringstream oss;

		for (const auto& segment : p.lexically_normal().relative_path())
		{
			if (segment == ".") continue;
			const auto& segmentStr = segment.string();
			if (segmentStr.find('.') != std::string::npos) oss << '"' << segmentStr << '"' << ".";
			else oss << segmentStr << ".";
		}

		const std::string result = oss.str();
		return result.substr(0, result.size() - 1);
	}

	inline size_t commonLength(const std::vector<std::string>& segmentListA, const std::vector<std::string>& segmentListB)
	{
		const size_t minLength = std::min(segmentListA.size(), segmentListB.size());
		for (size_t i = 0; i < minLength; ++i)
			if (segmentListA[i] != segmentListB[i]) return i;
		return minLength;
	}

} // namespace utils::file
