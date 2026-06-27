#pragma once
// PocketEngine — input system: keyboard (scancode-indexed), mouse, wheel
#include "pocket/core/types.h"
namespace pocket {
enum class MouseButton : u8 { Left = 0, Right = 1, Middle = 2, Count = 3 };
class Input {
public:
    void beginFrame();
    void endFrame();
    bool keyDown(int scancode) const;        // held
    bool keyPressed(int scancode) const;     // edge: pressed this frame
    bool keyReleased(int scancode) const;    // edge: released this frame
    bool mouseDown(MouseButton b) const;
    bool mousePressed(MouseButton b) const;
    bool mouseReleased(MouseButton b) const;
    float mouseX() const;
    float mouseY() const;
    float mouseDX() const;
    float mouseDY() const;
    float wheel() const;
    // internal — called by window poll
    void setMouse(int x, int y);
    void setButton(MouseButton b, bool down);
    void setWheel(float w);
    void setKey(int scancode, bool down);
    bool quitRequested() const;
    void setQuit(bool);
private:
    bool current_[512] = {false};
    bool previous_[512] = {false};
    bool mouseCur_[3] = {false};
    bool mousePrev_[3] = {false};
    float mx_ = 0, my_ = 0, mdx_ = 0, mdy_ = 0, wheel_ = 0;
    bool quit_ = false;
};
Input& input();
}
