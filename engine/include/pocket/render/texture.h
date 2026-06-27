#pragma once
// PocketEngine — texture cache (SDL_image + GL)
#include "pocket/core/types.h"

namespace pocket::render {

struct TextureInfo {
    u32 glId  = 0;     // OpenGL texture name
    int w     = 0;
    int h     = 0;
    int channels = 0;
};

class TextureCache {
public:
    TextureCache();
    ~TextureCache();

    TextureCache(const TextureCache&) = delete;
    TextureCache& operator=(const TextureCache&) = delete;

    // Must be called once after GL context is current.
    void init();

    // Create a 1x1 white default texture; returns its logical TextureId.
    // The id reserved for the default white texture is 1.
    u32 createDefaultWhite();

    // Load from disk via SDL_image. Returns INVALID_TEXTURE (0) on failure.
    // Cached by absolute/canonical path string.
    u32 load(const char* path);

    // Upload raw RGBA8 bytes. Returns assigned TextureId.
    u32 loadFromMemory(const u8* data, int w, int h, int channels);

    void free(u32 id);

    const TextureInfo* info(u32 id) const;
    int width(u32 id)  const;
    int height(u32 id) const;

    bool bind(u32 id) const;

    void shutdown();

private:
    u32 nextId_ = 1; // 0 = INVALID, 1 reserved for default white
    HashMap<u32, TextureInfo> textures_;
    HashMap<String, u32>      pathCache_;
};

} // namespace pocket::render
