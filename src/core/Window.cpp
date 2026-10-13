#include <glad/glad.h>

#include "core/Window.h"
#include <GLFW/glfw3.h>

#include <spdlog/spdlog.h>
#include <stdexcept>

namespace core
{

    namespace
    {

        void APIENTRY OnGlDebug(GLenum, GLenum type, GLuint, GLenum severity, GLsizei, const GLchar *msg, const void *)
        {
            if (type == GL_DEBUG_TYPE_PERFORMANCE)
                spdlog::debug("GL: {}", msg); // driver hints, e.g. one-time upload syncs
            else if (severity == GL_DEBUG_SEVERITY_HIGH)
                spdlog::error("GL: {}", msg);
            else if (severity == GL_DEBUG_SEVERITY_MEDIUM)
                spdlog::warn("GL: {}", msg);
        }

        Input &InputOf(GLFWwindow *w) { return *static_cast<Input *>(glfwGetWindowUserPointer(w)); }

    } // namespace

    Window::Window(int width, int height, const std::string &title)
    {
        if (!glfwInit())
            throw std::runtime_error("Failed to init GLFW");

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);

        m_window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
        if (!m_window)
        {
            glfwTerminate();
            throw std::runtime_error("Failed to create window (OpenGL 4.6 required)");
        }
        glfwMakeContextCurrent(m_window);
        glfwSwapInterval(1);

        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
            throw std::runtime_error("Failed to load OpenGL");
        spdlog::info("OpenGL {}", reinterpret_cast<const char *>(glGetString(GL_VERSION)));
        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageCallback(OnGlDebug, nullptr);

        glfwSetWindowUserPointer(m_window, &m_input);
        glfwSetKeyCallback(m_window, [](GLFWwindow *w, int key, int, int action, int)
                           { InputOf(w).HandleKey(key, action); });
        glfwSetCursorPosCallback(m_window, [](GLFWwindow *w, double x, double y)
                                 { InputOf(w).HandleMouseMove(x, y); });
        glfwSetMouseButtonCallback(m_window, [](GLFWwindow *w, int b, int a, int)
                                   { InputOf(w).HandleMouseButton(b, a); });
    }

    Window::~Window()
    {
        glfwDestroyWindow(m_window);
        glfwTerminate();
    }

    bool Window::ShouldClose() const { return glfwWindowShouldClose(m_window) != 0; }
    void Window::SetShouldClose() { glfwSetWindowShouldClose(m_window, GLFW_TRUE); }
    void Window::PollEvents() { glfwPollEvents(); }
    void Window::SwapBuffers() { glfwSwapBuffers(m_window); }
    void Window::SetTitle(const std::string &title) { glfwSetWindowTitle(m_window, title.c_str()); }
    double Window::Time() const { return glfwGetTime(); }

    void Window::SetCursorCaptured(bool captured)
    {
        glfwSetInputMode(m_window, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        m_input.ResetMouse();
    }

    glm::vec2 Window::CursorPosition() const
    {
        double x = 0.0, y = 0.0;
        glfwGetCursorPos(m_window, &x, &y);
        int ww = 0, wh = 0;
        glfwGetWindowSize(m_window, &ww, &wh);
        if (ww <= 0 || wh <= 0)
            return {-1.0f, -1.0f};
        const glm::ivec2 fb = FramebufferSize();
        return {static_cast<float>(x) * static_cast<float>(fb.x) / static_cast<float>(ww),
                static_cast<float>(y) * static_cast<float>(fb.y) / static_cast<float>(wh)};
    }

    glm::ivec2 Window::FramebufferSize() const
    {
        int w = 0, h = 0;
        glfwGetFramebufferSize(m_window, &w, &h);
        return {w, h};
    }

} // namespace core
