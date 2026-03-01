#pragma once

#include "types.hpp"
#include "utility.hpp"
#include <filesystem>
#include <string>
#include <vector>

class Resolution
{
public:
	static bool handleResolution(const std::string& detectedIncludeStr,
		const Config& config,
		const FileInfo& fileInfo,
		Output& result,
		DetectedIncludes& detectedIncludes)
	{
		return handleCurrentFolderResolution(detectedIncludeStr, config, fileInfo, result, detectedIncludes)
			   || handleIncludePathListResolution(detectedIncludeStr, config, fileInfo, result, detectedIncludes)
			   || handleStdResolution(detectedIncludeStr, config, fileInfo, result, detectedIncludes)
			   || handleUnresolved(detectedIncludeStr, config, fileInfo, result, detectedIncludes);
	}

private:
	static void handleResolvedInclude(const fs::path& resolvedIncludePath,
		const Config& config,
		const FileInfo& fileInfo,
		Output& result,
		DetectedIncludes& detectedIncludes)
	{
		using namespace utility;
		const auto& f = fileInfo; // shortcut

		const bool isForceIncluded = getPatternThatIncludesPath(resolvedIncludePath, config.forceIncludeScanPathList) != nullptr;
		const bool isExcluded
			= !isForceIncluded && getPatternThatIncludesPath(resolvedIncludePath, config.excludeScanPathList) != nullptr;
		if (isExcluded) return;
		// resolution not excluded

		const bool isAllowed
			= f.allowedToList == nullptr || getPatternThatIncludesPath(resolvedIncludePath, *f.allowedToList) != nullptr;
		if (isAllowed)
		{
			// resolution allowed
			const auto* includeGroupPath = getPatternThatIncludesPath(resolvedIncludePath, config.groupPathList);
			if (includeGroupPath == f.groupPath && includeGroupPath != nullptr) return;
			// resolution group different from file group

			detectedIncludes.allowedSet.insert(
				pathToDotted(includeGroupPath != nullptr ? *includeGroupPath : resolvedIncludePath));
		}
		else
		{
			// resolution forbidden
			auto& cppInclude = result.getIncludes(f.isSpecified, f.cppDottedPath);
			cppInclude.forbiddenSet.insert(pathToDotted(resolvedIncludePath));
		}
	}

	static bool handleCurrentFolderResolution(const std::string& detectedIncludeStr,
		const Config& config,
		const FileInfo& fileInfo,
		Output& result,
		DetectedIncludes& detectedIncludes)
	{
		using namespace utility;
		const auto& f = fileInfo; // shortcut

		const fs::path currentFolderIncludePath = f.cppPath->parent_path() / fs::path(detectedIncludeStr);

		if (!fs::exists(currentFolderIncludePath)) return false;
		// include resolved

		handleResolvedInclude(currentFolderIncludePath, config, fileInfo, result, detectedIncludes);
		return true;
	}

	static const fs::path* getResolutionIncludePath(
		const fs::path& detectedIncludePath, const std::vector<fs::path>& resolutionIncludePathList)
	{
		for (const auto& resolutionIncludePath : resolutionIncludePathList)
		{
			const fs::path& newIncludePath = resolutionIncludePath / detectedIncludePath;
			if (fs::exists(newIncludePath)) return &resolutionIncludePath;
		}
		return nullptr;
	}

	// returns true if the include is resolved
	static bool handleIncludePathListResolution(const std::string& detectedIncludeStr,
		const Config& config,
		const FileInfo& fileInfo,
		Output& result,
		DetectedIncludes& detectedIncludes)
	{
		using namespace utility;

		const fs::path detectedIncludePath = fs::path(detectedIncludeStr);

		const auto* resolutionIncludePath = getResolutionIncludePath(detectedIncludePath, config.resolutionIncludePathList);
		if (resolutionIncludePath == nullptr) return false;
		// include resolved

		const fs::path resolvedIncludePath = *resolutionIncludePath / detectedIncludePath;
		handleResolvedInclude(resolvedIncludePath, config, fileInfo, result, detectedIncludes);
		return true;
	}

	static bool handleStdResolution(const std::string& detectedIncludeStr,
		const Config& config,
		const FileInfo& /* fileInfo */,
		Output& /* result */,
		DetectedIncludes& detectedIncludes)
	{
		for (const auto& stdIncludePath : config.stdResolutionIncludePathList)
		{
			const fs::path& newIncludePath = stdIncludePath / fs::path(detectedIncludeStr);
			if (!fs::exists(newIncludePath)) continue;
			// std include resolved

			if (!config.bKeepStdInOutput) return true;
			// keep std in output

			detectedIncludes.allowedSet.insert(detectedIncludeStr);
			return true;
		}
		return false;
	}

	static bool handleUnresolved(const std::string& detectedIncludeStr,
		const Config& config,
		const FileInfo& fileInfo,
		Output& result,
		DetectedIncludes& /* detectedIncludes */)
	{
		using namespace utility;
		const auto& f = fileInfo; // shortcut

		auto& cppInclude = result.getIncludes(f.isSpecified, f.cppDottedPath);

		// current folder used for better display
		const fs::path currentFolderIncludePath = f.cppPath->parent_path() / fs::path(detectedIncludeStr);

		const bool isForceIncluded
			= getPatternThatIncludesPath(currentFolderIncludePath, config.forceIncludeScanPathList) != nullptr;
		const bool isExcluded
			= !isForceIncluded && getPatternThatIncludesPath(currentFolderIncludePath, config.excludeScanPathList) != nullptr;
		if (isExcluded) return false;
		// unresolved not excluded

		cppInclude.unresolvedSet.insert(pathToDotted(currentFolderIncludePath));
		return false;
	}
};