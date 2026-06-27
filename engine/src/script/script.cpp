// PocketEngine — Lua scripting bridge uygulaması (sol2)
#include "pocket/script/script.h"
#include "pocket/core/log.h"
#include "pocket/math/math.h"
#include "pocket/audio/audio.h"

#include <sol/sol.hpp>

#include <fstream>
#include <sstream>

namespace pocket::script {

// ---- Impl ----
struct ScriptEngine::Impl {
    sol::state lua;
    bool ready = false;

    ecs::Registry* reg = nullptr;
    KeyDownFn keyDown;

    struct Instance {
        ecs::Entity    entity;
        EntityId       entityId;
        sol::environment env;
        sol::protected_function on_start;
        sol::protected_function on_update;
        bool started = false;
        bool valid   = false;
    };
    // EntityId (u64) anahtar olarak — std::hash<EntityId> mevcut
    HashMap<EntityId, Instance> instances;
};

// ---- Ctor/Dtor ----
ScriptEngine::ScriptEngine() : impl_(nullptr) {}
ScriptEngine::~ScriptEngine() { shutdown(); }

// ---- init ----
bool ScriptEngine::init() {
    if (impl_) shutdown();
    impl_ = makeUnique<Impl>();

    impl_->lua.open_libraries(sol::lib::base,
                              sol::lib::math,
                              sol::lib::string,
                              sol::lib::table);

    registerBindings();
    impl_->ready = true;
    PE_INFO("script", "ScriptEngine init: sol2 ready");
    return true;
}

void ScriptEngine::shutdown() {
    if (!impl_) return;
    impl_->instances.clear();
    impl_->lua = sol::state();
    impl_->ready = false;
    impl_.reset();
    PE_INFO("script", "ScriptEngine shutdown");
}

bool ScriptEngine::isInitialized() const { return impl_ && impl_->ready; }
size_t ScriptEngine::scriptCount() const { return impl_ ? impl_->instances.size() : 0; }

// ---- registerBindings ----
void ScriptEngine::registerBindings() {
    auto& lua = impl_->lua;

    sol::table pocket = lua.create_named_table("pocket");

    // pocket.log(msg)
    pocket.set_function("log",
        [](const char* msg) { PE_INFO("lua", "%s", msg ? msg : ""); });

    // pocket.input table
    sol::table input = pocket.create_named("input");
    input.set_function("keyDown",
        [this](int scancode) -> bool {
            if (!impl_->keyDown) return false;
            return impl_->keyDown(scancode);
        });

    // pocket.getTransform(entityId) -> {x,y,z}
    pocket.set_function("getTransform",
        [this](u64 entityId) -> sol::table {
            sol::table t = impl_->lua.create_table();
            if (!impl_->reg) { t["x"]=0; t["y"]=0; t["z"]=0; return t; }
            ecs::Entity e{static_cast<EntityId>(entityId)};
            if (!impl_->reg->has<ecs::Transform>(e)) {
                t["x"]=0; t["y"]=0; t["z"]=0; return t;
            }
            auto& tr = *impl_->reg->get<ecs::Transform>(e);
            t["x"] = tr.position.x;
            t["y"] = tr.position.y;
            t["z"] = tr.position.z;
            return t;
        });

    // pocket.setTransform(entityId, x, y, z)
    pocket.set_function("setTransform",
        [this](u64 entityId, float x, float y, float z) {
            if (!impl_->reg) return;
            ecs::Entity e{static_cast<EntityId>(entityId)};
            if (!impl_->reg->has<ecs::Transform>(e)) {
                impl_->reg->emplace<ecs::Transform>(e);
            }
            auto& tr = *impl_->reg->get<ecs::Transform>(e);
            tr.position.x = x;
            tr.position.y = y;
            tr.position.z = z;
        });

    // pocket.spawn(tagName) -> entityId
    pocket.set_function("spawn",
        [this](const char* tag) -> u64 {
            if (!impl_->reg) return 0;
            auto e = impl_->reg->create();
            impl_->reg->emplace<ecs::Transform>(e);
            impl_->reg->add<ecs::Tag>(e, ecs::Tag{tag ? tag : "", true});
            PE_INFO("lua", "spawn entity=%llu tag=%s",
                    (unsigned long long)e.id, tag ? tag : "");
            return e.id;
        });

    // pocket.audio table
    sol::table a = pocket.create_named("audio");
    a.set_function("playSound",
        [](u32 id, float vol, float pitch, bool loop) {
            audio::audio().playSound(id, vol, pitch, loop);
        });
    a.set_function("playMusic",
        [](u32 id, float vol, bool loop) {
            audio::audio().playMusic(id, vol, loop);
        });
    a.set_function("stopMusic",
        []() { audio::audio().stopMusic(); });

    PE_INFO("script", "Lua bindings registered: pocket.{log,input,getTransform,setTransform,spawn,audio}");
}

void ScriptEngine::setInputCallback(KeyDownFn fn) { impl_->keyDown = std::move(fn); }
void ScriptEngine::setRegistry(ecs::Registry* reg) { impl_->reg = reg; }

// ---- loadScript ----
bool ScriptEngine::loadScript(ecs::Entity e, const char* path) {
    if (!isInitialized()) return false;
    if (!path || !path[0]) return false;

    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        PE_ERROR("script", "Script dosyası açılamadı: %s", path);
        return false;
    }
    std::stringstream ss;
    ss << ifs.rdbuf();
    String src = ss.str();

