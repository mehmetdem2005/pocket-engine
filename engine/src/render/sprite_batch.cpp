// PocketEngine — batched sprite + line renderer implementation
#include "pocket/render/sprite_batch.h"
#include "pocket/render/texture.h"
#include "pocket/core/log.h"

#include <GLES3/gl3.h>
#include <cmath>
#include <cstring>
#include <utility>

namespace pocket::render {

#define GL_CHECK(x) do { x; GLuint e = glGetError(); if(e) PE_ERROR("gl","%s err=0x%x at %s:%d", #x, e, __FILE__, __LINE__); } while(0)

// =====================================================================
//                              SpriteBatch
// =====================================================================
SpriteBatch::SpriteBatch() {
    std::memset(vertices_, 0, sizeof(vertices_));
}

SpriteBatch::~SpriteBatch() {
    shutdown();
}

bool SpriteBatch::init(Shader* spriteShader, TextureCache* textureCache) {
    if (initialized_) return true;
    if (!spriteShader || !spriteShader->valid()) {
        PE_ERROR("render", "SpriteBatch::init: invalid shader");
        return false;
    }
    shader_ = spriteShader;
    texCache_ = textureCache;

    GL_CHECK(glGenVertexArrays(1, &vao_));
    GL_CHECK(glGenBuffers(1, &vbo_));
    GL_CHECK(glGenBuffers(1, &ibo_));

    GL_CHECK(glBindVertexArray(vao_));

    // Static index buffer for the maximum number of quads.
    u16 indices[kMaxIndices];
    for (int i = 0; i < kMaxQuads; ++i) {
        u16 b = (u16)(i * 4);
        indices[i*6 + 0] = b + 0;
        indices[i*6 + 1] = b + 1;
        indices[i*6 + 2] = b + 2;
        indices[i*6 + 3] = b + 2;
        indices[i*6 + 4] = b + 3;
        indices[i*6 + 5] = b + 0;
    }
    GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_));
    GL_CHECK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW));

    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, vbo_));
    GL_CHECK(glBufferData(GL_ARRAY_BUFFER, sizeof(vertices_), nullptr, GL_DYNAMIC_DRAW));

    // a_pos  = location 0, 3 floats
    GL_CHECK(glEnableVertexAttribArray(0));
    GL_CHECK(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SpriteVertex),
                                   (const void*)offsetof(SpriteVertex, x)));
    // a_uv   = location 1, 2 floats
    GL_CHECK(glEnableVertexAttribArray(1));
    GL_CHECK(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(SpriteVertex),
                                   (const void*)offsetof(SpriteVertex, u)));
    // a_color= location 2, 4 floats
    GL_CHECK(glEnableVertexAttribArray(2));
    GL_CHECK(glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(SpriteVertex),
                                   (const void*)offsetof(SpriteVertex, r)));

    GL_CHECK(glBindVertexArray(0));
    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, 0));
    GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

    initialized_ = true;
    PE_DEBUG("render", "SpriteBatch initialized (max %d quads)", kMaxQuads);
    return true;
}

void SpriteBatch::shutdown() {
    if (!initialized_) return;
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (ibo_) glDeleteBuffers(1, &ibo_);
    vao_ = vbo_ = ibo_ = 0;
    initialized_ = false;
    vertexCount_ = 0;
    quadCount_   = 0;
    currentTex_  = 0;
    shader_ = nullptr;
    texCache_ = nullptr;
}

void SpriteBatch::begin(const math::Mat4& vp) {
    vp_ = vp;
    vertexCount_ = 0;
    quadCount_   = 0;
    currentTex_  = 0;
}

void SpriteBatch::setTexture(u32 tex) {
    if (tex == currentTex_) return;
    if (currentTex_ != 0 && vertexCount_ > 0) flush();
    currentTex_ = tex;
}

