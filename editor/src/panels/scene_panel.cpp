// PocketEngine Editor — Scene panel
//
// Renders the editor scene (entities + grid + selection gizmo) into an
// offscreen FBO and displays it as an ImGui::Image. Input is captured while
// the image is hovered:
//   * WASD           — fly the editor camera (pan)
//   * Mouse wheel    — zoom (clamped to [minZoom, maxZoom])
//   * Middle mouse   — drag-pan the camera
//   * Left click     — select nearest entity under the cursor
//
// The gizmo is hand-drawn (no ImGuizmo): for the selected entity, an X-axis
// red line and Y-axis green line emanate from its position. When the Move
// tool is active and the user drags near an axis, the entity follows.
//
// All GL state (FBO / texture / renderbuffer) is file-scope static; it is
// created lazily and torn down at shutdown via a destructor guard.

#include "editor/editor.h"
#include "editor/panels.h"

#include "pocket/core/input.h"
#include "pocket/core/time.h"
#include "pocket/core/log.h"
#include "pocket/ecs/ecs.h"
#include "pocket/render/renderer.h"

#include "imgui.h"

#include <GLES3/gl3.h>

#include <cmath>
#include <cstdio>

namespace pocket::editor {

namespace {

// ---- FBO state ----
GLuint g_fbo = 0, g_tex = 0, g_rbo = 0;
int    g_fboW = 0, g_fboH = 0;

void ensureFBO(int w, int h) {
    if (w <= 0 || h <= 0) return;
    if (g_fboW == w && g_fboH == h && g_fbo != 0) return;

    if (g_fbo) {
        glDeleteFramebuffers(1, &g_fbo);
        glDeleteTextures(1, &g_tex);
        glDeleteRenderbuffers(1, &g_rbo);
        g_fbo = g_tex = g_rbo = 0;
    }
    g_fboW = w; g_fboH = h;

    glGenFramebuffers(1, &g_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, g_fbo);

    glGenTextures(1, &g_tex);
    glBindTexture(GL_TEXTURE_2D, g_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_tex, 0);

    glGenRenderbuffers(1, &g_rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, g_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, g_rbo);

    GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (st != GL_FRAMEBUFFER_COMPLETE) {
        PE_ERROR("editor", "Scene FBO incomplete (status=0x%x)", (unsigned)st);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Last-frame viewport origin in ImGui screen coords (used for mouse picking).
ImVec2 g_viewportOrigin(0, 0);
ImVec2 g_viewportSize(0, 0);

// Helper: is the mouse inside the viewport image rect?
bool mouseInViewport() {
    ImVec2 m = ImGui::GetMousePos();
    return m.x >= g_viewportOrigin.x && m.x <= g_viewportOrigin.x + g_viewportSize.x &&
           m.y >= g_viewportOrigin.y && m.y <= g_viewportOrigin.y + g_viewportSize.y;
}

// Convert current mouse position to viewport-local (0..w, 0..h) coords.
math::Vec2 mouseViewportLocal() {
    ImVec2 m = ImGui::GetMousePos();
    return math::Vec2(m.x - g_viewportOrigin.x, m.y - g_viewportOrigin.y);
}

// ---- Editor camera input (WASD, wheel, MMB-drag) ----
void updateEditorCamera(EditorCamera& cam, bool viewportHovered, bool viewportFocused) {
    namespace pc = ::pocket;
    float dt = pc::time().delta();
    if (dt <= 0.0f) dt = 1.0f / 60.0f;

    // WASD panning — only when the scene panel has focus (so the user can
    // type in the inspector without moving the camera).
    if (viewportFocused) {
        math::Vec3 d(0,0,0);
        if (pc::input().keyDown(79 /*RIGHT*/) || pc::input().keyDown(7 /*D*/)) d.x += 1.0f;
        if (pc::input().keyDown(80 /*LEFT*/)  || pc::input().keyDown(4 /*A*/)) d.x -= 1.0f;
        if (pc::input().keyDown(82 /*UP*/)    || pc::input().keyDown(26 /*W*/)) d.y += 1.0f;
        if (pc::input().keyDown(81 /*DOWN*/)  || pc::input().keyDown(22 /*S*/)) d.y -= 1.0f;
        float speed = cam.moveSpeed * (1.0f / cam.zoom) * dt;
        cam.position.x += d.x * speed;
        cam.position.y += d.y * speed;
    }

    // Wheel zoom (only when hovering the viewport)
    if (viewportHovered) {
        float w = pc::input().wheel();
        if (w != 0.0f) {
            float factor = (w > 0.0f) ? 1.10f : (1.0f / 1.10f);
            cam.zoom = math::clamp(cam.zoom * factor, cam.minZoom, cam.maxZoom);
        }
    }

    // Middle-mouse drag to pan
    if (viewportHovered && pc::input().mouseDown(pc::MouseButton::Middle)) {
        float dx = pc::input().mouseDX();
        float dy = pc::input().mouseDY();
        // World units per pixel ≈ (2 * baseHalf) / viewportW
        const float baseHalf = 5.0f;
        float worldPerPx = (2.0f * baseHalf / cam.zoom) / float(g_fboW > 0 ? g_fboW : 1);
        cam.position.x -= dx * worldPerPx;
        cam.position.y += dy * worldPerPx; // Y flipped
    }
}

// ---- Picking: nearest entity within click radius ----
ecs::Entity pickEntity(const EditorCamera& cam, const math::Vec2& localPx) {
    namespace pc = ::pocket;
    math::Vec3 world = cam.screenToWorld(localPx.x, localPx.y, g_fboW, g_fboH);

    ecs::Entity best{};
    float bestDist = 1.5f; // 1.5 world units pick radius
    pc::ecs::registry().eachAll([&](ecs::Entity e) {
        auto* t = pc::ecs::registry().get<ecs::Transform>(e);
        if (!t) return;
        float dx = t->position.x - world.x;
        float dy = t->position.y - world.y;
        float d  = std::sqrt(dx*dx + dy*dy);
        // Scale-aware: bigger sprites are easier to pick.
        float r = 0.5f;
        if (auto* sp = pc::ecs::registry().get<ecs::SpriteComponent>(e)) {
            r = std::max(sp->size.x, sp->size.y) * 0.5f;
        }
        r += bestDist;
        if (d < r && d < bestDist + r) {
            bestDist = d;
            best = e;
        }
    });
    return best;
}

// ---- Gizmo (hand-drawn): X red line, Y green line from entity pos ----
void drawGizmo(const EditorCamera& cam, ecs::Entity sel) {
    namespace pc = ::pocket;
    if (!sel) return;
    auto* t = pc::ecs::registry().get<ecs::Transform>(sel);
    if (!t) return;

    const float gizmoLen = 1.5f / cam.zoom;
    math::Vec3 p = t->position;
    // X axis — red
    pc::render::renderer().drawLine(p, p + math::Vec3(gizmoLen, 0, 0), math::Color::red());
    // Y axis — green
    pc::render::renderer().drawLine(p, p + math::Vec3(0, gizmoLen, 0), math::Color::green());
    // Origin marker — small yellow square
    pc::render::renderer().drawRectLines(p - math::Vec3(0.05f, 0.05f, 0),
                                         math::Vec2(0.1f, 0.1f), math::Color::yellow());
}

// ---- Render the scene into the FBO ----
void renderSceneToFBO(int w, int h) {
    namespace pc = ::pocket;
    EditorCamera& cam = Editor::instance().sceneCamera();
    ensureFBO(w, h);

    // Bind our FBO; renderer.begin() will set glViewport to cam.viewport.
    glBindFramebuffer(GL_FRAMEBUFFER, g_fbo);

    render::Camera rcam = cam.toRenderCamera(float(w) / float(h), 0, 0, w, h);
    pc::render::renderer().setClearColor(math::Color{0.12f, 0.12f, 0.14f, 1.0f});
    pc::render::renderer().clear();
    pc::render::renderer().begin(rcam);

    // Grid centered on the camera (so the camera always sits over a grid line).
    math::Vec3 gridOrigin(cam.position.x - 50.0f, cam.position.y - 50.0f, 0.0f);
    pc::render::renderer().drawGrid(gridOrigin, 1.0f, 100, 100,
                                    math::Color{0.22f, 0.22f, 0.24f, 0.6f});

    // World origin cross-hair
    pc::render::renderer().drawLine(math::Vec3(-1, 0, 0), math::Vec3(1, 0, 0), math::Color{0.3f, 0.3f, 0.3f, 1.0f});
    pc::render::renderer().drawLine(math::Vec3(0, -1, 0), math::Vec3(0, 1, 0), math::Color{0.3f, 0.3f, 0.3f, 1.0f});

    // Render all sprite entities
    pc::ecs::registry().each<ecs::SpriteComponent>([&](ecs::Entity e, ecs::SpriteComponent& sp) {
        auto* t = pc::ecs::registry().get<ecs::Transform>(e);
        math::Vec3 pos = t ? t->position : math::Vec3{};
        math::Vec2 size = sp.size;
        float rot = t ? t->rotation.z : 0.0f;
        if (t) {
            size = math::Vec2(size.x * t->scale.x, size.y * t->scale.y);
        }
        pc::render::renderer().drawSprite(sp.textureId, pos, size, rot, sp.tint, sp.layer);
    });

    // Render selection outline + gizmo
    ecs::Entity sel = Editor::instance().selected();
    if (sel) {
        auto* t = pc::ecs::registry().get<ecs::Transform>(sel);
        if (t) {
            math::Vec2 outlineSize(1.0f, 1.0f);
            if (auto* sp = pc::ecs::registry().get<ecs::SpriteComponent>(sel)) {
                outlineSize = math::Vec2(sp->size.x * t->scale.x + 0.1f,
                                         sp->size.y * t->scale.y + 0.1f);
            }
            pc::render::renderer().drawRectLines(
                t->position - math::Vec3(outlineSize.x * 0.5f, outlineSize.y * 0.5f, 0),
                outlineSize, math::Color{1.0f, 0.65f, 0.0f, 1.0f});
        }
        drawGizmo(cam, sel);
    }

    pc::render::renderer().end();

    // Restore default framebuffer for ImGui rendering after this.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

} // namespace

void renderScenePanel() {
    ImGui::Begin("Scene", nullptr,
                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    // Available content region — this is the FBO size we render to.
    ImVec2 avail = ImGui::GetContentRegionAvail();
    int w = (int)avail.x;
    int h = (int)avail.y;
    if (w < 2 || h < 2) { ImGui::End(); return; }

    // Camera input — must run before rendering so movement is reflected.
    bool hovered = ImGui::IsWindowHovered();
    bool focused = ImGui::IsWindowFocused();
    updateEditorCamera(Editor::instance().sceneCamera(), hovered, focused);

    // Render scene to FBO
    renderSceneToFBO(w, h);

    // Display FBO color attachment as an ImGui::Image
    ImTextureID texId = (ImTextureID)(uintptr_t)g_tex;
    ImGui::Image(texId, avail, ImVec2(0, 1), ImVec2(1, 0)); // V flip for GLES
    // Record viewport origin for mouse-picking (Image is at cursor pos).
    ImVec2 cur = ImGui::GetItemRectMin();
    g_viewportOrigin = cur;
    g_viewportSize   = avail;

    // Left-click picking (only on press, not held)
    if (hovered && ::pocket::input().mousePressed(::pocket::MouseButton::Left)) {
        math::Vec2 local = mouseViewportLocal();
        ecs::Entity picked = pickEntity(Editor::instance().sceneCamera(), local);
        if (picked) {
            Editor::instance().select(picked);
        } else if (!ImGui::IsAnyItemActive()) {
            // Click on empty space — deselect.
            Editor::instance().select({});
        }
    }

    // Overlay: camera info (top-left of the viewport)
    ImGui::SetCursorPos(ImVec2(8.0f, 32.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0,0,0,0.45f));
    ImGui::BeginChild("##cam_overlay", ImVec2(220, 56), true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove);
    const EditorCamera& c = Editor::instance().sceneCamera();
    ImGui::TextDisabled("Editor Camera");
    ImGui::Text("pos (%.1f, %.1f)  zoom %.2f", (double)c.position.x, (double)c.position.y, (double)c.zoom);
    ImGui::Text("tool: %s", Editor::instance().tool() == EditorTool::Move   ? "Move"   :
                            Editor::instance().tool() == EditorTool::Rotate ? "Rotate" : "Scale");
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::End();
}

} // namespace pocket::editor
