// PocketEngine Editor — Inspector panel
//
// Displays the property editors for the currently selected entity.
// For each component present on the entity, we render a collapsing header
// with property controls. At the bottom, an "Add Component" dropdown lets
// the user add any of the built-in components.
//
// Components edited:
//   * Tag            — name (InputText), visible (checkbox)
//   * Transform      — position, rotation (deg), scale (3 floats each)
//   * Sprite         — texture (Load button), size, tint, layer
//   * Camera         — ortho, zoom, fov, near/far, clear color, primary
//   * Rigidbody2D    — body type (combo), damping, gravity scale, fixed rot
//   * Collider2D     — shape (combo), size/radius, friction, restitution, trigger
//   * Script         — path (InputText + Browse), enabled

#include "editor/editor.h"
#include "editor/panels.h"

#include "pocket/core/log.h"
#include "pocket/ecs/ecs.h"
#include "pocket/render/renderer.h"

#include "imgui.h"

#include <cstdio>
#include <cmath>

namespace pocket::editor {

namespace {

// Tiny drag-float3 helper for math::Vec3 with sensible speeds.
bool dragVec3(const char* label, math::Vec3& v, float speed, const char* fmt = "%.2f") {
    float arr[3] = { v.x, v.y, v.z };
    bool changed = ImGui::DragFloat3(label, arr, speed, 0.0f, 0.0f, fmt);
    if (changed) { v.x = arr[0]; v.y = arr[1]; v.z = arr[2]; }
    return changed;
}

bool dragVec2(const char* label, math::Vec2& v, float speed, const char* fmt = "%.2f") {
    float arr[2] = { v.x, v.y };
    bool changed = ImGui::DragFloat2(label, arr, speed, 0.0f, 0.0f, fmt);
    if (changed) { v.x = arr[0]; v.y = arr[1]; }
    return changed;
}

bool colorEdit(const char* label, math::Color& c) {
    float arr[4] = { c.r, c.g, c.b, c.a };
    bool changed = ImGui::ColorEdit4(label, arr, ImGuiColorEditFlags_AlphaPreview);
    if (changed) { c.r = arr[0]; c.g = arr[1]; c.b = arr[2]; c.a = arr[3]; }
    return changed;
}

void header(const char* title, bool& open) {
    ImGui::SetNextItemOpen(open, ImGuiCond_Once);
    open = ImGui::CollapsingHeader(title);
}

// ---- Tag editor ----
void editTag(ecs::Entity e) {
    auto* tag = ::pocket::ecs::registry().get<ecs::Tag>(e);
    if (!tag) return;
    bool open = true;
    header("Tag", open);
    if (!open) return;
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%s", tag->name.c_str());
    if (ImGui::InputText("Name", buf, sizeof(buf))) tag->name = buf;
    ImGui::Checkbox("Visible", &tag->visible);
}

// ---- Transform editor (rotation shown in degrees) ----
void editTransform(ecs::Entity e) {
    auto* t = ::pocket::ecs::registry().get<ecs::Transform>(e);
    if (!t) return;
    bool open = true;
    header("Transform", open);
    if (!open) return;

    dragVec3("Position", t->position, 0.1f);

    // Convert internal radians to degrees for editing.
    math::Vec3 deg(math::degrees(t->rotation.x),
                   math::degrees(t->rotation.y),
                   math::degrees(t->rotation.z));
    if (dragVec3("Rotation (deg)", deg, 1.0f, "%.1f")) {
        t->rotation.x = math::radians(deg.x);
        t->rotation.y = math::radians(deg.y);
        t->rotation.z = math::radians(deg.z);
    }

    dragVec3("Scale", t->scale, 0.05f);
    if (t->scale.x == 0) t->scale.x = 0.0001f;
    if (t->scale.y == 0) t->scale.y = 0.0001f;
    if (t->scale.z == 0) t->scale.z = 0.0001f;
}

// ---- Sprite editor ----
void editSprite(ecs::Entity e) {
    auto* sp = ::pocket::ecs::registry().get<ecs::SpriteComponent>(e);
    if (!sp) return;
    bool open = true;
    header("Sprite", open);
    if (!open) return;

    // Texture path — uses a static text buffer per entity (we just store the
    // last typed path; loading happens on button press).
    static char texPath[256] = "assets/sprites/default.png";
    ImGui::InputText("Texture Path", texPath, sizeof(texPath));
    ImGui::SameLine();
    if (ImGui::Button("Load##tex")) {
        ::pocket::render::TextureId id = ::pocket::render::renderer().loadTexture(texPath);
        if (id != ::pocket::render::INVALID_TEXTURE) {
            sp->textureId = id;
            auto sz = ::pocket::render::renderer().textureSize(id);
            if (sz.x > 0 && sz.y > 0) {
                // Keep sprite size proportional to texture (1 world unit per pixel group).
                // We DON'T overwrite user-set size, but we report it.
                PE_INFO("editor", "Texture %u loaded (%dx%d)", id, (int)sz.x, (int)sz.y);
            }
        } else {
            PE_ERROR("editor", "Failed to load texture: %s", texPath);
        }
    }
    ImGui::TextDisabled("Texture ID: %u", sp->textureId);

    dragVec2("Size", sp->size, 0.05f);
    dragVec2("UV0", sp->uv0, 0.01f);
    dragVec2("UV1", sp->uv1, 0.01f);
    colorEdit("Tint", sp->tint);
    ImGui::DragInt("Layer", &sp->layer, 1, -16, 16);
}

// ---- Camera editor ----
void editCamera(ecs::Entity e) {
    auto* c = ::pocket::ecs::registry().get<ecs::CameraComponent>(e);
    if (!c) return;
    bool open = true;
    header("Camera", open);
    if (!open) return;

    ImGui::Checkbox("Orthographic", &c->ortho);
    ImGui::DragFloat("Zoom",  &c->zoom,  0.01f, 0.01f, 100.0f);
    ImGui::DragFloat("FOV",    &c->fov,   0.5f,  10.0f, 170.0f);
    ImGui::DragFloat("Near Z", &c->nearZ, 0.5f, -1000.0f, 1000.0f);
    ImGui::DragFloat("Far Z",  &c->farZ,  0.5f, -1000.0f, 1000.0f);
    colorEdit("Clear Color", c->clearColor);
    ImGui::Checkbox("Primary", &c->primary);
    if (c->primary) {
        // Enforce single primary camera
        ::pocket::ecs::registry().each<ecs::CameraComponent>([e](ecs::Entity other, ecs::CameraComponent& oc) {
            if (other.id != e.id) oc.primary = false;
        });
    }
}

// ---- Rigidbody2D editor ----
void editRigidbody(ecs::Entity e) {
    auto* rb = ::pocket::ecs::registry().get<ecs::Rigidbody2D>(e);
    if (!rb) return;
    bool open = true;
    header("Rigidbody2D", open);
    if (!open) return;

    const char* typeNames[] = { "Static", "Kinematic", "Dynamic" };
    int ti = (int)rb->type;
    if (ImGui::Combo("Body Type", &ti, typeNames, 3)) rb->type = (ecs::Rigidbody2D::BodyType)ti;

    ImGui::DragFloat("Linear Damping",  &rb->linearDamping,  0.01f, 0.0f, 100.0f);
    ImGui::DragFloat("Angular Damping", &rb->angularDamping, 0.01f, 0.0f, 100.0f);
    ImGui::DragFloat("Gravity Scale",   &rb->gravityScale,   0.05f, -10.0f, 10.0f);
    ImGui::Checkbox("Fixed Rotation",   &rb->fixedRotation);
    ImGui::TextDisabled("Body handle: %d", rb->bodyHandle);
}

// ---- Collider2D editor ----
void editCollider(ecs::Entity e) {
    auto* co = ::pocket::ecs::registry().get<ecs::Collider2D>(e);
    if (!co) return;
    bool open = true;
    header("Collider2D", open);
    if (!open) return;

    const char* shapeNames[] = { "Box", "Circle" };
    int si = (int)co->shape;
    if (ImGui::Combo("Shape", &si, shapeNames, 2)) co->shape = (ecs::Collider2D::Shape)si;

    if (co->shape == ecs::Collider2D::Shape::Box) {
        dragVec2("Size", co->size, 0.05f);
    } else {
        ImGui::DragFloat("Radius", &co->radius, 0.05f, 0.001f, 100.0f);
    }
    ImGui::DragFloat("Friction",    &co->friction,    0.01f, 0.0f, 2.0f);
    ImGui::DragFloat("Restitution", &co->restitution, 0.01f, 0.0f, 1.0f);
    ImGui::Checkbox("Is Trigger",   &co->isTrigger);
}

// ---- Script editor ----
void editScript(ecs::Entity e) {
    auto* sc = ::pocket::ecs::registry().get<ecs::ScriptComponent>(e);
    if (!sc) return;
    bool open = true;
    header("Script", open);
    if (!open) return;

    char buf[256];
    std::snprintf(buf, sizeof(buf), "%s", sc->scriptPath.c_str());
    if (ImGui::InputText("Script Path", buf, sizeof(buf))) sc->scriptPath = buf;
    ImGui::SameLine();
    if (ImGui::Button("Browse##scr")) {
        // Stub: in a real editor this opens a file dialog. For now, the user
        // can drag-drop from the Project panel onto this InputText.
    }
    ImGui::Checkbox("Enabled", &sc->enabled);
}

// ---- Add Component dropdown ----
void addComponentMenu(ecs::Entity e) {
    ImGui::Separator();
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
    static int currentItem = 0;
    const char* items[] = {
        "(select component...)",
        "Transform", "Sprite", "Camera",
        "Rigidbody2D", "Collider2D", "Script", "Tag"
    };
    if (ImGui::Combo("##addcomp", &currentItem, items, IM_ARRAYSIZE(items))) {
        namespace ecs = ::pocket::ecs;
        switch (currentItem) {
            case 1: if (!ecs::registry().has<ecs::Transform>(e))     ecs::registry().emplace<ecs::Transform>(e);     break;
            case 2: if (!ecs::registry().has<ecs::SpriteComponent>(e)) ecs::registry().emplace<ecs::SpriteComponent>(e); break;
            case 3: if (!ecs::registry().has<ecs::CameraComponent>(e)) ecs::registry().emplace<ecs::CameraComponent>(e); break;
            case 4: if (!ecs::registry().has<ecs::Rigidbody2D>(e))   ecs::registry().emplace<ecs::Rigidbody2D>(e);   break;
            case 5: if (!ecs::registry().has<ecs::Collider2D>(e))    ecs::registry().emplace<ecs::Collider2D>(e);    break;
            case 6: if (!ecs::registry().has<ecs::ScriptComponent>(e)) ecs::registry().emplace<ecs::ScriptComponent>(e); break;
            case 7: if (!ecs::registry().has<ecs::Tag>(e))           ecs::registry().emplace<ecs::Tag>(e);           break;
            default: break;
        }
        currentItem = 0;
    }
    ImGui::PopItemWidth();
}

} // namespace

void renderInspectorPanel() {
    ImGui::Begin("Inspector");

    ecs::Entity sel = Editor::instance().selected();
    if (!sel) {
        ImGui::TextDisabled("No entity selected.");
        ImGui::End();
        return;
    }
    if (!::pocket::ecs::registry().valid(sel)) {
        Editor::instance().select({});
        ImGui::TextDisabled("Selection invalid.");
        ImGui::End();
        return;
    }

    // Header line — entity id + delete button
    ImGui::TextDisabled("Entity ID: %llu", (unsigned long long)sel.id);
    ImGui::SameLine(ImGui::GetContentRegionAvailWidth() - 80.0f);
    if (ImGui::SmallButton("Delete")) {
        ::pocket::ecs::registry().destroy(sel);
        Editor::instance().select({});
        ImGui::End();
        return;
    }
    ImGui::Separator();

    // Render each present component's editor.
    editTag(sel);
    editTransform(sel);
    editSprite(sel);
    editCamera(sel);
    editRigidbody(sel);
    editCollider(sel);
    editScript(sel);

    addComponentMenu(sel);

    ImGui::End();
}

} // namespace pocket::editor
