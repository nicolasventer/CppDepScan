#pragma once

#include "Glob.hpp"
#include <filesystem>
#include <string>
#include <vector>

struct ILanguage
{
	virtual ~ILanguage() = default;
	ILanguage() = default;
	ILanguage(ILanguage&&) noexcept = default;
	ILanguage(const ILanguage&) = delete;
	ILanguage& operator=(ILanguage&&) noexcept = default;
	ILanguage& operator=(const ILanguage&) = delete;

	virtual void initialize(const std::vector<Glob>& scanGlobList) = 0;
	[[nodiscard]] virtual bool isHeaderFile(const std::filesystem::path& path) const = 0;
	[[nodiscard]] virtual bool isSourceFile(const std::filesystem::path& path) const = 0;
	[[nodiscard]] virtual bool tryParseImportLine(const std::string& line, std::string& detectedModuleStr) const = 0;
	[[nodiscard]] virtual bool bNonStdModuleExist(
		const std::filesystem::path& path, const std::string& detectedModuleStr, std::filesystem::path& foundPath) const = 0;
	[[nodiscard]] virtual bool bStdModuleExist(const std::string& moduleStr, std::filesystem::path& foundPath) const = 0;
};
