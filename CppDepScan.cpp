#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
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
	std::vector<fs::path> excludeScanPathList;
	std::vector<fs::path> forceIncludeScanPathList;
	std::vector<fs::path> outputPathList; // empty = stdout
	bool bKeepStdInOutput = false;		  // default: false
	bool bUseJsonForStdOutput = false;	  // default: D2 lang
	std::vector<fs::path> resolutionIncludePathList;
	std::map<fs::path, size_t> allowedIncludeIndexMap;		   // from -> index of allowedIncludeListList
	std::vector<std::vector<fs::path>> allowedIncludeListList; // list of allowed include lists
	std::vector<fs::path> groupPathList;

	// automatically filled
	std::vector<std::string> stdResolutionIncludePathList;
};

inline std::ostream& toJsonString(std::ostream& os, const std::string& s)
{
	os << "\"";
	for (const auto c : s)
	{
		if (c == '"') os << "\\\"";
		else
			os << c;
	}
	os << "\"";
	return os;
}

template <typename T>
std::ostream& toJsonObject(std::ostream& os,
	const std::string& indent,
	const T& range,
	const std::function<void(const typename T::value_type&, const std::string& indent)>& func)
{
	os << "{";
	auto newIndent = indent + "  ";
	auto it = range.begin();
	auto itEnd = range.end();
	if (it != itEnd)
	{
		os << "\n" << newIndent;
		func(*it, newIndent);
		++it;
	}
	while (it != itEnd)
	{
		os << ",\n" << newIndent;
		func(*it, newIndent);
		++it;
	}
	os << "\n" << indent << "}";
	return os;
}

template <typename T>
std::ostream& toJsonArray(std::ostream& os, const T& range, const std::function<void(const typename T::value_type&)>& func)
{
	os << "[";
	auto it = range.begin();
	auto itEnd = range.end();
	if (it != itEnd)
	{
		func(*it);
		++it;
	}
	while (it != itEnd)
	{
		os << ",\n";
		func(*it);
		++it;
	}
	os << "]";
	return os;
}

enum class EDetectedIncludeStatus : uint8_t
{
	Allowed,
	Forbidden,
	Unresolved,
};

struct DetectedIncludes
{
	std::set<std::string> allowedSet;
	std::set<std::string> forbiddenSet;
	std::set<std::string> unresolvedSet;
};

// same as DetectedIncludes, but since origin is unspecified, resolved includes are allowed
using UnspecifiedDetectedIncludes = DetectedIncludes;

struct Output
{
	// specified origin -> DetectedIncludes
	std::map<std::string, DetectedIncludes> specifiedIncludesMap;
	// unspecified origin -> UnspecifiedDetectedIncludes
	std::map<std::string, UnspecifiedDetectedIncludes> unspecifiedIncludesMap;

	auto& getIncludes(bool isSpecified, const std::string& dottedPath)
	{
		return isSpecified ? specifiedIncludesMap[dottedPath] : unspecifiedIncludesMap[dottedPath];
	}
};

template <typename T> std::ostream& toJsonOutput(std::ostream& os, const std::string& /* indent */, const T& /* value */)
{
	static_assert(sizeof(T) == 0, "toJsonOutput not implemented for this type");
	return os;
}

template <>
std::ostream& toJsonOutput<DetectedIncludes>(std::ostream& os, const std::string& indent, const DetectedIncludes& value)
{
	auto members = {std::make_pair("allowedSet", &value.allowedSet),
		{"forbiddenSet", &value.forbiddenSet},
		{"unresolvedSet", &value.unresolvedSet}};

	toJsonObject(os,
		indent,
		members,
		[&](const auto& member, const std::string& /* indent */)
		{
			toJsonArray(
				toJsonString(os, member.first) << ": ", *member.second, [&](const auto& value) { toJsonString(os, value); });
		});

	return os;
}

template <> std::ostream& toJsonOutput<Output>(std::ostream& os, const std::string& indent, const Output& value)
{
	auto members = {std::make_pair("specifiedIncludeMap", &value.specifiedIncludesMap),
		{"unspecifiedIncludeMap", &value.unspecifiedIncludesMap}};

	toJsonObject(os,
		indent,
		members,
		[&](const auto& member, const std::string& indent)
		{
			toJsonObject(toJsonString(os, member.first) << ": ",
				indent,
				*member.second,
				[&](const auto& value, const std::string& indent)
				{ toJsonOutput(toJsonString(os, value.first) << ": ", indent, value.second); });
		});
	return os;
}

