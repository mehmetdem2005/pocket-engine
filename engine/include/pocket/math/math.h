#pragma once
// PocketEngine — matematik (vektör, matris, quaternion)
#include "pocket/core/types.h"
#include <cmath>
#include <cstring>
#include <algorithm>

namespace pocket::math {

constexpr f32 PI = 3.14159265358979323846f;
constexpr f32 DEG2RAD = PI / 180.0f;
constexpr f32 RAD2DEG = 180.0f / PI;
constexpr f32 EPSILON = 1e-6f;

inline f32 clamp(f32 v, f32 lo, f32 hi) { return std::max(lo, std::min(hi, v)); }
inline f32 lerp(f32 a, f32 b, f32 t) { return a + (b - a) * t; }
inline f32 radians(f32 deg) { return deg * DEG2RAD; }
inline f32 degrees(f32 rad) { return rad * RAD2DEG; }

// ---- Vec2 ----
struct Vec2 {
    f32 x, y;
    Vec2() : x(0), y(0) {}
    Vec2(f32 v) : x(v), y(v) {}
    Vec2(f32 x_, f32 y_) : x(x_), y(y_) {}
    Vec2 operator+(const Vec2& o) const { return {x+o.x, y+o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x-o.x, y-o.y}; }
    Vec2 operator*(f32 s) const { return {x*s, y*s}; }
    Vec2 operator/(f32 s) const { return {x/s, y/s}; }
    Vec2& operator+=(const Vec2& o){ x+=o.x; y+=o.y; return *this; }
    Vec2& operator-=(const Vec2& o){ x-=o.x; y-=o.y; return *this; }
    f32 dot(const Vec2& o) const { return x*o.x + y*o.y; }
    f32 length() const { return std::sqrt(x*x + y*y); }
    f32 lengthSq() const { return x*x + y*y; }
    Vec2 normalized() const { f32 l = length(); return l > EPSILON ? Vec2{x/l, y/l} : Vec2{}; }
    f32& operator[](int i){ return (&x)[i]; }
    f32 operator[](int i) const { return (&x)[i]; }
};

// ---- Vec3 ----
struct Vec3 {
    f32 x, y, z;
    Vec3() : x(0), y(0), z(0) {}
    Vec3(f32 v) : x(v), y(v), z(v) {}
    Vec3(f32 x_, f32 y_, f32 z_) : x(x_), y(y_), z(z_) {}
    Vec3 operator+(const Vec3& o) const { return {x+o.x, y+o.y, z+o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x-o.x, y-o.y, z-o.z}; }
    Vec3 operator*(f32 s) const { return {x*s, y*s, z*s}; }
    Vec3 operator/(f32 s) const { return {x/s, y/s, z/s}; }
    Vec3& operator+=(const Vec3& o){ x+=o.x; y+=o.y; z+=o.z; return *this; }
    f32 dot(const Vec3& o) const { return x*o.x + y*o.y + z*o.z; }
    Vec3 cross(const Vec3& o) const { return {y*o.z - z*o.y, z*o.x - x*o.z, x*o.y - y*o.x}; }
    f32 length() const { return std::sqrt(x*x + y*y + z*z); }
    Vec3 normalized() const { f32 l = length(); return l > EPSILON ? Vec3{x/l, y/l, z/l} : Vec3{}; }
    f32& operator[](int i){ return (&x)[i]; }
    f32 operator[](int i) const { return (&x)[i]; }
};

// ---- Vec4 ----
struct Vec4 {
    f32 x, y, z, w;
    Vec4() : x(0), y(0), z(0), w(0) {}
    Vec4(f32 v) : x(v), y(v), z(v), w(v) {}
    Vec4(f32 x_, f32 y_, f32 z_, f32 w_) : x(x_), y(y_), z(z_), w(w_) {}
    Vec4(const Vec3& v, f32 w_) : x(v.x), y(v.y), z(v.z), w(w_) {}
};

// ---- Mat4 (column-major, OpenGL uyumlu) ----
struct Mat4 {
    f32 m[16];
    Mat4() { std::memset(m, 0, sizeof(m)); identity(); }
    void identity() { std::memset(m,0,sizeof(m)); m[0]=m[5]=m[10]=m[15]=1.0f; }

