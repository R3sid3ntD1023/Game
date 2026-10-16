#pragma once

#include <any>
#include <memory>
#include <string>
#include <unordered_map>

namespace ai
{
	// A simple key-value store for AI data, using std::any to hold values of any type.
	class Blackboard
	{
	public:
		template <typename T>
		void Add(std::string_view key, const T &value)
		{
			data[std::string(key)] = std::any(value);
		}

		template <typename T>
		void Set(std::string_view key, const T &value)
		{
			if (data.contains(std::string(key)))
				data[std::string(key)] = std::any(value);
			else
				throw std::runtime_error("Key not found");
		}

		template <typename T>
		T &Get(std::string_view key)
		{
			if (Has<T>(key))
			{
				return std::any_cast<T &>(data.at(std::string(key)));
			}
			throw std::runtime_error("Key not found or type mismatch");
		}

		template <typename T>
		const T &Get(std::string_view key) const
		{
			if (Has<T>(key))
			{
				return std::any_cast<const T &>(data.at(std::string(key)));
			}
			throw std::runtime_error("Key not found or type mismatch");
		}

		template <typename T>
		bool Has(std::string_view key) const
		{
			return data.contains(std::string(key));
		}

		void Remove(std::string_view key) { data.erase(std::string(key)); }

	private:
		std::unordered_map<std::string, std::any> data;
	};
} // namespace ai