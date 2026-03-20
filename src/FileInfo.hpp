#pragma once

#include "Config.hpp"
#include "Glob.hpp"
#include "utils/file.hpp"
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct FileInfo
{
	FileInfo(const fs::path& cppPath, const Config& config) :
		cppPath(&cppPath), allowedToList(config.getAllowedToList(cppPath)),
		groupGlob(Glob::getGlobThatMatchesSegmentList(utils::file::toSegmentList(cppPath), config.groupGlobList)),
		cppDottedPath(utils::file::pathToDotted(cppPath)),
		isSpecified(config.allowedIncludeIndexMap.empty() || allowedToList != nullptr)
	{
	}

	const fs::path* cppPath; // cannot be nullptr

	const std::vector<Glob>* allowedToList; // can be nullptr
	const Glob* groupGlob;					// can be nullptr

	std::string cppDottedPath;
	bool isSpecified;
};
