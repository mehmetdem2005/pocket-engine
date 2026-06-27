// PocketEngine Editor — Project panel (asset browser)
//
// Scans the `assets/` directory (relative to the editor's CWD) and displays
// a tree of files. Tabs at the top filter by subfolder / asset type:
//   * All         — show everything
//   * Sprites     — assets/sprites/*.{png,jpg,bmp,tga}
//   * Shaders     — assets/shaders/*.{vert,frag,glsl}
//   * Audio       — assets/audio/*.{wav,ogg,mp3}
//   * Scripts     — assets/scripts/*.lua
//   * Scenes      — assets/scenes/*.scene.json
//
// Double-clicking:
//   * a Sprite  → assign to the selected entity's SpriteComponent.textureId
//   * a Script  → assign to the selected entity's ScriptComponent.scriptPath
//   * a Scene   → call Editor::loadScene(path)
//
// Files can also be drag-dropped onto the Inspector's Sprite texture input
// (we register a drag-drop payload with type "POCKET_ASSET_PATH").

#include "editor/editor.h"
#include "editor/panels.h"

#include "pocket/core/log.h"
#include "pocket/ecs/ecs.h"
#include "pocket/render/renderer.h"

#include "imgui.h"

#include <cstdio>
#include <filesystem>
#include <cstring>
#include <vector>
#include <string>

namespace fs = std::filesystem;

namespace pocket::editor {

namespace {

enum class AssetFilter : int { All = 0, Sprites, Shaders, Audio, Scripts, Scenes };

const char* kFilterNames[] = { "All", "Sprites", "Shaders", "Audio", "Scripts", "Scenes" };
const char* kFilterDirs[]  = { "",     "sprites", "shaders", "audio", "scripts", "scenes" };

const char* kFilterExts[] = {
    "", // All
    ".png,.jpg,.jpeg,.bmp,.tga,.webp",
    ".vert,.frag,.glsl",
    ".wav,.ogg,.mp3,.flac",
    ".lua",
    ".json,.scene"
};

bool matchesFilter(const fs::path& path, AssetFilter f) {
    if (f == AssetFilter::All) return true;
    std::string ext = path.extension().string();
    if (ext.empty()) return false;
    // Strip leading '.'
    if (ext[0] == '.') ext.erase(0, 1);
    // Compare against the filter's comma-separated extensions.
    std::string list = kFilterExts[(int)f];
    const char* p = list.c_str();
    while (*p) {
        const char* comma = std::strchr(p, ',');
        size_t len = comma ? (size_t)(comma - p) : std::strlen(p);
        if (ext.size() == len && std::strncmp(ext.c_str(), p, len) == 0) return true;
        if (!comma) break;
        p = comma + 1;
    }
    return false;
}

void scanDir(const fs::path& base, const fs::path& rel, AssetFilter f,
             std::vector<fs::path>& out) {
    fs::path full = base / rel;
    if (!fs::exists(full)) return;
    std::error_code ec;
    for (auto& entry : fs::directory_iterator(full, fs::directory_options::skip_permission_denied, ec)) {
        fs::path sub = rel / entry.path().filename();
        if (entry.is_directory()) {
            scanDir(base, sub, f, out);
        } else if (entry.is_regular_file()) {
            if (matchesFilter(entry.path(), f)) out.push_back(sub);
        }
    }
}

void onAssetDoubleClicked(const fs::path& relPath) {
    namespace pc = ::pocket;
    std::string fullStr = ("assets/" / relPath).string();
    std::string ext = relPath.extension().string();

    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga" || ext == ".webp") {
        ecs::Entity sel = Editor::instance().selected();
        if (sel) {
            auto* sp = pc::ecs::registry().get<ecs::SpriteComponent>(sel);
            if (sp) {
                auto id = pc::render::renderer().loadTexture(fullStr.c_str());
                if (id != pc::render::INVALID_TEXTURE) {
                    sp->textureId = id;
                    PE_INFO("editor", "Assigned texture %u (%s) to entity %llu",
                            id, fullStr.c_str(), (unsigned long long)sel.id);
                }
            } else {
                PE_WARN("editor", "Selected entity has no Sprite component");
            }
        }
    } else if (ext == ".lua") {
        ecs::Entity sel = Editor::instance().selected();
        if (sel) {
            auto* sc = pc::ecs::registry().get<ecs::ScriptComponent>(sel);
            if (sc) {
                sc->scriptPath = fullStr;
                PE_INFO("editor", "Assigned script %s to entity %llu",
                        fullStr.c_str(), (unsigned long long)sel.id);
            }
        }
    } else if (ext == ".json" || ext == ".scene") {
        Editor::instance().loadScene(fullStr);
    }
}

void renderAssetRow(const fs::path& relPath) {
    std::string name = relPath.filename().string();
    std::string fullStr = ("assets/" / relPath).string();

    ImGui::PushID(fullStr.c_str());
    ImGui::BulletText("%s", name.c_str());
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", fullStr.c_str());
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
        onAssetDoubleClicked(relPath);
    }
    // Drag-drop source: drag the asset path onto the Inspector
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        const char* payload = fullStr.c_str();
        ImGui::SetDragDropPayload("POCKET_ASSET_PATH", payload,
                                  fullStr.size() + 1); // include NUL
        ImGui::Text("→ %s", name.c_str());
        ImGui::EndDragDropSource();
    }
    ImGui::PopID();
}

} // namespace

void renderProjectPanel() {
    ImGui::Begin("Project");

    // Filter tabs
    static int currentFilter = 0;
    const int numFilters = (int)(sizeof(kFilterNames) / sizeof(kFilterNames[0]));
    for (int i = 0; i < numFilters; ++i) {
        if (i > 0) ImGui::SameLine();
        bool selected = (currentFilter == i);
        if (ImGui::Selectable(kFilterNames[i], selected, ImGuiSelectableFlags_None, ImVec2(60, 0))) {
            currentFilter = i;
        }
    }
    ImGui::Separator();

    // Scan assets/ directory
    AssetFilter f = (AssetFilter)currentFilter;
    std::vector<fs::path> files;
    scanDir("assets", fs::path{}, f, files);
    std::sort(files.begin(), files.end());

    if (files.empty()) {
        ImGui::TextDisabled("(no assets found under assets/%s)",
                            kFilterDirs[(int)f]);
        ImGui::End();
        return;
    }

    // Two-column layout: tree on the left, list on the right.
    float halfW = ImGui::GetContentRegionAvail().x * 0.30f;
    ImGui::BeginChild("##tree", ImVec2(halfW, 0), true);
    ImGui::TextDisabled("Folders");
    ImGui::Separator();
    // Display unique top-level subfolders as static bullet points (the actual
    // file list lives in the right pane).
    static const char* knownDirs[] = { "sprites", "shaders", "audio", "scripts", "scenes", "fonts" };
    for (const char* d : knownDirs) {
        fs::path p = fs::path("assets") / d;
        if (fs::is_directory(p)) {
            ImGui::BulletText("%s/", d);
            if (ImGui::IsItemClicked()) {
                currentFilter = 0; // jump to All when clicking a folder
            }
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("##files", ImVec2(0, 0), true);
    ImGui::TextDisabled("Files (%zu)", files.size());
    ImGui::Separator();
    for (const auto& rel : files) {
        renderAssetRow(rel);
    }
    ImGui::EndChild();

    ImGui::End();
}

} // namespace pocket::editor