template <typename...> struct ToD2OutputUnsupported;
template <typename T, typename... U> std::ostream& toD2Output(std::ostream& os, const T& /* value */, const U&... /* args */)
{
	static_assert(sizeof(ToD2OutputUnsupported<T, U...>) == 0, "toD2Output not implemented for these types");
	return os;
}

template <>
std::ostream& toD2Output<DetectedIncludes, std::string, EDetectedIncludeStatus>(
	std::ostream& os, const DetectedIncludes& value, const std::string& from, const EDetectedIncludeStatus& status)
{
	if (status == EDetectedIncludeStatus::Allowed)
		for (const auto& to : value.allowedSet) os << from << " -> " << to << "\n";
	else if (status == EDetectedIncludeStatus::Forbidden)
		for (const auto& to : value.forbiddenSet) os << from << " -> " << to << ": { class: forbidden }\n";
	else
		for (const auto& to : value.unresolvedSet) os << "# " << from << " -> " << to << "\n";
	return os;
}

template <> std::ostream& toD2Output<Output>(std::ostream& os, const Output& value)
{
	const auto& specifiedIncludeMap = value.specifiedIncludesMap;
	const auto& unspecifiedIncludeMap = value.unspecifiedIncludesMap;
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
		for (const auto& [from, include] : specifiedIncludeMap) toD2Output(os, include, from, EDetectedIncludeStatus::Allowed);
		os << "\n# forbidden:\n";
		for (const auto& [from, include] : specifiedIncludeMap) toD2Output(os, include, from, EDetectedIncludeStatus::Forbidden);
		os << "\n# unresolved:\n";
		for (const auto& [from, include] : specifiedIncludeMap) toD2Output(os, include, from, EDetectedIncludeStatus::Unresolved);
		os << "\n";
	}
	if (!unspecifiedIncludeMap.empty())
	{
		os << "\n# unspecified include list:\n";
		os << "\n# files:\n";
		for (const auto& [from, _] : unspecifiedIncludeMap) os << from << ".class: unspecified\n";
		os << "\n# resolved:\n";
		for (const auto& [from, include] : unspecifiedIncludeMap) toD2Output(os, include, from, EDetectedIncludeStatus::Allowed);
		os << "\n";
	}
	return os;
}

namespace utility
{
	inline bool bStartWith(std::string_view str, std::string_view prefix, std::string_view* strWithoutPrefix = nullptr)
	{
		if (str.size() < prefix.size()) return false;
		const bool result = str.substr(0, prefix.size()) == prefix;
		if (result && strWithoutPrefix) *strWithoutPrefix = str.substr(prefix.size());
		return result;
	}

	inline bool bEndWith(std::string_view str, std::string_view suffix, std::string_view* strWithoutSuffix = nullptr)
	{
		if (str.size() < suffix.size()) return false;
		auto offset = str.size() - suffix.size();
		const bool result = str.substr(offset) == suffix;
		if (result && strWithoutSuffix) *strWithoutSuffix = str.substr(0, offset);
		return result;
	}

	inline bool isCppFile(const std::string& filePath)
	{
		static auto cppExtensionList = {".h", ".hpp", ".hxx", ".hh", ".h++", ".c", ".cc", ".cpp", ".cxx"};
		for (const auto* cppExtension : cppExtensionList)
			if (bEndWith(filePath, cppExtension)) return true;
		return false;
	}

	inline std::string pathToDotted(const fs::path& p)
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

	inline bool bPathContains(const fs::path& container, const fs::path& contained)
	{
		return bStartWith(container.lexically_normal().string(), contained.lexically_normal().string());
	}

	inline const fs::path* getPatternThatIncludesPath(const fs::path& path, const std::vector<fs::path>& patternList)
	{
		// TODO: check if getPatternThatIncludesPath is correct
		for (const auto& pattern : patternList)
			if (bPathContains(pattern, path)) return &pattern;
		return nullptr;
	}

