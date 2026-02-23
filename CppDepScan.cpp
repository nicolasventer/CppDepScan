#include <array>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#include <io.h>
#define popen  _popen
#define pclose _pclose
#endif

namespace fs = std::filesystem;

struct Config
{
	std::vector<fs::path> scanPathList;
	std::vector<fs::path> includePathList;
	std::vector<fs::path> excludePathList;
	std::vector<fs::path> forceIncludePathList;
	std::vector<fs::path> outputPathList; // empty = stdout
	bool bIncludeStdLib = false;		  // default: false
	bool bOutputJson = false;			  // default: D2 lang

	// automatically filled
	std::vector<std::string> stdIncludePathList;
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

template <typename T> void foreach (const T& range, const std::function<void(const typename T::value_type&, bool)>& func)
{
	auto it = range.begin();
	auto itEnd = range.end();
	while (it != itEnd)
	{
		const auto& value = *it;
		++it;
		func(value, it == itEnd);
	}
}

enum class EIncludeStatus : uint8_t
{
	Allowed,
	Forbidden,
	Unresolved,
};

struct Include
{
	std::string from;
	std::vector<std::string> allowedToList;
	std::vector<std::string> forbiddenToList;
	std::vector<std::string> unresolvedToList;

	std::ostream& toJsonString(std::ostream& os) const
	{
		os << "\n  {\n";
		os << R"(    "from": ")" << escapeJson(from) << "\",\n";
		os << R"(    "allowedToList": [)";
		auto func = [&](const std::string& value, bool isLast)
		{
			os << '"' << escapeJson(value) << '"';
			if (!isLast) os << ", ";
		};
		foreach (allowedToList, func);
		os << "],\n";
		os << R"(    "forbiddenToList": [)";
		foreach (forbiddenToList, func);
		os << "],\n";
		os << R"(    "unresolvedToList": [)";
		foreach (unresolvedToList, func);
		os << "]\n";
		os << "  }";
		return os;
	}

	std::ostream& toD2String(std::ostream& os, EIncludeStatus status, bool isSpecified) const
	{
		// TODO: unspecified color
		if (status == EIncludeStatus::Allowed)
			for (const auto& to : allowedToList) os << from << " -> " << to << "\n";
		else if (status == EIncludeStatus::Forbidden)
			for (const auto& to : forbiddenToList) os << from << " -> " << to << "\n"; // TODO: forbidden color
		else
			for (const auto& to : unresolvedToList) os << "# " << from << " -> " << to << "\n";
		return os;
	}
};

struct Output
{
	std::vector<Include> specifiedIncludeList;
	std::vector<Include> unspecifiedIncludeList;

	std::ostream& toJsonString(std::ostream& os) const
	{
		os << "{\n";
		os << R"(  "specifiedIncludeList": [)";
		auto func = [&](const Include& value, bool isLast)
		{
			value.toJsonString(os);
			if (!isLast) os << ", ";
		};
		foreach (specifiedIncludeList, func);
		os << "],\n";
		os << R"(  "unspecifiedIncludeList": [)";
		foreach (unspecifiedIncludeList, func);
		os << "]\n";
		os << "}";
		return os;
	}

