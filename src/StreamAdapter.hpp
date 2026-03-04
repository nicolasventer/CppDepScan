#pragma once

#include <functional>
#include <iostream>
#include <utility>

using OStreamCallback = std::function<void(std::ostream& os)>;

class OStreamAdapter
{
public:
	explicit OStreamAdapter(OStreamCallback&& callback) : callback(std::move(callback)) {}

	friend std::ostream& operator<<(std::ostream& os, const OStreamAdapter& adapter)
	{
		adapter.callback(os);
		return os;
	}

private:
	OStreamCallback callback;
};

using IStreamCallback = std::function<void(std::istream& is)>;

class IStreamAdapter
{
public:
	explicit IStreamAdapter(IStreamCallback&& callback) : callback(std::move(callback)) {}

	friend std::istream& operator>>(std::istream& is, const IStreamAdapter& adapter)
	{
		adapter.callback(is);
		return is;
	}

private:
	IStreamCallback callback;
};
