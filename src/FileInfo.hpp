#pragma once

#include "Config.hpp"
#include "Glob.hpp"
#include "utils/file.hpp"
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

struct FileInfo
{
	FileInfo(const fs::path& importerPath, const Config& config) :
		importerPath(&importerPath), allowedToList(config.getAllowedToList(importerPath)),
		groupGlob(Glob::getGlobThatMatchesSegmentList(utils::file::toSegmentList(importerPath), config.groupGlobList)),
		isSpecified(config.allowedIncludeIndexMap.empty() || allowedToList != nullptr)
	{
	}

	const fs::path* importerPath; // cannot be nullptr

	const std::vector<Glob>* allowedToList; // can be nullptr
	const Glob* groupGlob;					// can be nullptr

	bool isSpecified;
};
