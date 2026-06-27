// PocketEngine — camera implementation
#include "pocket/render/camera.h"

namespace pocket::render {

math::Mat4 Camera::viewMatrix() const {
    // Translate by -position, then rotate by -rotation around Z, then scale by 1/zoom.
    math::Mat4 t = math::Mat4::translate({-position.x, -position.y, -position.z});
    math::Mat4 r = math::Mat4::rotateZ(-rotation);
    math::Mat4 s = math::Mat4::scale({1.0f / zoom, 1.0f / zoom, 1.0f / zoom});
    return s * r * t;
}

math::Mat4 Camera::projectionMatrix(float aspect) const {
    if (ortho) {
        // Orthographic box centered at origin, height = 2/zoom world units
        // (we already folded zoom into the view matrix; here use raw extent).
        // Use viewportH as the vertical world extent (in pixels-as-world-units).
        // If viewport is unset, fallback to a reasonable 1080-tall ortho.
        float h = (viewportH > 0) ? float(viewportH) : 1080.0f;
        float w = h * aspect;
        float left   = -w * 0.5f;
        float right  =  w * 0.5f;
        float bottom = -h * 0.5f;
        float top    =  h * 0.5f;
        return math::Mat4::ortho(left, right, bottom, top, nearZ, farZ);
    }
    return math::Mat4::perspective(math::radians(fov), aspect, nearZ, farZ);
}

math::Mat4 Camera::viewProjection(float aspect) const {
    return projectionMatrix(aspect) * viewMatrix();
}

} // namespace pocket::render
