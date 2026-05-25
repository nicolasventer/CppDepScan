#pragma once

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
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

	inline std::string pathToDotted(const fs::path& p)
	{
		std::ostringstream oss;

		for (const auto& segment : p.lexically_normal().relative_path())
		{
			if (segment == ".") continue;
			const auto& segmentStr = segment.string();
			if (segmentStr.find_first_of(".@") != std::string::npos) oss << '"' << segmentStr << '"' << ".";
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

	inline bool tryReadFile(const fs::path& path, std::string& content)
	{
		std::ifstream file(path, std::ios::binary);
		if (!file) return false;

		file.seekg(0, std::ios::end);
		const auto size = file.tellg();
		file.seekg(0, std::ios::beg);

		content.resize(static_cast<size_t>(size));
		file.read(content.data(), static_cast<std::streamsize>(size));
		return true;
	}

} // namespace utils::file
