#pragma once

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

	[[nodiscard]] virtual std::vector<std::string> getStdIncludePathList() const = 0;
	[[nodiscard]] virtual bool isHeaderFile(const std::filesystem::path& path) const = 0;
	[[nodiscard]] virtual bool isSourceFile(const std::filesystem::path& path) const = 0;
	[[nodiscard]] virtual bool tryParseImportLine(const std::string& line, std::string& detectedModuleStr) const = 0;
};