void SpriteBatch::flush() {
    if (vertexCount_ == 0 || quadCount_ == 0) return;
    if (!shader_) return;

    shader_->use();
    shader_->setMat4("u_vp", vp_);
    shader_->setInt ("u_tex", 0);

    GL_CHECK(glActiveTexture(GL_TEXTURE0));

    // Resolve current TextureId -> GL id via the TextureCache (if available).
    if (texCache_ && currentTex_ != 0) {
        const TextureInfo* ti = texCache_->info(currentTex_);
        if (ti && ti->glId) {
            GL_CHECK(glBindTexture(GL_TEXTURE_2D, ti->glId));
        }
    }

    GL_CHECK(glBindVertexArray(vao_));
    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, vbo_));
    GL_CHECK(glBufferSubData(GL_ARRAY_BUFFER, 0,
                             static_cast<GLsizeiptr>(vertexCount_ * sizeof(SpriteVertex)),
                             vertices_));
    GL_CHECK(glDrawElements(GL_TRIANGLES,
                            quadCount_ * kIndicesPerQuad,
                            GL_UNSIGNED_SHORT,
                            nullptr));
    GL_CHECK(glBindVertexArray(0));

    vertexCount_ = 0;
    quadCount_   = 0;
}

void SpriteBatch::drawQuad(u32 tex,
                           const math::Vec3& pos,
                           const math::Vec2& size,
                           float rot,
                           const math::Color& tint,
                           const math::Vec2& uv0,
                           const math::Vec2& uv1) {
    if (quadCount_ >= kMaxQuads) flush();
    setTexture(tex);

    const float hx = size.x * 0.5f;
    const float hy = size.y * 0.5f;
    const float c  = std::cos(rot);
    const float s  = std::sin(rot);

    auto rotCorner = [&](float lx, float ly) -> std::pair<float,float> {
        return { lx * c - ly * s + pos.x, lx * s + ly * c + pos.y };
    };

    auto p0 = rotCorner(-hx, -hy);
    auto p1 = rotCorner( hx, -hy);
    auto p2 = rotCorner( hx,  hy);
    auto p3 = rotCorner(-hx,  hy);

    SpriteVertex v0{ p0.first, p0.second, pos.z, uv0.x, uv0.y, tint.r, tint.g, tint.b, tint.a };
    SpriteVertex v1{ p1.first, p1.second, pos.z, uv1.x, uv0.y, tint.r, tint.g, tint.b, tint.a };
    SpriteVertex v2{ p2.first, p2.second, pos.z, uv1.x, uv1.y, tint.r, tint.g, tint.b, tint.a };
    SpriteVertex v3{ p3.first, p3.second, pos.z, uv0.x, uv1.y, tint.r, tint.g, tint.b, tint.a };

    vertices_[vertexCount_++] = v0;
    vertices_[vertexCount_++] = v1;
    vertices_[vertexCount_++] = v2;
    vertices_[vertexCount_++] = v3;
    quadCount_++;
}

void SpriteBatch::drawSolidQuad(const math::Vec3& pos,
                                const math::Vec2& size,
                                const math::Color& color) {
    // Use the default white texture (id=1) so tint = solid color.
    drawQuad(1, pos, size, 0.0f, color);
}

void SpriteBatch::drawCircleApprox(const math::Vec3& pos,
                                   float radius,
                                   int segments,
                                   const math::Color& color) {
    // Filled disc via triangle fan decomposed into quads (one quad per segment,
    // 4th vertex == center to keep the layout simple).
    if (segments < 3) segments = 3;
    const float twoPi = 2.0f * math::PI;
    for (int i = 0; i < segments; ++i) {
        float a0 = (float)i / segments * twoPi;
        float a1 = (float)(i + 1) / segments * twoPi;
        if (quadCount_ >= kMaxQuads) flush();
        setTexture(1); // white

        float x0 = pos.x + std::cos(a0) * radius;
        float y0 = pos.y + std::sin(a0) * radius;
        float x1 = pos.x + std::cos(a1) * radius;
        float y1 = pos.y + std::sin(a1) * radius;

        SpriteVertex v0{ x0, y0, pos.z, 0, 0, color.r, color.g, color.b, color.a };
        SpriteVertex v1{ x1, y1, pos.z, 1, 0, color.r, color.g, color.b, color.a };
        SpriteVertex v2{ pos.x, pos.y, pos.z, 1, 1, color.r, color.g, color.b, color.a };
        SpriteVertex v3{ pos.x, pos.y, pos.z, 0, 1, color.r, color.g, color.b, color.a };
        vertices_[vertexCount_++] = v0;
        vertices_[vertexCount_++] = v1;
        vertices_[vertexCount_++] = v2;
        vertices_[vertexCount_++] = v3;
        quadCount_++;
    }
}

