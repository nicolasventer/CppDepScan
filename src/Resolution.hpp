#pragma once

#include "Config.hpp"
#include "FileInfo.hpp"
#include "Glob.hpp"
#include "ResolutionOutput.hpp"
#include "SourceToHeaderMap.hpp"
#include "utils/file.hpp"
#include <filesystem>
#include <string>
#include <vector>

class Resolution
{
public:
	static bool handleResolution(const std::string& detectedIncludeStr,
		const Config& config,
		const FileInfo& fileInfo,
		ResolutionDetectedIncludes& cppIncludes,
		SourceToHeaderMap& sourceToHeaderMap)
	{
		return handleCurrentFolderResolution(detectedIncludeStr, config, fileInfo, cppIncludes, sourceToHeaderMap)
			|| handleIncludePathListResolution(detectedIncludeStr, config, fileInfo, cppIncludes, sourceToHeaderMap)
			|| handleStdResolution(detectedIncludeStr, config, fileInfo, cppIncludes, sourceToHeaderMap)
			|| handleUnresolved(detectedIncludeStr, config, fileInfo, cppIncludes, sourceToHeaderMap);
	}

private:
	static void handleGroupSourceHeader(
		const fs::path& resolvedIncludePath, const FileInfo& fileInfo, SourceToHeaderMap& sourceToHeaderMap)
	{
		const auto& f = fileInfo; // shortcut

		if (!utils::file::isSourceFile(*f.cppPath)) return;
		// current file is a source file
		if (!utils::file::isHeaderFile(resolvedIncludePath)) return;
		// resolved include is a header file
		const auto cppFileName = f.cppPath->filename().replace_extension("").string();
		const auto resolvedIncludeFileName = resolvedIncludePath.filename().replace_extension("").string();
		if (resolvedIncludeFileName != cppFileName) return;
		// resolved include filename is the same as current file
		if (sourceToHeaderMap.count(*f.cppPath) != 0) return;
		// current file is not already mapped to a header file
		sourceToHeaderMap[*f.cppPath] = resolvedIncludePath;
	}

	static void handleResolvedInclude(const fs::path& resolvedIncludePath,
		const Config& config,
		const FileInfo& fileInfo,
		ResolutionDetectedIncludes& cppIncludes,
		SourceToHeaderMap& sourceToHeaderMap)
	{
		const auto& f = fileInfo; // shortcut

		handleGroupSourceHeader(resolvedIncludePath, f, sourceToHeaderMap);

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
		if (isAllowed) cppIncludes.allowedSet.insert(resolvedIncludePath);
		else cppIncludes.forbiddenSet.insert(resolvedIncludePath);
	}

	static bool handleCurrentFolderResolution(const std::string& detectedIncludeStr,
		const Config& config,
		const FileInfo& fileInfo,
		ResolutionDetectedIncludes& cppIncludes,
		SourceToHeaderMap& sourceToHeaderMap)
	{
		const auto& f = fileInfo; // shortcut

		const fs::path currentFolderIncludePath = f.cppPath->parent_path() / fs::path(detectedIncludeStr);

		if (!fs::exists(currentFolderIncludePath)) return false;
		// include resolved

		handleResolvedInclude(currentFolderIncludePath, config, fileInfo, cppIncludes, sourceToHeaderMap);
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
		ResolutionDetectedIncludes& cppIncludes,
		SourceToHeaderMap& sourceToHeaderMap)
	{
		const fs::path detectedIncludePath(detectedIncludeStr);

		const auto* resolutionIncludePath = getResolutionIncludePath(detectedIncludePath, config.resolutionIncludePathList);
		if (resolutionIncludePath == nullptr) return false;
		// include resolved

		const fs::path resolvedIncludePath = *resolutionIncludePath / detectedIncludePath;
		handleResolvedInclude(resolvedIncludePath, config, fileInfo, cppIncludes, sourceToHeaderMap);
		return true;
	}

	static bool handleStdResolution(const std::string& detectedIncludeStr,
		const Config& config,
		const FileInfo& /* fileInfo */,
		ResolutionDetectedIncludes& cppIncludes,
		SourceToHeaderMap& /* sourceToHeaderMap */)
	{
		for (const auto& stdIncludePath : config.stdResolutionIncludePathList)
		{
			const fs::path& newIncludePath = stdIncludePath / fs::path(detectedIncludeStr);
			if (!fs::exists(newIncludePath)) continue;
			// std include resolved

			if (!config.bKeepStdInOutput) return true;
			// keep std in output

			cppIncludes.allowedSet.insert(detectedIncludeStr);
			return true;
		}
		return false;
	}

	static bool handleUnresolved(const std::string& detectedIncludeStr,
		const Config& config,
		const FileInfo& fileInfo,
		ResolutionDetectedIncludes& cppIncludes,
		SourceToHeaderMap& /* sourceToHeaderMap */)
	{
		const auto& f = fileInfo; // shortcut

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

		cppIncludes.unresolvedSet.insert(currentFolderIncludePath);
		return false;
	}
};