	std::ostream& toD2String(std::ostream& os) const
	{
		if (!specifiedIncludeList.empty())
		{
			os << "# specified include list:\n";
			os << "\n# allowed:\n";
			for (const auto& include : specifiedIncludeList) include.toD2String(os, EIncludeStatus::Allowed, true);
			os << "\n# forbidden:\n";
			for (const auto& include : specifiedIncludeList) include.toD2String(os, EIncludeStatus::Forbidden, true);
			os << "\n# unresolved:\n";
			for (const auto& include : specifiedIncludeList) include.toD2String(os, EIncludeStatus::Unresolved, true);
			os << "\n";
		}
		if (!unspecifiedIncludeList.empty())
		{
			os << "\n# unspecified include list:\n";
			os << "\n# allowed:\n";
			for (const auto& include : unspecifiedIncludeList) include.toD2String(os, EIncludeStatus::Allowed, false);
			os << "\n# forbidden:\n";
			for (const auto& include : unspecifiedIncludeList) include.toD2String(os, EIncludeStatus::Forbidden, false);
			os << "\n# unresolved:\n";
			for (const auto& include : unspecifiedIncludeList) include.toD2String(os, EIncludeStatus::Unresolved, false);
			os << "\n";
		}
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

	static bool isPathInclude(const fs::path& p1, const fs::path& p2)
	{
		std::string_view subStr;
		return bStartWith(p1.string(), p2.string(), subStr);
	}

	static void updateCppPathList(const fs::path& scanPath,
		std::vector<fs::path>& cppPathList,
		const std::vector<fs::path>& excludePathList,
		const std::vector<fs::path>& forceIncludePathList)
	{
		bool bForceInclude = false;
		for (const auto& forceIncludePath : forceIncludePathList)
		{
			if (isPathInclude(scanPath, forceIncludePath))
			{
				bForceInclude = true;
				break;
			}
		}
		if (!bForceInclude)
		{
			for (const auto& excludePath : excludePathList)
				if (isPathInclude(scanPath, excludePath)) return;
		}
		if (fs::is_regular_file(scanPath))
		{
			if (isCppFile(scanPath.string())) cppPathList.push_back(scanPath);
		}
		else if (fs::is_directory(scanPath))
		{
			for (const auto& file : fs::directory_iterator(scanPath))
			{
				if (file.is_directory())
				{
					updateCppPathList(file.path(), cppPathList, excludePathList, forceIncludePathList);
				}
				else
				{
					if (isCppFile(file.path().string())) cppPathList.push_back(file.path());
				}
			}
		}
	}
} // namespace utility

static std::vector<std::string> getStdIncludePathList()
{
	std::vector<std::string> pathList;

	const std::string compiler = "g++";

	const fs::path tmp = fs::temp_directory_path() / "print_compiler_path.tmp";
	{
		std::ofstream f(tmp);
		f << '\n';
	}
	const std::string cmd = compiler + " -E -x c++ \"" + tmp.string() + "\" -v 2>&1";
	FILE* pipe = popen(cmd.c_str(), "r"); // NOLINT -- need shell for 2>&1
	fs::remove(tmp);

	if (!pipe)
	{
		std::cerr << "Failed to run: " << compiler << "\n";
		return pathList;
	}

	std::ostringstream raw;
	std::array<char, 256> buf{};
	while (fgets(buf.data(), static_cast<int>(buf.size()), pipe)) raw << buf.data();
	pclose(pipe);

	const std::string out = raw.str();
	const std::string startMarker = "#include <...> search starts here:";
	const std::string endMarker = "End of search list.";

	auto startPos = out.find(startMarker);
	if (startPos == std::string::npos)
	{
		std::cerr << "Could not find include search list in compiler output.\n";
		return pathList;
	}

	startPos += startMarker.size();
	auto endPos = out.find(endMarker, startPos);
	if (endPos == std::string::npos) endPos = out.size();

	const std::string block = out.substr(startPos, endPos - startPos);
	std::istringstream iss(block);
	std::string line;

	while (std::getline(iss, line))
	{
		// Paths are indented with spaces
		size_t i = 0;
		while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
		if (i < line.size())
		{
			const std::string path = line.substr(i);
			if (!path.empty()) pathList.push_back(fs::path(path).lexically_normal().string());
		}
	}

	return pathList;
}

static Output getOutput(const Config& config)
{
	using namespace utility;

	Output result;

	std::vector<fs::path> cppPathList;
	for (const auto& scanPath : config.scanPathList)
		updateCppPathList(scanPath, cppPathList, config.excludePathList, config.forceIncludePathList);

	std::cout << "start parsing..." << "\n";

	for (size_t i = 0; i < cppPathList.size(); ++i)
	{
		std::cout << "\r[" << (i + 1) << "/" << cppPathList.size() << "]" << std::flush;
		const auto& cppPath = cppPathList[i];
		const auto dottedPath = pathToDotted(cppPath);
		Include include_;
		include_.from = dottedPath;
		std::ifstream ifs(cppPath);
		std::string line;
		while (std::getline(ifs, line))
		{
			std::string_view substr;
			if (!bStartWith(line, "#include ", substr)) continue;
			auto startPos = substr.find_first_of("\"<");
			if (startPos == std::string::npos) continue;
			auto endPos = substr.find_first_of("\">", startPos + 1);
			const std::string include = static_cast<std::string>(substr.substr(startPos + 1, endPos - startPos - 1));
			const fs::path& includePath = cppPath.parent_path() / fs::path(include);
			if (fs::exists(includePath)) include_.allowedToList.push_back(pathToDotted(includePath));
			else
			{
				bool isResolved = false;
				for (const auto& includePath : config.includePathList)
				{
					const fs::path& newIncludePath = includePath / fs::path(include);
					if (fs::exists(newIncludePath))
					{
						include_.allowedToList.push_back(pathToDotted(newIncludePath));
						isResolved = true;
						break;
					}
				}
				for (const auto& stdIncludePath : config.stdIncludePathList)
				{
					const fs::path& newIncludePath = stdIncludePath / fs::path(include);
					if (fs::exists(newIncludePath))
					{
						if (config.bIncludeStdLib) include_.allowedToList.push_back(include);
						isResolved = true;
						break;
					}
				}
				if (!isResolved) include_.unresolvedToList.push_back(include);
			}
		}
		result.specifiedIncludeList.push_back(std::move(include_));
	}

	std::cout << "\nDone\n";

	return result;
}

static void usage(const char* prog)
{
	std::cerr << "Usage: " << prog << " [options] <scan_path> [scan_path ...]\n"
			  << "  -I <path>     Add include path for resolution (folder or file)\n"
			  << "  -e <path>     Exclude path (folder or file); use -e !<path> to force-include\n"
			  << "  --stdlib      Include standard library headers in output (default: false)\n"
			  << "  --json        Output JSON (default: D2 lang)\n"
			  << "  -o <file>     Write output to file; may be repeated; .json → JSON, else D2 (default: stdout)\n"
			  << "  -h, --help    Print this help\n";
}

static Config parseArgs(int argc, char** argv)
{
	Config c;
	for (int i = 1; i < argc; ++i)
	{
		std::string a = argv[i];
		if (a == "-I" && i + 1 < argc)
		{
			std::string path = argv[++i];
			if (path.back() == '/') path = path.substr(0, path.size() - 1);
			c.includePathList.emplace_back(path);
		}
		else if (a == "-e" && i + 1 < argc)
		{
			std::string path = argv[++i];
			if (path.back() == '/') path = path.substr(0, path.size() - 1);
			if (path.front() == '!') c.forceIncludePathList.emplace_back(path.substr(1));
			else
				c.excludePathList.emplace_back(path);
		}
		else if (a == "-o" && i + 1 < argc)
			c.outputPathList.emplace_back(argv[++i]);
		else if (a == "--stdlib")
			c.bIncludeStdLib = true;
		else if (a == "--json")
			c.bOutputJson = true;
		else if (a == "-h" || a == "--help")
		{
			usage(argv[0]);
			std::exit(0);
		}
		else if (!a.empty() && a[0] == '-')
		{
			std::cerr << "Error: unknown option: " << a << "\n";
			usage(argv[0]);
			std::exit(1);
		}
		else
			c.scanPathList.emplace_back(a);
	}
	c.stdIncludePathList = getStdIncludePathList();
	return c;
}

int main(int argc, char** argv)
{
	const Config cfg = parseArgs(argc, argv);
	if (cfg.scanPathList.empty())
	{
		std::cerr << "Error: no scan paths provided\n";
		usage(argv[0]);
		return 1;
	}
	const Output output = getOutput(cfg);
	if (cfg.outputPathList.empty())
	{
		std::cout << "\n";
		if (cfg.bOutputJson) output.toJsonString(std::cout);
		else
			output.toD2String(std::cout);
	}
	else
	{
		for (const auto& outputPath : cfg.outputPathList)
		{
			std::string_view subStr;
			if (outputPath.has_parent_path()) std::filesystem::create_directories(outputPath.parent_path());
			std::ofstream ofs(outputPath);
			if (!ofs.is_open())
			{
				std::cerr << "Failed to open output file: " << outputPath << "\n";
				continue;
			}
			if (utility::bEndWith(outputPath.string(), ".json", subStr)) output.toJsonString(ofs);
			else
				output.toD2String(ofs);
			std::cout << "Generated: " << outputPath << "\n";
		}
	}
	return 0;
}
