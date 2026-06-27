#pragma once
// PocketEngine — ECS (Entity Component System)
// Sparse-set / HashMap tabanlı, type-erased component pool.
// Header-mostly: template gövderleri inline.
#include "pocket/core/types.h"
#include "pocket/core/log.h"
#include <cstring> // math.h uses std::memset; ensure declared before it
#include "pocket/math/math.h"
#include <functional>
#include <typeindex>
#include <any>
#include <type_traits>

namespace pocket::ecs {

// ---- Entity ----
// Lightweight handle. Sadece id taşır; bileşenler Registry'deki pool'larda tutulur.
struct Entity {
    EntityId id = INVALID_ENTITY;
    bool operator==(Entity o) const { return id == o.id; }
    bool operator!=(Entity o) const { return id != o.id; }
    explicit operator bool() const { return id != INVALID_ENTITY; }
};

// ---- Registry ----
// Merkezi ECS yöneticisi. Singleton'a `registry()` ile erişilir.
// Component pool'lar type-erased: std::type_index -> IComponentPool (virtual remove).
class Registry {
public:
    Registry() = default;
    ~Registry() = default;
    Registry(const Registry&) = delete;
    Registry& operator=(const Registry&) = delete;

    // ---- Entity lifecycle (non-template, impl in ecs.cpp) ----
    Entity create();
    void   destroy(Entity e);
    bool   valid(Entity e) const;
    void   clear();
    size_t size() const;

    // ---- Component ops (template, inline header) ----
    template <typename T> void add(Entity e, T component);
    template <typename T, typename... Args> T& emplace(Entity e, Args&&... args);
    template <typename T> T*   get(Entity e);
    template <typename T> bool has(Entity e) const;
    template <typename T> void remove(Entity e);
    template <typename T> size_t count() const;
    template <typename T> void each(std::function<void(Entity, T&)> fn);

    // ---- Iterate all alive entities (non-template, impl in ecs.cpp) ----
    void eachAll(std::function<void(Entity)> fn);

private:
    // Type-erased component storage: per-type SparseSet<T> (HashMap tabanlı).
    struct IComponentPool {
        virtual ~IComponentPool() = default;
        virtual void remove(EntityId) = 0;
        virtual size_t size() const = 0;
    };

    template <typename T>
    struct ComponentPool : IComponentPool {
        HashMap<EntityId, T> data;
        void remove(EntityId id) override { data.erase(id); }
        size_t size() const override { return data.size(); }
    };

    HashMap<std::type_index, Unique<IComponentPool>> pools_;
    EntityId nextId_ = 1;
    HashSet<EntityId> alive_;

    // Lazy-access (non-creating). Header'da tanımlı.
    template <typename T>
    ComponentPool<T>* getPool();
    template <typename T>
    const ComponentPool<T>* getPool() const;

    // Lazy-create, header'da tanımlı.
    template <typename T>
    ComponentPool<T>* ensurePool();
};

// ---- Global singleton ----
Registry& registry();

// ---- Built-in components ----
struct Tag {
    String name;
    bool visible = true;
};

struct Transform {
    math::Vec3 position{0, 0, 0};
    math::Vec3 scale{1, 1, 1};
    math::Vec3 rotation{0, 0, 0}; // euler radians
    Transform* parent = nullptr;
};

struct SpriteComponent {
    u32 textureId = 0;
    math::Vec2 size{1, 1};
    math::Vec2 uv0{0, 0};
    math::Vec2 uv1{1, 1};
    math::Color tint{1, 1, 1, 1};
    int layer = 0;
};

struct CameraComponent {
    float zoom = 1.0f;
    float fov = 60.0f;
    bool ortho = true;
    float nearZ = -100, farZ = 100;
    math::Color clearColor{0.15f, 0.15f, 0.17f, 1};
    bool primary = false;
};

struct Rigidbody2D {
    int bodyHandle = -1; // bridge to physics system
    enum class BodyType { Static = 0, Kinematic = 1, Dynamic = 2 } type = BodyType::Dynamic;
    float linearDamping = 0.0f;
    float angularDamping = 0.0f;
    float gravityScale = 1.0f;
    bool fixedRotation = false;
};

struct Collider2D {
    enum class Shape { Box, Circle } shape = Shape::Box;
    math::Vec2 size{1, 1};
    float radius = 0.5f;
    float friction = 0.3f;
    float restitution = 0.0f;
    bool isTrigger = false;
};

struct ScriptComponent {
    String scriptPath; // .lua file
    bool enabled = true;
};

} // namespace pocket::ecs

