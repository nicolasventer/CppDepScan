#pragma once

#include "utility.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;

enum class EDetectedIncludeStatus : uint8_t
{
	Allowed,
	Forbidden,
	Unresolved,
};

struct DetectedIncludes
{
	std::set<std::string> allowedSet;
	std::set<std::string> forbiddenSet;
	std::set<std::string> unresolvedSet;
};

// same as DetectedIncludes, but since origin is unspecified, resolved includes are allowed
using UnspecifiedDetectedIncludes = DetectedIncludes;

struct Output
{
	// specified origin -> DetectedIncludes
	std::map<std::string, DetectedIncludes> specifiedIncludesMap;
	// unspecified origin -> UnspecifiedDetectedIncludes
	std::map<std::string, UnspecifiedDetectedIncludes> unspecifiedIncludesMap;

	auto& getIncludes(bool isSpecified, const std::string& dottedPath)
	{
		return isSpecified ? specifiedIncludesMap[dottedPath] : unspecifiedIncludesMap[dottedPath];
	}
};

struct Config
{
	std::vector<fs::path> scanPathList;
	std::vector<fs::path> excludeScanPathList;
	std::vector<fs::path> forceIncludeScanPathList;
	std::vector<fs::path> resolutionIncludePathList;
	std::map<fs::path, size_t> allowedIncludeIndexMap;		   // from -> index of allowedIncludeListList
	std::vector<std::vector<fs::path>> allowedIncludeListList; // list of allowed include lists
	std::vector<fs::path> outputPathList;					   // empty implies stdout
	bool bKeepStdInOutput = false;							   // default: false
	bool bUseJsonForStdOutput = false;						   // default: D2 lang
	std::vector<fs::path> groupPathList;
	bool bHelp = false;

	// automatically filled
	std::vector<std::string> stdResolutionIncludePathList;

	const std::vector<fs::path>* getAllowedToList(const fs::path& cppPath) const
	{
		for (const auto& [from, index] : allowedIncludeIndexMap)
			if (utility::isPathContained(cppPath, from)) return &allowedIncludeListList[index];
		return nullptr;
	}

	inline bool parseArgs(int argc, char** argv);
};

struct FileInfo
{
	FileInfo(const fs::path& cppPath, const Config& config) :
		cppPath(&cppPath), allowedToList(config.getAllowedToList(cppPath)),
		groupPath(utility::getPatternThatIncludesPath(cppPath, config.groupPathList)),
		cppDottedPath(utility::pathToDotted(cppPath)),
		isSpecified(config.allowedIncludeIndexMap.empty() || allowedToList != nullptr)
	{
	}

	const fs::path* cppPath; // cannot be nullptr

	const std::vector<fs::path>* allowedToList; // can be nullptr
	const fs::path* groupPath;					// can be nullptr

	std::string cppDottedPath;
	bool isSpecified;
};

inline bool Config::parseArgs(int argc, char** argv)
{
	for (int i = 1; i < argc; ++i)
	{
		std::string a = argv[i];
		if (a == "-I" && i + 1 < argc)
		{
			std::string path = argv[++i];
			if (path.back() == '/') path.pop_back();
			resolutionIncludePathList.emplace_back(path);
		}
		else if (a == "-e" && i + 1 < argc)
		{
			std::string path = argv[++i];
			if (path.back() == '/') path.pop_back();
			if (path.front() == '!') forceIncludeScanPathList.emplace_back(path.substr(1));
			else
				excludeScanPathList.emplace_back(path);
		}
		else if ((a == "-A" || a == "--allowed") && i + 2 < argc)
		{
			std::string fromStr = argv[++i];
			std::string toStr = argv[++i];
			if (!fromStr.empty() && fromStr.back() == '/') fromStr.pop_back();
			if (!toStr.empty() && toStr.back() == '/') toStr.pop_back();
			const fs::path fromPath(fromStr);
			if (allowedIncludeIndexMap.find(fromPath) == allowedIncludeIndexMap.end())
			{
				allowedIncludeIndexMap[fromPath] = allowedIncludeListList.size();
				allowedIncludeListList.emplace_back();
			}
			allowedIncludeListList[allowedIncludeIndexMap[fromPath]].emplace_back(toStr);
		}
		else if (a == "-o" && i + 1 < argc)
			outputPathList.emplace_back(argv[++i]);
		else if (a == "--std")
			bKeepStdInOutput = true;
		else if (a == "--json")
			bUseJsonForStdOutput = true;
		else if ((a == "-g" || a == "--group") && i + 1 < argc)
		{
			std::string path = argv[++i];
			if (path.back() == '/') path.pop_back();
			groupPathList.emplace_back(path);
		}
		else if (a == "-h" || a == "--help")
		{
			bHelp = true;
			return false;
		}
		else if (!a.empty() && a[0] == '-')
		{
			std::cerr << "Error with option: " << a << "\n";
			return false;
		}
		else
			scanPathList.emplace_back(a);
	}
	if (scanPathList.empty())
	{
		std::cerr << "Error: no scan paths provided\n";
		return false;
	}
	stdResolutionIncludePathList = utility::getStdIncludePathList();
	return true;
}
