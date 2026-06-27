// PocketEngine — high-level OpenGL ES 3.0 renderer implementation
#include "pocket/render/renderer.h"
#include "pocket/render/shader.h"
#include "pocket/render/texture.h"
#include "pocket/render/sprite_batch.h"
#include "pocket/core/log.h"

#include <GLES3/gl3.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdlib>

namespace pocket::render {

#define GL_CHECK(x) do { x; GLuint e = glGetError(); if(e) PE_ERROR("gl","%s err=0x%x at %s:%d", #x, e, __FILE__, __LINE__); } while(0)

// ---- Local helpers ----
namespace {

// Generic 4x4 matrix inverse via Gauss-Jordan elimination (column-major data).
bool inverseMat4(const float* m, float* out) {
    // Build augmented [m | I]
    float a[4][8];
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            // column-major: m[c*4 + r]
            a[r][c] = m[c*4 + r];
        }
        for (int c = 4; c < 8; ++c) {
            a[r][c] = (c - 4 == r) ? 1.0f : 0.0f;
        }
    }
    for (int c = 0; c < 4; ++c) {
        // pivot
        int piv = c;
        float maxV = std::fabs(a[c][c]);
        for (int r = c + 1; r < 4; ++r) {
            float v = std::fabs(a[r][c]);
            if (v > maxV) { maxV = v; piv = r; }
        }
        if (maxV < 1e-9f) return false;
        if (piv != c) {
            for (int k = 0; k < 8; ++k) std::swap(a[c][k], a[piv][k]);
        }
        float d = a[c][c];
        for (int k = 0; k < 8; ++k) a[c][k] /= d;
        for (int r = 0; r < 4; ++r) {
            if (r == c) continue;
            float f = a[r][c];
            if (f == 0.0f) continue;
            for (int k = 0; k < 8; ++k) a[r][k] -= f * a[c][k];
        }
    }
    // Extract inverse (column-major).
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            out[c*4 + r] = a[r][c + 4];
        }
    }
    return true;
}

String shaderPath(const char* name) {
    const char* dir = std::getenv("POCKET_ASSETS_DIR");
    if (dir && *dir) {
        String s = dir;
        if (!s.empty() && s.back() != '/') s += '/';
        s += "shaders/";
        s += name;
        return s;
    }
    String s = "assets/shaders/";
    s += name;
    return s;
}

} // namespace

// ---- Pimpl holding subsystems ----
struct Renderer::Impl {
    Shader        spriteShader;
    Shader        lineShader;
    TextureCache  textures;
    SpriteBatch   spriteBatch;
    LineBatch     lineBatch;

    math::Mat4    vp;
    bool          inFrame = false;
};

// =====================================================================
//                            Renderer
// =====================================================================
Renderer& renderer() {
    static Renderer r;
    return r;
}

