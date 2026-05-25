#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "./yyjson/yyjson.h"

#ifdef IMPORT_YYJSON_IMPL
#include "./yyjson/yyjson.c" // NOLINT(bugprone-suspicious-include)
#endif

enum class EPrettyPrint : uint8_t
{
	None = YYJSON_WRITE_NOFLAG,
	Pretty = YYJSON_WRITE_PRETTY,
	PrettyTwoSpaces = YYJSON_WRITE_PRETTY_TWO_SPACES
};

namespace EReadFlag // NOLINT(readability-identifier-naming)
{
	enum Type : uint8_t // NOLINT(readability-identifier-naming)
	{
		None = YYJSON_READ_NOFLAG,
		AllowTrailingCommas = YYJSON_READ_ALLOW_TRAILING_COMMAS,
		AllowComments = YYJSON_READ_ALLOW_COMMENTS,
		AllowCommentsAndTrailingCommas = AllowComments | AllowTrailingCommas,
	};
} // namespace EReadFlag

// NOLINTBEGIN(google-explicit-constructor, hicpp-explicit-conversions, hicpp-no-malloc, cppcoreguidelines-no-malloc,
// cppcoreguidelines-owning-memory, cppcoreguidelines-avoid-c-arrays)

/**
 * @brief Owns an immutable yyjson document parsed from a JSON string.
 *
 * @par Example
 * @code{.cpp}
 * struct Person {
 *     std::string name;
 *     int age;
 *     std::vector<std::string> hobbies;
 * };
 *
 * template <> Person fromJson(const ValueWrapper& doc) {
 *     return Person{doc["name"], doc["age"], doc["hobbies"]};
 * }
 *
 * const std::string json = R"({"name":"Alice","age":25,"hobbies":["reading","coding"]})";
 * DocWrapper doc(json);
 * ValueWrapper root = doc;
 *
 * Person p = root; // uses fromJson<Person> via implicit conversion
 *
 * std::string name = root["name"];
 * int age = root["age"];
 * std::vector<std::string> hobbies = root["hobbies"];
 *
 * std::string pretty = doc.toString(EPrettyPrint::Pretty);
 * @endcode
 */
class DocWrapper
{
public:
	class ValueWrapper
	{
	public:
		ValueWrapper() = default;
		ValueWrapper(yyjson_val* val, yyjson_doc* doc) : val_(val), doc_(doc) {}

		// Conversion operators
		operator int() const { return yyjson_get_int(val_); }
		operator int64_t() const { return yyjson_get_sint(val_); }
		operator uint64_t() const { return yyjson_get_uint(val_); }
		operator double() const { return yyjson_get_num(val_); }
		operator bool() const { return yyjson_get_bool(val_); }
		operator std::string() const { return yyjson_get_str(val_); }
		template <typename T> operator std::vector<T>() const
		{
			std::vector<T> res;
			if (yyjson_is_arr(val_))
			{
				size_t idx = 0;
				size_t max = 0;
				yyjson_val* val = nullptr;
				yyjson_arr_foreach(val_, idx, max, val)
				{
					ValueWrapper elem(val, doc_);
					res.push_back(static_cast<T>(elem));
				}
			}
			return res;
		}
		template <typename T, size_t N> operator std::array<T, N>() const
		{
			std::array<T, N> res{};
			if (yyjson_is_arr(val_))
			{
				size_t idx = 0;
				size_t max = 0;
				yyjson_val* val = nullptr;
				yyjson_arr_foreach(val_, idx, max, val)
				{
					if (idx >= N) break;
					ValueWrapper elem(val, doc_);
					res[idx] = static_cast<T>(elem);
				}
			}
			return res;
		}
		// Fill a C-style array (cannot be returned by value)
		template <typename T, size_t N> void fillArray(T (&arr)[N]) const
		{
			if (!yyjson_is_arr(val_)) return;
			size_t i = 0;
			size_t idx = 0;
			size_t max = 0;
			yyjson_val* val = nullptr;
			yyjson_arr_foreach(val_, idx, max, val)
			{
				if (i >= N) break;
				ValueWrapper elem(val, doc_);
				arr[i++] = static_cast<T>(elem);
			}
		}
		template <typename T, typename U> operator std::map<T, U>() const
		{
			std::map<T, U> res;
			if (yyjson_is_obj(val_))
			{
				yyjson_obj_iter iter = yyjson_obj_iter_with(val_);
				yyjson_val *key, *val;
				while ((key = yyjson_obj_iter_next(&iter)))
				{
					val = yyjson_obj_iter_get_val(key);
					res[static_cast<T>(ValueWrapper(key, doc_))] = static_cast<U>(ValueWrapper(val, doc_));
				}
			}
			return res;
		}
		template <typename T, typename U> operator std::vector<std::pair<T, U>>() const
		{
			std::vector<std::pair<T, U>> res;
			if (yyjson_is_obj(val_))
			{
				yyjson_obj_iter iter = yyjson_obj_iter_with(val_);
				yyjson_val *key, *val;
				while ((key = yyjson_obj_iter_next(&iter)))
				{
					val = yyjson_obj_iter_get_val(key);
					res.emplace_back({static_cast<T>(key), static_cast<U>(val)});
				}
			}
			return res;
		}
		template <typename T> operator T() const; // Custom types: uses fromJson<T>
		// Explicit conversion, second type is the intermediate type
		template <typename TTo, typename TFrom> TTo internal_cast() const { return static_cast<TTo>(static_cast<TFrom>(*this)); }

