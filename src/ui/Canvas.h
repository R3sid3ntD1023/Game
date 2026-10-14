#pragma once
#include "ui/Widget.h"

namespace ui
{

    // Owns the widget tree and the Renderer. Call Frame() once per frame after the 3D scene is drawn.
    class Canvas
    {
    public:
        Widget &Root() { return m_root; }

        // `mouse` is in framebuffer pixels; pass a negative position to disable interaction (e.g. cursor captured).
        void Frame(const glm::vec2 &screen, const glm::vec2 &mouse, bool mouseDown);

    private:
        Renderer m_renderer;
        Widget m_root;
        Widget *m_hot = nullptr;
        Widget *m_active = nullptr;
        bool m_prevDown = false;
    };

} // namespace ui
