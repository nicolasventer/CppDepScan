#pragma once

#include <string>

namespace utils::special
{
	inline bool isSpecial(const std::string& str)
	{
		return (str.find_first_of(".@$") != std::string::npos) || str == "classes" || str == "top";
	}
} // namespace utils::special