// PocketEngine — 2D fizik uygulaması (Box2D v2.4 C++ API)
// ecs::Transform / ecs::Rigidbody2D / ecs::Collider2D <-> b2Body köprüsü.
#include "pocket/physics/physics2d.h"
#include "pocket/core/log.h"

#include <box2d/box2d.h>

#include <unordered_map>

namespace pocket::physics {

// ---- Impl ----
struct PhysicsWorld2D::Impl {
    b2World* world = nullptr;
    int velocityIter = 8;
    int positionIter = 3;
    // EntityId -> b2Body* (EntityId u64 olduğu için std::hash<EntityId> mevcut)
    HashMap<EntityId, b2Body*> bodies;
};

// ---- Ctor/Dtor ----
PhysicsWorld2D::PhysicsWorld2D() : impl_(nullptr) {}
PhysicsWorld2D::~PhysicsWorld2D() { shutdown(); }

bool PhysicsWorld2D::init(float gravityX, float gravityY,
                          int velocityIter, int positionIter) {
    if (impl_) shutdown();
    impl_ = makeUnique<Impl>();
    impl_->world = new b2World(b2Vec2(gravityX, gravityY));
    impl_->velocityIter = velocityIter;
    impl_->positionIter = positionIter;
    PE_INFO("physics", "PhysicsWorld2D init: gravity=(%.2f,%.2f) velIter=%d posIter=%d",
            gravityX, gravityY, velocityIter, positionIter);
    return impl_->world != nullptr;
}

void PhysicsWorld2D::shutdown() {
    if (!impl_) return;
    if (impl_->world) {
        delete impl_->world;
        impl_->world = nullptr;
    }
    impl_->bodies.clear();
    impl_.reset();
    PE_INFO("physics", "PhysicsWorld2D shutdown");
}

bool PhysicsWorld2D::isInitialized() const { return impl_ && impl_->world; }
int  PhysicsWorld2D::bodyCount() const { return impl_ ? (int)impl_->bodies.size() : 0; }

// ---- step ----
void PhysicsWorld2D::step(float dt) {
    if (!impl_ || !impl_->world) return;
    if (dt <= 0.0f) return;
    if (dt > 0.1f) dt = 0.1f; // spiral-of-death koruması
    impl_->world->Step(dt, impl_->velocityIter, impl_->positionIter);
}

// ---- syncFromEcs: Transform -> b2Body (Static/Kinematic için) ----
void PhysicsWorld2D::syncFromEcs(ecs::Registry& reg) {
    if (!impl_ || !impl_->world) return;
    reg.each<ecs::Rigidbody2D>([this](ecs::Entity e, ecs::Rigidbody2D& rb) {
        if (rb.bodyHandle < 0) return;
        auto it = impl_->bodies.find(e.id);
        if (it == impl_->bodies.end() || !it->second) return;
        b2Body* body = it->second;
        // Sadece kinematic/static body'leri ECS'ten güncelle
        if (body->GetType() == b2_dynamicBody) return;
        ecs::Transform* t = nullptr;
        if (reg.has<ecs::Transform>(e)) t = reg.get<ecs::Transform>(e);
        if (!t) return;
        body->SetTransform(b2Vec2(t->position.x, t->position.y), t->rotation.z);
    });
}

// ---- syncToEcs: b2Body -> Transform ----
void PhysicsWorld2D::syncToEcs(ecs::Registry& reg) {
    if (!impl_ || !impl_->world) return;
    reg.each<ecs::Rigidbody2D>([this, &reg](ecs::Entity e, ecs::Rigidbody2D& rb) {
        if (rb.bodyHandle < 0) return;
        auto it = impl_->bodies.find(e.id);
        if (it == impl_->bodies.end() || !it->second) return;
        b2Body* body = it->second;
        const b2Vec2& p = body->GetPosition();
        if (reg.has<ecs::Transform>(e)) {
            auto& t = *reg.get<ecs::Transform>(e);
            t.position.x = p.x;
            t.position.y = p.y;
            t.rotation.z = body->GetAngle();
        }
    });
}

// ---- createBodies ----
void PhysicsWorld2D::createBodies(ecs::Registry& reg) {
    if (!impl_ || !impl_->world) {
        PE_ERROR("physics", "createBodies called before init()");
        return;
    }
    reg.each<ecs::Rigidbody2D>([this, &reg](ecs::Entity e, ecs::Rigidbody2D& rb) {
        if (rb.bodyHandle >= 0) {
            // Zaten oluşturulmuş; map'i tutarlı tut
            if (impl_->bodies.find(e.id) == impl_->bodies.end()) {
                // bodyHandle>=0 ama body yok — tutarsız, yeniden oluştur
            } else {
                return;
            }
        }
        b2BodyDef bd;
        switch (rb.type) {
            case ecs::Rigidbody2D::BodyType::Static:    bd.type = b2_staticBody;    break;
            case ecs::Rigidbody2D::BodyType::Kinematic: bd.type = b2_kinematicBody; break;
            case ecs::Rigidbody2D::BodyType::Dynamic:
            default:                                     bd.type = b2_dynamicBody;   break;
        }
        bd.gravityScale   = rb.gravityScale;
        bd.linearDamping  = rb.linearDamping;
        bd.angularDamping = rb.angularDamping;
        bd.fixedRotation  = rb.fixedRotation;
        bd.allowSleep     = true;
        bd.awake          = true;

        if (reg.has<ecs::Transform>(e)) {
            auto& t = *reg.get<ecs::Transform>(e);
            bd.position.Set(t.position.x, t.position.y);
            bd.angle = t.rotation.z;
        }

        b2Body* body = impl_->world->CreateBody(&bd);
        rb.bodyHandle = 0; // "oluşturuldu" işareti
        impl_->bodies[e.id] = body;

        // Collider2D varsa fixture ekle
        if (reg.has<ecs::Collider2D>(e)) {
            auto& c = *reg.get<ecs::Collider2D>(e);
            b2FixtureDef fd;
            fd.density     = 1.0f;
            fd.friction    = c.friction;
            fd.restitution = c.restitution;
            fd.isSensor    = c.isTrigger;
            if (c.shape == ecs::Collider2D::Shape::Box) {
                b2PolygonShape box;
                // Collider2D.size tam boyut; Box2D yarım boyut ister
                box.SetAsBox(c.size.x * 0.5f, c.size.y * 0.5f);
                fd.shape = &box;
            } else {
                b2CircleShape circle;
                circle.m_p.Set(0.0f, 0.0f);
                circle.m_radius = c.radius;
                fd.shape = &circle;
            }
            body->CreateFixture(&fd);
        } else if (rb.type == ecs::Rigidbody2D::BodyType::Dynamic) {
            // Dynamic body için varsayılan kutu (kütle kazanır)
            b2PolygonShape box;
            box.SetAsBox(0.5f, 0.5f);
            b2FixtureDef fd;
            fd.shape = &box;
            fd.density = 1.0f;
            fd.friction = 0.3f;
            body->CreateFixture(&fd);
        }

        PE_DEBUG("physics", "Created b2Body entity=%llu type=%d",
                 (unsigned long long)e.id, (int)rb.type);
    });
}

// ---- destroyBody ----
void PhysicsWorld2D::destroyBody(ecs::Entity e) {
    if (!impl_ || !impl_->world) return;
    auto it = impl_->bodies.find(e.id);
    if (it == impl_->bodies.end()) return;
    if (it->second) impl_->world->DestroyBody(it->second);
    impl_->bodies.erase(it);
    // Not: ECS tarafındaki Rigidbody2D.bodyHandle çağıran tarafından sıfırlanmalı
}

// ---- Singleton ----
PhysicsWorld2D& physics2d() {
    static PhysicsWorld2D s;
    return s;
}

} // namespace pocket::physics
