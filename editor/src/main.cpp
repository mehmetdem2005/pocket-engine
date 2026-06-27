// PocketEngine Editor — entry point
//
// Boots the engine subsystems (window, renderer, time, input), initialises
// ImGui with SDL2 + OpenGL ES 3 backends, applies the Unity-dark theme, seeds
// a default scene (primary camera + test sprite), then runs the main loop.
//
// All engine APIs are pulled from the engine static lib via `pocket::*`
// singletons. ImGui sources are vendored under third_party/imgui.
//
// Landscape orientation is enforced via WindowDesc.landscape=true; the layout
// assumes a wide aspect (e.g. 1280x720, 1920x906 after Termux:X11 chrome).

#include "pocket/core/window.h"
#include "pocket/core/input.h"
#include "pocket/core/time.h"
#include "pocket/core/log.h"
#include "pocket/render/renderer.h"
#include "pocket/ecs/ecs.h"

#include "editor/editor.h"
#include "editor/panels.h"
#include "editor/theme.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#include <SDL2/SDL.h>

#include <cstdio>

namespace {

// SDL2 + GLES3 ImGui backend init. Returns false on failure (logged).
bool initImGui(pocket::Window& win) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // Viewports need careful GL state on GLES; disabled for Termux stability.
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.IniFilename = "imgui.ini";

    // Style + fonts (Unity-dark, landscape-scaled)
    pocket::editor::applyEditorTheme();

    SDL_Window*   sdlWin = win.nativeHandle();
    SDL_GLContext glCtx  = win.glContext();
    if (!ImGui_ImplSDL2_InitForOpenGL(sdlWin, glCtx)) {
        PE_ERROR("editor", "ImGui_ImplSDL2_InitForOpenGL failed");
        return false;
    }
    if (!ImGui_ImplOpenGL3_Init("#version 300 es")) {
        PE_ERROR("editor", "ImGui_ImplOpenGL3_Init failed");
        return false;
    }
    return true;
}

void shutdownImGui() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}

// Seed a tiny default scene: a primary ortho camera + a test sprite.
void seedDefaultScene() {
    using namespace pocket::ecs;
    auto& reg = registry();
    reg.clear();

    // Primary camera
    Entity cam = reg.create();
    reg.emplace<Tag>(cam, Tag{"Main Camera"});
    reg.emplace<Transform>(cam, Transform{});
    reg.emplace<CameraComponent>(cam, CameraComponent{});
    reg.get<CameraComponent>(cam)->primary = true;
    reg.get<CameraComponent>(cam)->zoom = 1.0f;

    // Test sprite
    Entity spr = reg.create();
    reg.emplace<Tag>(spr, Tag{"Player"});
    Transform st;
    st.position = {0.0f, 0.0f, 0.0f};
    st.scale    = {1.0f, 1.0f, 1.0f};
    reg.emplace<Transform>(spr, st);
    SpriteComponent sc;
    sc.size   = {2.0f, 2.0f};
    sc.tint   = pocket::math::Color{0.85f, 0.55f, 0.20f, 1.0f};
    sc.layer  = 0;
    reg.emplace<SpriteComponent>(spr, sc);

    PE_INFO("editor", "Default scene seeded: camera + sprite");
}

} // namespace

int main(int /*argc*/, char** /*argv*/) {
    using namespace pocket;

    PE_INFO("editor", "PocketEngine Editor starting up (landscape GLES3)");

    // ---- Window (landscape GLES3) ----
    WindowDesc desc;
    desc.width     = 1280;
    desc.height    = 720;
    desc.title     = "PocketEngine Editor";
    desc.fullscreen = false;
    desc.resizable  = true;
    desc.landscape  = true;
    desc.glMajor    = 3;
    desc.glMinor    = 0;
    if (!window().create(desc)) {
        PE_FATAL("editor", "Window create failed");
        return 1;
    }

    // ---- Renderer (uses the window's GL context) ----
    if (!render::renderer().init(window().nativeHandle())) {
        PE_FATAL("editor", "Renderer init failed");
        window().destroy();
        return 1;
    }

    // ---- ImGui ----
    if (!initImGui(window())) {
        PE_FATAL("editor", "ImGui init failed");
        render::renderer().shutdown();
        window().destroy();
        return 1;
    }

    // ---- Default scene + editor state ----
    seedDefaultScene();
    editor::Editor::instance().init();

    // ---- Main loop ----
    bool running = true;
    while (running) {
        input().beginFrame();
        window().pollEvents();
        time().update();

        if (window().shouldClose() || input().quitRequested()) {
            running = false;
        }

        // ---- ImGui new frame ----
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // ---- Editor render (dockspace + panels + toolbar + stats) ----
        editor::Editor::instance().render();

        if (editor::Editor::instance().showDemoWindow()) {
            ImGui::ShowDemoWindow();
        }

        // ---- ImGui render (draw data) ----
        ImGui::Render();
        // Clear default framebuffer to a dark editor bg before ImGui draws.
        render::renderer().setClearColor(math::Color{0.15f, 0.15f, 0.17f, 1.0f});
        render::renderer().clear();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // ---- Present ----
        window().swap();

        input().endFrame();
    }

    // ---- Shutdown (reverse order) ----
    editor::Editor::instance().shutdown();
    shutdownImGui();
    render::renderer().shutdown();
    window().destroy();

    PE_INFO("editor", "PocketEngine Editor shut down cleanly");
    return 0;
}
