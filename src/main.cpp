#include "Config.hpp"
#include "FileInfo.hpp"
#include "Output.hpp"
#include "Resolution.hpp"
#include "ResolutionOutput.hpp"
#include "SourceToHeaderMap.hpp"
#include "utils/StreamAdapter.hpp"
#include "utils/json.hpp"
#include "utils/str.hpp"
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

using OStreamAdapter = utils::OStreamAdapter;

template <typename T> static OStreamAdapter toJsonOutput(const std::string& /* indent */, const T& /* value */)
{
	static_assert(sizeof(T) == 0, "toJsonOutput not implemented for this type");
	return OStreamAdapter([](std::ostream& /* os */) {});
}

template <> OStreamAdapter toJsonOutput<DetectedModules>(const std::string& indent, const DetectedModules& value)
{
	return OStreamAdapter(
		[&](std::ostream& os)
		{
			using namespace utils::json;

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

template <> OStreamAdapter toJsonOutput<Output>(const std::string& indent, const Output& value)
{
	return OStreamAdapter(
		[&](std::ostream& os)
		{
			using namespace utils::json;

			auto members = {
				// std::make_pair("commandLine", &value.commandLine), // TODO: add commandLine to JSON output
				std::make_pair("specifiedModulesMap", &value.specifiedModulesMap),
				std::make_pair("unspecifiedModulesMap", &value.unspecifiedModulesMap),
				// std::make_pair("importerAllowInfoMap", &value.importerAllowInfoMap), // TODO: add importerAllowInfoMap to JSON output
			};

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
template <typename T, typename... U> static OStreamAdapter toD2Output(const T& /* value */, const U&... /* args */)
{
	static_assert(sizeof(ToD2OutputUnsupported<T, U...>) == 0, "toD2Output not implemented for these types");
	return OStreamAdapter([](std::ostream& /* os */) {});
}

template <>
OStreamAdapter toD2Output<DetectedModules, std::string, EDetectedModuleStatus>(
	const DetectedModules& value, const std::string& importer, const EDetectedModuleStatus& status)
{
	return OStreamAdapter(
		[&](std::ostream& os)
		{
			if (status == EDetectedModuleStatus::Allowed)
				for (const auto& module : value.allowedSet) os << importer << " -> " << module << "\n";
			else if (status == EDetectedModuleStatus::Forbidden)
				for (const auto& module : value.forbiddenSet) os << importer << " -> " << module << ": {class: forbidden}\n";
			else
				for (const auto& module : value.unresolvedSet)
					os << module << ".class: unresolved\n" << importer << " -> " << module << ": {class: unresolved}\n";
		});
}

template <> OStreamAdapter toD2Output<Output>(const Output& value)
{
	return OStreamAdapter(
		[&](std::ostream& os)
		{
			const auto& specifiedModulesMap = value.specifiedModulesMap;
			const auto& unspecifiedModulesMap = value.unspecifiedModulesMap;
			const auto& importerAllowInfoMap = value.importerAllowInfoMap;
			os << "#";
			for (const auto& arg : value.commandLine) os << " " << arg;
			os << R"(

classes: {
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
			if (!specifiedModulesMap.empty())
			{
				os << "\n# specified importer list:\n";
				os << "\n# files:\n";
				for (const auto& [importer, _] : specifiedModulesMap)
				{
					os << importer;
					auto it = importerAllowInfoMap.find(importer);
					if (it != importerAllowInfoMap.end())
					{
						os << ": " << it->second.fileName << " {tooltip: part of \"" << it->second.from << "\", can include: ";
						auto listIt = it->second.toList->begin();
						auto listItEnd = it->second.toList->end();
						if (listIt == listItEnd) os << "None";
						else
						{
							os << "\"" << listIt->getPattern() << "\"";
							++listIt;
							while (listIt != listItEnd)
							{
								os << ", \"" << listIt->getPattern() << "\"";
								++listIt;
							}
						}
						os << "}";
					}
					os << "\n";
				}
				os << "\n# allowed:\n";
				for (const auto& [importer, modules] : specifiedModulesMap)
					os << toD2Output(modules, importer, EDetectedModuleStatus::Allowed);
				os << "\n# forbidden:\n";
				for (const auto& [importer, modules] : specifiedModulesMap)
					os << toD2Output(modules, importer, EDetectedModuleStatus::Forbidden);
				os << "\n# unresolved:\n";
				for (const auto& [importer, modules] : specifiedModulesMap)
					os << toD2Output(modules, importer, EDetectedModuleStatus::Unresolved);
			}
			if (!unspecifiedModulesMap.empty())
			{
				os << "\n# unspecified importer list:\n";
				os << "\n# files:\n";
				for (const auto& [importer, _] : unspecifiedModulesMap) os << importer << ".class: unspecified\n";
				os << "\n# resolved:\n";
				for (const auto& [importer, modules] : unspecifiedModulesMap)
					os << toD2Output(modules, importer, EDetectedModuleStatus::Allowed);
			}
		});
}

static Output getOutput(const Config& config)
{
	ResolutionOutput resolution;

	std::vector<fs::path> importerPathList = config.getImporterPathList();

	std::cout << "start parsing..." << "\n";

	SourceToHeaderMap sourceToHeaderMap;

	std::cout << "step 1/2: resolving modules..." << "\n";

	for (size_t i = 0; i < importerPathList.size(); ++i)
	{
		std::cout << "\r[" << (i + 1) << "/" << importerPathList.size() << "]" << std::flush;

		const FileInfo fileInfo(importerPathList[i], config);

		auto& modules = resolution.getModules(fileInfo.isSpecified, importerPathList[i]);

		std::ifstream ifs(importerPathList[i]);
		std::string line;
		while (std::getline(ifs, line))
		{
			std::string_view substr;
			if (!utils::str::bStartWith(line, "#include ", &substr)) continue;

			// module detected

			auto startPos = substr.find_first_of("\"<");
			if (startPos == std::string::npos) continue;
			auto endPos = substr.find_first_of("\">", startPos + 1);
			const std::string detectedModuleStr = static_cast<std::string>(substr.substr(startPos + 1, endPos - startPos - 1));

			Resolution::handleResolution(detectedModuleStr, config, fileInfo, modules, sourceToHeaderMap);
		}
	}

	std::cout << "\nstep 2/2: generating output..." << "\n";

	Output output(config, resolution, sourceToHeaderMap);

	std::cout << "\nDone\n";

	return output;
}

static void usage(const char* prog)
{
	std::cerr
		<< "Usage: " << prog << " [options] <scan_glob> [scan_glob ...]\n"
		<< "\n"
		<< "Scan globs (positional): files, directories, or glob patterns; may be repeated\n"
		<< "\n"
		<< "Scan filters:\n"
		<< "  -e <glob>                   Exclude glob for scanning (folder, file, or pattern); may be repeated\n"
		<< "  -e !<glob>                  Keep glob for scanning even if it lies under an exclude\n"
		<< "\n"
		<< "Resolution:\n"
		<< "  -i <path>                   Add include path for resolution (folder or file); may be repeated\n"
		<< "  -a, --allowed <from> <to>   Specify that <from> may include <to>; <from> and <to> are globs; may be repeated\n"
		<< "\n"
		<< "Output:\n"
		<< "  -o <file>                   Write output to file; may be repeated; .json -> JSON, else D2\n"
		<< "  --json                       Use JSON for stdout (default: D2)\n"
		<< "  --std                        Include standard library headers in output (default: false)\n"
		<< "  --brother-links              Make links always between elements in the same folder (default: false)\n"
		<< "  --group-source-header        Group source with its corresponding header file (default: false)\n"
		<< "  -g, --group <glob>           Gather files by group glob; may be repeated\n"
		<< "\n"
		<< "Glob syntax:\n"
		<< "  '*'  matches within one path segment\n"
		<< "  '**' matches across segments\n"
		<< "\n"
		<< "  - Note (Windows/cmd): quote glob patterns to avoid shell expansion\n"
		<< "\n"
		<< "  -h, --help                  Print this help\n";
}

int main(int argc, char* argv[])
{
	Config cfg;
	if (!cfg.parseArgs(static_cast<size_t>(argc), argv))
	{
		usage(argv[0]);
		return cfg.bHelp ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	const Output output = getOutput(cfg);
	if (cfg.outputPathList.empty())
	{
		std::cout << "\n";
		if (cfg.bUseJsonForStdOutput) std::cout << toJsonOutput("", output);
		else std::cout << toD2Output(output);
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
			if (utils::str::bEndWith(outputPath.string(), ".json")) ofs << toJsonOutput("", output);
			else ofs << toD2Output(output);
			std::cout << "Generated: " << outputPath << "\n";
		}
	}

	if (output.hasUnresolved || !output.unspecifiedModulesMap.empty()) return EXIT_FAILURE;
	return EXIT_SUCCESS;
}
