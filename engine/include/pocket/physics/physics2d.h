#pragma once
// PocketEngine — 2D fizik (Box2D v2.4 sarmalayıcı)
// ECS Transform + Rigidbody2D + Collider2D <-> b2Body senkronizasyonu.
//
// NOT: Rigidbody2D, Collider2D, Transform bileşenleri pocket/ecs/ecs.h içinde
// tanımlıdır (pocket::ecs namespace). Bu başlık onları yeniden tanımlamaz.
#include "pocket/core/types.h"
#include "pocket/math/math.h"
#include "pocket/ecs/ecs.h"

namespace pocket::physics {

// 2B fizik dünyası. Birden çok sahne için ayrı dünyalar oluşturulabilir;
// öntanımlı olarak physics2d() singleton'u kullanılır.
class PhysicsWorld2D {
public:
    PhysicsWorld2D();
    ~PhysicsWorld2D();

    PhysicsWorld2D(const PhysicsWorld2D&) = delete;
    PhysicsWorld2D& operator=(const PhysicsWorld2D&) = delete;

    bool init(float gravityX = 0.0f, float gravityY = -9.81f,
              int velocityIter = 8, int positionIter = 3);
    void shutdown();

    // Dünyayı dt saniye ilerlet (negatif/çok büyük dt kırpılır)
    void step(float dt);

    // ECS Transform -> b2Body transform
    // (sadece Static/Kinematic body'ler için; dynamic body'ler fizik motorunca yönetilir)
    void syncFromEcs(ecs::Registry& reg);
    // b2Body transform -> ECS Transform (2B için position.x/y + rotation.z)
    void syncToEcs(ecs::Registry& reg);

    // ecs::Rigidbody2D (+opsiyonel ecs::Collider2D) taşıyan tüm entity'ler için
    // b2Body + fixture oluştur. Zaten oluşturulmuşları atlar (bodyHandle >= 0).
    void createBodies(ecs::Registry& reg);

    // Bir entity'nin body'sini yok et (entity silinince çağrılmalı)
    void destroyBody(ecs::Entity e);

    bool isInitialized() const;
    int bodyCount() const;

private:
    struct Impl;
    Unique<Impl> impl_;
};

// Global singleton (ana thread)
PhysicsWorld2D& physics2d();

} // namespace pocket::physics
