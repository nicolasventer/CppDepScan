#pragma once

#include "Config.hpp"
#include "Glob.hpp"
#include "ResolutionOutput.hpp"
#include "SourceToHeaderMap.hpp"
#include "utils/file.hpp"
#include "utils/segment_list.hpp"
#include <cstdint>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

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
	Output(const Config& config, const ResolutionOutput& resolution, const SourceToHeaderMap& sourceToHeaderMap);

	// specified origin -> DetectedIncludes
	std::map<std::string, DetectedIncludes> specifiedIncludesMap;
	// unspecified origin -> UnspecifiedDetectedIncludes
	std::map<std::string, UnspecifiedDetectedIncludes> unspecifiedIncludesMap;

	std::vector<std::string> commandLine;

	bool hasUnresolved = false;
	bool hasForbidden = false;

	auto& getIncludes(bool isSpecified, const std::string& dottedPath)
	{
		return isSpecified ? specifiedIncludesMap[dottedPath] : unspecifiedIncludesMap[dottedPath];
	}
};

inline std::vector<std::string> getSegmentList(
	const fs::path& sourcePath, const Config& config, const SourceToHeaderMap& sourceToHeaderMap)
{
	if (config.bGroupSourceHeader)
	{
		auto sourceIt = sourceToHeaderMap.find(sourcePath);
		auto usedSourcePath = sourceIt != sourceToHeaderMap.end() ? sourceIt->second : sourcePath;
		const auto* sourceGroupGlob
			= Glob::getGlobThatMatchesSegmentList(utils::file::toSegmentList(usedSourcePath), config.groupGlobList);
		if (sourceGroupGlob != nullptr) return sourceGroupGlob->getSegmentList();
		const auto usedPathNoExtension = fs::path(usedSourcePath).replace_extension("");
		const auto usedPathSegmentList = utils::file::toSegmentList(usedPathNoExtension);
		return usedPathSegmentList;
	}
	const auto* sourceGroupGlob
		= Glob::getGlobThatMatchesSegmentList(utils::file::toSegmentList(sourcePath), config.groupGlobList);
	return sourceGroupGlob != nullptr ? sourceGroupGlob->getSegmentList() : utils::file::toSegmentList(sourcePath);
}

inline Output::Output(const Config& config, const ResolutionOutput& resolution, const SourceToHeaderMap& sourceToHeaderMap) :
	commandLine(config.commandLine)
{
	for (const auto& [sourcePath, detectedIncludes] : resolution.specifiedIncludesMap)
	{
		const auto usedSourceSegmentList = getSegmentList(sourcePath, config, sourceToHeaderMap);
		const auto usedSourceDottedPath = utils::segment_list::toDotted(usedSourceSegmentList, usedSourceSegmentList.size());
		auto& usedIncludes = this->getIncludes(true, usedSourceDottedPath);
		for (const auto& includePath : detectedIncludes.allowedSet)
		{
			const auto includeSegmentList = getSegmentList(includePath, config, sourceToHeaderMap);
			const auto includeDottedPath = utils::segment_list::toDotted(includeSegmentList, includeSegmentList.size());
			if (includeDottedPath == usedSourceDottedPath) continue;
			if (config.bBrotherLinks)
			{
				const auto commonLength = utils::segment_list::commonLength(usedSourceSegmentList, includeSegmentList);
				const auto sourceBrotherDottedPath = utils::segment_list::toDotted(usedSourceSegmentList, commonLength + 1);
				const auto includeBrotherDottedPath = utils::segment_list::toDotted(includeSegmentList, commonLength + 1);
				this->getIncludes(true, sourceBrotherDottedPath).allowedSet.insert(includeBrotherDottedPath);
			}
			else usedIncludes.allowedSet.insert(includeDottedPath);
		}
		const auto sourceDottedPath = utils::file::pathToDotted(sourcePath);
		for (const auto& includePath : detectedIncludes.forbiddenSet)
		{
			this->hasForbidden = true;
			auto& cppIncludes = this->getIncludes(true, sourceDottedPath);
			const auto includeDottedPath = utils::file::pathToDotted(includePath);
			cppIncludes.forbiddenSet.insert(includeDottedPath);
		}
		for (const auto& includePath : detectedIncludes.unresolvedSet)
		{
			this->hasUnresolved = true;
			auto& cppIncludes = this->getIncludes(true, sourceDottedPath);
			const auto includeDottedPath = utils::file::pathToDotted(includePath);
			cppIncludes.unresolvedSet.insert(includeDottedPath);
		}
	}
	for (const auto& [sourcePath, detectedIncludes] : resolution.unspecifiedIncludesMap)
	{
		const auto sourceDottedPath = utils::file::pathToDotted(sourcePath);
		auto& cppIncludes = this->getIncludes(false, sourceDottedPath);
		for (const auto& includePath : detectedIncludes.allowedSet)
		{
			const auto includeDottedPath = utils::file::pathToDotted(includePath);
			cppIncludes.allowedSet.insert(includeDottedPath);
		}
	}
}
