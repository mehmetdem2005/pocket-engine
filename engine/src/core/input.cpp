// PocketEngine — Input implementation: edge detection + per-frame delta
#include "pocket/core/input.h"
#include "pocket/core/log.h"
#include <cstring>

namespace pocket {

void Input::beginFrame() {
    // Snapshot current → previous for edge detection.
    std::memcpy(previous_, current_, sizeof(current_));
    std::memcpy(mousePrev_, mouseCur_, sizeof(mouseCur_));
    // Per-frame accumulators reset; position (mx_,my_) retained so deltas can be
    // accumulated across multiple motion events within the same frame.
    mdx_   = 0.0f;
    mdy_   = 0.0f;
    wheel_ = 0.0f;
}

void Input::endFrame() {
    // Reserved for future bookkeeping (e.g. auto-repeat buffering).
}

// ---- Keyboard ----
bool Input::keyDown(int scancode) const {
    if (scancode < 0 || scancode >= 512) return false;
    return current_[scancode];
}
bool Input::keyPressed(int scancode) const {
    if (scancode < 0 || scancode >= 512) return false;
    return current_[scancode] && !previous_[scancode];
}
bool Input::keyReleased(int scancode) const {
    if (scancode < 0 || scancode >= 512) return false;
    return !current_[scancode] && previous_[scancode];
}
void Input::setKey(int scancode, bool down) {
    if (scancode < 0 || scancode >= 512) {
        PE_WARN("input", "scancode out of range: %d", scancode);
        return;
    }
    current_[scancode] = down;
}

// ---- Mouse buttons ----
bool Input::mouseDown(MouseButton b) const {
    int i = (int)b;
    if (i < 0 || i >= 3) return false;
    return mouseCur_[i];
}
bool Input::mousePressed(MouseButton b) const {
    int i = (int)b;
    if (i < 0 || i >= 3) return false;
    return mouseCur_[i] && !mousePrev_[i];
}
bool Input::mouseReleased(MouseButton b) const {
    int i = (int)b;
    if (i < 0 || i >= 3) return false;
    return !mouseCur_[i] && mousePrev_[i];
}
void Input::setButton(MouseButton b, bool down) {
    int i = (int)b;
    if (i < 0 || i >= 3) return;
    mouseCur_[i] = down;
}

// ---- Mouse motion ----
float Input::mouseX() const { return mx_; }
float Input::mouseY() const { return my_; }
float Input::mouseDX() const { return mdx_; }
float Input::mouseDY() const { return mdy_; }

void Input::setMouse(int x, int y) {
    // Accumulate motion delta relative to last known position. Because beginFrame
    // retains mx_/my_ (only mdx_/mdy_ are zeroed), multiple MOUSEMOTION events in
    // the same frame produce the correct cumulative delta.
    mdx_ += (float)x - mx_;
    mdy_ += (float)y - my_;
    mx_ = (float)x;
    my_ = (float)y;
}

// ---- Wheel ----
float Input::wheel() const { return wheel_; }
void Input::setWheel(float w) {
    // Accumulate (multiple wheel events per frame are possible).
    wheel_ += w;
}

// ---- Quit ----
bool Input::quitRequested() const { return quit_; }
void Input::setQuit(bool q) { quit_ = q; }

Input& input() {
    static Input s;
    return s;
}

} // namespace pocket