		// Object/array access
		ValueWrapper operator[](const char* key) const { return {yyjson_obj_get(val_, key), doc_}; }
		ValueWrapper operator[](size_t index) const { return {yyjson_arr_get(val_, index), doc_}; }
		ValueWrapper operator[](int index) const { return {yyjson_arr_get(val_, static_cast<size_t>(index)), doc_}; }

		bool hasKey(const char* key) const { return yyjson_obj_get(val_, key) != nullptr; }

		std::string toString(EPrettyPrint prettyPrint = EPrettyPrint::None) const
		{
			char* json = yyjson_val_write(val_, static_cast<uint32_t>(prettyPrint), nullptr);
			std::string result(json);
			free(json);
			return result;
		}

		yyjson_val* val_ = nullptr; // Direct access to underlying value
		yyjson_doc* doc_ = nullptr; // Direct access to underlying document
	};

	DocWrapper() = default;
	// Parse JSON from string
	DocWrapper(const char* data, size_t len, EReadFlag::Type readFlag = EReadFlag::None) :
		doc(yyjson_read(data, len, readFlag)), root(yyjson_doc_get_root(doc))
	{
	}
	DocWrapper(const std::string& data, EReadFlag::Type readFlag = EReadFlag::None) :
		doc(yyjson_read(data.c_str(), data.length(), readFlag)), root(yyjson_doc_get_root(doc))
	{
	}
	DocWrapper(const DocWrapper&) = delete;
	DocWrapper& operator=(const DocWrapper&) = delete;
	DocWrapper(DocWrapper&& other) noexcept : doc(other.doc), root(other.root)
	{
		other.doc = nullptr;
		other.root = nullptr;
	}
	DocWrapper& operator=(DocWrapper&& other) noexcept
	{
		if (this != &other)
		{
			if (doc) yyjson_doc_free(doc);
			doc = other.doc;
			root = other.root;
			other.doc = nullptr;
			other.root = nullptr;
		}
		return *this;
	}
	~DocWrapper()
	{
		if (doc) yyjson_doc_free(doc);
	}

	operator ValueWrapper() const { return {root, doc}; }
	std::string toString(EPrettyPrint prettyPrint = EPrettyPrint::None) const
	{
		char* json = yyjson_write(doc, static_cast<uint32_t>(prettyPrint), nullptr);
		std::string result(json);
		free(json);
		return result;
	}

	yyjson_doc* doc = nullptr;
	yyjson_val* root = nullptr;
};

