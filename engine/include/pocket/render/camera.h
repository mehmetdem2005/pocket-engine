#pragma once
// PocketEngine — camera (2D/3D view-projection)
#include "pocket/math/math.h"

namespace pocket::render {

struct Camera {
    math::Vec3 position{0,0,0};
    float rotation = 0.0f;     // radians, Z axis
    float zoom = 1.0f;
    float fov = 60.0f;
    bool  ortho = true;
    float nearZ = -100.0f, farZ = 100.0f;
    int   viewportX = 0, viewportY = 0, viewportW = 0, viewportH = 0;

    math::Mat4 viewMatrix() const;
    // aspect = width / height
    math::Mat4 projectionMatrix(float aspect) const;
    math::Mat4 viewProjection(float aspect) const;
};

} // namespace pocket::render