bool Renderer::init(void* sdlWindow) {
    if (impl_) return true;
    if (!sdlWindow) {
        PE_ERROR("render", "Renderer::init: null SDL_Window");
        return false;
    }
    window_ = sdlWindow;

    // Make the GL context current (the window module may have already created
    // the context; we expect to be handed an SDL_Window* and to either find an
    // existing context or create one).
    SDL_Window* win = static_cast<SDL_Window*>(window_);

    // Try to obtain an existing context first; if none, create one.
    SDL_GLContext existing = SDL_GL_GetCurrentContext();
    if (existing) {
        glCtx_ = existing;
    } else {
        // Request a GLES 3.0 context.
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
        SDL_GL_SetAttribute(SDL_GL_RED_SIZE,   8);
        SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,  8);
        SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        glCtx_ = SDL_GL_CreateContext(win);
        if (!glCtx_) {
            PE_ERROR("render", "SDL_GL_CreateContext failed: %s", SDL_GetError());
            return false;
        }
    }
    if (SDL_GL_MakeCurrent(win, glCtx_) != 0) {
        PE_ERROR("render", "SDL_GL_MakeCurrent failed: %s", SDL_GetError());
        return false;
    }

    // Enable vsync if available.
    SDL_GL_SetSwapInterval(1);

    // Query drawable size.
    int dw = 0, dh = 0;
    SDL_GL_GetDrawableSize(win, &dw, &dh);
    if (dw <= 0 || dh <= 0) {
        SDL_GetWindowSize(win, &dw, &dh);
    }
    width_  = dw > 0 ? dw : 1280;
    height_ = dh > 0 ? dh : 720;

    PE_INFO("render", "Renderer::init drawable=%dx%d", width_, height_);

    // Print GL info.
    const char* vendor   = (const char*)glGetString(GL_VENDOR);
    const char* renderer_ = (const char*)glGetString(GL_RENDERER);
    const char* version  = (const char*)glGetString(GL_VERSION);
    const char* sl       = (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
    PE_INFO("render", "GL vendor=%s renderer=%s", vendor ? vendor : "?", renderer_ ? renderer_ : "?");
    PE_INFO("render", "GL version=%s  GLSL=%s", version ? version : "?", sl ? sl : "?");

    impl_ = new Impl();

    // Shaders.
    String vSprite = shaderPath("sprite.vert");
    String fSprite = shaderPath("sprite.frag");
    String vLine   = shaderPath("line.vert");
    String fLine   = shaderPath("line.frag");
    if (!impl_->spriteShader.loadFromFile(vSprite.c_str(), fSprite.c_str())) {
        // Fall back to embedded sources directly.
        if (!impl_->spriteShader.loadFromSource(kDefaultSpriteVertSrc, kDefaultSpriteFragSrc)) {
            PE_ERROR("render", "sprite shader load failed (file + fallback)");
            shutdown();
            return false;
        }
    }
    if (!impl_->lineShader.loadFromFile(vLine.c_str(), fLine.c_str())) {
        if (!impl_->lineShader.loadFromSource(kDefaultLineVertSrc, kDefaultLineFragSrc)) {
            PE_ERROR("render", "line shader load failed (file + fallback)");
            shutdown();
            return false;
        }
    }

    // Init texture cache and reserve the default white texture (id=1).
    impl_->textures.init();
    TextureId white = impl_->textures.createDefaultWhite();
    if (white != 1) {
        PE_WARN("render", "default white texture id != 1 (got %u)", white);
    }

    // Init batches.
    if (!impl_->spriteBatch.init(&impl_->spriteShader, &impl_->textures)) {
        PE_ERROR("render", "SpriteBatch::init failed");
        shutdown();
        return false;
    }
    if (!impl_->lineBatch.init(&impl_->lineShader)) {
        PE_ERROR("render", "LineBatch::init failed");
        shutdown();
        return false;
    }

    // GL state defaults.
    GL_CHECK(glEnable(GL_BLEND));
    GL_CHECK(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    GL_CHECK(glDisable(GL_DEPTH_TEST));
    GL_CHECK(glViewport(0, 0, width_, height_));

    PE_INFO("render", "Renderer::init complete");
    return true;
}

void Renderer::shutdown() {
    if (!impl_) return;
    impl_->spriteBatch.shutdown();
    impl_->lineBatch.shutdown();
    impl_->textures.shutdown();
    impl_->spriteShader.destroy();
    impl_->lineShader.destroy();
    delete impl_;
    impl_ = nullptr;

    if (glCtx_) {
        // Only destroy if we own it. We can't perfectly detect ownership, so
        // we leave context destruction to the window module to avoid double-free.
        // If we created it, we should destroy it. We track that with a flag.
    }
    glCtx_  = nullptr;
    window_ = nullptr;
    width_  = height_ = 0;
}

void Renderer::setClearColor(const math::Color& c) {
    clearColor_ = c;
}

void Renderer::clear() {
    GL_CHECK(glClearColor(clearColor_.r, clearColor_.g, clearColor_.b, clearColor_.a));
    GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
}

void Renderer::begin(const Camera& cam) {
    if (!impl_) return;
    int vpW = cam.viewportW > 0 ? cam.viewportW : width_;
    int vpH = cam.viewportH > 0 ? cam.viewportH : height_;
    GL_CHECK(glViewport(cam.viewportX, cam.viewportY, vpW, vpH));
    clear();

    float aspect = (vpH > 0) ? float(vpW) / float(vpH) : 1.0f;
    impl_->vp = cam.viewProjection(aspect);
    impl_->spriteBatch.begin(impl_->vp);
    impl_->lineBatch.begin(impl_->vp);
    impl_->inFrame = true;
}

void Renderer::end() {
    if (!impl_ || !impl_->inFrame) return;
    impl_->spriteBatch.flush();
    impl_->lineBatch.flush();
    impl_->inFrame = false;
}

void Renderer::present() {
    if (!window_) return;
    // Ensure any pending batches are flushed before swapping.
    if (impl_ && impl_->inFrame) {
        impl_->spriteBatch.flush();
        impl_->lineBatch.flush();
        impl_->inFrame = false;
    }
    SDL_GL_SwapWindow(static_cast<SDL_Window*>(window_));
}

// ---- Textures ----
TextureId Renderer::loadTexture(const char* path) {
    if (!impl_) return INVALID_TEXTURE;
    return impl_->textures.load(path);
}

TextureId Renderer::loadTextureFromMemory(const u8* data, int w, int h, int channels) {
    if (!impl_) return INVALID_TEXTURE;
    return impl_->textures.loadFromMemory(data, w, h, channels);
}

void Renderer::freeTexture(TextureId id) {
    if (!impl_) return;
    if (id == 1) return; // do not free default white
    impl_->textures.free(id);
}

math::Vec2 Renderer::textureSize(TextureId id) const {
    if (!impl_) return {};
    return math::Vec2{ (float)impl_->textures.width(id),
                       (float)impl_->textures.height(id) };
}

// ---- Drawing primitives ----
void Renderer::drawSprite(TextureId tex, const math::Vec3& pos, const math::Vec2& size,
                          float rot, const math::Color& tint, int /*layer*/) {
    if (!impl_ || !impl_->inFrame) return;
    if (tex == INVALID_TEXTURE) tex = 1;
    impl_->spriteBatch.drawQuad(tex, pos, size, rot, tint);
}

void Renderer::drawRect(const math::Vec3& pos, const math::Vec2& size, const math::Color& color) {
    if (!impl_ || !impl_->inFrame) return;
    impl_->spriteBatch.drawSolidQuad(pos, size, color);
}

void Renderer::drawRectLines(const math::Vec3& pos, const math::Vec2& size, const math::Color& color) {
    if (!impl_ || !impl_->inFrame) return;
    impl_->lineBatch.drawRectLines(pos, size, color);
}

void Renderer::drawCircle(const math::Vec3& pos, float radius, const math::Color& color) {
    if (!impl_ || !impl_->inFrame) return;
    int seg = 32;
    if (radius > 0.0f) seg = std::max(8, std::min(64, (int)(radius * 0.5f) + 16));
    impl_->spriteBatch.drawCircleApprox(pos, radius, seg, color);
}

void Renderer::drawLine(const math::Vec3& a, const math::Vec3& b, const math::Color& color) {
    if (!impl_ || !impl_->inFrame) return;
    impl_->lineBatch.drawLine(a, b, color);
}

void Renderer::drawGrid(const math::Vec3& origin, float cellSize, int cellsX, int cellsY,
                        const math::Color& color) {
    if (!impl_ || !impl_->inFrame) return;
    impl_->lineBatch.drawGrid(origin, cellSize, cellsX, cellsY, color);
}

// ---- Coordinate conversions ----
math::Vec2 Renderer::worldToScreen(const math::Vec3& world, const Camera& cam) const {
    int vpW = cam.viewportW > 0 ? cam.viewportW : width_;
    int vpH = cam.viewportH > 0 ? cam.viewportH : height_;
    float aspect = (vpH > 0) ? float(vpW) / float(vpH) : 1.0f;
    math::Mat4 vp = cam.viewProjection(aspect);
    math::Vec4 clip = vp * math::Vec4{ world.x, world.y, world.z, 1.0f };
    if (std::fabs(clip.w) < 1e-6f) return { 0, 0 };
    float ndcX = clip.x / clip.w;
    float ndcY = clip.y / clip.w;
    // Y flipped: NDC top = +1, screen top = 0
    float sx = (ndcX * 0.5f + 0.5f) * float(vpW) + float(cam.viewportX);
    float sy = (1.0f - (ndcY * 0.5f + 0.5f)) * float(vpH) + float(cam.viewportY);
    return { sx, sy };
}

math::Vec3 Renderer::screenToWorld(const math::Vec2& screen, const Camera& cam) const {
    int vpW = cam.viewportW > 0 ? cam.viewportW : width_;
    int vpH = cam.viewportH > 0 ? cam.viewportH : height_;
    float aspect = (vpH > 0) ? float(vpW) / float(vpH) : 1.0f;
    math::Mat4 vp = cam.viewProjection(aspect);

    float inv[16];
    if (!inverseMat4(vp.m, inv)) return {};

    // Screen -> NDC.
    float ndcX = (screen.x - float(cam.viewportX)) / float(vpW) * 2.0f - 1.0f;
    float ndcY = 1.0f - (screen.y - float(cam.viewportY)) / float(vpH) * 2.0f;

    // Apply inverse VP to (ndcX, ndcY, 0, 1).
    float wx = inv[0]*ndcX + inv[4]*ndcY + inv[8]*0.0f  + inv[12]*1.0f;
    float wy = inv[1]*ndcX + inv[5]*ndcY + inv[9]*0.0f  + inv[13]*1.0f;
    float wz = inv[2]*ndcX + inv[6]*ndcY + inv[10]*0.0f + inv[14]*1.0f;
    float ww = inv[3]*ndcX + inv[7]*ndcY + inv[11]*0.0f + inv[15]*1.0f;
    if (std::fabs(ww) < 1e-6f) return {};
    return { wx / ww, wy / ww, wz / ww };
}

#undef GL_CHECK

} // namespace pocket::render
