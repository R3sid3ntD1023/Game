#include "gfx/VertexArray.h"

namespace gfx
{

    VertexArray::VertexArray() { glCreateVertexArrays(1, &m_id); }
    VertexArray::~VertexArray() { glDeleteVertexArrays(1, &m_id); }

    void VertexArray::SetVertexBuffer(const VertexBuffer &vb, GLsizei stride, const std::vector<VertexAttribute> &layout)
    {
        glVertexArrayVertexBuffer(m_id, 0, vb.Id(), 0, stride);
        for (const auto &a : layout)
        {
            glEnableVertexArrayAttrib(m_id, a.location);
            if (a.integer)
                glVertexArrayAttribIFormat(m_id, a.location, a.components, GL_INT, a.offset);
            else
                glVertexArrayAttribFormat(m_id, a.location, a.components, GL_FLOAT, GL_FALSE, a.offset);
            glVertexArrayAttribBinding(m_id, a.location, 0);
        }
    }

    void VertexArray::SetIndexBuffer(const IndexBuffer &ib) { glVertexArrayElementBuffer(m_id, ib.Id()); }

} // namespace gfx
