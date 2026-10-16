#pragma once

#include <memory>
#include <string>
#include <unordered_map>

namespace ai
{

    struct BlackboardKey
    {
        virtual ~BlackboardKey() = default;
    };

    template <typename T>
    struct BlackboardValue : public BlackboardKey
    {
        T value;
    };

    class Blackboard
    {
    public:
        template <typename T>
        T Get(const std::string &key)
        {
            auto it = data.find(key);
            if (it != data.end())
            {
                auto derived = dynamic_cast<BlackboardValue<T> *>(it->second.get());
                if (derived)
                    return derived->value;
            }
            throw std::runtime_error("Key not found or type mismatch");
        }

        template <typename T>
        void Set(const std::string &key, const T &value)
        {
            if (data.contains(key))
                dynamic_cast<BlackboardValue<T> *>(data[key].get())->value = value;
            else
                throw std::runtime_error("Key not found");
        }

        template <typename T>
        void Add(const std::string &key, const T &value)
        {
            data[key] = std::make_unique<BlackboardValue<T>>();
            dynamic_cast<BlackboardValue<T> *>(data[key].get())->value = value;
        }

    private:
        std::unordered_map<std::string, std::unique_ptr<BlackboardKey>> data;
    };
}