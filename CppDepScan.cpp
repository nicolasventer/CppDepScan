#include <array>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <set>
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
	std::vector<fs::path> outputPathList;					   // empty = stdout
	std::map<fs::path, size_t> allowedIncludeIndexMap;		   // from -> index of allowedIncludeListList
	std::vector<std::vector<fs::path>> allowedIncludeListList; // list of allowed include lists
	bool bIncludeStd = false;								   // default: false
	bool bOutputJson = false;								   // default: D2 lang
	std::vector<fs::path> groupPathList;

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
	std::set<std::string> allowedSet;
	std::set<std::string> forbiddenSet;
	std::set<std::string> unresolvedSet;

	std::ostream& toJsonString(std::ostream& os) const
	{
		os << R"({ "allowedSet": [)";
		auto func = [&](const std::string& value, bool isLast)
		{
			os << '"' << escapeJson(value) << '"';
			if (!isLast) os << ", ";
		};
		foreach (allowedSet, func);
		os << "],";
		os << R"( "forbiddenSet": [)";
		foreach (forbiddenSet, func);
		os << "],";
		os << R"( "unresolvedSet": [)";
		foreach (unresolvedSet, func);
		os << "] }";
		return os;
	}

	std::ostream& toD2String(std::ostream& os, const std::string& from, EIncludeStatus status) const
	{
		if (status == EIncludeStatus::Allowed)
			for (const auto& to : allowedSet) os << from << " -> " << to << "\n";
		else if (status == EIncludeStatus::Forbidden)
			for (const auto& to : forbiddenSet) os << from << " -> " << to << ": { class: forbidden }\n";
		else
			for (const auto& to : unresolvedSet) os << "# " << from << " -> " << to << "\n";
		return os;
	}
};

struct Output
{
	std::map<std::string, Include> specifiedIncludeMap;	  // from -> Include
	std::map<std::string, Include> unspecifiedIncludeMap; // TODO: change type to UnspecifiedInclude

	std::ostream& toJsonString(std::ostream& os) const
	{
		os << "{\n";
		os << R"(  "specifiedIncludeMap":)" << "\n  {";
		auto func = [&](const std::pair<std::string, Include>& pair, bool isLast)
		{
			os << "\n    \"" << escapeJson(pair.first) << "\": ";
			pair.second.toJsonString(os);
			if (!isLast) os << ", ";
		};
		foreach (specifiedIncludeMap, func);
		os << "\n  },\n";
		os << R"(  "unspecifiedIncludeMap":)" << "\n  {";
		foreach (unspecifiedIncludeMap, func);
		os << "\n  }\n";
		os << "}";
		return os;
	}

