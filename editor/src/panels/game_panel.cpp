// PocketEngine Editor — Game panel
//
// Renders the "game view" — what the primary CameraComponent sees. If no
// entity has a CameraComponent with `primary=true`, falls back to a default
// ortho camera at the world origin.
//
// Like the Scene panel, we render into an offscreen FBO and present it as an
// ImGui::Image. No editor overlays (no grid, no gizmo) — this is what the
// player would see.

#include "editor/editor.h"
#include "editor/panels.h"

#include "pocket/core/log.h"
#include "pocket/ecs/ecs.h"
#include "pocket/render/renderer.h"

#include "imgui.h"

#include <GLES3/gl3.h>

namespace pocket::editor {

namespace {

GLuint g_gameFbo = 0, g_gameTex = 0, g_gameRbo = 0;
int    g_gameW = 0, g_gameH = 0;

void ensureGameFBO(int w, int h) {
    if (w <= 0 || h <= 0) return;
    if (g_gameW == w && g_gameH == h && g_gameFbo != 0) return;

    if (g_gameFbo) {
        glDeleteFramebuffers(1, &g_gameFbo);
        glDeleteTextures(1, &g_gameTex);
        glDeleteRenderbuffers(1, &g_gameRbo);
        g_gameFbo = g_gameTex = g_gameRbo = 0;
    }
    g_gameW = w; g_gameH = h;

    glGenFramebuffers(1, &g_gameFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, g_gameFbo);

    glGenTextures(1, &g_gameTex);
    glBindTexture(GL_TEXTURE_2D, g_gameTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_gameTex, 0);

    glGenRenderbuffers(1, &g_gameRbo);
    glBindRenderbuffer(GL_RENDERBUFFER, g_gameRbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, g_gameRbo);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Find the primary camera entity, falling back to the first camera found.
ecs::Entity findPrimaryCamera() {
    ecs::Entity found{};
    bool anyPrimary = false;
    ::pocket::ecs::registry().each<ecs::CameraComponent>([&](ecs::Entity e, ecs::CameraComponent& c) {
        if (c.primary && !anyPrimary) {
            anyPrimary = true;
            found = e;
        } else if (!found) {
            found = e;
        }
    });
    return found;
}

} // namespace

void renderGamePanel() {
    ImGui::Begin("Game", nullptr,
                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImVec2 avail = ImGui::GetContentRegionAvail();
    int w = (int)avail.x;
    int h = (int)avail.y;
    if (w < 2 || h < 2) { ImGui::End(); return; }

    ensureGameFBO(w, h);
    glBindFramebuffer(GL_FRAMEBUFFER, g_gameFbo);

    // Build the game camera from the primary CameraComponent + Transform.
    ecs::Entity camEnt = findPrimaryCamera();
    render::Camera rcam;
    math::Color clearColor(0.15f, 0.15f, 0.17f, 1.0f);
    if (camEnt) {
        auto* cc = ::pocket::ecs::registry().get<ecs::CameraComponent>(camEnt);
        auto* tr = ::pocket::ecs::registry().get<ecs::Transform>(camEnt);
        if (cc) {
            rcam.zoom  = cc->zoom;
            rcam.fov   = cc->fov;
            rcam.ortho = cc->ortho;
            rcam.nearZ = cc->nearZ;
            rcam.farZ  = cc->farZ;
            clearColor = cc->clearColor;
        }
        if (tr) rcam.position = tr->position;
    }
    rcam.viewportX = 0; rcam.viewportY = 0;
    rcam.viewportW = w; rcam.viewportH = h;

    ::pocket::render::renderer().setClearColor(clearColor);
    ::pocket::render::renderer().clear();
    ::pocket::render::renderer().begin(rcam);

    // Render all visible sprite entities
    ::pocket::ecs::registry().each<ecs::SpriteComponent>([&](ecs::Entity e, ecs::SpriteComponent& sp) {
        // Honour the entity's Tag.visible flag if present
        auto* tag = ::pocket::ecs::registry().get<ecs::Tag>(e);
        if (tag && !tag->visible) return;
        auto* t = ::pocket::ecs::registry().get<ecs::Transform>(e);
        math::Vec3 pos = t ? t->position : math::Vec3{};
        math::Vec2 size = sp.size;
        float rot = t ? t->rotation.z : 0.0f;
        if (t) {
            size = math::Vec2(size.x * t->scale.x, size.y * t->scale.y);
        }
        ::pocket::render::renderer().drawSprite(sp.textureId, pos, size, rot, sp.tint, sp.layer);
    });

    ::pocket::render::renderer().end();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    ImTextureID texId = (ImTextureID)(uintptr_t)g_gameTex;
    ImGui::Image(texId, avail, ImVec2(0, 1), ImVec2(1, 0));

    // Tiny "no camera" hint when there's nothing to render through
    if (!camEnt) {
        ImGui::SameLine();
        ImGui::SetCursorPosX(8.0f);
        ImGui::SetCursorPosY(32.0f);
        ImGui::TextDisabled("(no Camera in scene — using default view)");
    }

    ImGui::End();
}

} // namespace pocket::editor
