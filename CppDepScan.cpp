#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

struct Config
{
	std::vector<fs::path> scanPathList;
	std::vector<fs::path> includePathList;
	std::vector<fs::path> excludePathList;
	bool includeStdLib = false;
	bool outputJson = false;
	bool outputD2 = false;
	fs::path outputPath; // empty = stdout
};

std::string escapeJson(const std::string& s)
{
	std::string result;
	for (const auto c : s)
	{
		if (c == '"') result += "\\\"";
		else
			result += c;
	}
	return result;
}

struct Include
{
	std::string from;
	std::vector<std::string> toList; // TODO: see if adding status here

	std::ostream& toJsonString(std::ostream& os, const std::string& indent) const
	{
		os << indent << "{\n";
		os << indent << R"(  "from": ")" << escapeJson(from) << "\",\n";
		os << indent << "  \"to\": [\n";
		for (size_t i = 0; i < toList.size(); ++i)
		{
			os << indent << "    \"" << escapeJson(toList[i]) << "\"";
			if (i == toList.size() - 1) os << "\n";
			else
				os << ",\n";
		}
		os << indent << "  ]\n";
		os << indent << "}";
		return os;
	}

	std::ostream& toD2String(std::ostream& os) const
	{
		for (const auto& to : toList) os << from << " -> " << to << "\n";
		return os;
	}
};

struct Output
{
	std::vector<Include> includeList;
	std::vector<Include> unresolvedIncludeList;

	std::ostream& toJsonString(std::ostream& os) const
	{
		os << "{\n";
		os << "  \"includeList\": [\n";
		for (size_t i = 0; i < includeList.size(); ++i)
		{
			includeList[i].toJsonString(os, "  ");
			if (i == includeList.size() - 1) os << "\n";
			else
				os << ",\n";
		}
		os << "  ],\n";
		os << "  \"unresolvedIncludeList\": [\n";
		for (size_t i = 0; i < unresolvedIncludeList.size(); ++i)
		{
			unresolvedIncludeList[i].toJsonString(os, "  ");
			if (i == unresolvedIncludeList.size() - 1) os << "\n";
			else
				os << ",\n";
		}
		os << "  ]\n";
		os << "}\n";
		return os;
	}
	std::ostream& toD2String(std::ostream& os) const
	{
		os << "# include list\n";
		for (const auto& include : includeList) include.toD2String(os);
		os << "\n# unresolved include list\n";
		for (const auto& unresolvedInclude : unresolvedIncludeList) unresolvedInclude.toD2String(os);
		return os;
	}
};

namespace utility
{
	static bool bStartWith(std::string_view str, std::string_view prefix, std::string_view& subStr)
	{
		if (str.size() < prefix.size()) return false;
		const bool result = str.substr(0, prefix.size()) == prefix;
		if (result) subStr = str.substr(prefix.size());
		return result;
	}

	static bool bEndWith(std::string_view str, std::string_view suffix, std::string_view& subStr)
	{
		if (str.size() < suffix.size()) return false;
		auto offset = str.size() - suffix.size();
		const bool result = str.substr(offset) == suffix;
		if (result) subStr = str.substr(0, offset);
		return result;
	}

	static fs::path normalize(const fs::path& p) { return fs::absolute(p).lexically_normal(); }

	static bool isCppFile(const std::string& filePath)
	{
		static auto cppExtensionList = {".h", ".hpp", ".hxx", ".hh", ".h++", ".c", ".cc", ".cpp", ".cxx"};
		static std::string_view subStr;
		for (const auto* cppExtension : cppExtensionList)
			if (bEndWith(filePath, cppExtension, subStr)) return true;
		return false;
	}

	static std::string pathToDotted(const fs::path& p)
	{
		std::string result;

		for (const auto& segment : p.relative_path())
		{
			const auto& segmentStr = segment.string();
			if (segmentStr.find('.') != std::string::npos) result += '"' + segmentStr + '"' + ".";
			else
				result += segmentStr + ".";
		}

		return result.substr(0, result.size() - 1);
	}
	static void updateCppFileList(const fs::path& scanPath, std::vector<fs::path>& cppFileList)
	{
		if (fs::is_regular_file(scanPath))
		{
			if (isCppFile(scanPath.string())) cppFileList.push_back(scanPath);
		}
		else if (fs::is_directory(scanPath))
		{
			for (const auto& file : fs::directory_iterator(scanPath))
			{
				if (file.is_directory())
				{
					updateCppFileList(file.path(), cppFileList);
				}
				else
				{
					if (isCppFile(file.path().string())) cppFileList.push_back(file.path());
				}
			}
		}
	}
} // namespace utility