	inline void updateCppPathList(const fs::path& scanPath,
		std::vector<fs::path>& cppPathList,
		const std::vector<fs::path>& excludePathList,
		const std::vector<fs::path>& forceIncludePathList)
	{
		const bool isForceIncluded = getPatternThatIncludesPath(scanPath, forceIncludePathList) != nullptr;
		const bool isExcluded = !isForceIncluded && getPatternThatIncludesPath(scanPath, excludePathList) != nullptr;
		if (isExcluded) return;
		if (fs::is_regular_file(scanPath))
		{
			if (isCppFile(scanPath.string())) cppPathList.push_back(scanPath);
		}
		else if (fs::is_directory(scanPath))
		{
			for (const auto& file : fs::directory_iterator(scanPath))
				updateCppPathList(file.path(), cppPathList, excludePathList, forceIncludePathList);
		}
	}
} // namespace utility

inline std::vector<std::string> getStdIncludePathList()
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

inline const std::vector<fs::path>* getAllowedToList(const std::map<fs::path, size_t>& allowedIncludeIndexMap,
	const std::vector<std::vector<fs::path>>& allowedIncludeListList,
	const fs::path& cppPath)
{
	for (const auto& [from, index] : allowedIncludeIndexMap)
		if (utility::bPathContains(cppPath, from)) return &allowedIncludeListList[index];
	return nullptr;
}

inline const fs::path* getResolutionIncludePath(
	const fs::path& detectedIncludePath, const std::vector<fs::path>& resolutionIncludePathList)
{
	for (const auto& resolutionIncludePath : resolutionIncludePathList)
	{
		const fs::path& newIncludePath = resolutionIncludePath / detectedIncludePath;
		if (fs::exists(newIncludePath)) return &resolutionIncludePath;
	}
	return nullptr;
}

struct FileInfo
{
	FileInfo(const fs::path& cppPath, const Config& config) :
		cppPath(&cppPath), allowedToList(getAllowedToList(config.allowedIncludeIndexMap, config.allowedIncludeListList, cppPath)),
		groupPath(utility::getPatternThatIncludesPath(cppPath, config.groupPathList)),
		cppDottedPath(utility::pathToDotted(cppPath)),
		isSpecified(config.allowedIncludeIndexMap.empty() || allowedToList != nullptr)
	{
	}

	const fs::path* cppPath; // cannot be nullptr

	const std::vector<fs::path>* allowedToList; // can be nullptr
	const fs::path* groupPath;					// can be nullptr

	std::string cppDottedPath;
	bool isSpecified;
};

inline bool handleCurrentFolderResolution(const std::string& detectedIncludeStr,
	const Config& config,
	const FileInfo& fileInfo,
	Output& /* result */,
	DetectedIncludes& detectedIncludes)
{
	using namespace utility;
	const auto& f = fileInfo; // shortcut

	const fs::path currentFolderIncludePath = f.cppPath->parent_path() / fs::path(detectedIncludeStr);

	if (!fs::exists(currentFolderIncludePath)) return false;
	// include resolved

	const auto* includeGroupPath = getPatternThatIncludesPath(currentFolderIncludePath, config.groupPathList);
	if (includeGroupPath == f.groupPath && includeGroupPath != nullptr) return true;
	// resolution group different from file group

	detectedIncludes.allowedSet.insert(pathToDotted(includeGroupPath != nullptr ? *includeGroupPath : currentFolderIncludePath));
	return true;
}

