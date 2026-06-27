// PocketEngine Editor — Hierarchy panel
//
// Tree view of all entities in the registry. Root entities (those whose
// Transform has no parent) appear at the top level; their children (entities
// whose Transform.parent points to another entity's Transform) are nested.
//
// Features:
//   * Click entity name → select it (Editor::select)
//   * Right-click empty area → context menu (Add Empty / Add Sprite / Add Camera)
//   * Right-click entity row → context menu (Rename / Duplicate / Delete)
//   * Drag-drop entity onto another entity → reparent (sets child.parent)
//   * Double-click name → inline rename (ImGui::InputText)
//   * Selected entity row highlighted with accent color
//
// The mapping from child to parent is recovered by iterating all Transforms
// and grouping by parent pointer; we then resolve the parent's Entity via a
// reverse lookup map built once per frame.

#include "editor/editor.h"
#include "editor/panels.h"

#include "pocket/core/log.h"
#include "pocket/ecs/ecs.h"

#include "imgui.h"

#include <cstdio>
#include <unordered_map>

namespace pocket::editor {

namespace {

// Snapshot of entity → its Transform* (so we can resolve reparent targets).
struct HierarchyState {
    // Map parent Transform* -> vector of child EntityIds (rebuilt each frame).
    std::unordered_map<ecs::Transform*, std::vector<pocket::EntityId>> childrenOf;
    // Set of root entity ids (Transform.parent == nullptr).
    std::vector<pocket::EntityId> roots;
    // Map EntityId -> Transform* (for reparenting).
    std::unordered_map<pocket::EntityId, ecs::Transform*> entityToTransform;
};

void rebuildHierarchy(HierarchyState& s) {
    s.childrenOf.clear();
    s.roots.clear();
    s.entityToTransform.clear();

    namespace pc = ::pocket;
    pc::ecs::registry().eachAll([&](ecs::Entity e) {
        auto* t = pc::ecs::registry().get<ecs::Transform>(e);
        if (!t) {
            // Treat entities without Transform as roots.
            s.roots.push_back(e.id);
            return;
        }
        s.entityToTransform[e.id] = t;
        if (t->parent) {
            s.childrenOf[t->parent].push_back(e.id);
        } else {
            s.roots.push_back(e.id);
        }
    });
}

const char* entityName(ecs::Entity e) {
    auto* tag = ::pocket::ecs::registry().get<ecs::Tag>(e);
    if (tag && !tag->name.empty()) return tag->name.c_str();
    static char buf[32];
    std::snprintf(buf, sizeof(buf), "Entity_%llu", (unsigned long long)e.id);
    return buf;
}

void setEntityName(ecs::Entity e, const char* name) {
    auto* tag = ::pocket::ecs::registry().get<ecs::Tag>(e);
    if (tag) tag->name = name;
}

// Recursively render an entity and its children. Returns true if the
// selection changed (caller can ignore — selection is updated in place).
void renderEntityNode(HierarchyState& s, pocket::EntityId id, bool& renamed) {
    namespace pc = ::pocket;
    ecs::Entity e{id};
    if (!pc::ecs::registry().valid(e)) return;

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    bool selected = (Editor::instance().selected().id == id);
    if (selected) flags |= ImGuiTreeNodeFlags_Selected;

    // Find children — check if this entity has a Transform that others point to.
    auto* t = pc::ecs::registry().get<ecs::Transform>(e);
    bool hasChildren = false;
    if (t) {
        auto it = s.childrenOf.find(t);
        if (it != s.childrenOf.end() && !it->second.empty()) hasChildren = true;
    }
    if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf;

    const char* name = entityName(e);

    // Inline rename mode for this entity id (only one at a time).
    static pocket::EntityId renamingId = 0;
    bool isRenaming = (renamingId == id);

    ImGui::PushID((int)id);
    if (isRenaming) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "%s", name);
        ImGui::SetNextItemWidth(180.0f);
        if (ImGui::InputText("##rename", buf, sizeof(buf),
                             ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
            setEntityName(e, buf);
            renamingId = 0;
            renamed = true;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) renamingId = 0;
        if (!ImGui::IsItemActive() && !ImGui::IsItemFocused()) renamingId = 0;
    } else {
        bool nodeOpen = ImGui::TreeNodeEx(name, flags);
        if (ImGui::IsItemClicked()) Editor::instance().select(e);
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            renamingId = id;
        }