    static Mat4 translate(const Vec3& t) {
        Mat4 r; r.m[12]=t.x; r.m[13]=t.y; r.m[14]=t.z; return r;
    }
    static Mat4 scale(const Vec3& s) {
        Mat4 r; r.m[0]=s.x; r.m[5]=s.y; r.m[10]=s.z; return r;
    }
    static Mat4 rotateZ(f32 rad) {
        f32 c = std::cos(rad), s = std::sin(rad);
        Mat4 r; r.m[0]=c; r.m[1]=s; r.m[4]=-s; r.m[5]=c; return r;
    }
    static Mat4 rotateY(f32 rad) {
        f32 c = std::cos(rad), s = std::sin(rad);
        Mat4 r; r.m[0]=c; r.m[2]=-s; r.m[8]=s; r.m[10]=c; return r;
    }
    static Mat4 rotateX(f32 rad) {
        f32 c = std::cos(rad), s = std::sin(rad);
        Mat4 r; r.m[5]=c; r.m[6]=s; r.m[9]=-s; r.m[10]=c; return r;
    }
    static Mat4 ortho(f32 left, f32 right, f32 bottom, f32 top, f32 nearZ, f32 farZ) {
        Mat4 r;
        r.m[0]  = 2.0f / (right - left);
        r.m[5]  = 2.0f / (top - bottom);
        r.m[10] = -2.0f / (farZ - nearZ);
        r.m[12] = -(right + left) / (right - left);
        r.m[13] = -(top + bottom) / (top - bottom);
        r.m[14] = -(farZ + nearZ) / (farZ - nearZ);
        return r;
    }
    static Mat4 perspective(f32 fovy, f32 aspect, f32 nearZ, f32 farZ) {
        f32 f = 1.0f / std::tan(fovy * 0.5f);
        Mat4 r;
        r.m[0]  = f / aspect;
        r.m[5]  = f;
        r.m[10] = (farZ + nearZ) / (nearZ - farZ);
        r.m[11] = -1.0f;
        r.m[14] = (2.0f * farZ * nearZ) / (nearZ - farZ);
        r.m[15] = 0.0f;
        return r;
    }

    Mat4 operator*(const Mat4& o) const {
        Mat4 r;
        for (int c = 0; c < 4; ++c) {
            for (int row = 0; row < 4; ++row) {
                f32 sum = 0;
                for (int k = 0; k < 4; ++k)
                    sum += m[k*4 + row] * o.m[c*4 + k];
                r.m[c*4 + row] = sum;
            }
        }
        return r;
    }
    Vec4 operator*(const Vec4& v) const {
        return Vec4{
            m[0]*v.x + m[4]*v.y + m[8]*v.z  + m[12]*v.w,
            m[1]*v.x + m[5]*v.y + m[9]*v.z  + m[13]*v.w,
            m[2]*v.x + m[6]*v.y + m[10]*v.z + m[14]*v.w,
            m[3]*v.x + m[7]*v.y + m[11]*v.z + m[15]*v.w
        };
    }
};

// ---- Color (RGBA 0..1) ----
struct Color {
    f32 r, g, b, a;
    Color() : r(1), g(1), b(1), a(1) {}
    Color(f32 r_, f32 g_, f32 b_, f32 a_ = 1.0f) : r(r_), g(g_), b(b_), a(a_) {}
    static Color white()   { return {1,1,1,1}; }
    static Color black()   { return {0,0,0,1}; }
    static Color red()     { return {1,0,0,1}; }
    static Color green()   { return {0,1,0,1}; }
    static Color blue()    { return {0,0,1,1}; }
    static Color yellow()  { return {1,1,0,1}; }
    static Color cyan()    { return {0,1,1,1}; }
    static Color magenta() { return {1,0,1,1}; }
    static Color gray()    { return {0.5f,0.5f,0.5f,1}; }
    // 0xAARRGGBB packed
    static Color fromARGB(u32 c) {
        return {((c>>16)&0xff)/255.0f, ((c>>8)&0xff)/255.0f, (c&0xff)/255.0f, ((c>>24)&0xff)/255.0f};
    }
    u32 toABGR() const {
        return ((u32)(a*255)<<24) | ((u32)(b*255)<<16) | ((u32)(g*255)<<8) | (u32)(r*255);
    }
};

} // namespace pocket::math
