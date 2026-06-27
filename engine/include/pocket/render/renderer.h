#pragma once
// PocketEngine — high-level OpenGL ES 3.0 renderer
#include "pocket/core/types.h"
#include "pocket/math/math.h"
#include "pocket/render/camera.h"

struct SDL_Window;

namespace pocket::render {

using TextureId = u32;
constexpr TextureId INVALID_TEXTURE = 0;

class Renderer {
public:
    bool init(void* sdlWindow);
    void shutdown();

    void setClearColor(const math::Color& c);
    void clear();
    void begin(const Camera& cam);
    void end();
    void present();

    TextureId loadTexture(const char* path);
    TextureId loadTextureFromMemory(const u8* data, int w, int h, int channels);
    void freeTexture(TextureId id);
    math::Vec2 textureSize(TextureId id) const;

    void drawSprite(TextureId tex, const math::Vec3& pos, const math::Vec2& size,
                    float rot, const math::Color& tint, int layer = 0);
    void drawRect(const math::Vec3& pos, const math::Vec2& size, const math::Color& color);
    void drawRectLines(const math::Vec3& pos, const math::Vec2& size, const math::Color& color);
    void drawCircle(const math::Vec3& pos, float radius, const math::Color& color);
    void drawLine(const math::Vec3& a, const math::Vec3& b, const math::Color& color);
    void drawGrid(const math::Vec3& origin, float cellSize, int cellsX, int cellsY, const math::Color& color);

    math::Vec2 worldToScreen(const math::Vec3& world, const Camera& cam) const;
    math::Vec3 screenToWorld(const math::Vec2& screen, const Camera& cam) const;

    int width()  const { return width_; }
    int height() const { return height_; }

private:
    void* window_ = nullptr;       // SDL_Window*
    void* glCtx_  = nullptr;       // SDL_GLContext
    int   width_  = 0;
    int   height_ = 0;
    math::Color clearColor_{0,0,0,1};

    // Subsystems (owned, allocated lazily).
    struct Impl;
    Impl* impl_ = nullptr;
};

Renderer& renderer();

} // namespace pocket::render