// returns true if the include is resolved
inline bool handleIncludePathListResolution(const std::string& detectedIncludeStr,
	const Config& config,
	const FileInfo& fileInfo,
	Output& result,
	DetectedIncludes& detectedIncludes)
{
	using namespace utility;
	const auto& f = fileInfo; // shortcut

	const fs::path detectedIncludePath = fs::path(detectedIncludeStr);

	const auto* resolutionIncludePath = getResolutionIncludePath(detectedIncludePath, config.resolutionIncludePathList);
	if (resolutionIncludePath == nullptr) return false;
	// include resolved

	const fs::path resolvedIncludePath = *resolutionIncludePath / detectedIncludePath;

	const bool isForceIncluded = getPatternThatIncludesPath(resolvedIncludePath, config.forceIncludeScanPathList) != nullptr;
	const bool isExcluded
		= !isForceIncluded && getPatternThatIncludesPath(resolvedIncludePath, config.excludeScanPathList) != nullptr;
	if (isExcluded) return true;
	// resolution not excluded

	const bool isAllowed
		= f.allowedToList == nullptr || getPatternThatIncludesPath(resolvedIncludePath, *f.allowedToList) != nullptr;
	if (isAllowed)
	{
		// resolution allowed
		const auto* includeGroupPath = getPatternThatIncludesPath(resolvedIncludePath, config.groupPathList);
		if (includeGroupPath == f.groupPath && includeGroupPath != nullptr) return true;
		// resolution group different from file group

		detectedIncludes.allowedSet.insert(pathToDotted(includeGroupPath != nullptr ? *includeGroupPath : resolvedIncludePath));
	}
	else
	{
		// resolution forbidden
		auto& cppInclude = result.getIncludes(f.isSpecified, f.cppDottedPath);
		cppInclude.forbiddenSet.insert(pathToDotted(resolvedIncludePath));
	}
	return true;
}

inline bool handleStdResolution(const std::string& detectedIncludeStr,
	const Config& config,
	const FileInfo& /* fileInfo */,
	Output& /* result */,
	DetectedIncludes& detectedIncludes)
{
	for (const auto& stdIncludePath : config.stdResolutionIncludePathList)
	{
		const fs::path& newIncludePath = stdIncludePath / fs::path(detectedIncludeStr);
		if (!fs::exists(newIncludePath)) continue;
		// std include resolved

		if (!config.bKeepStdInOutput) return true;
		// keep std in output

		detectedIncludes.allowedSet.insert(detectedIncludeStr);
		return true;
	}
	return false;
}

inline void handleResolution(const std::string& detectedIncludeStr,
	const Config& config,
	const FileInfo& fileInfo,
	Output& result,
	DetectedIncludes& detectedIncludes)
{
	using namespace utility;
	const auto& f = fileInfo; // shortcut

	if (handleCurrentFolderResolution(detectedIncludeStr, config, fileInfo, result, detectedIncludes)
		|| handleIncludePathListResolution(detectedIncludeStr, config, fileInfo, result, detectedIncludes)
		|| handleStdResolution(detectedIncludeStr, config, fileInfo, result, detectedIncludes))
		return;
	// include not resolved

	auto& cppInclude = result.getIncludes(f.isSpecified, f.cppDottedPath);
	cppInclude.unresolvedSet.insert(detectedIncludeStr);
}

inline Output getOutput(const Config& config)
{
	using namespace utility;

	Output result;

	std::vector<fs::path> cppPathList;
	for (const auto& scanPath : config.scanPathList)
		updateCppPathList(scanPath, cppPathList, config.excludeScanPathList, config.forceIncludeScanPathList);

	std::cout << "start parsing..." << "\n";

	for (size_t i = 0; i < cppPathList.size(); ++i)
	{
		std::cout << "\r[" << (i + 1) << "/" << cppPathList.size() << "]" << std::flush;
		FileInfo fileInfo(cppPathList[i], config);
		const auto& f = fileInfo; // shortcut
		const auto groupOrCppDottedPath = pathToDotted(f.groupPath != nullptr ? *f.groupPath : cppPathList[i]);
		auto& detectedIncludes = result.getIncludes(f.isSpecified, groupOrCppDottedPath);
		std::ifstream ifs(cppPathList[i]);
		std::string line;

		while (std::getline(ifs, line))
		{
			std::string_view substr;
			if (!bStartWith(line, "#include ", &substr)) continue;
			// include detected

			auto startPos = substr.find_first_of("\"<");
			if (startPos == std::string::npos) continue;
			auto endPos = substr.find_first_of("\">", startPos + 1);
			const std::string detectedIncludeStr = static_cast<std::string>(substr.substr(startPos + 1, endPos - startPos - 1));

			handleResolution(detectedIncludeStr, config, fileInfo, result, detectedIncludes);
		}
	}

	std::cout << "\nDone\n";

	return result;
}

