#pragma once
// PocketEngine — SDL2 window abstraction (landscape-oriented, OpenGL ES 3.0)
#include "pocket/core/types.h"
struct SDL_Window;
typedef void* SDL_GLContext;
namespace pocket {
struct WindowDesc {
    int width = 1280;
    int height = 720;
    const char* title = "PocketEngine";
    bool fullscreen = false;
    bool resizable = true;
    bool landscape = true;       // landscape-optimized
    int glMajor = 3, glMinor = 0; // OpenGL ES 3.0
};
class Window {
public:
    bool create(const WindowDesc& desc = {});
    void destroy();
    void pollEvents();
    void swap();
    bool shouldClose() const;
    int width() const;
    int height() const;
    bool isLandscape() const;
    void setLandscape(bool);
    SDL_Window* nativeHandle() const;
    SDL_GLContext glContext() const;
    float aspect() const;
private:
    SDL_Window* window_ = nullptr;
    SDL_GLContext gl_ = nullptr;
    bool shouldClose_ = false;
    int width_ = 0, height_ = 0;
    bool landscape_ = true;
};
Window& window();
}
