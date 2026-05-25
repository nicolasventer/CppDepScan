#pragma once

#include "Glob.hpp"
#include "ILanguage.hpp"
#include "LanguageFactory.hpp"
#include "utils/file.hpp"
#include "utils/str.hpp"
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
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
	std::vector<Glob> groupGlobList;

	std::vector<std::string> commandLine;
	std::string language = "cpp";

	bool bKeepStdInOutput = false;	   // default: false
	bool bUseJsonForStdOutput = false; // default: D2 lang
	bool bBrotherLinks = false;		   // default: false
	bool bGroupSourceHeader = false;   // default: false
	bool bHelp = false;

	std::shared_ptr<ILanguage> languageImpl;

	[[nodiscard]] const std::vector<Glob>* getAllowedToList(const fs::path& importerPath) const
	{
		const auto segmentList = utils::file::toSegmentList(importerPath);
		for (const auto& [from, index] : allowedIncludeIndexMap)
			if (from.bMatch(segmentList)) return &allowedIncludeGlobListList[index];
		return nullptr;
	}

	[[nodiscard]] const Glob* getAllowedToGlob(const fs::path& importerPath) const
	{
		const auto segmentList = utils::file::toSegmentList(importerPath);
		for (const auto& [from, index] : allowedIncludeIndexMap)
			if (from.bMatch(segmentList)) return &from;
		return nullptr;
	}

	bool parseArgs(size_t argc, char* argv[])
	{
		commandLine.assign(argv, argv + argc);
		std::vector<std::string> argList(argc);
		for (size_t i = 0; i < argc; ++i) argList[i] = utils::str::toLower(utils::str::removeQuotes(argv[i]));
		for (size_t i = 1; i < argc; ++i)
		{
			const std::string& a = argList[i];
			if (a == "-i" && i + 1 < argc)
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
			else if ((a == "-a" || a == "--allowed") && i + 2 < argc)
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
			else if (a == "--brother-links") bBrotherLinks = true;
			else if (a == "--group-source-header") bGroupSourceHeader = true;
			else if ((a == "-g" || a == "--group") && i + 1 < argc)
			{
				const std::string& path = argList[++i];
				// if (path.back() == '/') path.pop_back(); // trailing slash must be preserved
				groupGlobList.emplace_back(path);
			}
			else if (a == "--lang" && i + 1 < argc)
			{
				language = argList[++i];
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
		languageImpl = createLanguage(language);
		if (languageImpl == nullptr)
		{
			std::cerr << "Unknown or unsupported language: " << language << "\n";
			return false;
		}
		languageImpl->initialize(scanGlobList);
		return true;
	}

	[[nodiscard]] std::vector<fs::path> getImporterPathList() const
	{
		std::vector<fs::path> importerPathList;
		for (const auto& scanGlob : scanGlobList)
		{
			auto rootPath = scanGlob.toRootPath();
			auto scanPathSegmentList = utils::file::toSegmentList(rootPath);
			updateImporterPathList(scanGlob, rootPath, scanPathSegmentList, importerPathList);
		}
		return importerPathList;
	}

private:
	void updateImporterPathList(const Glob& scanGlob,
		const fs::path& scanPath,
		std::vector<std::string>& scanPathSegmentList,
		std::vector<fs::path>& importerPathList) const
	{
		if (fs::is_regular_file(scanPath))
		{
			if (!languageImpl->isSourceFile(scanPath) && !languageImpl->isHeaderFile(scanPath)) return;
			if (!scanGlob.bMatch(scanPathSegmentList)) return;
			const bool isForceIncluded
				= Glob::getGlobThatMatchesSegmentList(scanPathSegmentList, forceIncludeScanGlobList) != nullptr;
			const bool isExcluded
				= !isForceIncluded && Glob::getGlobThatMatchesSegmentList(scanPathSegmentList, excludeScanGlobList) != nullptr;
			if (isExcluded) return;
			importerPathList.push_back(scanPath);
		}
		else if (fs::is_directory(scanPath))
		{
			for (const auto& file : fs::directory_iterator(scanPath))
			{
				scanPathSegmentList.emplace_back(file.path().filename().string());
				updateImporterPathList(scanGlob, file.path(), scanPathSegmentList, importerPathList);
				scanPathSegmentList.pop_back();
			}
		}
	}
};
