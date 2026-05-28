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

namespace fs = std::filesystem;

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
		const Config& config, const fs::path& resolvedModulePath, const FileInfo& fileInfo, SourceToHeaderMap& sourceToHeaderMap)
	{
		const auto& f = fileInfo; // shortcut

		if (!config.languageImpl->isSourceFile(*f.importerPath)) return;
		// current file is a source file
		if (!config.languageImpl->isHeaderFile(resolvedModulePath)) return;
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

		handleGroupSourceHeader(config, resolvedModulePath, f, sourceToHeaderMap);

		auto resolvedModuleSegmentList = utils::file::toSegmentList(resolvedModulePath);

		const bool isForceIncluded
			= Glob::getGlobThatMatchesSegmentList(resolvedModuleSegmentList, config.forceIncludeScanGlobList) != nullptr;
		const bool isExcluded
			= !isForceIncluded
		   && Glob::getGlobThatMatchesSegmentList(resolvedModuleSegmentList, config.excludeScanGlobList) != nullptr;
		if (isExcluded) return;

		const bool isAllowed = f.allowedToList == nullptr
							|| Glob::getGlobThatMatchesSegmentList(resolvedModuleSegmentList, *f.allowedToList) != nullptr;
		if (isAllowed) modules.allowedSet.insert(resolvedModulePath);
		else if (!config.bExcludeForbidden) modules.forbiddenSet.insert(resolvedModulePath);
	}

	static bool handleCurrentFolderResolution(const std::string& detectedModuleStr,
		const Config& config,
		const FileInfo& fileInfo,
		ResolutionDetectedModules& modules,
		SourceToHeaderMap& sourceToHeaderMap)
	{
		const auto& f = fileInfo; // shortcut

		const fs::path currentFolderModulePath = (f.importerPath->parent_path() / fs::path(detectedModuleStr)).lexically_normal();

		fs::path foundPath;
		if (!config.languageImpl->bNonStdModuleExist(currentFolderModulePath, detectedModuleStr, foundPath)) return false;
		// module resolved

		handleResolvedModule(foundPath, config, fileInfo, modules, sourceToHeaderMap);
		return true;
	}

	static bool getResolutionIncludePath(const std::string& detectedModuleStr,
		const std::vector<fs::path>& resolutionIncludePathList,
		const Config& config,
		fs::path& foundPath)
	{
		for (const auto& resolutionIncludePath : resolutionIncludePathList)
		{
			const fs::path candidatePath = resolutionIncludePath / detectedModuleStr;
			if (config.languageImpl->bNonStdModuleExist(candidatePath, detectedModuleStr, foundPath)) return true;
		}
		return false;
	}

	// returns true if the module is resolved
	static bool handleIncludePathListResolution(const std::string& detectedModuleStr,
		const Config& config,
		const FileInfo& fileInfo,
		ResolutionDetectedModules& modules,
		SourceToHeaderMap& sourceToHeaderMap)
	{

		fs::path foundPath;
		if (!getResolutionIncludePath(detectedModuleStr, config.resolutionIncludePathList, config, foundPath)) return false;

		// module resolved
		handleResolvedModule(foundPath, config, fileInfo, modules, sourceToHeaderMap);
		return true;
	}

	static bool handleStdResolution(const std::string& detectedModuleStr,
		const Config& config,
		const FileInfo& /* fileInfo */,
		ResolutionDetectedModules& modules,
		SourceToHeaderMap& /* sourceToHeaderMap */)
	{
		fs::path foundPath;
		if (!config.languageImpl->bStdModuleExist(detectedModuleStr, foundPath)) return false;
		// std module resolved

		if (!config.bKeepStdInOutput) return true;
		// keep std in output

		modules.allowedSet.insert(foundPath); // TODO: see if modules.allowedSet.insert(detectedModuleStr);
		return true;
	}

	static bool handleUnresolved(const std::string& detectedModuleStr,
		const Config& config,
		const FileInfo& fileInfo,
		ResolutionDetectedModules& modules,
		SourceToHeaderMap& /* sourceToHeaderMap */)
	{
		if (config.bExcludeUnresolved) return false;

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
