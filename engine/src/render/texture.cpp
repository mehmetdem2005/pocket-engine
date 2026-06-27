// PocketEngine — texture cache implementation (SDL_image + GL ES3)
#include "pocket/render/texture.h"
#include "pocket/core/log.h"

#include <GLES3/gl3.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_surface.h>

#include <cstring>

namespace pocket::render {

#define GL_CHECK(x) do { x; GLuint e = glGetError(); if(e) PE_ERROR("gl","%s err=0x%x at %s:%d", #x, e, __FILE__, __LINE__); } while(0)

TextureCache::TextureCache() = default;
TextureCache::~TextureCache() { shutdown(); }

void TextureCache::init() {
    // 0 reserved as INVALID. ids start at 1.
}

namespace {
GLenum channelsToFormat(int ch) {
    switch (ch) {
        case 1:  return GL_LUMINANCE;       // not in core GLES3 but tolerated by drivers
        case 3:  return GL_RGB;
        case 4:  return GL_RGBA;
        default: return GL_RGBA;
    }
}
GLenum channelsToInternal(int ch) {
    switch (ch) {
        case 1:  return GL_LUMINANCE;
        case 3:  return GL_RGB8;
        case 4:  return GL_RGBA8;
        default: return GL_RGBA8;
    }
}
} // namespace

u32 TextureCache::createDefaultWhite() {
    // If already created (id=1), do nothing.
    auto it = textures_.find(1);
    if (it != textures_.end()) return 1;

    u32 tex = 0;
    GL_CHECK(glGenTextures(1, &tex));
    if (!tex) {
        PE_ERROR("gl", "createDefaultWhite: glGenTextures failed");
        return 0;
    }
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, tex));
    u8 white[4] = { 255, 255, 255, 255 };
    GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));

    TextureInfo ti;
    ti.glId = tex;
    ti.w = 1; ti.h = 1; ti.channels = 4;
    textures_[1] = ti;
    PE_DEBUG("render", "default white texture created id=1 glId=%u", tex);
    return 1;
}

u32 TextureCache::loadFromMemory(const u8* data, int w, int h, int channels) {
    if (!data || w <= 0 || h <= 0) return 0;
    u32 id = ++nextId_;
    if (id == 1) id = ++nextId_; // skip default-white slot
    u32 tex = 0;
    GL_CHECK(glGenTextures(1, &tex));
    if (!tex) return 0;
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, tex));
    GLenum fmt = channelsToFormat(channels);
    GLenum internalFmt = channelsToInternal(channels);
    GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));

    TextureInfo ti;
    ti.glId = tex;
    ti.w = w; ti.h = h; ti.channels = channels;
    textures_[id] = ti;
    return id;
}

u32 TextureCache::load(const char* path) {
    if (!path || !*path) return 0;
    auto it = pathCache_.find(path);
    if (it != pathCache_.end()) return it->second;

    SDL_Surface* surf = IMG_Load(path);
    if (!surf) {
        PE_ERROR("render", "IMG_Load '%s' failed: %s", path, IMG_GetError());
        return 0;
    }
    // Ensure RGBA for predictable uploads.
    SDL_Surface* conv = nullptr;
    int channels = surf->format->BytesPerPixel;
    if (channels != 4) {
        conv = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_ABGR8888, 0);
        SDL_FreeSurface(surf);
        surf = conv;
        if (!surf) {
            PE_ERROR("render", "SDL_ConvertSurfaceFormat '%s' failed", path);
            return 0;
        }
        channels = 4;
    }
    // surf is now RGBA8 (ABGR8888 in SDL = RGBA byte order in memory on little-endian).
    u32 id = loadFromMemory(reinterpret_cast<const u8*>(surf->pixels),
                            surf->w, surf->h, channels);
    SDL_FreeSurface(surf);
    if (id) {
        pathCache_[path] = id;
        PE_DEBUG("render", "texture loaded '%s' id=%u %dx%d ch=%d",
                 path, id, width(id), height(id), channels);
    }
    return id;
}

void TextureCache::free(u32 id) {
    auto it = textures_.find(id);
    if (it == textures_.end()) return;
    if (it->second.glId) {
        GL_CHECK(glDeleteTextures(1, &it->second.glId));
    }
    textures_.erase(it);
    // Remove from path cache as well.
    for (auto pit = pathCache_.begin(); pit != pathCache_.end(); ++pit) {
        if (pit->second == id) { pathCache_.erase(pit); break; }
    }
}

const TextureInfo* TextureCache::info(u32 id) const {
    auto it = textures_.find(id);
    return it == textures_.end() ? nullptr : &it->second;
}
int TextureCache::width(u32 id) const {
    auto* ti = info(id);
    return ti ? ti->w : 0;
}
int TextureCache::height(u32 id) const {
    auto* ti = info(id);
    return ti ? ti->h : 0;
}

bool TextureCache::bind(u32 id) const {
    auto* ti = info(id);
    if (!ti || !ti->glId) return false;
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, ti->glId));
    return true;
}

void TextureCache::shutdown() {
    for (auto& kv : textures_) {
        if (kv.second.glId) {
            glDeleteTextures(1, &kv.second.glId);
        }
    }
    textures_.clear();
    pathCache_.clear();
    nextId_ = 1;
}

#undef GL_CHECK

} // namespace pocket::render
