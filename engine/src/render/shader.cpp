// PocketEngine — OpenGL ES 3.0 shader program implementation
#include "pocket/render/shader.h"
#include "pocket/core/log.h"

#include <GLES3/gl3.h>
#include <cstdio>
#include <cstring>
#include <string>

namespace pocket::render {

// ---- GL error check macro ----
#define GL_CHECK(x) do { x; GLuint e = glGetError(); if(e) PE_ERROR("gl","%s err=0x%x at %s:%d", #x, e, __FILE__, __LINE__); } while(0)

// ---- Embedded fallback shader sources (used when files cannot be loaded) ----
const char* kDefaultSpriteVertSrc = R"GLSL(#version 300 es
precision highp float;
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in vec4 a_color;
uniform mat4 u_vp;
out vec2 v_uv;
out vec4 v_color;
void main() {
    v_uv = a_uv;
    v_color = a_color;
    gl_Position = u_vp * vec4(a_pos, 1.0);
}
)GLSL";

const char* kDefaultSpriteFragSrc = R"GLSL(#version 300 es
precision mediump float;
uniform sampler2D u_tex;
in vec2 v_uv;
in vec4 v_color;
out vec4 frag;
void main() {
    frag = texture(u_tex, v_uv) * v_color;
}
)GLSL";

const char* kDefaultLineVertSrc = R"GLSL(#version 300 es
precision highp float;
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec4 a_color;
uniform mat4 u_vp;
out vec4 v_color;
void main() {
    v_color = a_color;
    gl_Position = u_vp * vec4(a_pos, 1.0);
}
)GLSL";

const char* kDefaultLineFragSrc = R"GLSL(#version 300 es
precision mediump float;
in vec4 v_color;
out vec4 frag;
void main() {
    frag = v_color;
}
)GLSL";

// ---- Helpers ----
namespace {
String readFileToString(const char* path) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return {};
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    if (sz < 0) { std::fclose(f); return {}; }
    std::fseek(f, 0, SEEK_SET);
    String out;
    out.resize(static_cast<size_t>(sz));
    size_t rd = std::fread(&out[0], 1, static_cast<size_t>(sz), f);
    out.resize(rd);
    std::fclose(f);
    return out;
}
} // namespace

// ---- Shader class ----
Shader::Shader() = default;

Shader::~Shader() {
    destroy();
}

void Shader::destroy() {
    if (program_) {
        GL_CHECK(glDeleteProgram(program_));
        program_ = 0;
    }
    uniformCache_.clear();
}

u32 Shader::compileStage(u32 type, const char* src) {
    if (!src || !*src) return 0;
    GLuint sh = glCreateShader(type);
    if (!sh) {
        PE_ERROR("gl", "glCreateShader failed type=%u err=0x%x", type, glGetError());
        return 0;
    }
    const char* srcs[1] = { src };
    glShaderSource(sh, 1, srcs, nullptr);
    glCompileShader(sh);
    GLint ok = GL_FALSE;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(sh, sizeof(log), nullptr, log);
        PE_ERROR("gl", "shader compile fail: %s", log);
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

bool Shader::loadFromSource(const char* vertSrc, const char* fragSrc) {
    destroy();
    if (!vertSrc || !fragSrc) {
        PE_ERROR("render", "Shader::loadFromSource null source");
        return false;
    }
    GLuint vs = compileStage(GL_VERTEX_SHADER, vertSrc);
    GLuint fs = compileStage(GL_FRAGMENT_SHADER, fragSrc);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return false;
    }
    program_ = glCreateProgram();
    if (!program_) {
        PE_ERROR("gl", "glCreateProgram failed err=0x%x", glGetError());
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return false;
    }
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glLinkProgram(program_);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = GL_FALSE;
    glGetProgramiv(program_, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(program_, sizeof(log), nullptr, log);
        PE_ERROR("gl", "shader link fail: %s", log);
        glDeleteProgram(program_);
        program_ = 0;
        return false;
    }
    PE_DEBUG("render", "Shader program linked id=%u", program_);
    return true;
}

bool Shader::loadFromFile(const char* vertPath, const char* fragPath) {
    String vs = readFileToString(vertPath);
    String fs = readFileToString(fragPath);

    const char* vsrc = !vs.empty() ? vs.c_str() : nullptr;
    const char* fsrc = !fs.empty() ? fs.c_str() : nullptr;

    if (!vsrc) {
        PE_WARN("render", "Shader: cannot load vert '%s' — using embedded fallback", vertPath);
        vsrc = kDefaultSpriteVertSrc;
    }
    if (!fsrc) {
        PE_WARN("render", "Shader: cannot load frag '%s' — using embedded fallback", fragPath);
        fsrc = kDefaultSpriteFragSrc;
    }
    return loadFromSource(vsrc, fsrc);
}

void Shader::use() const {
    if (program_) {
        GL_CHECK(glUseProgram(program_));
    }
}

int Shader::location(const char* name) {
    if (!program_ || !name) return -1;
    auto it = uniformCache_.find(name);
    if (it != uniformCache_.end()) return it->second;
    int loc = glGetUniformLocation(program_, name);
    uniformCache_[name] = loc;
    return loc;
}

void Shader::setMat4(const char* name, const math::Mat4& v) {
    int loc = location(name);
    if (loc >= 0) GL_CHECK(glUniformMatrix4fv(loc, 1, GL_FALSE, v.m));
}

void Shader::setVec4(const char* name, const math::Vec4& v) {
    int loc = location(name);
    if (loc >= 0) GL_CHECK(glUniform4f(loc, v.x, v.y, v.z, v.w));
}

void Shader::setVec3(const char* name, const math::Vec3& v) {
    int loc = location(name);
    if (loc >= 0) GL_CHECK(glUniform3f(loc, v.x, v.y, v.z));
}

void Shader::setVec2(const char* name, const math::Vec2& v) {
    int loc = location(name);
    if (loc >= 0) GL_CHECK(glUniform2f(loc, v.x, v.y));
}

void Shader::setInt(const char* name, int v) {
    int loc = location(name);
    if (loc >= 0) GL_CHECK(glUniform1i(loc, v));
}

void Shader::setFloat(const char* name, float v) {
    int loc = location(name);
    if (loc >= 0) GL_CHECK(glUniform1f(loc, v));
}

#undef GL_CHECK

} // namespace pocket::render
