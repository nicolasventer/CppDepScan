#pragma once

#include "Glob.hpp"
#include "ILanguage.hpp"
#include "libs/yyjsonWrap/yyjsonWrap.hpp"
#include "utils/file.hpp"
#include "utils/str.hpp"
#include <array>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

class TypeScriptLanguage : public ILanguage
{
public:
	void initialize(const std::vector<Glob>& scanGlobList) override
	{
		nodeModulesPath = "node_modules";
		for (const auto& scanGlob : scanGlobList)
		{
			const auto candidatePath = scanGlob.toRootPath() / "node_modules";
			if (fs::exists(candidatePath))
			{
				nodeModulesPath = candidatePath;
				// TODO: handle compilerOptions.paths (pathListMap)
				// "compilerOptions": {
				//   "paths": {
				//     "@/*": ["./src/*"]
				//   }
				// }
				break;
			}
		}
	}

	[[nodiscard]] bool isHeaderFile(const fs::path& /* path */) const override { return false; }

	[[nodiscard]] bool isSourceFile(const fs::path& path) const override
	{
		static constexpr auto SOURCE_EXTENSION_LIST = {".ts", ".tsx", ".mts", ".cts", ".js", ".jsx", ".mjs", ".cjs"};
		for (const auto* sourceExtension : SOURCE_EXTENSION_LIST)
			if (path.extension() == sourceExtension) return true;
		return false;
	}

	[[nodiscard]] bool tryParseImportLine(const std::string& line, std::string& detectedModuleStr) const override
	{
		std::string_view lineView = line;

		size_t start = 0;
		while (start < lineView.size() && (std::isspace(static_cast<unsigned char>(lineView[start])) != 0)) ++start;
		lineView = lineView.substr(start);
		if (lineView.empty() || utils::str::bStartWith(lineView, "//")) return false;

		const auto requirePos = lineView.find("require(");
		if (requirePos != std::string_view::npos
			&& tryExtractQuotedModule(lineView, requirePos + std::string_view("require(").size(), detectedModuleStr))
			return true;

		const auto dynamicImportPos = lineView.find("import(");
		if (dynamicImportPos != std::string_view::npos
			&& tryExtractQuotedModule(lineView, dynamicImportPos + std::string_view("import(").size(), detectedModuleStr))
			return true;

		const auto fromPos = lineView.rfind(" from ");
		if (fromPos != std::string_view::npos
			&& tryExtractQuotedModule(lineView, fromPos + std::string_view(" from ").size(), detectedModuleStr))
			return true;

		if (!utils::str::bStartWith(lineView, "import")) return false;

		std::string_view rest;
		if (!utils::str::bStartWith(lineView, "import ", &rest)) return false;
		if (utils::str::bStartWith(rest, "type ")) rest = rest.substr(std::string_view("type ").size());
		while (!rest.empty() && (std::isspace(static_cast<unsigned char>(rest.front())) != 0)) rest.remove_prefix(1);
		if (rest.empty() || (rest.front() != '\'' && rest.front() != '"')) return false;

		return tryExtractQuotedModule(lineView, lineView.find(rest.front()), detectedModuleStr);
	}

	[[nodiscard]] bool bNonStdModuleExist(const fs::path& path, std::filesystem::path& foundPath) const override
	{
		if (fs::exists(path))
		{
			if (fs::is_directory(path)) return moduleExistsInDirectory(path, foundPath);
			foundPath = path;
			return true;
		}

		if (!path.has_extension())
		{
			static constexpr auto EXTENSION_LIST = {".ts",
				".tsx",
				".mts",
				".cts",
				".js",
				".jsx",
				".mjs",
				".cjs",
				".json",
				".css",
				".png",
				".jpg",
				".jpeg",
				".gif",
				".svg",
				".ico",
				".webp",
				".avif",
				".webp",
				".bmp",
				".tiff",
				".tif"};
			for (const auto* ext : EXTENSION_LIST)
			{
				const fs::path candidatePath = fs::path(path.string() + ext);
				if (fs::exists(candidatePath))
				{
					foundPath = candidatePath;
					return true;
				}
			}
		}

		return moduleExistsInDirectory(path, foundPath);
	}

	[[nodiscard]] bool bStdModuleExist(const std::string& moduleStr, std::filesystem::path& foundPath) const override
	{
		return bNonStdModuleExist(nodeModulesPath / moduleStr, foundPath);
	}

private:
	static bool tryExtractQuotedModule(std::string_view line, size_t startPos, std::string& detectedModuleStr)
	{
		const auto quotePos = line.find_first_of("'\"", startPos);
		if (quotePos == std::string_view::npos) return false;

		const char quote = line[quotePos];
		const auto endPos = line.find(quote, quotePos + 1);
		if (endPos == std::string_view::npos) return false;

		detectedModuleStr = std::string(line.substr(quotePos + 1, endPos - quotePos - 1));
		return !detectedModuleStr.empty();
	}

	[[nodiscard]] static bool moduleExistsInDirectory(const fs::path& dirPath, std::filesystem::path& foundPath)
	{
		if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) return false;

		static constexpr std::array INDEX_FILES
			= {"index.ts", "index.tsx", "index.mts", "index.cts", "index.js", "index.jsx", "index.mjs", "index.cjs"};
		for (const auto* indexFile : INDEX_FILES)
		{
			const fs::path candidatePath = dirPath / indexFile;
			if (fs::exists(candidatePath))
			{
				foundPath = candidatePath;
				return true;
			}
		}

		//  parse package.json to get the main field
		const fs::path packageJsonPath = dirPath / "package.json";
		if (!fs::exists(packageJsonPath)) return false;
		// package.json file exists
		std::string content;
		if (!utils::file::tryReadFile(packageJsonPath, content)) return false;
		// package.json file read successfully
		DocWrapper doc(content);
		ValueWrapper val(doc);
		if (!val.hasKey("main")) return false;
		// main field exists
		const fs::path candidatePath = dirPath / static_cast<std::string>(val["main"]);
		if (fs::exists(candidatePath))
		{
			foundPath = candidatePath;
			return true;
		}
		return false;
	}

	fs::path nodeModulesPath;
};