static void usage(const char* prog)
{
	std::cerr << "Usage: " << prog << " [options] <scan_path> [scan_path ...]\n"
			  << "\n"
			  << "Scan paths:\n"
			  << "  -e <path>                   Exclude path for scanning (folder or file); may be repeated\n"
			  << "  -e !<path>                  Keep path for scanning even if it lies under an exclude\n"
			  << "\n"
			  << "Resolution:\n"
			  << "  -I <path>                   Add include path for resolution (folder or file); may be repeated\n"
			  << "  -A, --allowed <from> <to>   <from> may include <to>; may be repeated\n"
			  << "\n"
			  << "Output:\n"
			  << "  -o <file>                   Write output to file; may be repeated; .json → JSON, else D2\n"
			  << "  --json                       Use JSON for stdout (default: D2)\n"
			  << "  --std                        Include standard library headers in output (default: off)\n"
			  << "  -g, --group <path>           Gather files by group; may be repeated\n"
			  << "\n"
			  << "  -h, --help                  Print this help\n";
}

static bool parseArgs(int argc, char** argv, Config& c)
{
	for (int i = 1; i < argc; ++i)
	{
		std::string a = argv[i];
		if (a == "-I" && i + 1 < argc)
		{
			std::string path = argv[++i];
			if (path.back() == '/') path.pop_back();
			c.resolutionIncludePathList.emplace_back(path);
		}
		else if (a == "-e" && i + 1 < argc)
		{
			std::string path = argv[++i];
			if (path.back() == '/') path.pop_back();
			if (path.front() == '!') c.forceIncludeScanPathList.emplace_back(path.substr(1));
			else
				c.excludeScanPathList.emplace_back(path);
		}
		else if ((a == "-A" || a == "--allowed") && i + 2 < argc)
		{
			std::string fromStr = argv[++i];
			std::string toStr = argv[++i];
			if (!fromStr.empty() && fromStr.back() == '/') fromStr.pop_back();
			if (!toStr.empty() && toStr.back() == '/') toStr.pop_back();
			const fs::path fromPath(fromStr);
			if (c.allowedIncludeIndexMap.find(fromPath) == c.allowedIncludeIndexMap.end())
			{
				c.allowedIncludeIndexMap[fromPath] = c.allowedIncludeListList.size();
				c.allowedIncludeListList.emplace_back();
			}
			c.allowedIncludeListList[c.allowedIncludeIndexMap[fromPath]].emplace_back(toStr);
		}
		else if (a == "-o" && i + 1 < argc)
			c.outputPathList.emplace_back(argv[++i]);
		else if (a == "--std")
			c.bKeepStdInOutput = true;
		else if (a == "--json")
			c.bUseJsonForStdOutput = true;
		else if ((a == "-g" || a == "--group") && i + 1 < argc)
		{
			std::string path = argv[++i];
			if (path.back() == '/') path.pop_back();
			c.groupPathList.emplace_back(path);
		}
		else if (a == "-h" || a == "--help")
		{
			usage(argv[0]);
			return false; // TODO: see how to still return EXIT_SUCCESS for -h/--help
		}
		else if (!a.empty() && a[0] == '-')
		{
			std::cerr << "Error with option: " << a << "\n";
			usage(argv[0]);
			return false;
		}
		else
			c.scanPathList.emplace_back(a);
	}
	if (c.scanPathList.empty())
	{
		std::cerr << "Error: no scan paths provided\n";
		usage(argv[0]);
		return false;
	}
	c.stdResolutionIncludePathList = getStdIncludePathList();
	return true;
}

int main(int argc, char** argv)
{
	Config cfg;
	if (!parseArgs(argc, argv, cfg)) return EXIT_FAILURE;

	const Output output = getOutput(cfg);
	if (cfg.outputPathList.empty())
	{
		std::cout << "\n";
		if (cfg.bUseJsonForStdOutput) toJsonOutput(std::cout, "", output);
		else
			toD2Output(std::cout, output);
	}
	else
	{
		for (const auto& outputPath : cfg.outputPathList)
		{
			if (outputPath.has_parent_path()) fs::create_directories(outputPath.parent_path());
			std::ofstream ofs(outputPath);
			if (!ofs.is_open())
			{
				std::cerr << "Failed to open output file: " << outputPath << "\n";
				continue;
			}
			if (utility::bEndWith(outputPath.string(), ".json")) toJsonOutput(ofs, "", output);
			else
				toD2Output(ofs, output);
			std::cout << "Generated: " << outputPath << "\n";
		}
	}
	return EXIT_SUCCESS;
}
