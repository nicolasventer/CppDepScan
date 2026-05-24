#pragma once

#include <filesystem>
#include <map>


// source file path -> corresponding header file path
using SourceToHeaderMap = std::map<std::filesystem::path, std::filesystem::path>;