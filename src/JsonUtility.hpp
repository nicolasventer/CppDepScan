#pragma once

#include "StreamAdapter.hpp"
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>

namespace json_utility
{
	inline auto toJsonString(const std::string& s) { return std::quoted(s); }

	template <typename T>
	auto toJsonObject(const std::string& indent,
		const T& range,
		const std::function<void(const typename T::value_type&, const std::string& indent)>& func)
	{
		return StreamAdapter(
			[&](std::ostream& os)
			{
				os << '{';
				auto newIndent = indent + "  ";
				auto it = range.begin();
				auto itEnd = range.end();
				if (it != itEnd)
				{
					os << '\n' << newIndent;
					func(*it, newIndent);
					++it;
				}
				while (it != itEnd)
				{
					os << ",\n" << newIndent;
					func(*it, newIndent);
					++it;
				}
				os << '\n' << indent << '}';
			});
	}

	template <typename T> auto toJsonArray(const T& range, const std::function<void(const typename T::value_type&)>& func)
	{
		return StreamAdapter(
			[&](std::ostream& os)
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
			});
	}
} // namespace json_utility