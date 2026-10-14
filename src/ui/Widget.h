#pragma once
#include "ui/Renderer.h"
#include "ui/WidgetBinding.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ui
{

    // Where a widget attaches inside its parent. `offset` is a margin from the anchored edge
    // (or a shift from the center), so positive offsets always move a widget inward.
    enum class Anchor
    {
        TopLeft,
        Top,
        TopRight,
        Left,
        Center,
        Right,
        BottomLeft,
        Bottom,
        BottomRight
    };
    enum class Align
    {
        Left,
        Center,
        Right
    };

    // Retained-mode widget: created once, holds its own state, laid out and drawn by Canvas every frame.
    // Sizes and offsets are in design pixels (720p reference) and scale with the window height.
    class Widget
    {
    public:
        virtual ~Widget() = default;

        template <class T, class... Args>
        T &Add(Args &&...args)
        {
            auto child = std::make_unique<T>(std::forward<Args>(args)...);
            T &ref = *child;
            m_children.push_back(std::move(child));
            return ref;
        }

        void Layout(const Rect &parent, float scale);
        void PlaceAt(const glm::vec2 &pos, float scale); // used by containers that position children themselves
        virtual void Draw(Renderer &r) { DrawChildren(r); }
        Widget *HitTest(const glm::vec2 &p); // topmost interactive widget under p

        virtual bool Interactive() const { return false; }
        virtual void OnHover(bool) {}
        virtual void OnPress(bool) {}
        virtual void OnClick() {}
        virtual void OnMouseMoved(const glm::vec2 &) {}

        void Stretch() { stretchX = stretchY = true; }
        const Rect &GetRect() const { return m_rect; }

        Anchor anchor = Anchor::TopLeft;
        glm::vec2 offset{0.0f};
        glm::vec2 size{0.0f};
        bool stretchX = false; // fill the parent's width / height instead of using `size`
        bool stretchY = false;
        bool visible = true;

    protected:
        virtual void LayoutChildren(float scale);
        void DrawChildren(Renderer &r);

        Rect m_rect;
        float m_scale = 1.0f;
        std::vector<std::unique_ptr<Widget>> m_children;
    };

    class Panel : public Widget
    {
    public:
        void Draw(Renderer &r) override;

        Color color{0.0f, 0.0f, 0.0f, 0.5f};
        Color colorBottom{0.0f, 0.0f, 0.0f, 0.5f}; // used when `gradient` is set
        bool gradient = false;
        Color border{0.0f};
        float borderWidth = 0.0f;
    };

    class Label : public Widget
    {
    public:
        void Draw(Renderer &r) override;

        std::string text;
        float textSize = 20.0f;
        Color color{1.0f};
        Align align = Align::Left;
        bool shadow = true;
    };

    class ProgressBar : public Widget
    {
    public:
        void Draw(Renderer &r) override;

        float value = 1.0f; // 0..1
        Color fill{0.2f, 0.8f, 0.3f, 0.9f};
        Color background{0.0f, 0.0f, 0.0f, 0.6f};
        Color border{1.0f, 1.0f, 1.0f, 0.45f};
    };

    class Button : public Widget
    {
    public:
        void Draw(Renderer &r) override;
        bool Interactive() const override { return enabled; }
        void OnHover(bool h) override { m_hover = h; }
        void OnPress(bool p) override { m_pressed = p; }
        void OnClick() override
        {
            if (onClick)
                onClick();
        }

        std::string text;
        float textSize = 22.0f;
        bool enabled = true;
        std::function<void()> onClick;

    private:
        bool m_hover = false;
        bool m_pressed = false;
    };

    class Checkbox : public Widget
    {
    public:
        void Draw(Renderer &r) override;
        bool Interactive() const override { return true; }
        void OnHover(bool h) override { m_hover = h; }
        void OnClick() override
        {
            checked = !checked;
            if (onChange)
                onChange(checked);
        }

        std::string text;
        float textSize = 20.0f;
        bool checked = false;
        std::function<void(bool)> onChange;

    private:
        bool m_hover = false;
    };

    class Slider : public Widget
    {
    public:
        void Draw(Renderer &r) override;
        bool Interactive() const override { return true; }
        void OnHover(bool h) override { m_hover = h; }
        void OnPress(bool p) override
        {
            m_pressed = p;
        }
        void OnMouseMoved(const glm::vec2 &mousePos) override;

        std::string text;
        float textSize = 20.0f;
        WidgetBinding<float> binding;

    private:
        bool m_hover = false;
        bool m_pressed = false;
        float knobSize = 16.0f * m_scale;
    };

    // Lays children out top to bottom, centered horizontally, `spacing` apart. Children use their own `size`.
    class StackPanel : public Widget
    {
    public:
        float spacing = 12.0f;

    protected:
        void LayoutChildren(float scale) override;
    };

} // namespace ui
