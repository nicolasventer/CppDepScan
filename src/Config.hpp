#pragma once

#include "Glob.hpp"
#include "utils/compiler.hpp"
#include "utils/file.hpp"
#include "utils/str.hpp"
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct Config
{
	std::vector<Glob> scanGlobList;
	std::vector<Glob> excludeScanGlobList;
	std::vector<Glob> forceIncludeScanGlobList;
	std::vector<fs::path> resolutionIncludePathList;
	std::map<Glob, size_t> allowedIncludeIndexMap;			   // from -> index of allowedIncludeGlobListList
	std::vector<std::vector<Glob>> allowedIncludeGlobListList; // list of allowed include lists
	std::vector<fs::path> outputPathList;					   // empty implies stdout
	bool bKeepStdInOutput = false;							   // default: false
	bool bUseJsonForStdOutput = false;						   // default: D2 lang
	std::vector<Glob> groupGlobList;
	bool bHelp = false;

	// automatically filled
	std::vector<std::string> stdResolutionIncludePathList;

	[[nodiscard]] const std::vector<Glob>* getAllowedToList(const fs::path& cppPath) const
	{
		const auto segmentList = utils::file::toSegmentList(cppPath);
		for (const auto& [from, index] : allowedIncludeIndexMap)
			if (from.bMatch(segmentList)) return &allowedIncludeGlobListList[index];
		return nullptr;
	}

	bool parseArgs(int argc, char* argv[])
	{
		std::vector<std::string> argList(argc);
		for (int i = 0; i < argc; ++i) argList[i] = utils::str::removeQuotes(argv[i]);
		for (int i = 1; i < argc; ++i)
		{
			const std::string& a = argList[i];
			if (a == "-I" && i + 1 < argc)
			{
				std::string& path = argList[++i];
				if (path.back() == '/') path.pop_back();
				resolutionIncludePathList.emplace_back(path);
			}
			else if (a == "-e" && i + 1 < argc)
			{
				const std::string& path = argList[++i];
				// if (path.back() == '/') path.pop_back(); // trailing slash must be preserved
				if (path.front() == '!') forceIncludeScanGlobList.emplace_back(path.substr(1));
				else excludeScanGlobList.emplace_back(path);
			}
			else if ((a == "-A" || a == "--allowed") && i + 2 < argc)
			{
				const std::string& fromStr = argList[++i];
				const std::string& toStr = argList[++i];
				// if (!fromStr.empty() && fromStr.back() == '/') fromStr.pop_back(); // trailing slash must be preserved
				// if (!toStr.empty() && toStr.back() == '/') toStr.pop_back(); // trailing slash must be preserved
				const Glob fromGlob(fromStr);
				if (allowedIncludeIndexMap.find(fromGlob) == allowedIncludeIndexMap.end())
				{
					allowedIncludeIndexMap[fromGlob] = allowedIncludeGlobListList.size();
					allowedIncludeGlobListList.emplace_back();
				}
				allowedIncludeGlobListList[allowedIncludeIndexMap[fromGlob]].emplace_back(toStr);
			}
			else if (a == "-o" && i + 1 < argc) outputPathList.emplace_back(argList[++i]);
			else if (a == "--std") bKeepStdInOutput = true;
			else if (a == "--json") bUseJsonForStdOutput = true;
			else if ((a == "-g" || a == "--group") && i + 1 < argc)
			{
				const std::string& path = argList[++i];
				// if (path.back() == '/') path.pop_back(); // trailing slash must be preserved
				groupGlobList.emplace_back(path);
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
			else scanGlobList.emplace_back(a);
		}
		if (scanGlobList.empty())
		{
			std::cerr << "Error: no scan paths provided\n";
			return false;
		}
		stdResolutionIncludePathList = utils::compiler::getStdIncludePathList();
		return true;
	}

	[[nodiscard]] std::vector<fs::path> getCppPathList() const
	{
		std::vector<fs::path> cppPathList;
		for (const auto& scanGlob : scanGlobList)
		{
			auto rootPath = scanGlob.toRootPath();
			auto scanPathSegmentList = utils::file::toSegmentList(rootPath);
			updateCppPathList(scanGlob, rootPath, scanPathSegmentList, cppPathList);
		}
		return cppPathList;
	}

private:
	void updateCppPathList(const Glob& scanGlob,
		const fs::path& scanPath,
		std::vector<std::string>& scanPathSegmentList,
		std::vector<fs::path>& cppPathList) const
	{
		if (fs::is_regular_file(scanPath))
		{
			if (!utils::file::isCppFile(scanPath)) return;
			if (!scanGlob.bMatch(scanPathSegmentList)) return;
			const bool isForceIncluded
				= Glob::getGlobThatMatchesSegmentList(scanPathSegmentList, forceIncludeScanGlobList) != nullptr;
			const bool isExcluded
				= !isForceIncluded && Glob::getGlobThatMatchesSegmentList(scanPathSegmentList, excludeScanGlobList) != nullptr;
			if (isExcluded) return;
			cppPathList.push_back(scanPath);
		}
		else if (fs::is_directory(scanPath))
		{
			for (const auto& file : fs::directory_iterator(scanPath))
			{
				scanPathSegmentList.emplace_back(file.path().filename().string());
				updateCppPathList(scanGlob, file.path(), scanPathSegmentList, cppPathList);
				scanPathSegmentList.pop_back();
			}
		}
	}
};