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
	static bool handleResolution(const std::string& detectedModuleStr,
		const Config& config,
		const FileInfo& fileInfo,
		ResolutionDetectedModules& modules,
		SourceToHeaderMap& sourceToHeaderMap)
	{
		return handleCurrentFolderResolution(detectedModuleStr, config, fileInfo, modules, sourceToHeaderMap)
			|| handleIncludePathListResolution(detectedModuleStr, config, fileInfo, modules, sourceToHeaderMap)
			|| handleStdResolution(detectedModuleStr, config, fileInfo, modules, sourceToHeaderMap)
			|| handleUnresolved(detectedModuleStr, config, fileInfo, modules, sourceToHeaderMap);
	}

private:
	static void handleGroupSourceHeader(
		const fs::path& resolvedModulePath, const FileInfo& fileInfo, SourceToHeaderMap& sourceToHeaderMap)
	{
		const auto& f = fileInfo; // shortcut

		if (!utils::file::isSourceFile(*f.importerPath)) return;
		// current file is a source file
		if (!utils::file::isHeaderFile(resolvedModulePath)) return;
		// resolved module is a header file
		const auto importerFileName = f.importerPath->filename().replace_extension("").string();
		const auto resolvedModuleFileName = resolvedModulePath.filename().replace_extension("").string();
		if (resolvedModuleFileName != importerFileName) return;
		// resolved module filename is the same as current file
		if (sourceToHeaderMap.count(*f.importerPath) != 0) return;
		// current file is not already mapped to a header file
		sourceToHeaderMap[*f.importerPath] = resolvedModulePath;
	}

	static void handleResolvedModule(const fs::path& resolvedModulePath,
		const Config& config,
		const FileInfo& fileInfo,
		ResolutionDetectedModules& modules,
		SourceToHeaderMap& sourceToHeaderMap)
	{
		const auto& f = fileInfo; // shortcut

		handleGroupSourceHeader(resolvedModulePath, f, sourceToHeaderMap);

		auto resolvedModuleSegmentList = utils::file::toSegmentList(resolvedModulePath);

		const bool isForceIncluded
			= Glob::getGlobThatMatchesSegmentList(resolvedModuleSegmentList, config.forceIncludeScanGlobList) != nullptr;
		const bool isExcluded
			= !isForceIncluded
		   && Glob::getGlobThatMatchesSegmentList(resolvedModuleSegmentList, config.excludeScanGlobList) != nullptr;
		if (isExcluded) return;
		// resolution not excluded

		const bool isAllowed = f.allowedToList == nullptr
							|| Glob::getGlobThatMatchesSegmentList(resolvedModuleSegmentList, *f.allowedToList) != nullptr;
		if (isAllowed) modules.allowedSet.insert(resolvedModulePath);
		else modules.forbiddenSet.insert(resolvedModulePath);
	}

	static bool handleCurrentFolderResolution(const std::string& detectedModuleStr,
		const Config& config,
		const FileInfo& fileInfo,
		ResolutionDetectedModules& modules,
		SourceToHeaderMap& sourceToHeaderMap)
	{
		const auto& f = fileInfo; // shortcut

		const fs::path currentFolderModulePath = f.importerPath->parent_path() / fs::path(detectedModuleStr);

		if (!fs::exists(currentFolderModulePath)) return false;
		// module resolved

		handleResolvedModule(currentFolderModulePath, config, fileInfo, modules, sourceToHeaderMap);
		return true;
	}

	static const fs::path* getResolutionIncludePath(
		const fs::path& detectedModulePath, const std::vector<fs::path>& resolutionIncludePathList)
	{
		for (const auto& resolutionIncludePath : resolutionIncludePathList)
		{
			const fs::path& newIncludePath = resolutionIncludePath / detectedModulePath;
			if (fs::exists(newIncludePath)) return &resolutionIncludePath;
		}
		return nullptr;
	}

	// returns true if the module is resolved
	static bool handleIncludePathListResolution(const std::string& detectedModuleStr,
		const Config& config,
		const FileInfo& fileInfo,
		ResolutionDetectedModules& modules,
		SourceToHeaderMap& sourceToHeaderMap)
	{
		const fs::path detectedModulePath(detectedModuleStr);

		const auto* resolutionIncludePath = getResolutionIncludePath(detectedModulePath, config.resolutionIncludePathList);
		if (resolutionIncludePath == nullptr) return false;
		// module resolved

		const fs::path resolvedModulePath = *resolutionIncludePath / detectedModulePath;
		handleResolvedModule(resolvedModulePath, config, fileInfo, modules, sourceToHeaderMap);
		return true;
	}

	static bool handleStdResolution(const std::string& detectedModuleStr,
		const Config& config,
		const FileInfo& /* fileInfo */,
		ResolutionDetectedModules& modules,
		SourceToHeaderMap& /* sourceToHeaderMap */)
	{
		for (const auto& stdIncludePath : config.stdResolutionIncludePathList)
		{
			const fs::path& newIncludePath = stdIncludePath / fs::path(detectedModuleStr);
			if (!fs::exists(newIncludePath)) continue;
			// std module resolved

			if (!config.bKeepStdInOutput) return true;
			// keep std in output

			modules.allowedSet.insert(detectedModuleStr);
			return true;
		}
		return false;
	}

	static bool handleUnresolved(const std::string& detectedModuleStr,
		const Config& config,
		const FileInfo& fileInfo,
		ResolutionDetectedModules& modules,
		SourceToHeaderMap& /* sourceToHeaderMap */)
	{
		const auto& f = fileInfo; // shortcut

		// current folder used for better display
		const fs::path currentFolderModulePath = f.importerPath->parent_path() / fs::path(detectedModuleStr);

		auto currentFolderSegmentList = utils::file::toSegmentList(currentFolderModulePath);

		const bool isForceIncluded
			= Glob::getGlobThatMatchesSegmentList(currentFolderSegmentList, config.forceIncludeScanGlobList) != nullptr;
		const bool isExcluded
			= !isForceIncluded
		   && Glob::getGlobThatMatchesSegmentList(currentFolderSegmentList, config.excludeScanGlobList) != nullptr;
		if (isExcluded) return false;
		// unresolved not excluded

		modules.unresolvedSet.insert(currentFolderModulePath);
		return false;
	}
};
