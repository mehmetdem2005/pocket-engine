#pragma once
// PocketEngine — Editor panel declarations
//
// Each panel is a free function in `pocket::editor` that renders one ImGui
// window for the current frame. The Editor::render() function calls them in
// the correct order (dockspace first, then toolbar, then panels, then stats).
//
// All panels pull state from `Editor::instance()` (selection, play mode, tool)
// or from `pocket::ecs::registry()` (entity data) — they are stateless on
// their own except for a few file-scope statics (FBO handles, scroll positions,
// filter buffers) which are clearly documented at the top of each .cpp file.

#include "pocket/core/types.h"
#include "pocket/core/log.h"   // for pocket::LogLevel used by EditorConsole

namespace pocket::editor {

// ---- Dockspace (editor/src/dock/dockspace.cpp) ----
// Renders the host dockspace window and, on first run, builds the default
// Unity-style landscape layout via ImGui::DockBuilder.
void renderDockspace();

// ---- Menu bar (drawn inside renderDockspace via ImGui::BeginMainMenuBar) ----
void renderMenuBar();

// ---- Toolbar (editor/src/panels/toolbar.cpp) ----
// Play/Pause/Stop, tool select (Move/Rotate/Scale), grid snap, save scene.
void renderToolbar();

// ---- Panels (editor/src/panels/*.cpp) ----
void renderScenePanel();
void renderGamePanel();
void renderHierarchyPanel();
void renderInspectorPanel();
void renderProjectPanel();
void renderConsolePanel();

// ---- Stats overlay (editor/src/panels/stats_overlay.cpp) ----
// Top-right floating overlay with FPS / frame time / draw calls / entities.
void renderStatsOverlay();

// ---- Theme (editor/src/theme/theme.cpp) ----
// Apply Unity-dark ImGui style + landscape font scaling. Call after
// ImGui::CreateContext() and before the first frame.
void applyEditorTheme();

// ---- Editor console log sink ----
// Captures engine PE_LOG output by redirecting stderr to a pipe and parsing
// the formatted lines back into structured entries. See console_panel.cpp.
struct ConsoleEntry;
class EditorConsole {
public:
    static EditorConsole& instance();

    // Captures stderr via dup2 + pipe. Call from Editor::init().
    void init();
    // Stops reader thread, restores stderr. Call from Editor::shutdown().
    void shutdown();

    // Thread-safe access to the captured entries (copy snapshot).
    // `filterMask` is a bitmask of (1 << LogLevel) for entries to include.
    void getEntries(pocket::Vector<ConsoleEntry>& out, u32 filterMask, const char* textFilter) const;

    void clear();

    // Allow editor code to push its own messages without going through stderr.
    void push(pocket::LogLevel lvl, const char* tag, const char* fmt, ...);

    // ---- UI-level accessors (used by renderConsolePanel) ----
    u32  levelMask() const;
    void setLevelMask(u32 mask);
    // Returns a writable pointer to the internal text filter buffer (128 bytes).
    char* textFilterBuffer();
    const char* textFilter() const;

private:
    EditorConsole();
    ~EditorConsole();
    EditorConsole(const EditorConsole&) = delete;
    EditorConsole& operator=(const EditorConsole&) = delete;
    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace pocket::editor