/**
 * @brief Owns a mutable yyjson document for building or modifying JSON.
 *
 * @par Example
 * @code{.cpp}
 * struct Person {
 *     std::string name;
 *     int age;
 *     std::vector<std::string> hobbies;
 * };
 *
 * template <> void toJson(MutValueWrapper& value, const Person& p) {
 *     value.set("name", p.name, "age", p.age, "hobbies", p.hobbies);
 * }
 *
 * MutDocWrapper mutDoc;
 * MutValueWrapper root = mutDoc;
 *
 * Person p{"Alice", 25, {"reading", "coding"}};
 * root = p; // uses toJson<Person> via assignment
 *
 * root.set("score", 100);
 * root.addMap(std::map<std::string, int>{{"bonus", 10}});
 *
 * std::string compact = mutDoc.toString();
 * std::string pretty = mutDoc.toString(EPrettyPrint::Pretty);
 * @endcode
 */
class MutDocWrapper
{
public:
	class MutValueWrapper
	{
	public:
		MutValueWrapper() = default;
		MutValueWrapper(yyjson_mut_val* val, yyjson_mut_doc* mutDoc) : val_(val), mutDoc_(mutDoc) {}

		// Ensure value is an object
		MutValueWrapper& asObj()
		{
			if (!yyjson_mut_is_obj(val_)) yyjson_mut_set_obj(val_);
			return *this;
		}
		// Set key-value pairs (variadic). Does not override existing keys; add only.
		template <typename... Args> void set(Args&&... args)
		{
			asObj();
			setNoCheck(std::forward<Args>(args)...);
		}
		// Append all elements of a map to the object.
		template <typename T, typename U> void addMap(const std::map<T, U>& valueList)
		{
			asObj();
			for (const auto& [key, value] : valueList) setNoCheck(key.c_str(), value);
		}
		// Append all elements of a vector of pairs to the object.
		template <typename T, typename U> void addMap(const std::vector<std::pair<T, U>>& valueList)
		{
			asObj();
			for (const auto& [key, value] : valueList) setNoCheck(key.c_str(), value);
		}

		void setNoCheck() {}
		template <typename T, typename... Args> void setNoCheck(const char* key, const T& value, Args&&... args)
		{
			setNoCheck(key, value);
			setNoCheck(std::forward<Args>(args)...);
		}
		template <typename T> void setNoCheck(const char* key, const T& value)
		{
			yyjson_mut_val* val = createValue(value);
			yyjson_mut_obj_add_val(mutDoc_, val_, key, val);
		}

		// Ensure value is an array
		MutValueWrapper& asArr()
		{
			if (!yyjson_mut_is_arr(val_)) yyjson_mut_set_arr(val_);
			return *this;
		}
		// Append one or more elements to the array (variadic).
		template <typename... Args> void add(Args&&... args)
		{
			asArr();
			addNoCheck(std::forward<Args>(args)...);
		}
		// Append all elements of a vector to the array.
		template <typename T> void addArray(const std::vector<T>& valueList)
		{
			asArr();
			for (const auto& value : valueList) addNoCheck(value);
		}
		// Append all elements of a std::array to the array.
		template <typename T, size_t N> void addArray(const std::array<T, N>& valueList)
		{
			asArr();
			for (const auto& value : valueList) addNoCheck(value);
		}
		// Append all elements of a C-style array to the array.
		template <typename T, size_t N> void addArray(const T (&valueList)[N])
		{
			asArr();
			for (size_t i = 0; i < N; ++i) addNoCheck(valueList[i]);
		}

		void addNoCheck() {}
		template <typename T> void addNoCheck(const T& value)
		{
			yyjson_mut_val* val = createValue(value);
			yyjson_mut_arr_append(val_, val);
		}
		template <typename T, typename... Args> void addNoCheck(const T& value, Args&&... args)
		{
			addNoCheck(value);
			addNoCheck(std::forward<Args>(args)...);
		}

		std::string toString(EPrettyPrint prettyPrint = EPrettyPrint::None) const
		{
			char* json = yyjson_mut_val_write(val_, static_cast<uint32_t>(prettyPrint), nullptr);
			std::string result(json);
			free(json);
			return result;
		}

		// Assignment: set this value to a primitive, vector, array, or custom type

