#pragma once
// PocketEngine — OpenGL ES 3.0 shader program wrapper
#include "pocket/core/types.h"
#include "pocket/math/math.h"

namespace pocket::render {

class Shader {
public:
    Shader();
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    // Load vertex + fragment shader sources from files; on failure,
    // embeds fallback default shaders so rendering keeps working.
    bool loadFromFile(const char* vertPath, const char* fragPath);
    // Load from in-memory source strings.
    bool loadFromSource(const char* vertSrc, const char* fragSrc);

    void use() const;
    void destroy();

    bool valid() const { return program_ != 0; }
    u32  id()    const { return program_; }

    // Uniform setters (cached lookup by name).
    void setMat4 (const char* name, const math::Mat4& v);
    void setVec4 (const char* name, const math::Vec4& v);
    void setVec3 (const char* name, const math::Vec3& v);
    void setVec2 (const char* name, const math::Vec2& v);
    void setInt  (const char* name, int v);
    void setFloat(const char* name, float v);

private:
    u32 program_ = 0;
    // Cache uniform locations to avoid glGetUniformLocation each call.
    HashMap<String, int> uniformCache_;

    int location(const char* name);
    static u32 compileStage(u32 type, const char* src);
};

// Built-in default shader sources used if a file fails to load.
extern const char* kDefaultSpriteVertSrc;
extern const char* kDefaultSpriteFragSrc;
extern const char* kDefaultLineVertSrc;
extern const char* kDefaultLineFragSrc;

} // namespace pocket::render
