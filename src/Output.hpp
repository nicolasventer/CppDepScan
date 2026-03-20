#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <string>

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

	bool hasUnresolved = false;

	auto& getIncludes(bool isSpecified, const std::string& dottedPath)
	{
		return isSpecified ? specifiedIncludesMap[dottedPath] : unspecifiedIncludesMap[dottedPath];
	}
};
