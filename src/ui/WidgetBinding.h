#pragma once

namespace ui
{
    template <class T>
    struct WidgetBinding
    {
        WidgetBinding() = default;

        WidgetBinding(const WidgetBinding &other) : value(other.value) {}

        explicit WidgetBinding(T *v) : value(v) {}

        WidgetBinding &operator=(const WidgetBinding &other)
        {
            value = other.value;
            return *this;
        }
        WidgetBinding &operator=(T *v)
        {
            value = v;
            return *this;
        }

        operator T *() { return value != nullptr ? value : &defaultValue; }

    private:
        T *value = nullptr;
        T defaultValue{};
    };
}