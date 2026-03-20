#pragma once

#include "Config.hpp"
#include "FileInfo.hpp"
#include "Output.hpp"
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
		const auto& f = fileInfo; // shortcut

		auto resolvedIncludeSegmentList = utils::file::toSegmentList(resolvedIncludePath);

		const bool isForceIncluded
			= Glob::getGlobThatMatchesSegmentList(resolvedIncludeSegmentList, config.forceIncludeScanGlobList) != nullptr;
		const bool isExcluded
			= !isForceIncluded
		   && Glob::getGlobThatMatchesSegmentList(resolvedIncludeSegmentList, config.excludeScanGlobList) != nullptr;
		if (isExcluded) return;
		// resolution not excluded

		const bool isAllowed = f.allowedToList == nullptr
							|| Glob::getGlobThatMatchesSegmentList(resolvedIncludeSegmentList, *f.allowedToList) != nullptr;
		if (isAllowed)
		{
			// resolution allowed
			const auto* includeGroupGlob = Glob::getGlobThatMatchesSegmentList(resolvedIncludeSegmentList, config.groupGlobList);
			if (includeGroupGlob == f.groupGlob && includeGroupGlob != nullptr) return;
			// resolution group different from file group

			auto dottedPath
				= includeGroupGlob != nullptr ? includeGroupGlob->toDotted() : utils::file::pathToDotted(resolvedIncludePath);
			detectedIncludes.allowedSet.insert(dottedPath);
		}
		else
		{
			// resolution forbidden
			auto& cppInclude = result.getIncludes(f.isSpecified, f.cppDottedPath);
			cppInclude.forbiddenSet.insert(utils::file::pathToDotted(resolvedIncludePath));
		}
	}

	static bool handleCurrentFolderResolution(const std::string& detectedIncludeStr,
		const Config& config,
		const FileInfo& fileInfo,
		Output& result,
		DetectedIncludes& detectedIncludes)
	{
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
		const fs::path detectedIncludePath(detectedIncludeStr);

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
		const auto& f = fileInfo; // shortcut

		auto& cppInclude = result.getIncludes(f.isSpecified, f.cppDottedPath);

		// current folder used for better display
		const fs::path currentFolderIncludePath = f.cppPath->parent_path() / fs::path(detectedIncludeStr);

		auto currentFolderSegmentList = utils::file::toSegmentList(currentFolderIncludePath);

		const bool isForceIncluded
			= Glob::getGlobThatMatchesSegmentList(currentFolderSegmentList, config.forceIncludeScanGlobList) != nullptr;
		const bool isExcluded
			= !isForceIncluded
		   && Glob::getGlobThatMatchesSegmentList(currentFolderSegmentList, config.excludeScanGlobList) != nullptr;
		if (isExcluded) return false;
		// unresolved not excluded

		cppInclude.unresolvedSet.insert(utils::file::pathToDotted(currentFolderIncludePath));
		return false;
	}
};