// PocketEngine Editor — ImGui dockspace host + default landscape layout
//
// On the first frame, we use ImGui::DockBuilder to install a Unity-style
// landscape layout:
//
//   +--------------------------------------------------------------+
//   | Menu Bar: File Edit Entity Assets View Help                  |
//   +--------------------------------------------------------------+
//   | Toolbar                                                       |
//   +--------+--------------------------------------+--------------+
//   |        |  Scene | Game                        |  Inspector   |
//   | Hier   |                                      |              |
//   |        |                                      |              |
//   |        +--------------------------------------+              |
//   |        |  Console                             |              |
//   +--------+--------------------------------------+--------------+
//   | Project (tabs)                                               |
//   +--------------------------------------------------------------+
//   | Status bar                                                    |
//   +--------------------------------------------------------------+
//
// ImGui persists the user's customisations to imgui.ini afterwards.

#include "editor/editor.h"
#include "editor/panels.h"

#include "pocket/core/log.h"

#include "imgui.h"
#include "imgui_internal.h"   // DockBuilder API

#include <cstdio>

namespace pocket::editor {

namespace {

constexpr const char* kDockspaceName = "PocketEditorDockspace";

} // namespace

// ---- Menu bar (called inside the host window's Begin/End) ----
// Must be invoked between ImGui::BeginMenuBar() and ImGui::EndMenuBar().
static void drawMenuBarItems() {
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New Scene", "Ctrl+N"))  Editor::instance().newScene();
        if (ImGui::MenuItem("Save Scene", "Ctrl+S")) Editor::instance().saveScene();
        if (ImGui::MenuItem("Load Scene", "Ctrl+O")) Editor::instance().loadScene("assets/scenes/default.scene.json");
        ImGui::Separator();
        if (ImGui::MenuItem("Quit", "Alt+F4")) {
            PE_INFO("editor", "File → Quit requested");
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Undo", "Ctrl+Z")) {} // stub
        if (ImGui::MenuItem("Redo", "Ctrl+Y")) {} // stub
        ImGui::Separator();
        if (ImGui::MenuItem("Preferences...")) {} // stub
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Entity")) {
        namespace ecs = ::pocket::ecs;
        if (ImGui::MenuItem("Add Empty")) {
            ecs::Entity e = ecs::registry().create();
            ecs::registry().emplace<ecs::Tag>(e, ecs::Tag{"Entity"});
            ecs::registry().emplace<ecs::Transform>(e, ecs::Transform{});
            Editor::instance().select(e);
        }
        if (ImGui::MenuItem("Add Sprite")) {
            ecs::Entity e = ecs::registry().create();
            ecs::registry().emplace<ecs::Tag>(e, ecs::Tag{"Sprite"});
            ecs::registry().emplace<ecs::Transform>(e, ecs::Transform{});
            ecs::registry().emplace<ecs::SpriteComponent>(e, ecs::SpriteComponent{});
            Editor::instance().select(e);
        }
        if (ImGui::MenuItem("Add Camera")) {
            ecs::Entity e = ecs::registry().create();
            ecs::registry().emplace<ecs::Tag>(e, ecs::Tag{"Camera"});
            ecs::registry().emplace<ecs::Transform>(e, ecs::Transform{});
            ecs::registry().emplace<ecs::CameraComponent>(e, ecs::CameraComponent{});
            Editor::instance().select(e);
        }
        if (ImGui::MenuItem("Delete Selected", "Del")) {
            ecs::Entity sel = Editor::instance().selected();
            if (sel) ecs::registry().destroy(sel);
            Editor::instance().select({});
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Assets")) {
        if (ImGui::MenuItem("Refresh Asset List")) {} // project panel rescans
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        bool demo = Editor::instance().showDemoWindow();
        if (ImGui::MenuItem("Show ImGui Demo", nullptr, &demo)) {
            Editor::instance().showDemoWindow(demo);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Reset Layout")) {
            PE_INFO("editor", "View → Reset Layout (delete imgui.ini to reseed)");
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help")) {
        ImGui::TextDisabled("PocketEngine Editor v0.1 — landscape GLES3");
        ImGui::TextDisabled("WASD: pan scene | Wheel: zoom | MMB: drag");
        ImGui::EndMenu();
    }
}

// ---- Install default Unity-style landscape layout (DockBuilder) ----
static void installDefaultLayout(ImGuiID dockspaceId) {
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->WorkSize);

    // Split: left (Hierarchy) 20%, right area 80%.
    ImGuiID main = dockspaceId;
    ImGuiID left, rest;
    ImGui::DockBuilderSplitNode(main, ImGuiDir_Left, 0.20f, &left, &rest);

    // From `rest`: split right (Inspector) 25%, center 75%.
    ImGuiID right, center;
    ImGui::DockBuilderSplitNode(rest, ImGuiDir_Right, 0.25f, &right, &center);

    // From `center`: split up (Scene/Game) 72%, down (Console) 28%.
    ImGuiID centerTop, centerBottom;
    ImGui::DockBuilderSplitNode(center, ImGuiDir_Up, 0.72f, &centerTop, &centerBottom);

    // From `main`: split down (Project) 20%, leaving the upper 80% as-is.
    // (We split the original main node AFTER the left/right/up/down splits
    // above; DockBuilder handles this by re-using existing child nodes.)
    ImGuiID top, bottom;
    ImGui::DockBuilderSplitNode(main, ImGuiDir_Down, 0.20f, &bottom, &top);

    // Dock panels into the leaf nodes.
    ImGui::DockBuilderDockWindow("Hierarchy", left);
    ImGui::DockBuilderDockWindow("Scene",     centerTop);
    ImGui::DockBuilderDockWindow("Game",      centerTop);  // tabbed with Scene
    ImGui::DockBuilderDockWindow("Console",   centerBottom);
    ImGui::DockBuilderDockWindow("Inspector", right);
    ImGui::DockBuilderDockWindow("Project",   bottom);

    ImGui::DockBuilderFinish(dockspaceId);
    PE_INFO("editor", "Default landscape dock layout installed");
}

// ---- Public entry ----
void renderDockspace() {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);

    ImGuiWindowFlags hostFlags =
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_MenuBar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin(kDockspaceName, nullptr, hostFlags);
    ImGui::PopStyleVar(3);

    // Menu bar (host window has ImGuiWindowFlags_MenuBar set).
    if (ImGui::BeginMenuBar()) {
        drawMenuBarItems();
        ImGui::EndMenuBar();
    }

    // Toolbar — rendered as a thin strip at the top of the host content area.
    renderToolbar();

    // Dockspace fills the rest of the host content.
    ImGuiID dockspaceId = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f),
                     ImGuiDockNodeFlags_PassthruCentralNode);

    ImGui::End();

    // First-run: install default landscape layout.
    if (Editor::instance().consumeFirstRun()) {
        installDefaultLayout(dockspaceId);
    }
}

} // namespace pocket::editor
