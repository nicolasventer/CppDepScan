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

enum class EDetectedModuleStatus : uint8_t
{
	Allowed,
	Forbidden,
	Unresolved,
};

struct DetectedModules
{
	std::set<std::string> allowedSet;
	std::set<std::string> forbiddenSet;
	std::set<std::string> unresolvedSet;
};

// same as DetectedModules, but since importer is unspecified, resolved modules are allowed
using UnspecifiedDetectedModules = DetectedModules;

struct ImporterAllowInfo
{
	std::string fileName;
	std::string from;
	const std::vector<Glob>* toList = nullptr;
};

struct Output
{
	Output(const Config& config, const ResolutionOutput& resolution, const SourceToHeaderMap& sourceToHeaderMap);

	std::vector<std::string> commandLine;

	// specified importer -> DetectedModules
	std::map<std::string, DetectedModules> specifiedModulesMap;
	// unspecified importer -> UnspecifiedDetectedModules
	std::map<std::string, UnspecifiedDetectedModules> unspecifiedModulesMap;
	// importer with forbidden modules -> ImporterAllowInfo (importer's group and its allowed modules)
	std::map<std::string, ImporterAllowInfo> importerAllowInfoMap;

	bool hasUnresolved = false;
	bool hasForbidden = false;

	auto& getModules(bool isSpecified, const std::string& dottedPath)
	{
		return isSpecified ? specifiedModulesMap[dottedPath] : unspecifiedModulesMap[dottedPath];
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
	for (const auto& [importerPath, detectedModules] : resolution.specifiedImportersMap)
	{
		const auto usedSourceSegmentList = getSegmentList(importerPath, config, sourceToHeaderMap);
		const auto usedSourceDottedPath = utils::segment_list::toDotted(usedSourceSegmentList, usedSourceSegmentList.size());
		auto& usedModules = this->getModules(true, usedSourceDottedPath);
		for (const auto& modulePath : detectedModules.allowedSet)
		{
			const auto moduleSegmentList = getSegmentList(modulePath, config, sourceToHeaderMap);
			const auto moduleDottedPath = utils::segment_list::toDotted(moduleSegmentList, moduleSegmentList.size());
			if (moduleDottedPath == usedSourceDottedPath) continue;
			if (config.bBrotherLinks)
			{
				const auto commonLength = utils::segment_list::commonLength(usedSourceSegmentList, moduleSegmentList);
				const auto importerBrotherDottedPath = utils::segment_list::toDotted(usedSourceSegmentList, commonLength + 1);
				const auto moduleBrotherDottedPath = utils::segment_list::toDotted(moduleSegmentList, commonLength + 1);
				this->getModules(true, importerBrotherDottedPath).allowedSet.insert(moduleBrotherDottedPath);
			}
			else usedModules.allowedSet.insert(moduleDottedPath);
		}
		const auto importerDottedPath = utils::file::pathToDotted(importerPath);
		for (const auto& modulePath : detectedModules.forbiddenSet)
		{
			this->hasForbidden = true;
			auto& modules = this->getModules(true, importerDottedPath);
			const auto moduleDottedPath = utils::file::pathToDotted(modulePath);
			modules.forbiddenSet.insert(moduleDottedPath);
			auto it = this->importerAllowInfoMap.find(importerDottedPath);
			if (it == this->importerAllowInfoMap.end())
			{
				this->importerAllowInfoMap[importerDottedPath] = ImporterAllowInfo{.fileName = importerPath.filename().string(),
					.from = config.getAllowedToGlob(importerPath)->getPattern(),
					.toList = config.getAllowedToList(importerPath)};
			}
		}
		for (const auto& modulePath : detectedModules.unresolvedSet)
		{
			this->hasUnresolved = true;
			auto& modules = this->getModules(true, importerDottedPath);
			const auto moduleDottedPath = utils::file::pathToDotted(modulePath);
			modules.unresolvedSet.insert(moduleDottedPath);
		}
	}
	for (const auto& [importerPath, detectedModules] : resolution.unspecifiedImportersMap)
	{
		const auto importerDottedPath = utils::file::pathToDotted(importerPath);
		auto& modules = this->getModules(false, importerDottedPath);
		for (const auto& modulePath : detectedModules.allowedSet)
		{
			const auto moduleDottedPath = utils::file::pathToDotted(modulePath);
			modules.allowedSet.insert(moduleDottedPath);
		}
	}
}
