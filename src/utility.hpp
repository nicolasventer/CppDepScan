#pragma once

#include <array>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#define popen  _popen
#define pclose _pclose
#endif

namespace utility
{
	namespace fs = std::filesystem;

	inline bool bStartWith(std::string_view str, std::string_view prefix, std::string_view* strWithoutPrefix = nullptr)
	{
		if (str.size() < prefix.size()) return false;
		const bool result = str.substr(0, prefix.size()) == prefix;
		if (result && strWithoutPrefix) *strWithoutPrefix = str.substr(prefix.size());
		return result;
	}

	inline bool bEndWith(std::string_view str, std::string_view suffix, std::string_view* strWithoutSuffix = nullptr)
	{
		if (str.size() < suffix.size()) return false;
		auto offset = str.size() - suffix.size();
		const bool result = str.substr(offset) == suffix;
		if (result && strWithoutSuffix) *strWithoutSuffix = str.substr(0, offset);
		return result;
	}

	inline bool isCppFile(const std::string& filePath)
	{
		static auto cppExtensionList = {".h", ".hpp", ".hxx", ".hh", ".h++", ".c", ".cc", ".cpp", ".cxx"};
		for (const auto* cppExtension : cppExtensionList)
			if (bEndWith(filePath, cppExtension)) return true;
		return false;
	}

	inline std::string pathToDotted(const fs::path& p)
	{
		std::string result;

		for (const auto& segment : p.relative_path())
		{
			if (segment == ".") continue;
			const auto& segmentStr = segment.string();
			if (segmentStr.find('.') != std::string::npos) result += '"' + segmentStr + '"' + ".";
			else
				result += segmentStr + ".";
		}

		return result.substr(0, result.size() - 1);
	}

	inline bool isPathContained(const fs::path& path, const fs::path& container)
	{
		return bStartWith(path.lexically_normal().string(), container.lexically_normal().string());
	}

	inline const fs::path* getPatternThatIncludesPath(const fs::path& path, const std::vector<fs::path>& patternList)
	{
		for (const auto& pattern : patternList)
			if (isPathContained(path, pattern)) return &pattern;
		return nullptr;
	}

	inline void updateCppPathList(const fs::path& scanPath,
		std::vector<fs::path>& cppPathList,
		const std::vector<fs::path>& excludePathList,
		const std::vector<fs::path>& forceIncludePathList)
	{
		if (fs::is_regular_file(scanPath))
		{
			if (!isCppFile(scanPath.string())) return;
			const bool isForceIncluded = getPatternThatIncludesPath(scanPath, forceIncludePathList) != nullptr;
			const bool isExcluded = !isForceIncluded && getPatternThatIncludesPath(scanPath, excludePathList) != nullptr;
			if (isExcluded) return;
			cppPathList.push_back(scanPath);
		}
		else if (fs::is_directory(scanPath))
		{
			for (const auto& file : fs::directory_iterator(scanPath))
				updateCppPathList(file.path(), cppPathList, excludePathList, forceIncludePathList);
		}
	}

	inline std::vector<std::string> getStdIncludePathList()
	{
		std::vector<std::string> pathList;

		const std::string compiler = "g++";

		const fs::path tmp = fs::temp_directory_path() / "print_compiler_path.tmp";
		{
			std::ofstream f(tmp);
			f << '\n';
		}
		const std::string cmd = compiler + " -E -x c++ \"" + tmp.string() + "\" -v 2>&1";
		FILE* pipe = popen(cmd.c_str(), "r"); // NOLINT -- need shell for 2>&1
		fs::remove(tmp);

		if (!pipe)
		{
			std::cerr << "Failed to run: " << compiler << "\n";
			return pathList;
		}

		std::ostringstream raw;
		std::array<char, 256> buf{};
		while (fgets(buf.data(), static_cast<int>(buf.size()), pipe)) raw << buf.data();
		pclose(pipe);

		const std::string out = raw.str();
		const std::string startMarker = "#include <...> search starts here:";
		const std::string endMarker = "End of search list.";

		auto startPos = out.find(startMarker);
		if (startPos == std::string::npos)
		{
			std::cerr << "Could not find include search list in compiler output.\n";
			return pathList;
		}

		startPos += startMarker.size();
		auto endPos = out.find(endMarker, startPos);
		if (endPos == std::string::npos) endPos = out.size();

		const std::string block = out.substr(startPos, endPos - startPos);
		std::istringstream iss(block);
		std::string line;

		while (std::getline(iss, line))
		{
			// Paths are indented with spaces
			size_t i = 0;
			while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
			if (i < line.size())
			{
				const std::string path = line.substr(i);
				if (!path.empty()) pathList.push_back(fs::path(path).lexically_normal().string());
			}
		}

		return pathList;
	}
} // namespace utility
