#pragma once
#include "gfx/Buffer.h"
#include <vector>

namespace gfx {

struct VertexAttribute {
    GLuint location;
    GLint components;
    GLuint offset;
    bool integer = false;
};

class VertexArray {
public:
    VertexArray();
    ~VertexArray();
    VertexArray(const VertexArray&) = delete;
    VertexArray& operator=(const VertexArray&) = delete;

    void SetVertexBuffer(const VertexBuffer& vb, GLsizei stride, const std::vector<VertexAttribute>& layout);
    void SetIndexBuffer(const IndexBuffer& ib);
    void Bind() const { glBindVertexArray(m_id); }

private:
    GLuint m_id = 0;
};

} // namespace gfx
