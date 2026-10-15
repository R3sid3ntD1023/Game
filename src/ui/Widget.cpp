#include "ui/Widget.h"

namespace ui
{

    void Widget::Layout(const Rect &parent, float scale)
    {
        m_scale = scale;
        glm::vec2 sz = size * scale;
        const glm::vec2 off = offset * scale;
        if (stretchX)
            sz.x = parent.size.x;
        if (stretchY)
            sz.y = parent.size.y;

        const int index = static_cast<int>(anchor);
        const int h = index % 3; // 0 left, 1 center, 2 right
        const int v = index / 3; // 0 top, 1 middle, 2 bottom

        float x = parent.pos.x;
        if (!stretchX)
        {
            if (h == 0)
                x += off.x;
            else if (h == 1)
                x += (parent.size.x - sz.x) * 0.5f + off.x;
            else
                x += parent.size.x - sz.x - off.x;
        }
        float y = parent.pos.y;
        if (!stretchY)
        {
            if (v == 0)
                y += off.y;
            else if (v == 1)
                y += (parent.size.y - sz.y) * 0.5f + off.y;
            else
                y += parent.size.y - sz.y - off.y;
        }

        m_rect = {{x, y}, sz};
        LayoutChildren(scale);
    }

    void Widget::PlaceAt(const glm::vec2 &pos, float scale)
    {
        m_scale = scale;
        m_rect = {pos, size * scale};
        LayoutChildren(scale);
    }

    void Widget::LayoutChildren(float scale)
    {
        for (auto &c : m_children)
            if (c->visible)
                c->Layout(m_rect, scale);
    }

    void Widget::DrawChildren(Renderer &r)
    {
        for (auto &c : m_children)
            if (c->visible)
                c->Draw(r);
    }

    Widget *Widget::HitTest(const glm::vec2 &p)
    {
        for (auto it = m_children.rbegin(); it != m_children.rend(); ++it)
            if ((*it)->visible)
                if (Widget *hit = (*it)->HitTest(p))
                    return hit;
        return Interactive() && m_rect.Contains(p) ? this : nullptr;
    }

    void Panel::Draw(Renderer &r)
    {
        if (gradient)
            r.FillGradient(m_rect, color, colorBottom);
        else
            r.FillRect(m_rect, color);
        if (borderWidth > 0.0f && border.a > 0.0f)
            r.StrokeRect(m_rect, border, borderWidth * m_scale);
        DrawChildren(r);
    }

    void Label::Draw(Renderer &r)
    {
        if (!text.empty())
        {
            const float s = textSize * m_scale;
            const glm::vec2 extent = r.MeasureText(text, s);
            float x = m_rect.pos.x;
            if (align == Align::Center)
                x += (m_rect.size.x - extent.x) * 0.5f;
            else if (align == Align::Right)
                x += m_rect.size.x - extent.x;
            const float y = m_rect.pos.y + (m_rect.size.y - s) * 0.5f;

            if (shadow)
                r.Text(text, {x + 1.5f * m_scale, y + 1.5f * m_scale}, s, {0.0f, 0.0f, 0.0f, 0.8f * color.a});
            r.Text(text, {x, y}, s, color);
        }
        DrawChildren(r);
    }

    void ProgressBar::Draw(Renderer &r)
    {
        r.FillRect(m_rect, background);
        const float v = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
        r.FillRect({m_rect.pos, {m_rect.size.x * v, m_rect.size.y}}, fill);
        r.StrokeRect(m_rect, border, 1.5f * m_scale);
        DrawChildren(r);
    }

    void Button::Draw(Renderer &r)
    {
        Color finalBackgroundColor = backgroundColor;
        if (!enabled)
            finalBackgroundColor = disabledColor;
        else if (m_pressed)
            finalBackgroundColor = pressedColor;
        else if (m_hover)
            finalBackgroundColor = hoverColor;

        r.FillRect(m_rect, finalBackgroundColor);
        r.StrokeRect(m_rect, m_hover && enabled ? hoverColor : borderColor, 1.5f * m_scale);

        if (!text.empty())
        {
            const float s = textSize * m_scale;
            const glm::vec2 extent = r.MeasureText(text, s);
            const glm::vec2 p(m_rect.pos.x + (m_rect.size.x - extent.x) * 0.5f, m_rect.pos.y + (m_rect.size.y - s) * 0.5f + (m_pressed ? 1.0f : 0.0f));
            const float a = enabled ? 1.0f : 0.5f;
            textColor.a *= a;
            r.Text(text, p, s, textColor);
        }
        DrawChildren(r);
    }

