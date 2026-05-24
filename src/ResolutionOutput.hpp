#pragma once

#include <filesystem>
#include <map>
#include <set>

struct ResolutionDetectedIncludes
{
	std::set<std::filesystem::path> allowedSet;
	std::set<std::filesystem::path> forbiddenSet;
	std::set<std::filesystem::path> unresolvedSet;
};

// same as ResolutionDetectedIncludes, but since origin is unspecified, resolved includes are allowed
using ResolutionUnspecifiedDetectedIncludes = ResolutionDetectedIncludes;

struct ResolutionOutput
{
	// specified origin -> ResolutionDetectedIncludes
	std::map<std::filesystem::path, ResolutionDetectedIncludes> specifiedIncludesMap;
	// unspecified origin -> ResolutionUnspecifiedDetectedIncludes
	std::map<std::filesystem::path, ResolutionUnspecifiedDetectedIncludes> unspecifiedIncludesMap;

	[[nodiscard]] auto& getIncludes(bool isSpecified, const std::filesystem::path& path)
	{
		return isSpecified ? specifiedIncludesMap[path] : unspecifiedIncludesMap[path];
	}
};
