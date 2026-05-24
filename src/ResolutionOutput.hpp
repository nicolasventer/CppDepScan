#pragma once

#include <filesystem>
#include <map>
#include <set>

struct ResolutionDetectedModules
{
	std::set<std::filesystem::path> allowedSet;
	std::set<std::filesystem::path> forbiddenSet;
	std::set<std::filesystem::path> unresolvedSet;
};

// same as ResolutionDetectedModules, but since importer is unspecified, resolved modules are allowed
using ResolutionUnspecifiedDetectedModules = ResolutionDetectedModules;

struct ResolutionOutput
{
	// specified importer -> ResolutionDetectedModules
	std::map<std::filesystem::path, ResolutionDetectedModules> specifiedImportersMap;
	// unspecified importer -> ResolutionUnspecifiedDetectedModules
	std::map<std::filesystem::path, ResolutionUnspecifiedDetectedModules> unspecifiedImportersMap;

	[[nodiscard]] auto& getModules(bool isSpecified, const std::filesystem::path& path)
	{
		return isSpecified ? specifiedImportersMap[path] : unspecifiedImportersMap[path];
	}
};
