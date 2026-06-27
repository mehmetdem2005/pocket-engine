// PocketEngine — Window implementation (SDL2 + OpenGL ES 3.0)
#include "pocket/core/window.h"
#include "pocket/core/input.h"
#include "pocket/core/log.h"
#include <SDL2/SDL.h>

namespace pocket {

namespace {

Uint32 g_sdlInitCount = 0;

MouseButton sdlButtonToPe(Uint8 b) {
    switch (b) {
    case SDL_BUTTON_LEFT:   return MouseButton::Left;
    case SDL_BUTTON_RIGHT:  return MouseButton::Right;
    case SDL_BUTTON_MIDDLE: return MouseButton::Middle;
    default:                return MouseButton::Count;
    }
}

} // anonymous namespace

bool Window::create(const WindowDesc& desc) {
    if (window_) {
        PE_WARN("window", "create() called while window already exists; reusing");
        return true;
    }

    // Hint allowed orientations BEFORE SDL_Init (Android/iOS respect this).
    if (desc.landscape) {
        SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    } else {
        SDL_SetHint(SDL_HINT_ORIENTATIONS, "Portrait");
    }

    if (g_sdlInitCount == 0) {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
            PE_ERROR("window", "SDL_Init failed: %s", SDL_GetError());
            return false;
        }
    }
    ++g_sdlInitCount;

    // OpenGL ES context attributes
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, desc.glMajor);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, desc.glMinor);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE,     8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,   8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,    8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE,   8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,   24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
    if (desc.fullscreen) flags |= SDL_WINDOW_FULLSCREEN;
    if (desc.resizable)  flags |= SDL_WINDOW_RESIZABLE;

    window_ = SDL_CreateWindow(
        desc.title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        desc.width, desc.height,
        flags);
    if (!window_) {
        PE_ERROR("window", "SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }

    gl_ = SDL_GL_CreateContext(window_);
    if (!gl_) {
        PE_ERROR("window", "SDL_GL_CreateContext failed: %s", SDL_GetError());
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        return false;
    }

    // VSync on, best-effort.
    if (SDL_GL_SetSwapInterval(1) != 0) {
        PE_WARN("window", "VSync enable failed: %s", SDL_GetError());
    }

    width_ = desc.width;
    height_ = desc.height;
    landscape_ = desc.landscape;
    shouldClose_ = false;

    PE_INFO("window", "created %dx%d %s (GLES %d.%d, fullscreen=%s, resizable=%s)",
            width_, height_,
            landscape_ ? "landscape" : "portrait",
            desc.glMajor, desc.glMinor,
            desc.fullscreen ? "yes" : "no",
            desc.resizable  ? "yes" : "no");
    if (landscape_ && width_ < height_) {
        PE_WARN("window", "landscape=true but width<height — user logic should treat width as long axis");
    }

    // Seed input mouse position so first-frame mouseDX/DY is sane (no huge jump from 0,0).
    int mx = 0, my = 0;
    SDL_GetMouseState(&mx, &my);
    input().setMouse(mx, my);

    return true;
}

void Window::destroy() {
    if (gl_) { SDL_GL_DeleteContext(gl_); gl_ = nullptr; }
    if (window_) { SDL_DestroyWindow(window_); window_ = nullptr; }
    if (g_sdlInitCount > 0) {
        --g_sdlInitCount;
        if (g_sdlInitCount == 0) {
            SDL_Quit();
        }
    }
    shouldClose_ = false;
    width_ = 0;
    height_ = 0;
}

void Window::pollEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT:
            shouldClose_ = true;
            input().setQuit(true);
            break;

        case SDL_WINDOWEVENT:
            switch (e.window.event) {
            case SDL_WINDOWEVENT_CLOSE:
                shouldClose_ = true;
                input().setQuit(true);
                break;
            case SDL_WINDOWEVENT_RESIZED:
            case SDL_WINDOWEVENT_SIZE_CHANGED:
                width_  = e.window.data1;
                height_ = e.window.data2;
                PE_INFO("window", "resized to %dx%d", width_, height_);
                break;
            default: break;
            }
            break;

        case SDL_KEYDOWN:
            input().setKey((int)e.key.keysym.scancode, true);
            break;
        case SDL_KEYUP:
            input().setKey((int)e.key.keysym.scancode, false);
            break;

        case SDL_MOUSEMOTION:
            input().setMouse(e.motion.x, e.motion.y);
            break;

        case SDL_MOUSEBUTTONDOWN: {
            MouseButton b = sdlButtonToPe(e.button.button);
            if (b != MouseButton::Count) input().setButton(b, true);
            break;
        }
        case SDL_MOUSEBUTTONUP: {
            MouseButton b = sdlButtonToPe(e.button.button);
            if (b != MouseButton::Count) input().setButton(b, false);
            break;
        }

        case SDL_MOUSEWHEEL: {
            float w = (float)e.wheel.y;
            // SDL2 may report flipped direction (natural scroll / touchpads).
            if (e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) w = -w;
            input().setWheel(w);
            break;
        }

        default: break;
        }
    }
}

void Window::swap() {
    if (window_ && gl_) SDL_GL_SwapWindow(window_);
}

bool Window::shouldClose() const { return shouldClose_; }
int  Window::width()       const { return width_; }
int  Window::height()      const { return height_; }
bool Window::isLandscape() const { return landscape_; }

void Window::setLandscape(bool l) {
    if (l == landscape_) return;
    landscape_ = l;
    // Update hint for future windows / re-create scenarios (current window unaffected on desktop).
    SDL_SetHint(SDL_HINT_ORIENTATIONS,
                l ? "LandscapeLeft LandscapeRight" : "Portrait");
    PE_INFO("window", "landscape set to %s", l ? "true" : "false");
}

SDL_Window*   Window::nativeHandle() const { return window_; }
SDL_GLContext Window::glContext()    const { return gl_; }

float Window::aspect() const {
    if (height_ == 0) return 1.0f;
    return (float)width_ / (float)height_;
}

Window& window() {
    static Window w;
    return w;
}

} // namespace pocket