	std::ostream& toD2String(std::ostream& os) const
	{
		os << R"(classes: {
  forbidden: {
    style: {
      stroke: "red"
    }
  }
  unspecified: {
    style: {
      stroke: "orange"
    }
  }
}
)";
		if (!specifiedIncludeMap.empty())
		{
			os << "# specified include list:\n";
			os << "\n# files:\n";
			for (const auto& [from, _] : specifiedIncludeMap) os << from << "\n";
			os << "\n# allowed:\n";
			for (const auto& [from, include] : specifiedIncludeMap) include.toD2String(os, from, EIncludeStatus::Allowed);
			os << "\n# forbidden:\n";
			for (const auto& [from, include] : specifiedIncludeMap) include.toD2String(os, from, EIncludeStatus::Forbidden);
			os << "\n# unresolved:\n";
			for (const auto& [from, include] : specifiedIncludeMap) include.toD2String(os, from, EIncludeStatus::Unresolved);
			os << "\n";
		}
		if (!unspecifiedIncludeMap.empty())
		{
			os << "\n# unspecified include list:\n";
			os << "\n# files:\n";
			for (const auto& [from, _] : unspecifiedIncludeMap) os << from << ".class: unspecified\n";
			os << "\n# resolved:\n";
			for (const auto& [from, include] : unspecifiedIncludeMap) include.toD2String(os, from, EIncludeStatus::Allowed);
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
		return bStartWith(p1.lexically_normal().string(), p2.lexically_normal().string(), subStr);
	}

	static const fs::path* getPathThatIncludeFromList(const fs::path& p1, const std::vector<fs::path>& pathList)
	{
		for (const auto& p : pathList)
			if (isPathInclude(p1, p)) return &p;
		return nullptr;
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

static const std::vector<fs::path>* getAllowedToList(const std::map<fs::path, size_t>& allowedIncludeIndexMap,
	const std::vector<std::vector<fs::path>>& allowedIncludeListList,
	const fs::path& cppPath)
{
	for (const auto& [from, index] : allowedIncludeIndexMap)
		if (utility::isPathInclude(cppPath, from)) return &allowedIncludeListList[index];
	return nullptr;
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
		const auto* allowedToList = getAllowedToList(config.allowedIncludeIndexMap, config.allowedIncludeListList, cppPath);
		const auto* groupPath = getPathThatIncludeFromList(cppPath, config.groupPathList);
		const auto dottedPath = pathToDotted(groupPath != nullptr ? *groupPath : cppPath);
		const auto cppDottedPath = pathToDotted(cppPath); // used only for forbidden or unresolved includes
		bool isSpecified = config.allowedIncludeIndexMap.empty() || allowedToList != nullptr;
		auto& include_ = isSpecified ? result.specifiedIncludeMap[dottedPath] : result.unspecifiedIncludeMap[dottedPath];
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
			if (fs::exists(includePath))
			{
				const auto* includeGroupPath = getPathThatIncludeFromList(includePath, config.groupPathList);
				if (includeGroupPath != groupPath || includeGroupPath == nullptr)
					include_.allowedSet.insert(pathToDotted(includeGroupPath != nullptr ? *includeGroupPath : includePath));
			}
			else
			{
				bool isResolved = false;
				for (const auto& includePath : config.includePathList)
				{
					const fs::path& newIncludePath = includePath / fs::path(include);
					if (fs::exists(newIncludePath))
					{
						bool isAllowed
							= allowedToList == nullptr || getPathThatIncludeFromList(newIncludePath, *allowedToList) != nullptr;
						bool isExcluded = getPathThatIncludeFromList(newIncludePath, config.excludePathList) != nullptr;
						if (!isExcluded)
						{
							if (isAllowed)
							{
								const auto* includeGroupPath = getPathThatIncludeFromList(newIncludePath, config.groupPathList);
								if (includeGroupPath != groupPath || includeGroupPath == nullptr)
									include_.allowedSet.insert(
										pathToDotted(includeGroupPath != nullptr ? *includeGroupPath : newIncludePath));
							}
							else
							{
								auto& cppInclude = isSpecified ? result.specifiedIncludeMap[cppDottedPath]
															   : result.unspecifiedIncludeMap[cppDottedPath];
								cppInclude.forbiddenSet.insert(pathToDotted(newIncludePath));
							}
						}
						isResolved = true;
						break;
					}
				}
				if (!isResolved)
				{
					for (const auto& stdIncludePath : config.stdIncludePathList)
					{
						const fs::path& newIncludePath = stdIncludePath / fs::path(include);
						if (fs::exists(newIncludePath))
						{
							if (config.bIncludeStd) include_.allowedSet.insert(include);
							isResolved = true;
							break;
						}
					}
				}
				if (!isResolved)
				{
					auto& cppInclude
						= isSpecified ? result.specifiedIncludeMap[cppDottedPath] : result.unspecifiedIncludeMap[cppDottedPath];
					cppInclude.unresolvedSet.insert(include);
				}
			}
		}
	}

	std::cout << "\nDone\n";

	return result;
}

static void usage(const char* prog)
{
	std::cerr << "Usage: " << prog << " [options] <scan_path> [scan_path ...]\n"
			  << "  -I <path>     Add include path for resolution (folder or file)\n"
			  << "  -e <path>     Exclude path (folder or file); may be repeated; use -e !<path> to force-include\n"
			  << "  -A, --allowed <from> <to>  Allowed include: <from> may include <to>; may be repeated\n"
			  << "  --std      Include standard library headers in output (default: false)\n"
			  << "  --json        Output JSON (default: D2 lang)\n"
			  << "  -g, --group [path]  Group D2 diagram by directory; path may be repeated (default: off)\n"
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
			if (path.back() == '/') path.pop_back();
			c.includePathList.emplace_back(path);
		}
		else if (a == "-e" && i + 1 < argc)
		{
			std::string path = argv[++i];
			if (path.back() == '/') path.pop_back();
			if (path.front() == '!') c.forceIncludePathList.emplace_back(path.substr(1));
			else
				c.excludePathList.emplace_back(path);
		}
		else if ((a == "-A" || a == "--allowed") && i + 2 < argc)
		{
			std::string fromStr = argv[++i];
			std::string toStr = argv[++i];
			if (!fromStr.empty() && fromStr.back() == '/') fromStr.pop_back();
			if (!toStr.empty() && toStr.back() == '/') toStr.pop_back();
			fs::path fromPath(fromStr);
			if (c.allowedIncludeIndexMap.find(fromPath) == c.allowedIncludeIndexMap.end())
				c.allowedIncludeIndexMap[fromPath] = c.allowedIncludeListList.size();
			c.allowedIncludeListList[c.allowedIncludeIndexMap[fromPath]].emplace_back(toStr);
		}
		else if (a == "-o" && i + 1 < argc)
			c.outputPathList.emplace_back(argv[++i]);
		else if (a == "--std")
			c.bIncludeStd = true;
		else if (a == "--json")
			c.bOutputJson = true;
		else if ((a == "-g" || a == "--group") && i + 1 < argc)
		{
			std::string path = argv[++i];
			if (path.back() == '/') path.pop_back();
			c.groupPathList.emplace_back(path);
		}
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
