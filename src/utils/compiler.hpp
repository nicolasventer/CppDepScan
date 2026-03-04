#pragma once

#include <array>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#define popen  _popen
#define pclose _pclose
#endif

namespace utils::compiler
{
	namespace fs = std::filesystem;

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
} // namespace utils::compiler