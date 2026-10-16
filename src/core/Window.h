#pragma once
#include "core/Input.h"
#include <glm/glm.hpp>
#include <string>

struct GLFWwindow;

namespace core
{

    class Window
    {
    public:
        Window(int width, int height, const std::string &title);
        ~Window();
        Window(const Window &) = delete;
        Window &operator=(const Window &) = delete;

        bool ShouldClose() const;
        void SetShouldClose();
        void PollEvents();
        void SwapBuffers();
        void SetTitle(const std::string &title);
        void SetCursorCaptured(bool captured);
        void SetVSync(bool enabled);

        glm::ivec2 FramebufferSize() const;
        glm::vec2 CursorPosition() const; // framebuffer pixels, origin top-left
        double Time() const;
        Input &GetInput() { return m_input; }
        GLFWwindow *Handle() const { return m_window; }

    private:
        GLFWwindow *m_window = nullptr;
        Input m_input;
    };

} // namespace core
