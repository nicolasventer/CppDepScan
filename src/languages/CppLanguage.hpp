#pragma once

#include "Glob.hpp"
#include "ILanguage.hpp"
#include "utils/str.hpp"
#include <array>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#define popen  _popen
#define pclose _pclose
#endif

namespace fs = std::filesystem;

class CppLanguage : public ILanguage
{
public:
	void initialize(const std::vector<Glob>& /* scanGlobList */) override
	{
		const std::string compiler = "g++";

		const fs::path tmp = fs::temp_directory_path() / "print_compiler_path.tmp";
		{
			std::ofstream f(tmp);
			f << '\n';
		}
		const std::string cmd = compiler + " -E -x c++ \"" + tmp.string() + "\" -v 2>&1";
		FILE* pipe = popen(cmd.c_str(), "r"); // NOLINT -- need shell for 2>&1
		fs::remove(tmp);

		if (pipe == nullptr)
		{
			std::cerr << "Failed to run: " << compiler << "\n";
			return;
		}

		std::ostringstream raw;
		static constexpr size_t BUF_SIZE = 256;
		std::array<char, BUF_SIZE> buf{};
		while (fgets(buf.data(), static_cast<int>(buf.size()), pipe) != nullptr) raw << buf.data();
		pclose(pipe);

		const std::string out = raw.str();
		const std::string startMarker = "#include <...> search starts here:";
		const std::string endMarker = "End of search list.";

		auto startPos = out.find(startMarker);
		if (startPos == std::string::npos)
		{
			std::cerr << "Could not find include search list in compiler output.\n";
			return;
		}

		startPos += startMarker.size();
		auto endPos = out.find(endMarker, startPos);
		if (endPos == std::string::npos) endPos = out.size();

		const std::string block = out.substr(startPos, endPos - startPos);
		std::istringstream iss(block);
		std::string line;

		while (std::getline(iss, line))
		{
			size_t i = 0;
			while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
			if (i < line.size())
			{
				const std::string path = line.substr(i);
				if (!path.empty()) stdModulePathList.push_back(fs::path(path).lexically_normal().string());
			}
		}
	}

	[[nodiscard]] bool isHeaderFile(const fs::path& path) const override
	{
		static constexpr auto HEADER_EXTENSION_LIST = {".h", ".hpp", ".hxx", ".hh"};
		for (const auto* headerExtension : HEADER_EXTENSION_LIST)
			if (path.extension() == headerExtension) return true;
		return false;
	}

	[[nodiscard]] bool isSourceFile(const fs::path& path) const override
	{
		static constexpr auto SOURCE_EXTENSION_LIST = {".c", ".cc", ".cpp", ".cxx"};
		for (const auto* sourceExtension : SOURCE_EXTENSION_LIST)
			if (path.extension() == sourceExtension) return true;
		return false;
	}

	[[nodiscard]] bool tryParseImportLine(const std::string& line, std::string& detectedModuleStr) const override
	{
		std::string_view substr;
		if (!utils::str::bStartWith(line, "#include ", &substr)) return false;

		const auto startPos = substr.find_first_of("\"<");
		if (startPos == std::string_view::npos) return false;
		const auto endPos = substr.find_first_of("\">", startPos + 1);
		if (endPos == std::string_view::npos) return false;

		detectedModuleStr = std::string(substr.substr(startPos + 1, endPos - startPos - 1));
		return true;
	}

	[[nodiscard]] bool bNonStdModuleExist(
		const fs::path& path, const std::string& /* detectedModuleStr */, std::filesystem::path& foundPath) const override
	{
		if (fs::exists(path))
		{
			foundPath = path;
			return true;
		}
		return false;
	}

	[[nodiscard]] bool bStdModuleExist(const std::string& moduleStr, std::filesystem::path& foundPath) const override
	{
		std::filesystem::path unusedFoundPath;
		for (const auto& stdModulePath : stdModulePathList)
			if (bNonStdModuleExist(fs::path(stdModulePath) / moduleStr, moduleStr, unusedFoundPath))
			{
				foundPath = moduleStr;
				return true;
			}
		return false;
	}

private:
	std::vector<std::string> stdModulePathList;
};