Output getOutput(const Config& config)
{
	using namespace utility;

	Output result;

	std::vector<fs::path> cppFileList;
	for (const auto& scanPath : config.scanPathList) updateCppFileList(scanPath, cppFileList);

	std::cout << "start parsing..." << "\n";

	for (size_t i = 0; i < cppFileList.size(); ++i)
	{
		std::cout << "\r[" << (i + 1) << "/" << cppFileList.size() << "]" << std::flush;
		const auto& cppFile = cppFileList[i];
		const auto dottedPath = pathToDotted(cppFile);
		Include resolvedInclude;
		resolvedInclude.from = dottedPath;
		Include unresolvedInclude;
		unresolvedInclude.from = dottedPath;
		std::ifstream ifs(cppFile);
		std::string line;
		while (std::getline(ifs, line))
		{
			std::string_view substr;
			if (!bStartWith(line, "#include ", substr)) continue;

			auto startPos = substr.find_first_of("\"<");
			if (startPos == std::string::npos) continue;
			auto endPos = substr.find_first_of("\">", startPos + 1);
			const std::string include = static_cast<std::string>(substr.substr(startPos + 1, endPos - startPos - 1));

			const fs::path& includePath = cppFile.parent_path() / fs::path(include);

			if (fs::exists(includePath)) resolvedInclude.toList.push_back(pathToDotted(includePath));
			else
			{
				bool bResolved = false;
				for (const auto& includePath : config.includePathList)
				{
					const fs::path& newIncludePath = includePath.parent_path() / fs::path(include);
					if (fs::exists(newIncludePath))
					{
						resolvedInclude.toList.push_back(pathToDotted(newIncludePath));
						bResolved = true;
						break;
					}
				}
				if (!bResolved) unresolvedInclude.toList.push_back(pathToDotted(includePath));
			}
		}
		result.includeList.push_back(std::move(resolvedInclude));
		if (!unresolvedInclude.toList.empty()) result.unresolvedIncludeList.push_back(std::move(unresolvedInclude));
	}

	std::cout << "\nDone\n";

	return result;
}

// --- CLI ---
static void usage(const char* prog)
{
	std::cerr << "Usage: " << prog << " [options] <scan_path> [scan_path ...]\n"
			  << "  -I <path>     Add include path for resolution (folder or file)\n"
			  << "  -e <path>     Exclude path (folder or file)\n"
			  << "  --stdlib      Show included standard library files\n"
			  << "  --d2          Output D2 lang (default format)\n"
			  << "  --json        Output JSON\n"
			  << "  -o <file>     Write output to file (else stdout)\n";
}

static Config parseArgs(int argc, char** argv)
{
	Config c;
	for (int i = 1; i < argc; ++i)
	{
		std::string a = argv[i];
		if (a == "-I" && i + 1 < argc) c.includePathList.push_back(utility::normalize(argv[++i]));
		else if (a == "-e" && i + 1 < argc)
			c.excludePathList.push_back(utility::normalize(argv[++i]));
		else if (a == "--stdlib")
			c.includeStdLib = true;
		else if (a == "--json")
			c.outputJson = true;
		else if (a == "--d2")
			c.outputD2 = true;
		else if (a == "-o" && i + 1 < argc)
			c.outputPath = argv[++i];
		else if (a == "-h" || a == "--help")
		{
			usage(argv[0]);
			std::exit(0);
		}
		else if (!a.empty() && a[0] != '-')
			c.scanPathList.emplace_back(a);
	}
	if (!c.outputJson && !c.outputD2)
	{
		std::string_view subStr;
		if (utility::bEndWith(c.outputPath.string(), ".json", subStr)) c.outputJson = true;
		else
			c.outputD2 = true;
	}
	return c;
}

int main(int argc, char** argv)
{
	const Config cfg = parseArgs(argc, argv);
	if (cfg.scanPathList.empty())
	{
		usage(argv[0]);
		return 1;
	}
	const Output output = getOutput(cfg);
	if (cfg.outputJson)
	{
		if (cfg.outputPath.empty())
		{
			std::cout << "\n";
			output.toJsonString(std::cout);
		}
		else
		{
			std::ofstream ofs(cfg.outputPath);
			output.toJsonString(ofs);
			std::cout << "Generated: " << cfg.outputPath << "\n";
		}
	}
	else if (cfg.outputD2)
	{
		if (cfg.outputPath.empty())
		{
			std::cout << "\n";
			output.toD2String(std::cout);
		}
		else
		{
			std::ofstream ofs(cfg.outputPath);
			output.toD2String(ofs);
			std::cout << "Generated: " << cfg.outputPath << "\n";
		}
	}
	return 0;
}