#pragma once

#include "languages/CppLanguage.hpp"
#include "ILanguage.hpp"
#include <memory>
#include <string>

[[nodiscard]] inline std::shared_ptr<ILanguage> createLanguage(const std::string& language)
{
	if (language == "cpp" || language == "c++") return std::make_shared<CppLanguage>();
	if (language == "typescript" || language == "ts") return nullptr;
	return nullptr;
}
