#include "JsonUtility.hpp"
#include "Resolution.hpp"
#include "types.hpp"
#include "utility.hpp"
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

template <typename T> StreamAdapter toJsonOutput(const std::string& /* indent */, const T& /* value */)
{
	static_assert(sizeof(T) == 0, "toJsonOutput not implemented for this type");
	return StreamAdapter([](std::ostream& /* os */) {});
}

template <> StreamAdapter toJsonOutput<DetectedIncludes>(const std::string& indent, const DetectedIncludes& value)
{
	return StreamAdapter(
		[&](std::ostream& os)
		{
			using namespace json_utility;

			auto members = {std::make_pair("allowedSet", &value.allowedSet),
				{"forbiddenSet", &value.forbiddenSet},
				{"unresolvedSet", &value.unresolvedSet}};

			os << toJsonObject(indent,
				members,
				[&](const auto& member, const std::string& /* indent */)
				{
					os << toJsonString(member.first) << ": ";
					os << toJsonArray(*member.second, [&](const auto& value) { os << toJsonString(value); });
				});
		});
}

template <> StreamAdapter toJsonOutput<Output>(const std::string& indent, const Output& value)
{
	return StreamAdapter(
		[&](std::ostream& os)
		{
			using namespace json_utility;

			auto members = {std::make_pair("specifiedIncludeMap", &value.specifiedIncludesMap),
				{"unspecifiedIncludeMap", &value.unspecifiedIncludesMap}};

			os << toJsonObject(indent,
				members,
				[&](const auto& member, const std::string& indent)
				{
					os << toJsonString(member.first) << ": ";
					os << toJsonObject(indent,
						*member.second,
						[&](const auto& value, const std::string& indent)
						{
							os << toJsonString(value.first) << ": ";
							os << toJsonOutput(indent, value.second);
						});
				});
		});
}

template <typename...> struct ToD2OutputUnsupported;
template <typename T, typename... U> StreamAdapter toD2Output(const T& /* value */, const U&... /* args */)
{
	static_assert(sizeof(ToD2OutputUnsupported<T, U...>) == 0, "toD2Output not implemented for these types");
	return StreamAdapter([](std::ostream& /* os */) {});
}

template <>
StreamAdapter toD2Output<DetectedIncludes, std::string, EDetectedIncludeStatus>(
	const DetectedIncludes& value, const std::string& from, const EDetectedIncludeStatus& status)
{
	return StreamAdapter(
		[&](std::ostream& os)
		{
			if (status == EDetectedIncludeStatus::Allowed)
				for (const auto& to : value.allowedSet) os << from << " -> " << to << "\n";
			else if (status == EDetectedIncludeStatus::Forbidden)
				for (const auto& to : value.forbiddenSet) os << from << " -> " << to << ": {class: forbidden}\n";
			else
			{
				for (const auto& to : value.unresolvedSet)
					os << to << ".class: unresolved\n" << from << " -> " << to << ": {class: unresolved}\n";
			}
		});
}

template <> StreamAdapter toD2Output<Output>(const Output& value)
{
	return StreamAdapter(
		[&](std::ostream& os)
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
	  fill: "orange"
    }
  }
  unresolved: {
    style: {
      stroke: "gray"
	  fill: "gray"
    }
  }
}
vars: {
  d2-legend: {
    a: {
      label: Specified
    }
    b: Unspecified {
      label: Unspecified
      class: unspecified
    }
    c: Unresolved {
      label: Unresolved
      class: unresolved
    }
    a -> b: Allowed
    a -> b: Forbidden {
      class: forbidden
    }
    a -> b: Unresolved {
      class: unresolved
    }
  }
}
)";
			if (!specifiedIncludeMap.empty())
			{
				os << "\n# specified include list:\n";
				os << "\n# files:\n";
				for (const auto& [from, _] : specifiedIncludeMap) os << from << "\n";
				os << "\n# allowed:\n";
				for (const auto& [from, include] : specifiedIncludeMap)
					os << toD2Output(include, from, EDetectedIncludeStatus::Allowed);
				os << "\n# forbidden:\n";
				for (const auto& [from, include] : specifiedIncludeMap)
					os << toD2Output(include, from, EDetectedIncludeStatus::Forbidden);
				os << "\n# unresolved:\n";
				for (const auto& [from, include] : specifiedIncludeMap)
					os << toD2Output(include, from, EDetectedIncludeStatus::Unresolved);
			}
			if (!unspecifiedIncludeMap.empty())
			{
				os << "\n# unspecified include list:\n";
				os << "\n# files:\n";
				for (const auto& [from, _] : unspecifiedIncludeMap) os << from << ".class: unspecified\n";
				os << "\n# resolved:\n";
				for (const auto& [from, include] : unspecifiedIncludeMap)
					os << toD2Output(include, from, EDetectedIncludeStatus::Allowed);
			}
		});
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

		const FileInfo fileInfo(cppPathList[i], config);
		const auto& f = fileInfo; // shortcut
		const auto groupOrCppDottedPath = pathToDotted(f.groupPath != nullptr ? *f.groupPath : cppPathList[i]);

		// ensure file/group is created even if no includes are detected
		auto& detectedIncludes = result.getIncludes(f.isSpecified, f.isSpecified ? groupOrCppDottedPath : f.cppDottedPath);

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

			Resolution::handleResolution(detectedIncludeStr, config, fileInfo, result, detectedIncludes);
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

int main(int argc, char** argv)
{
	Config cfg;
	if (!cfg.parseArgs(argc, argv))
	{
		usage(argv[0]);
		return cfg.bHelp ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	const Output output = getOutput(cfg);
	if (cfg.outputPathList.empty())
	{
		std::cout << "\n";
		if (cfg.bUseJsonForStdOutput) std::cout << toJsonOutput("", output);
		else
			std::cout << toD2Output(output);
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
			if (utility::bEndWith(outputPath.string(), ".json")) ofs << toJsonOutput("", output);
			else
				ofs << toD2Output(output);
			std::cout << "Generated: " << outputPath << "\n";
		}
	}

	return EXIT_SUCCESS;
}
