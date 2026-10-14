#include "ui/Canvas.h"

namespace ui
{

    void Canvas::Frame(const glm::vec2 &screen, const glm::vec2 &mouse, bool mouseDown)
    {
        const float scale = screen.y / 720.0f;
        m_root.Stretch();
        m_root.Layout({{0.0f, 0.0f}, screen}, scale);

        Widget *hit = mouse.x >= 0.0f && mouse.y >= 0.0f ? m_root.HitTest(mouse) : nullptr;
        if (hit != m_hot)
        {
            if (m_hot)
                m_hot->OnHover(false);
            m_hot = hit;
            if (m_hot)
                m_hot->OnHover(true);
        }

        if (mouseDown && !m_prevDown)
        {
            if (hit)
            {
                m_active = hit;
                hit->OnPress(true);
            }
        }
        else if (!mouseDown && m_prevDown && m_active)
        {
            Widget *pressed = m_active;
            m_active = nullptr;
            pressed->OnPress(false);
            if (pressed == hit)
                pressed->OnClick();
        }

        m_prevDown = mouseDown;
        if (m_active)
            m_active->OnMouseMoved(mouse);
        m_renderer.Begin(screen);
        m_root.Draw(m_renderer);
        m_renderer.End();
    }

} // namespace ui
