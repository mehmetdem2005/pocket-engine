// PocketEngine — Time/clock implementation using SDL_GetTicks64 (ms)
#include "pocket/core/time.h"
#include "pocket/core/log.h"
#include <SDL2/SDL.h>

namespace pocket {

void Time::update() {
    const u64 now = SDL_GetTicks64();

    // First call: seed baseline. frameCount_==0 acts as the "uninitialized" flag
    // (robust against SDL_GetTicks64() returning 0 immediately after SDL_Init).
    if (frameCount_ == 0) {
        startTicks_ = now;
        lastTicks_  = now;
        delta_      = 0.0f;
        total_      = 0.0f;
        fps_        = 60.0f;
        fpsAccum_   = 0.0f;
        fpsFrames_  = 0;
        frameCount_ = 1;
        return;
    }

    // Elapsed seconds since last update (double precision for stability).
    float d = (float)((double)(now - lastTicks_) / 1000.0);
    lastTicks_ = now;

    // Clamp to maxDelta to prevent "spiral of death" after a long stall
    // (e.g. window dragged, debugger breakpoint, GC pause).
    if (d < 0.0f)        d = 0.0f;          // clock skew / wrap guard
    if (d > maxDelta_)   d = maxDelta_;

    delta_ = d;
    total_ += d;
    ++frameCount_;

    // Smoothed FPS over ~0.5s window.
    fpsAccum_  += d;
    ++fpsFrames_;
    if (fpsAccum_ >= 0.5f) {
        fps_ = (float)((double)fpsFrames_ / (double)fpsAccum_);
        fpsAccum_  = 0.0f;
        fpsFrames_ = 0;
    }
}

float Time::delta() const { return delta_; }
float Time::total() const { return total_; }
float Time::fps()   const { return fps_;   }

void  Time::setFixedDelta(float s) {
    if (s > 0.0f) fixedDelta_ = s;
    else PE_WARN("time", "setFixedDelta ignored: non-positive value (%f)", s);
}
float Time::fixedDelta() const { return fixedDelta_; }

void  Time::setMaxDelta(float s) {
    if (s > 0.0f) maxDelta_ = s;
    else PE_WARN("time", "setMaxDelta ignored: non-positive value (%f)", s);
}
float Time::maxDelta() const { return maxDelta_; }

int Time::frameCount() const { return frameCount_; }

Time& time() {
    static Time t;
    return t;
}

} // namespace pocket
