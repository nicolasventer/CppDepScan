#pragma once

#include "ILanguage.hpp"
#include "languages/CppLanguage.hpp"
#include "languages/TypeScriptLanguage.hpp"
#include <memory>
#include <string>

[[nodiscard]] inline std::shared_ptr<ILanguage> createLanguage(const std::string& language)
{
	if (language == "cpp" || language == "c++") return std::make_shared<CppLanguage>();
	if (language == "typescript" || language == "ts") return std::make_shared<TypeScriptLanguage>();
	return nullptr;
}