    void Checkbox::Draw(Renderer &r)
    {
        const float box = 22.0f * m_scale;
        const Rect boxRect{{m_rect.pos.x, m_rect.pos.y + (m_rect.size.y - box) * 0.5f}, {box, box}};
        r.FillRect(boxRect, {0.0f, 0.0f, 0.0f, 0.6f});
        r.StrokeRect(boxRect, m_hover ? Color(1.0f, 0.85f, 0.4f, 0.95f) : Color(1.0f, 1.0f, 1.0f, 0.5f), 1.5f * m_scale);
        if (checked)
        {
            const float inset = 5.0f * m_scale;
            r.FillRect({boxRect.pos + glm::vec2(inset), boxRect.size - glm::vec2(2.0f * inset)}, {0.4f, 0.9f, 0.5f, 1.0f});
        }

        const float s = textSize * m_scale;
        const glm::vec2 p(m_rect.pos.x + box + 12.0f * m_scale, m_rect.pos.y + (m_rect.size.y - s) * 0.5f);
        r.Text(text, {p.x + 1.0f, p.y + 1.0f}, s, {0.0f, 0.0f, 0.0f, 0.7f});
        r.Text(text, p, s, {1.0f, 1.0f, 1.0f, 1.0f});
        DrawChildren(r);
    }

    void StackPanel::LayoutChildren(float scale)
    {
        float y = m_rect.pos.y;
        for (auto &c : m_children)
        {
            if (!c->visible)
                continue;
            const glm::vec2 sz = c->size * scale;
            c->PlaceAt({m_rect.pos.x + (m_rect.size.x - sz.x) * 0.5f + c->offset.x * scale, y + c->offset.y * scale}, scale);
            y += sz.y + spacing * scale;
        }
    }

    void Slider::Draw(Renderer &r)
    {
        ui::Color knobColor = m_hover && m_pressed ? Color(0.4f, 0.9f, 0.5f, 1.0f) : Color(0.3f, 0.7f, 0.4f, 1.0f);

        const float s = textSize * m_scale;
        const auto extent = r.MeasureText(text, s);
        r.Text(text, {m_rect.pos.x, m_rect.pos.y + extent.y * 0.5f}, s, {1.0f, 1.0f, 1.0f, 1.0f});

        const float trackHeight = 4.0f * m_scale;
        const float offsetX = extent.x + 12.0f * m_scale;
        const Rect trackRect{{m_rect.pos.x + offsetX, m_rect.pos.y + (m_rect.size.y - trackHeight) * 0.5f}, {m_rect.size.x - offsetX, trackHeight}};
        r.FillRect(trackRect, {0.0f, 0.0f, 0.0f, 0.6f});
        const float knobPos = m_rect.pos.x + offsetX + *binding * (m_rect.size.x - offsetX - knobSize);
        const Rect knobRect{{knobPos, m_rect.pos.y + (m_rect.size.y - knobSize) * 0.5f}, {knobSize, knobSize}};
        r.FillRect(knobRect, knobColor);
        DrawChildren(r);
    }

    void Slider::OnMouseMoved(const glm::vec2 &mousePos)
    {
        if (m_hover && m_pressed)
        {
            float value = (mousePos.x - m_rect.pos.x - knobSize * 0.5f) / (m_rect.size.x - knobSize);
            value = glm::clamp(value, 0.0f, 1.0f);

            if (binding)
                *binding = value;
        }
    }

    void Image::Draw(Renderer &r)
    {
        if (texture)
        {
            r.DrawImage(texture, m_rect, color);
        }
        else
            r.FillRect(m_rect, color);
        DrawChildren(r);
    }

} // namespace ui
