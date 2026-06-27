#pragma once
// PocketEngine — Lua scripting bridge (sol2)
//
// Lua API (her script, kendi environment'ında çalışır; `entity_id` globali
// script'in bağlandığı entity'nin EntityId'sidir):
//
//   pocket.log(msg)                            -> PE_INFO
//   pocket.input.keyDown(scancode)             -> bool
//   pocket.getTransform(entityId)              -> {x=,y=,z=}
//   pocket.setTransform(entityId, x, y, z)
//   pocket.spawn(tagName)                      -> entityId
//   pocket.audio.playSound(soundId, vol, pitch, loop)
//   pocket.audio.playMusic(musicId, vol, loop)
//   pocket.audio.stopMusic()
//
// Script fonksiyonları (opsiyonel, her environment'da):
//   function on_start() end
//   function on_update(dt) end
//
// NOT: ScriptComponent, ecs::Transform, ecs::Rigidbody2D vb. bileşenler
// pocket/ecs/ecs.h içinde tanımlıdır. Bu başlık onları yeniden tanımlamaz.
//   ecs::Transform { math::Vec3 position, scale, rotation; }
// (2B fizik için rotation.z kullanılır.)
#include "pocket/core/types.h"
#include "pocket/ecs/ecs.h"

#include <functional>

namespace pocket::script {

// Input callback imzası: SDL scancode -> basılı mı?
using KeyDownFn = std::function<bool(int)>;

class ScriptEngine {
public:
    ScriptEngine();
    ~ScriptEngine();

    ScriptEngine(const ScriptEngine&) = delete;
    ScriptEngine& operator=(const ScriptEngine&) = delete;

    // sol::state açar, kütüphaneleri yükler, binding'leri kaydeder
    bool init();
    void shutdown();

    // Bir entity için Lua scripti yükler (chunk'ı çalıştırır, on_start/on_update
    // referanslarını saklar). on_start ilk onUpdate/onStart çağrısında tetiklenir.
    bool loadScript(ecs::Entity e, const char* path);
    // Bir entity'nin scriptini kaldırır
    void unload(ecs::Entity e);

    // Tüm scriptli entity'ler için on_start() çağırır (henüz çağrılmayanlar)
    void onStart(ecs::Registry& reg);
    // Tüm scriptli entity'ler için on_update(dt) çağırır
    void onUpdate(ecs::Registry& reg, float dt);

    // Lua API binding'lerini (pocket.* tablosu) kurar
    void registerBindings();

    // Dış sistemler (SDL input) callback kaydetmek için
    void setInputCallback(KeyDownFn fn);
    // Script motorunun ECS'e erişimi için registry set (init'ten sonra çağrılır)
    void setRegistry(ecs::Registry* reg);

    bool isInitialized() const;
    size_t scriptCount() const;
private:
    struct Impl;
    Unique<Impl> impl_;
};

// Global singleton (ana thread)
ScriptEngine& scripts();

} // namespace pocket::script