// =====================================================================
//                              LineBatch
// =====================================================================
LineBatch::LineBatch() {
    std::memset(vertices_, 0, sizeof(vertices_));
}
LineBatch::~LineBatch() { shutdown(); }

bool LineBatch::init(Shader* lineShader) {
    if (initialized_) return true;
    if (!lineShader || !lineShader->valid()) {
        PE_ERROR("render", "LineBatch::init: invalid shader");
        return false;
    }
    shader_ = lineShader;

    GL_CHECK(glGenVertexArrays(1, &vao_));
    GL_CHECK(glGenBuffers(1, &vbo_));

    GL_CHECK(glBindVertexArray(vao_));
    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, vbo_));
    GL_CHECK(glBufferData(GL_ARRAY_BUFFER, sizeof(vertices_), nullptr, GL_DYNAMIC_DRAW));

    // a_pos = location 0, 3 floats
    GL_CHECK(glEnableVertexAttribArray(0));
    GL_CHECK(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex),
                                   (const void*)offsetof(LineVertex, x)));
    // a_color = location 1, 4 floats
    GL_CHECK(glEnableVertexAttribArray(1));
    GL_CHECK(glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(LineVertex),
                                   (const void*)offsetof(LineVertex, r)));

    GL_CHECK(glBindVertexArray(0));
    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, 0));

    initialized_ = true;
    PE_DEBUG("render", "LineBatch initialized (max %d verts)", kMaxVertices);
    return true;
}

void LineBatch::shutdown() {
    if (!initialized_) return;
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    vao_ = vbo_ = 0;
    initialized_ = false;
    vertexCount_ = 0;
    shader_ = nullptr;
}

void LineBatch::begin(const math::Mat4& vp) {
    vp_ = vp;
    vertexCount_ = 0;
}

void LineBatch::flush() {
    if (vertexCount_ == 0 || !shader_) return;
    shader_->use();
    shader_->setMat4("u_vp", vp_);

    GL_CHECK(glBindVertexArray(vao_));
    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, vbo_));
    GL_CHECK(glBufferSubData(GL_ARRAY_BUFFER, 0,
                             static_cast<GLsizeiptr>(vertexCount_ * sizeof(LineVertex)),
                             vertices_));
    GL_CHECK(glDrawArrays(GL_LINES, 0, vertexCount_));
    GL_CHECK(glBindVertexArray(0));
    vertexCount_ = 0;
}

void LineBatch::drawLine(const math::Vec3& a, const math::Vec3& b, const math::Color& color) {
    if (vertexCount_ + 2 > kMaxVertices) flush();
    LineVertex va{ a.x, a.y, a.z, color.r, color.g, color.b, color.a };
    LineVertex vb{ b.x, b.y, b.z, color.r, color.g, color.b, color.a };
    vertices_[vertexCount_++] = va;
    vertices_[vertexCount_++] = vb;
}

void LineBatch::drawRectLines(const math::Vec3& pos, const math::Vec2& size, const math::Color& color) {
    if (vertexCount_ + 8 > kMaxVertices) flush();
    float x0 = pos.x, y0 = pos.y;
    float x1 = pos.x + size.x, y1 = pos.y + size.y;
    math::Vec3 a{x0, y0, pos.z}, b{x1, y0, pos.z};
    math::Vec3 c{x1, y1, pos.z}, d{x0, y1, pos.z};
    drawLine(a, b, color);
    drawLine(b, c, color);
    drawLine(c, d, color);
    drawLine(d, a, color);
}

void LineBatch::drawGrid(const math::Vec3& origin, float cellSize,
                         int cellsX, int cellsY, const math::Color& color) {
    if (cellSize <= 0.0f || cellsX <= 0 || cellsY <= 0) return;
    float x0 = origin.x;
    float y0 = origin.y;
    // Vertical lines
    for (int i = 0; i <= cellsX; ++i) {
        float x = x0 + i * cellSize;
        drawLine(math::Vec3{x, y0, origin.z},
                 math::Vec3{x, y0 + cellsY * cellSize, origin.z}, color);
    }
    // Horizontal lines
    for (int j = 0; j <= cellsY; ++j) {
        float y = y0 + j * cellSize;
        drawLine(math::Vec3{x0, y, origin.z},
                 math::Vec3{x0 + cellsX * cellSize, y, origin.z}, color);
    }
}

#undef GL_CHECK

} // namespace pocket::render
