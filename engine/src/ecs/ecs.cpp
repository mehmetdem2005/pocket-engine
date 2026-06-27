// PocketEngine — ECS Registry implementation (non-template methods + singleton)
#include "pocket/ecs/ecs.h"

namespace pocket::ecs {

Entity Registry::create() {
    EntityId id = nextId_++;
    alive_.insert(id);
    PE_TRACE("ecs", "create entity id=%llu", (unsigned long long)id);
    return Entity{id};
}

void Registry::destroy(Entity e) {
    if (!valid(e)) {
        PE_WARN("ecs", "destroy: invalid entity %llu", (unsigned long long)e.id);
        return;
    }
    // Once tum component pool'larindan kaldir (type-erased remove).
    for (auto& [ti, pool] : pools_) {
        if (pool) pool->remove(e.id);
    }
    alive_.erase(e.id);
    PE_TRACE("ecs", "destroy entity id=%llu", (unsigned long long)e.id);
}

bool Registry::valid(Entity e) const {
    if (e.id == INVALID_ENTITY) return false;
    return alive_.find(e.id) != alive_.end();
}

void Registry::clear() {
    pools_.clear();
    alive_.clear();
    // Not: nextId_ sifirlanmaz; monotonic ID garanti boylece stale handle'lar
    // carpismaz. Eger tam reset istenirse nextId_ = 1 yapilabilir.
    PE_TRACE("ecs", "clear: pools=%zu alive=%zu",
             (size_t)0u, (size_t)0u);
}

size_t Registry::size() const {
    return alive_.size();
}

void Registry::eachAll(std::function<void(Entity)> fn) {
    // once id'leri kopyala: callback sirasinda create/destroy iterator bozmasin
    Vector<EntityId> ids;
    ids.reserve(alive_.size());
    for (auto id : alive_) ids.push_back(id);
    for (auto id : ids) {
        if (alive_.find(id) == alive_.end()) continue; // callback sirasinda silinmis
        fn(Entity{id});
    }
}

Registry& registry() {
    static Registry s_instance;
    return s_instance;
}

} // namespace pocket::ecs
