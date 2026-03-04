#pragma once

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

	inline bool isCppFile(const fs::path& path)
	{
		static auto cppExtensionList = {".h", ".hpp", ".hxx", ".hh", ".h++", ".c", ".cc", ".cpp", ".cxx"};
		for (const auto* cppExtension : cppExtensionList)
			if (path.extension() == cppExtension) return true;
		return false;
	}

	inline std::string pathToDotted(const fs::path& p)
	{
		std::ostringstream oss;

		for (const auto& segment : p.lexically_normal().relative_path())
		{
			if (segment == ".") continue;
			const auto& segmentStr = segment.string();
			if (segmentStr.find('.') != std::string::npos) oss << '"' << segmentStr << '"' << ".";
			else
				oss << segmentStr << ".";
		}

		std::string result = oss.str();
		return result.substr(0, result.size() - 1);
	}

} // namespace utils::file
