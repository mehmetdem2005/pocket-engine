#pragma once
// PocketEngine — batched sprite / primitive renderer (OpenGL ES 3.0)
#include "pocket/core/types.h"
#include "pocket/math/math.h"
#include "pocket/render/shader.h"

namespace pocket::render {

class TextureCache;

// Vertex layout: pos(3) + uv(2) + color(4) = 9 floats
struct SpriteVertex {
    f32 x, y, z;
    f32 u, v;
    f32 r, g, b, a;
};

class SpriteBatch {
public:
    static constexpr int kMaxQuads      = 4096;
    static constexpr int kVertsPerQuad  = 4;
    static constexpr int kIndicesPerQuad = 6;
    static constexpr int kMaxVertices   = kMaxQuads * kVertsPerQuad;
    static constexpr int kMaxIndices    = kMaxQuads * kIndicesPerQuad;

    SpriteBatch();
    ~SpriteBatch();

    SpriteBatch(const SpriteBatch&) = delete;
    SpriteBatch& operator=(const SpriteBatch&) = delete;

    // Initialize GL objects. The sprite shader must already be loaded.
    // textureCache is used to resolve TextureId -> GL id during flush().
    bool init(Shader* spriteShader, TextureCache* textureCache);
    void shutdown();

    // Start a new frame batch. vp = view-projection matrix.
    void begin(const math::Mat4& vp);
    // Flush any pending quads.
    void flush();

    // Submit a textured quad (centered at pos with given size and Z rotation in radians).
    // `tex` is a TextureId (u32) from Renderer/TextureCache; 0 is invalid.
    void drawQuad(u32 tex,
                  const math::Vec3& pos,
                  const math::Vec2& size,
                  float rot,
                  const math::Color& tint,
                  const math::Vec2& uv0 = {0,0},
                  const math::Vec2& uv1 = {1,1});

    // Convenience: solid-color quad using the default white texture.
    void drawSolidQuad(const math::Vec3& pos,
                       const math::Vec2& size,
                       const math::Color& color);

    // Optional ring/disc approximation for circles via N quads.
    void drawCircleApprox(const math::Vec3& pos,
                          float radius,
                          int segments,
                          const math::Color& color);

    // Bind texture for the next batch segment (called automatically by drawQuad).
    void setTexture(u32 tex);

    int quadCount() const { return quadCount_; }

private:
    Shader* shader_ = nullptr;
    TextureCache* texCache_ = nullptr;
    u32 vao_ = 0;
    u32 vbo_ = 0;
    u32 ibo_ = 0;

    SpriteVertex vertices_[kMaxVertices];
    int   vertexCount_ = 0;
    int   quadCount_   = 0;

    u32   currentTex_ = 0; // 0 = INVALID_TEXTURE
    math::Mat4 vp_;

    bool initialized_ = false;
};

// ---- Line batch (no texture, separate shader) ----
struct LineVertex {
    f32 x, y, z;
    f32 r, g, b, a;
};

class LineBatch {
public:
    static constexpr int kMaxVertices = 1 << 16; // 65536

    LineBatch();
    ~LineBatch();

    LineBatch(const LineBatch&) = delete;
    LineBatch& operator=(const LineBatch&) = delete;

    bool init(Shader* lineShader);
    void shutdown();

    void begin(const math::Mat4& vp);
    void flush();

    // Submit a single line (2 verts, GL_LINES draw call per flush).
    void drawLine(const math::Vec3& a, const math::Vec3& b, const math::Color& color);

    // Submit a rectangle outline as 4 lines (8 verts).
    void drawRectLines(const math::Vec3& pos, const math::Vec2& size, const math::Color& color);

    // Submit a grid of cellsX*cellsY cells starting at origin.
    void drawGrid(const math::Vec3& origin, float cellSize,
                  int cellsX, int cellsY, const math::Color& color);

    int vertexCount() const { return vertexCount_; }

private:
    Shader* shader_ = nullptr;
    u32 vao_ = 0;
    u32 vbo_ = 0;
    LineVertex vertices_[kMaxVertices];
    int vertexCount_ = 0;
    math::Mat4 vp_;
    bool initialized_ = false;
};

} // namespace pocket::render