        // Drag-drop source — drag this entity onto another to reparent.
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
            pocket::EntityId dragId = id;
            ImGui::SetDragDropPayload("POCKET_ENTITY", &dragId, sizeof(dragId));
            ImGui::Text("→ %s", name);
            ImGui::EndDragDropSource();
        }
        // Drag-drop target — drop another entity onto this one to reparent
        // the dropped entity under this one.
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("POCKET_ENTITY")) {
                pocket::EntityId childId;
                if (p->DataSize == sizeof(childId)) {
                    std::memcpy(&childId, p->Data, sizeof(childId));
                    ecs::Entity child{childId};
                    auto* ct = pc::ecs::registry().get<ecs::Transform>(child);
                    auto* pt = pc::ecs::registry().get<ecs::Transform>(e);
                    if (ct && pt && childId != id) {
                        ct->parent = pt;
                        PE_INFO("editor", "Reparented entity %llu under %llu",
                                (unsigned long long)childId, (unsigned long long)id);
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        // Right-click context menu for this entity row
        if (ImGui::BeginPopupContextItem("ctx")) {
            if (ImGui::MenuItem("Rename")) renamingId = id;
            if (ImGui::MenuItem("Duplicate")) {
                ecs::Entity ne = pc::ecs::registry().create();
                pc::ecs::registry().emplace<ecs::Tag>(ne, ecs::Tag{entityName(e)});
                pc::ecs::registry().emplace<ecs::Transform>(ne, ecs::Transform{});
                if (auto* sp = pc::ecs::registry().get<ecs::SpriteComponent>(e)) {
                    pc::ecs::registry().emplace<ecs::SpriteComponent>(ne, *sp);
                }
                if (auto* cc = pc::ecs::registry().get<ecs::CameraComponent>(e)) {
                    pc::ecs::registry().emplace<ecs::CameraComponent>(ne, *cc);
                }
                Editor::instance().select(ne);
            }
            if (ImGui::MenuItem("Delete")) {
                pc::ecs::registry().destroy(e);
                if (Editor::instance().selected().id == id) Editor::instance().select({});
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Detach from parent")) {
                auto* ct = pc::ecs::registry().get<ecs::Transform>(e);
                if (ct) ct->parent = nullptr;
            }
            ImGui::EndPopup();
        }

        if (nodeOpen) {
            if (t) {
                auto it = s.childrenOf.find(t);
                if (it != s.childrenOf.end()) {
                    for (auto childId : it->second) {
                        renderEntityNode(s, childId, renamed);
                    }
                }
            }
            ImGui::TreePop();
        }
    }
    ImGui::PopID();
}

} // namespace

void renderHierarchyPanel() {
    ImGui::Begin("Hierarchy");

    // Filter box
    static char filter[64] = "";
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::InputTextWithHint("##filter", "Filter entities...", filter, sizeof(filter));
    ImGui::Separator();

    HierarchyState state;
    rebuildHierarchy(state);

    // Render root entities (and recurse into children).
    bool renamed = false;
    for (auto id : state.roots) {
        // Optional filter: skip roots whose name doesn't match (children still
        // rendered if the parent matches; we keep it simple).
        if (filter[0]) {
            ecs::Entity e{id};
            const char* nm = entityName(e);
            // crude substring search
            bool match = false;
            for (const char* p = nm; *p; ++p) {
                const char* a = p; const char* b = filter;
                while (*a && *b && *a == *b) { ++a; ++b; }
                if (*b == 0) { match = true; break; }
            }
            if (!match) continue;
        }
        renderEntityNode(state, id, renamed);
    }

    // Empty-area right-click → Add menu
    if (ImGui::BeginPopupContextWindow("hier_ctx", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        namespace ecs = ::pocket::ecs;
        if (ImGui::MenuItem("Add Empty Entity")) {
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
        ImGui::EndPopup();
    }

    // Footer: entity count
    ImGui::Separator();
    ImGui::TextDisabled("Entities: %zu", ::pocket::ecs::registry().size());

    ImGui::End();
}

} // namespace pocket::editor
