#pragma once

#include <functional>
#include <iostream>
#include <string>

namespace json_utility
{
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
			os << ", ";
			func(*it);
			++it;
		}
		os << "]";
		return os;
	}
} // namespace json_utility