#pragma once

#include <string_view>

namespace utils::str
{

	inline bool bStartWith(std::string_view str, std::string_view prefix, std::string_view* strWithoutPrefix = nullptr)
	{
		if (str.size() < prefix.size()) return false;
		const bool result = str.substr(0, prefix.size()) == prefix;
		if (result && (strWithoutPrefix != nullptr)) *strWithoutPrefix = str.substr(prefix.size());
		return result;
	}

	inline bool bEndWith(std::string_view str, std::string_view suffix, std::string_view* strWithoutSuffix = nullptr)
	{
		if (str.size() < suffix.size()) return false;
		auto offset = str.size() - suffix.size();
		const bool result = str.substr(offset) == suffix;
		if (result && (strWithoutSuffix != nullptr)) *strWithoutSuffix = str.substr(0, offset);
		return result;
	}

	inline std::string_view removeQuotes(std::string_view str)
	{
		if (str.front() == '\'') return str.substr(1, str.size() - 2);
		return str;
	}
}; // namespace utils::str
