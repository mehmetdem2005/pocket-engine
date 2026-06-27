#pragma once
// PocketEngine — Editor orchestrator (Unity/Godot-style, ImGui, landscape Android)
//
// The Editor class is a singleton that owns:
//   * the current selection (Entity)
//   * play/pause/stop state
//   * the active transform tool (Move/Rotate/Scale) and grid-snap toggle
//   * a dedicated editor (scene) camera, independent of any CameraComponent
//   * a first-run flag used by dockspace.cpp to seed the default layout
//
// Lifecycle is driven by main.cpp:
//     Editor::instance().init();
//     while (running) {
//         ...input/time/window...
//         ImGui new frame;
//         Editor::instance().render();
//         ImGui::Render() + backend render;
//     }
//     Editor::instance().shutdown();

#include "pocket/core/types.h"
#include "pocket/ecs/ecs.h"
#include "pocket/math/math.h"
#include "pocket/render/camera.h"

#include <string>

namespace pocket::editor {

// ---- Transform tool (toolbar) ----
enum class EditorTool : u8 {
    Move    = 0,
    Rotate  = 1,
    Scale   = 2
};

// ---- Editor (scene-view) camera ----
// Separate from any ecs::CameraComponent so the user can fly around the scene
// without affecting gameplay cameras. Uses orthographic projection.
struct EditorCamera {
    math::Vec3 position{0.0f, 0.0f, 0.0f};
    float      zoom      = 1.0f;
    float      rotation  = 0.0f;     // radians, Z axis
    float      moveSpeed = 8.0f;     // world units / second for WASD
    float      minZoom   = 0.05f;
    float      maxZoom   = 20.0f;

    // Build a render::Camera that the engine renderer can consume.
    // `aspect` is viewport w/h; viewport rect is filled from (vx,vy,vw,vh).
    render::Camera toRenderCamera(float aspect, int vx, int vy, int vw, int vh) const;

    // Convert a screen-space (ImGui) point to world space given viewport origin.
    math::Vec3 screenToWorld(float screenXinViewport, float screenYinViewport,
                             int viewportW, int viewportH) const;
};

// ---- Editor singleton ----
class Editor {
public:
    static Editor& instance();

    // Called from main.cpp after window/renderer/ImGui are up.
    // Seeds the default scene (camera entity + test sprite) and registers
    // the console log sink.
    bool init();
    void shutdown();

    // One frame. Renders menubar, dockspace, toolbar, all panels, stats overlay.
    void render();

    // ---- Scene ops (stubs — serialize to assets/scenes/*.json) ----
    void newScene();
    void saveScene(const std::string& path = "");
    void loadScene(const std::string& path);

    // ---- Play mode ----
    void play();
    void pause();
    void stop();
    bool isPlaying() const { return playMode_; }
    bool isPaused()  const { return paused_; }

    // ---- Selection ----
    void select(ecs::Entity e) { selected_ = e; }
    ecs::Entity selected() const { return selected_; }

    // ---- Tools ----
    void       setTool(EditorTool t) { tool_ = t; }
    EditorTool tool() const { return tool_; }
    bool       gridSnap() const { return gridSnap_; }
    void       setGridSnap(bool v) { gridSnap_ = v; }

    // ---- Camera ----
    EditorCamera& sceneCamera() { return sceneCam_; }
    const EditorCamera& sceneCamera() const { return sceneCam_; }

    // ---- Misc UI toggles ----
    void showDemoWindow(bool v) { showDemo_ = v; }
    bool showDemoWindow() const { return showDemo_; }

    // ---- Layout bootstrap ----
    // Returns true on the first call only; dockspace.cpp uses this to know
    // when to run ImGui::DockBuilder to install the default layout.
    bool consumeFirstRun();

    // ---- Convenience: stats values shown in the status bar / overlay ----
    int  entityCount() const;
    int  drawCallCount() const;     // best-effort — engine doesn't expose; placeholder
    float memoryMB() const;         // best-effort

private:
    Editor();
    ~Editor();
    Editor(const Editor&) = delete;
    Editor& operator=(const Editor&) = delete;

    ecs::Entity   selected_{};
    bool          playMode_  = false;
    bool          paused_    = false;
    EditorTool    tool_      = EditorTool::Move;
    bool          gridSnap_  = false;
    EditorCamera  sceneCam_;
    bool          showDemo_  = false;
    bool          firstRun_  = true;
};

} // namespace pocket::editor
