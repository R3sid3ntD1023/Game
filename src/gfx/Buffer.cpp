#include "gfx/Buffer.h"
#include <algorithm>
#include <utility>

namespace gfx
{

    GpuBuffer::GpuBuffer(GpuBuffer &&o) noexcept
        : m_id(std::exchange(o.m_id, 0)), m_size(std::exchange(o.m_size, 0)), m_capacity(std::exchange(o.m_capacity, 0)) {}

    GpuBuffer &GpuBuffer::operator=(GpuBuffer &&o) noexcept
    {
        if (this != &o)
        {
            if (m_id)
                glDeleteBuffers(1, &m_id);
            m_id = std::exchange(o.m_id, 0);
            m_size = std::exchange(o.m_size, 0);
            m_capacity = std::exchange(o.m_capacity, 0);
        }
        return *this;
    }

    GpuBuffer::~GpuBuffer()
    {
        if (m_id)
            glDeleteBuffers(1, &m_id);
    }

    void GpuBuffer::Create(size_t initialCapacityBytes)
    {
        if (m_id)
            glDeleteBuffers(1, &m_id);
        glCreateBuffers(1, &m_id);
        m_capacity = initialCapacityBytes;
        m_size = 0;
        glNamedBufferData(m_id, static_cast<GLsizeiptr>(m_capacity), nullptr, GL_DYNAMIC_DRAW);
    }

    void GpuBuffer::Reserve(size_t bytes)
    {
        if (bytes <= m_capacity)
            return;
        size_t newCap = std::max(bytes, m_capacity * 2);
        GLuint newId = 0;
        glCreateBuffers(1, &newId);
        glNamedBufferData(newId, static_cast<GLsizeiptr>(newCap), nullptr, GL_DYNAMIC_DRAW);
        if (m_size > 0)
            glCopyNamedBufferSubData(m_id, newId, 0, 0, static_cast<GLsizeiptr>(m_size));
        glDeleteBuffers(1, &m_id);
        m_id = newId;
        m_capacity = newCap;
    }

    size_t GpuBuffer::Append(const void *data, size_t bytes)
    {
        // Growing replaces the GL name, so VAOs must re-attach after Append (MeshManager does).
        Reserve(m_size + bytes);
        size_t offset = m_size;
        glNamedBufferSubData(m_id, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(bytes), data);
        m_size += bytes;
        return offset;
    }

    void GpuBuffer::Upload(const void *data, size_t bytes)
    {
        if (!m_id)
            glCreateBuffers(1, &m_id);
        glNamedBufferData(m_id, static_cast<GLsizeiptr>(bytes), data, GL_STREAM_DRAW);
        m_size = bytes;
        m_capacity = bytes;
    }

} // namespace gfx
