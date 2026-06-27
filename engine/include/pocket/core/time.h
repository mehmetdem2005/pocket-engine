#pragma once
// PocketEngine — time/clock: delta, total, smoothed FPS, fixed-step support
#include "pocket/core/types.h"
namespace pocket {
class Time {
public:
    void update();
    float delta() const;          // seconds
    float total() const;          // seconds since start
    float fps() const;            // smoothed
    void setFixedDelta(float s);
    float fixedDelta() const;
    void setMaxDelta(float s);    // clamp (spiral of death guard)
    float maxDelta() const;
    int frameCount() const;
private:
    u64 startTicks_ = 0, lastTicks_ = 0;
    float delta_ = 0, total_ = 0, fps_ = 60;
    float fixedDelta_ = 1.0f / 60.0f;
    float maxDelta_ = 0.25f;
    int frameCount_ = 0;
    float fpsAccum_ = 0; int fpsFrames_ = 0;
};
Time& time();
}