    sol::environment env(impl_->lua, sol::create);
    env["entity_id"] = static_cast<u64>(e.id);

    sol::protected_function_result res = impl_->lua.safe_script(
        src, env, sol::script_pass_on_error, path);
    if (!res.valid()) {
        sol::error err = res;
        PE_ERROR("script", "Lua load error in %s: %s", path, err.what());
        return false;
    }

    Impl::Instance inst;
    inst.entity    = e;
    inst.entityId  = e.id;
    inst.env       = env;
    inst.on_start  = env["on_start"];
    inst.on_update = env["on_update"];
    inst.started   = false;
    inst.valid     = true;
    impl_->instances[e.id] = std::move(inst);

    PE_INFO("script", "Script loaded: entity=%llu path=%s on_start=%d on_update=%d",
            (unsigned long long)e.id, path,
            (bool)inst.on_start, (bool)inst.on_update);
    return true;
}

void ScriptEngine::unload(ecs::Entity e) {
    if (!impl_) return;
    impl_->instances.erase(e.id);
}

void ScriptEngine::onStart(ecs::Registry& reg) {
    if (!isInitialized()) return;
    if (impl_->reg != &reg) impl_->reg = &reg;
    for (auto& [id, inst] : impl_->instances) {
        if (inst.started) continue;
        if (inst.on_start.valid()) {
            auto res = inst.on_start();
            if (!res.valid()) {
                sol::error err = res;
                PE_ERROR("script", "on_start error entity=%llu: %s",
                         (unsigned long long)id, err.what());
            }
        }
        inst.started = true;
    }
}

void ScriptEngine::onUpdate(ecs::Registry& reg, float dt) {
    if (!isInitialized()) return;
    if (impl_->reg != &reg) impl_->reg = &reg;
    for (auto& [id, inst] : impl_->instances) {
        if (!inst.valid) continue;
        if (!inst.started) {
            if (inst.on_start.valid()) {
                auto r = inst.on_start();
                if (!r.valid()) {
                    sol::error err = r;
                    PE_ERROR("script", "on_start(late) error entity=%llu: %s",
                             (unsigned long long)id, err.what());
                }
            }
            inst.started = true;
        }
        if (inst.on_update.valid()) {
            auto res = inst.on_update(dt);
            if (!res.valid()) {
                sol::error err = res;
                PE_ERROR("script", "on_update error entity=%llu: %s",
                         (unsigned long long)id, err.what());
            }
        }
    }
}

// ---- Singleton ----
ScriptEngine& scripts() {
    static ScriptEngine s;
    return s;
}

} // namespace pocket::script