// ============================================================
// ---- Template implementations (header) ----
// ============================================================
namespace pocket::ecs {

template <typename T>
Registry::ComponentPool<T>* Registry::getPool() {
    auto it = pools_.find(std::type_index(typeid(T)));
    if (it == pools_.end()) return nullptr;
    return static_cast<ComponentPool<T>*>(it->second.get());
}

template <typename T>
const Registry::ComponentPool<T>* Registry::getPool() const {
    auto it = pools_.find(std::type_index(typeid(T)));
    if (it == pools_.end()) return nullptr;
    return static_cast<const ComponentPool<T>*>(it->second.get());
}

template <typename T>
Registry::ComponentPool<T>* Registry::ensurePool() {
    auto ti = std::type_index(typeid(T));
    auto it = pools_.find(ti);
    if (it != pools_.end()) {
        return static_cast<ComponentPool<T>*>(it->second.get());
    }
    auto up = makeUnique<ComponentPool<T>>();
    ComponentPool<T>* raw = up.get();
    pools_.emplace(ti, std::move(up));
    return raw;
}

template <typename T>
void Registry::add(Entity e, T component) {
    if (!valid(e)) {
        PE_WARN("ecs", "add: invalid entity %llu", (unsigned long long)e.id);
        return;
    }
    auto* pool = ensurePool<T>();
    pool->data[e.id] = std::move(component);
}

template <typename T, typename... Args>
T& Registry::emplace(Entity e, Args&&... args) {
    if (!valid(e)) {
        PE_WARN("ecs", "emplace: invalid entity %llu", (unsigned long long)e.id);
        // Yine de boş bir slot oluşturup referans dönelim (geri dönüş referansı).
        static T fallback{};
        return fallback;
    }
    auto* pool = ensurePool<T>();
    auto it = pool->data.find(e.id);
    if (it == pool->data.end()) {
        it = pool->data.emplace(e.id, T(std::forward<Args>(args)...)).first;
    } else {
        it->second = T(std::forward<Args>(args)...);
    }
    return it->second;
}

template <typename T>
T* Registry::get(Entity e) {
    auto* pool = getPool<T>();
    if (!pool) return nullptr;
    auto it = pool->data.find(e.id);
    if (it == pool->data.end()) return nullptr;
    return &it->second;
}

template <typename T>
bool Registry::has(Entity e) const {
    const auto* pool = getPool<T>();
    if (!pool) return false;
    return pool->data.find(e.id) != pool->data.end();
}

template <typename T>
void Registry::remove(Entity e) {
    auto* pool = getPool<T>();
    if (!pool) return;
    pool->data.erase(e.id);
}

template <typename T>
size_t Registry::count() const {
    const auto* pool = getPool<T>();
    return pool ? pool->data.size() : 0;
}

template <typename T>
void Registry::each(std::function<void(Entity, T&)> fn) {
    auto* pool = getPool<T>();
    if (!pool) return;
    // Not: callback, pool'a yeni eleman eklerse iterator invalidation olabilir.
    // Bu yüzden önce id'leri topla, sonra callback'i çağır.
    Vector<EntityId> ids;
    ids.reserve(pool->data.size());
    for (const auto& kv : pool->data) ids.push_back(kv.first);
    for (auto id : ids) {
        auto it = pool->data.find(id);
        if (it == pool->data.end()) continue; // callback sırasında silinmiş olabilir
        fn(Entity{id}, it->second);
    }
}

} // namespace pocket::ecs