		MutValueWrapper& operator=(const int& value)
		{
			yyjson_mut_set_int(val_, value);
			return *this;
		}
		MutValueWrapper& operator=(const int64_t& value)
		{
			yyjson_mut_set_sint(val_, value);
			return *this;
		}
		MutValueWrapper& operator=(const uint64_t& value)
		{
			yyjson_mut_set_uint(val_, value);
			return *this;
		}
		MutValueWrapper& operator=(const double& value)
		{
			yyjson_mut_set_real(val_, value);
			return *this;
		}
		MutValueWrapper& operator=(const bool& value)
		{
			yyjson_mut_set_bool(val_, value);
			return *this;
		}
		MutValueWrapper& operator=(const std::string& value)
		{
			yyjson_mut_set_str(val_, value.c_str());
			return *this;
		}
		MutValueWrapper& operator=(const char* value)
		{
			yyjson_mut_set_str(val_, value);
			return *this;
		}
		template <typename T> MutValueWrapper& operator=(const std::vector<T>& value)
		{
			val_ = createValue(value);
			return *this;
		}
		template <typename T, size_t N> MutValueWrapper& operator=(const std::array<T, N>& value)
		{
			val_ = createValue(value);
			return *this;
		}
		template <typename T, size_t N> MutValueWrapper& operator=(const T (&value)[N])
		{
			val_ = createValue(value);
			return *this;
		}
		template <typename T, typename U> MutValueWrapper& operator=(const std::map<T, U>& value)
		{
			val_ = createValue(value);
			return *this;
		}
		template <typename T, typename U> MutValueWrapper& operator=(const std::vector<std::pair<T, U>>& value)
		{
			val_ = createValue(value);
			return *this;
		}
		template <typename T> MutValueWrapper& operator=(const T& value)
		{
			toJson(*this, value);
			return *this;
		}

		yyjson_mut_val* val_ = nullptr;	   // Direct access to underlying value
		yyjson_mut_doc* mutDoc_ = nullptr; // Direct access to underlying document
	private:
		yyjson_mut_val* createValue(int value) { return yyjson_mut_int(mutDoc_, value); }
		yyjson_mut_val* createValue(int64_t value) { return yyjson_mut_sint(mutDoc_, value); }
		yyjson_mut_val* createValue(uint64_t value) { return yyjson_mut_uint(mutDoc_, value); }
		yyjson_mut_val* createValue(double value) { return yyjson_mut_real(mutDoc_, value); }
		yyjson_mut_val* createValue(bool value) { return yyjson_mut_bool(mutDoc_, value); }
		yyjson_mut_val* createValue(const std::string& value) { return yyjson_mut_strcpy(mutDoc_, value.c_str()); }
		yyjson_mut_val* createValue(const char* value) { return yyjson_mut_strcpy(mutDoc_, value); }
		template <typename T> yyjson_mut_val* createValue(const std::vector<T>& value)
		{
			yyjson_mut_val* arr = yyjson_mut_arr(mutDoc_);
			for (const auto& v : value)
			{
				yyjson_mut_val* val = createValue(v);
				yyjson_mut_arr_append(arr, val);
			}
			return arr;
		}
		template <typename T, size_t N> yyjson_mut_val* createValue(const std::array<T, N>& value)
		{
			yyjson_mut_val* arr = yyjson_mut_arr(mutDoc_);
			for (const auto& v : value)
			{
				yyjson_mut_val* val = createValue(v);
				yyjson_mut_arr_append(arr, val);
			}
			return arr;
		}
		template <typename T, size_t N> yyjson_mut_val* createValue(const T (&value)[N])
		{
			yyjson_mut_val* arr = yyjson_mut_arr(mutDoc_);
			for (size_t i = 0; i < N; ++i)
			{
				yyjson_mut_val* val = createValue(value[i]);
				yyjson_mut_arr_append(arr, val);
			}
			return arr;
		}
		template <typename T, typename U> yyjson_mut_val* createValue(const std::map<T, U>& value)
		{
			yyjson_mut_val* obj = yyjson_mut_obj(mutDoc_);
			for (const auto& [key, value] : value)
			{
				yyjson_mut_val* val = createValue(value);
				yyjson_mut_obj_add_val(mutDoc_, obj, key.c_str(), val);
			}
			return obj;
		}
		template <typename T, typename U> yyjson_mut_val* createValue(const std::vector<std::pair<T, U>>& value)
		{
			yyjson_mut_val* obj = yyjson_mut_obj(mutDoc_);
			for (const auto& [key, value] : value)
			{
				yyjson_mut_val* val = createValue(value);
				yyjson_mut_obj_add_val(mutDoc_, obj, key.c_str(), val);
			}
			return obj;
		}
		template <typename T> yyjson_mut_val* createValue(const T& value)
		{
			yyjson_mut_val* obj = yyjson_mut_obj(mutDoc_);
			MutValueWrapper wrapper(obj, mutDoc_);
			toJson(wrapper, value);
			return obj;
		}
	};

