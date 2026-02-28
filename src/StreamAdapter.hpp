#pragma once

#include <functional>
#include <iostream>
#include <utility>

using StreamCallback = std::function<void(std::ostream&)>;

class StreamAdapter
{
public:
	explicit StreamAdapter(StreamCallback&& callback) : callback(std::move(callback)) {}

	friend std::ostream& operator<<(std::ostream& os, const StreamAdapter& adapter)
	{
		adapter.callback(os);
		return os;
	}

private:
	StreamCallback callback;
};