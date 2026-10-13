#pragma once
#include <cstddef>
#include <cstdint>
#include <glad/glad.h>

namespace gfx
{

    // Growable GPU buffer; Append returns the byte offset of the written data.
    class GpuBuffer
    {
    public:
        GpuBuffer() = default;
        GpuBuffer(const GpuBuffer &) = delete;
        GpuBuffer &operator=(const GpuBuffer &) = delete;
        GpuBuffer(GpuBuffer &&o) noexcept;
        GpuBuffer &operator=(GpuBuffer &&o) noexcept;
        ~GpuBuffer();

        void Create(size_t initialCapacityBytes);
        size_t Append(const void *data, size_t bytes);
        void Reserve(size_t bytes);
        void Upload(const void *data, size_t bytes); // replaces contents (orphans)

        GLuint Id() const { return m_id; }
        size_t Size() const { return m_size; }

    private:
        GLuint m_id = 0;
        size_t m_size = 0;
        size_t m_capacity = 0;
    };

    class VertexBuffer : public GpuBuffer
    {
    };
    class IndexBuffer : public GpuBuffer
    {
    };

} // namespace gfx