	MutDocWrapper() : mutDoc(yyjson_mut_doc_new(nullptr))
	{
		if (mutDoc) yyjson_mut_doc_set_root(mutDoc, yyjson_mut_obj(mutDoc));
	}
	MutDocWrapper(const MutDocWrapper&) = delete;
	MutDocWrapper& operator=(const MutDocWrapper&) = delete;
	MutDocWrapper(MutDocWrapper&& other) noexcept : mutDoc(other.mutDoc) { other.mutDoc = nullptr; }
	MutDocWrapper& operator=(MutDocWrapper&& other) noexcept
	{
		if (this != &other)
		{
			if (mutDoc) yyjson_mut_doc_free(mutDoc);
			mutDoc = other.mutDoc;
			other.mutDoc = nullptr;
		}
		return *this;
	}
	~MutDocWrapper()
	{
		if (mutDoc) yyjson_mut_doc_free(mutDoc);
	}

	operator MutValueWrapper() const { return {yyjson_mut_doc_get_root(mutDoc), mutDoc}; }
	std::string toString(EPrettyPrint prettyPrint = EPrettyPrint::None) const
	{
		char* json = yyjson_mut_write(mutDoc, static_cast<uint32_t>(prettyPrint), nullptr);
		std::string result(json);
		free(json);
		return result;
	}

	yyjson_mut_doc* mutDoc = nullptr;
};

// NOLINTEND(google-explicit-constructor, hicpp-explicit-conversions, hicpp-no-malloc, cppcoreguidelines-no-malloc,
// cppcoreguidelines-owning-memory, cppcoreguidelines-avoid-c-arrays)

// Custom type conversion (specialize fromJson and toJson for your types)
template <typename T> T fromJson(const DocWrapper::ValueWrapper& /* val */)
{
	static_assert(sizeof(T) == 0, "fromJson not implemented for this type");
	return T();
}

template <typename T> DocWrapper::ValueWrapper::operator T() const { return fromJson<T>(*this); }

template <typename T> void toJson(MutDocWrapper::MutValueWrapper& /* value */, const T& /* data */)
{
	static_assert(sizeof(T) == 0, "toJson not implemented for this type");
}

using ValueWrapper = DocWrapper::ValueWrapper;
using MutValueWrapper = MutDocWrapper::MutValueWrapper;

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define PRIMITIVE_CONVERSION(toType, fromType)                                                \
	template <>(toType) inline fromJson<toType>(const DocWrapper::ValueWrapper& val)          \
	{                                                                                         \
		return val.internal_cast<toType, fromType>();                                         \
	}                                                                                         \
	template <> void inline toJson(MutDocWrapper::MutValueWrapper& value, const toType& data) \
	{                                                                                         \
		value = static_cast<fromType>(data);                                                  \
	}

#ifdef USE_PRIMITIVE_CONVERSION

PRIMITIVE_CONVERSION(unsigned int, uint64_t)
PRIMITIVE_CONVERSION(signed char, int)
PRIMITIVE_CONVERSION(unsigned char, uint64_t)
PRIMITIVE_CONVERSION(unsigned short, int)
PRIMITIVE_CONVERSION(float, double)

#endif
