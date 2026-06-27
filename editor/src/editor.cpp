// PocketEngine Editor — orchestrator implementation
//
// Editor::render() drives the full per-frame UI pipeline:
//   1. main menubar (File/Edit/Entity/Assets/View/Help)
//   2. dockspace host window + default layout (first run only)
//   3. toolbar (Play/Pause/Stop, tool select, grid snap, save)
//   4. all panels (Hierarchy, Scene, Game, Inspector, Console, Project)
//   5. stats overlay (top-right corner)
//
// All concrete panel rendering lives in editor/src/panels/*.cpp and is
// declared in editor/panels.h. This file is intentionally thin: it just
// sequences them.

#include "editor/editor.h"
#include "editor/panels.h"
#include "editor/theme.h"

#include "pocket/core/log.h"
#include "pocket/core/time.h"
#include "pocket/ecs/ecs.h"
#include "pocket/render/renderer.h"

#include "imgui.h"

#include <cstdio>
#include <cstdlib>

namespace pocket::editor {

// ============================================================
// EditorCamera
// ============================================================
render::Camera EditorCamera::toRenderCamera(float aspect, int vx, int vy, int vw, int vh) const {
    render::Camera c;
    c.position  = position;
    c.rotation  = rotation;
    c.zoom      = zoom;
    c.ortho     = true;
    c.fov       = 60.0f;
    c.nearZ     = -100.0f;
    c.farZ      = 100.0f;
    c.viewportX = vx;
    c.viewportY = vy;
    c.viewportW = vw;
    c.viewportH = vh;
    (void)aspect; // ortho zoom already encodes aspect via viewport
    return c;
}

math::Vec3 EditorCamera::screenToWorld(float sx, float sy, int vw, int vh) const {
    // Normalised 0..1 within the viewport, centered at 0.5
    float nx = (sx / float(vw)) - 0.5f;
    float ny = (sy / float(vh)) - 0.5f;
    // World half-extents (ortho): each side spans (1/zoom) world units * 0.5
    // Using a heuristic aspect compensation: width = (1/zoom) * aspect, height = (1/zoom)
    // We use a symmetric ortho where half-w = (1.0/zoom) * (vw/2) and half-h = (1.0/zoom) * (vh/2)
    // scaled by a base unit of 5 world units per half-viewport.
    const float baseHalf = 5.0f;
    float hw = baseHalf / zoom;
    float hh = baseHalf / zoom;
    math::Vec3 world;
    world.x = position.x + nx * 2.0f * hw;
    world.y = position.y - ny * 2.0f * hh; // Y flipped (screen down, world up)
    world.z = 0.0f;
    return world;
}

// ============================================================
// Editor singleton
// ============================================================
Editor& Editor::instance() {
    static Editor s;
    return s;
}

Editor::Editor()  = default;
Editor::~Editor() = default;

bool Editor::init() {
    // EditorConsole captures stderr — call BEFORE any heavy logging.
    EditorConsole::instance().init();

    // Sensible default scene-camera position.
    sceneCam_.position = {0.0f, 0.0f, 0.0f};
    sceneCam_.zoom     = 1.0f;
    PE_INFO("editor", "Editor::init complete (Unity-dark theme, landscape layout)");
    return true;
}

void Editor::shutdown() {
    EditorConsole::instance().shutdown();
    PE_INFO("editor", "Editor::shutdown complete");
}

bool Editor::consumeFirstRun() {
    if (firstRun_) { firstRun_ = false; return true; }
    return false;
}

// ============================================================
// Per-frame render
// ============================================================
void Editor::render() {
    // 1. Main menu bar (drawn inside renderDockspace)
    // 2. Dockspace host
    renderDockspace();

    // 3. Toolbar (always docked at the top of the dockspace)
    renderToolbar();

    // 4. Panels — these call ImGui::Begin/End themselves; ImGui's docking
    //    system routes them into the dockspace based on imgui.ini or the
    //    DockBuilder layout installed on first run.
    renderHierarchyPanel();
    renderScenePanel();
    renderGamePanel();
    renderInspectorPanel();
    renderConsolePanel();
    renderProjectPanel();

    // 5. Floating stats overlay (top-right corner)
    renderStatsOverlay();
}

// ============================================================
// Play mode
// ============================================================
void Editor::play() {
    if (playMode_) return;
    playMode_ = true;
    paused_   = false;
    PE_INFO("editor", ">>> PLAY (physics + scripts now stepping)");
}
void Editor::pause() {
    if (!playMode_) return;
    paused_ = !paused_;
    PE_INFO("editor", paused_ ? "|| PAUSE" : ">> RESUME");
}
void Editor::stop() {
    if (!playMode_) return;
    playMode_ = false;
    paused_   = false;
    PE_INFO("editor", "[] STOP (back to edit mode)");
}

// ============================================================
// Scene ops (tiny hand-rolled JSON serializer, no external lib)
// ============================================================
namespace {

// Minimal JSON string escape for a C string → std::string.
std::string jsonEscape(const char* s) {
    std::string out;
    if (!s) return out;
    for (const char* p = s; *p; ++p) {
        char c = *p;
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if ((unsigned char)c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

// Crude JSON writer for the supported components. No parser pretension —
// round-trips well enough for our schema and is fully self-contained.
std::string serializeScene() {
    using namespace pocket::ecs;
    auto& reg = registry();
    std::string s;
    s.reserve(4096);
    s += "{\n  \"entities\": [\n";
    bool first = true;
    reg.eachAll([&](Entity e) {
        if (!first) s += ",\n";
        first = false;
        char header[64];
        std::snprintf(header, sizeof(header), "    {\n      \"id\": %llu,\n",
                      (unsigned long long)e.id);
        s += header;

        // Tag
        if (auto* tag = reg.get<Tag>(e)) {
            s += "      \"tag\": { \"name\": \"";
            s += jsonEscape(tag->name.c_str());
            char v[32]; std::snprintf(v, sizeof(v), "\", \"visible\": %s },\n",
                                      tag->visible ? "true" : "false");
            s += v;
        }

        // Transform
        if (auto* t = reg.get<Transform>(e)) {
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                "      \"transform\": { \"pos\": [%g,%g,%g], \"rot\": [%g,%g,%g], \"scale\": [%g,%g,%g] },\n",
                t->position.x, t->position.y, t->position.z,
                t->rotation.x, t->rotation.y, t->rotation.z,
                t->scale.x, t->scale.y, t->scale.z);
            s += buf;
        }

        // Sprite
        if (auto* sp = reg.get<SpriteComponent>(e)) {
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                "      \"sprite\": { \"tex\": %u, \"size\": [%g,%g], \"tint\": [%g,%g,%g,%g], \"layer\": %d },\n",
                sp->textureId, sp->size.x, sp->size.y,
                sp->tint.r, sp->tint.g, sp->tint.b, sp->tint.a, sp->layer);
            s += buf;
        }

        // Camera
        if (auto* c = reg.get<CameraComponent>(e)) {
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                "      \"camera\": { \"zoom\": %g, \"fov\": %g, \"ortho\": %s, \"near\": %g, \"far\": %g, \"primary\": %s },\n",
                c->zoom, c->fov, c->ortho ? "true" : "false", c->nearZ, c->farZ,
                c->primary ? "true" : "false");
            s += buf;
        }

        // Script
        if (auto* sc = reg.get<ScriptComponent>(e)) {
            s += "      \"script\": { \"path\": \"";
            s += jsonEscape(sc->scriptPath.c_str());
            char v[32]; std::snprintf(v, sizeof(v), "\", \"enabled\": %s },\n",
                                      sc->enabled ? "true" : "false");
            s += v;
        }

        // Rigidbody2D
        if (auto* rb = reg.get<Rigidbody2D>(e)) {
            char buf[192];
            std::snprintf(buf, sizeof(buf),
                "      \"rigidbody2d\": { \"type\": %d, \"linDamp\": %g, \"angDamp\": %g, \"gravity\": %g, \"fixedRot\": %s },\n",
                (int)rb->type, rb->linearDamping, rb->angularDamping, rb->gravityScale,
                rb->fixedRotation ? "true" : "false");
            s += buf;
        }

        // Collider2D
        if (auto* co = reg.get<Collider2D>(e)) {
            char buf[192];
            std::snprintf(buf, sizeof(buf),
                "      \"collider2d\": { \"shape\": %d, \"size\": [%g,%g], \"radius\": %g, \"friction\": %g, \"restitution\": %g, \"trigger\": %s }\n",
                (int)co->shape, co->size.x, co->size.y, co->radius, co->friction, co->restitution,
                co->isTrigger ? "true" : "false");
            s += buf;
        } else {
            // strip trailing comma from last component
            if (!s.empty() && s.back() == '\n') {
                size_t i = s.size() - 1;
                if (i > 0 && s[i-1] == ',') s[i-1] = '\n';
            }
        }
        s += "    }";
    });
    s += "\n  ]\n}\n";
    return s;
}

} // namespace

void Editor::newScene() {
    using namespace pocket::ecs;
    auto& reg = registry();
    reg.clear();
    selected_ = {};

    Entity cam = reg.create();
    reg.emplace<Tag>(cam, Tag{"Main Camera"});
    reg.emplace<Transform>(cam, Transform{});
    reg.emplace<CameraComponent>(cam, CameraComponent{});
    reg.get<CameraComponent>(cam)->primary = true;
    PE_INFO("editor", "New empty scene (camera only)");
}

void Editor::saveScene(const std::string& path) {
    std::string p = path;
    if (p.empty()) p = "assets/scenes/default.scene.json";
    FILE* f = std::fopen(p.c_str(), "w");
    if (!f) {
        PE_ERROR("editor", "saveScene: cannot open %s for write", p.c_str());
        return;
    }
    std::string s = serializeScene();
    std::fwrite(s.data(), 1, s.size(), f);
    std::fclose(f);
    PE_INFO("editor", "Scene saved to %s (%zu bytes)", p.c_str(), s.size());
}

void Editor::loadScene(const std::string& path) {
    // Parser is intentionally a stub: we log a warning and return. A full
    // hand-rolled parser would mirror serializeScene() field-for-field.
    PE_WARN("editor", "loadScene(%s): parser not implemented — use saveScene first",
            path.c_str());
}

// ============================================================
// Stats
// ============================================================
int Editor::entityCount() const {
    return (int)ecs::registry().size();
}

int Editor::drawCallCount() const {
    // Engine renderer does not expose draw-call counter yet (TODO: hook into
    // SpriteBatch::flush). Return a stable placeholder so the overlay shows
    // something non-zero in the absence of a real metric.
    return 0;
}

float Editor::memoryMB() const {
    // Best-effort: pull resident-set size from /proc/self/status on Linux.
    FILE* f = std::fopen("/proc/self/status", "r");
    if (!f) return 0.0f;
    char line[256];
    float vmRssKB = 0.0f;
    while (std::fgets(line, sizeof(line), f)) {
        if (std::sscanf(line, "VmRSS: %f kB", &vmRssKB) == 1) break;
    }
    std::fclose(f);
    return vmRssKB / 1024.0f;
}

} // namespace pocket::editor
