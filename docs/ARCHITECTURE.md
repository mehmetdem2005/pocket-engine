# PocketEngine — Architecture

Bu doküman motorun katmanlı mimarisini, modül sorumluluklarını, veri akışını,
bellek modelini, thread modelini, ECS tasarımını ve render pipeline'ını anlatır.

## 1. Katman Diyagramı (ASCII)

```
┌────────────────────────────────────────────────────────────┐
│                      EDITOR (pocket_editor)                 │
│   ImGui dockable panels — Hierarchy / Inspector / Viewport  │
│   Asset Browser / Console / Profiler / Scene Serializer     │
└────────────────────────────────────────────────────────────┘
                              │  (header-only link to engine lib)
┌────────────────────────────────────────────────────────────┐
│                  POCKET ENGINE (static lib)                 │
│                                                             │
│   ┌─────────────── Application Layer ───────────────┐       │
│   │  core/window  core/input  core/time  core/log   │       │
│   │  core/memory  core/types  math/math             │       │
│   └─────────────────────────────────────────────────┘       │
│                              │                              │
│   ┌─────────────── Systems Layer ───────────────────┐       │
│   │  ecs/      — Registry, Entity, ComponentPool<T> │       │
│   │  render/   — Renderer, Camera, Texture, Shader, │       │
│   │              SpriteBatch, LineBatch             │       │
│   │  physics/  — PhysicsWorld2D (Box2D bridge)      │       │
│   │  audio/    — AudioEngine (OpenAL)               │       │
│   │  script/   — ScriptEngine (Lua + sol2)          │       │
│   │  db/       — Database (SQLite)                  │       │
│   │  thread/   — ThreadPool, parallelFor, MTGuard   │       │
│   │  asset/    — AssetManager (ref-counted cache)   │       │
│   └─────────────────────────────────────────────────┘       │
│                              │                              │
│   ┌─────────────── Platform Layer ──────────────────┐       │
│   │  SDL2 (window/event)  OpenGL ES 3.0  EGL        │       │
│   │  Box2D v2.4  OpenAL-Soft  Lua 5.4  SQLite 3     │       │
│   └─────────────────────────────────────────────────┘       │
└────────────────────────────────────────────────────────────┘
                              │  (HTTP/HTTPS)
┌────────────────────────────────────────────────────────────┐
│            CLOUD BACKEND (Render.com, Node.js)              │
│   JWT auth  ·  Project sync  ·  Asset CDN  ·  SQLite/pg     │
└────────────────────────────────────────────────────────────┘
```

## 2. Modül Sorumlulukları

| Modül | Sorumluluk | Singleton accessor |
|-------|------------|--------------------|
| `core/window`     | SDL2 pencere, GL context, event polling, vsync | `pocket::core::window()` |
| `core/input`      | Klavye/fare/touch state, edge detection, delta  | `pocket::core::input()` |
| `core/time`       | Delta time, fixed delta, FPS smoothing          | `pocket::core::time()` |
| `core/log`        | Leveled logging (trace/debug/info/warn/error)   | `pocket::core::log` (macros) |
| `core/memory`     | Arena + Pool allocator, Unique/SharedPtr        | — |
| `math/math`       | Vec2/3/4, Mat4, Quat, color utils               | — |
| `ecs/ecs`         | Registry, Entity, ComponentPool<T> sparse-set   | `pocket::ecs::registry()` |
| `render/renderer` | GL viewport, camera, draw calls, present        | `pocket::render::renderer()` |
| `render/camera`   | Ortho/persp projection, view matrix             | — |
| `render/texture`  | GL texture cache, RGBA8 upload                  | — |
| `render/shader`   | GL program compile/link, uniform set            | — |
| `render/sprite_batch` | Dynamic VBO/IBO, 4096 quad/flush, line batch | — |
| `physics/physics2d` | Box2D world, body sync, raycast               | `pocket::physics::physics2d()` |
| `audio/audio`     | OpenAL device/context, 8 source pool, WAV       | `pocket::audio::audio()` |
| `script/script`   | sol2 state, sandbox env, on_start/on_update     | `pocket::script::scripts()` |
| `db/database`     | SQLite open/CRUD, WAL, projects/assets/settings | `pocket::db::database()` |
| `thread/thread_pool` | Worker pool, submit<R>, parallelFor, MTGuard | `pocket::thread::pool()` |
| `asset/asset_manager` | Ref-counted cache, async load + GPU pump    | `pocket::asset::assets()` |

## 3. Veri Akışı (Main Loop)

```
main()
  │
  ├─ window().create(...)            // SDL2 + GLES3 context
  ├─ renderer().init(window)
  ├─ audio().init()
  ├─ scripts().init()
  ├─ database().open(nullptr); initSchema()
  ├─ assets().registerLoader(Texture, ...)  // render modülü
  ├─ assets().registerLoader(Audio,    ...)  // audio modülü
  ├─ MainThreadGuard guard;          // main thread id'yi capture et
  │
  └─ while (!window().shouldClose()) {
        input().beginFrame();        // cur→prev snapshot, delta reset
        window().pollEvents();       // SDL events → input()
        time().update();             // delta + fps hesapla

        // --- Fixed-tick (fizik) ---
        time().accumulateFixed(dt);
        while (time().consumeFixed()) {
            physics2d().syncFromEcs(reg);
            physics2d().step(time().fixedDelta());
            physics2d().syncToEcs(reg);
        }

        // --- Variable-tick (script + game logic) ---
        scripts().onUpdate(dt);
        // ... user systems ...

        // --- Render ---
        assets().pumpPendingUploads(); // ana thread'te GPU upload
        renderer().begin(camera);
        //   sprite batch / line batch draw calls
        renderer().end();
        editor.render();              // ImGui draw
        window().swap();
        input().endFrame();
     }
```

## 4. Bellek Modeli

İki ana allocator tipi, ikisi de `core/memory.h` içinde:

- **Arena** — büyük blok ayırıp bump-pointer ile dağıtır. Frame-local geçici
  veriler (string formatlama, scratch buffer) için ideal. Frame sonunda
  `arena.reset()` ile sıfırlanır.
- **Pool<T>** — sabit boyutlu slot havuzu, free-list ile geri kazanım.
  ComponentPool<T> bunu kullanabilir; handle sabit kalır, pointer invalidation
  yok.

Asset cache ref-counted: `AssetHandle` shared_ptr gibi davranır, son ref
düşünce `UnloaderFn` çağrılır (örn. `glDeleteTextures`).

## 5. Threading Modeli

- **Ana thread** — SDL2/GL/ImGui çağrıları zorunlu olarak ana thread'te
  (GL context affinity). `MainThreadGuard` bu thread id'yi `call_once` ile
  capture eder; başka thread'ten çağrı olunca soft-warn loglar.
- **Worker pool** — `[2, 8]` worker (Termux'ta `hardware_concurrency` clamped).
  Standart producer/consumer + condition_variable.
- **submit<R>(fn)** — `std::future<R>` döner; paketleme `shared_ptr<packaged_task>`.
- **parallelFor(begin, end, grainSize, fn)** — aralığı chunk'lara böler,
  her chunk'ı worker'a yollar, hepsini `future.get()` ile bekler.
- **GPU upload constraint** — Texture/audio yüklemeleri worker'da dosya
  okur ama GPU upload'ı `assets().pumpPendingUploads()` ile ana thread'te
  yapılır (OpenGL context paylaşımı yok). Pending queue mutex ile korunur.

## 6. ECS Tasarımı

- **Archetype-friendly SoA** — Her ComponentPool<T> ayrı bir HashMap<EntityId, T>.
  Bu, tek-component query'leri için cache-friendly (aynı T aynı bellek
  bölgesinde). Multi-component view'lar entity-id kesişimi ile yapılır.
- **Type-erased base** — `IComponentPool` (virtual `remove/size`),
  `ComponentPool<T>` türetilir. `Registry::pools_` HashMap<type_index, Unique<IComponentPool>>.
- **Entity handle** — Monotonic id (1'den başlar, asla geri dönüştürülmez).
  Stale handle'lar collision yaratmaz.
- **Safe iteration** — `each<T>()` önce entity id listesini snapshot'lar,
  callback içinde pool mutate edilse bile iterator invalidation yok.
  `eachAll()` benzer şekilde `alive_` setini snapshot'lar.
- **Built-in components** — Tag, Transform, SpriteComponent, CameraComponent,
  Rigidbody2D (+BodyType), Collider2D (+Shape), ScriptComponent.

## 7. Render Pipeline

```
Renderer::begin(camera)
  │
  ├─ glViewport(framebufferW, framebufferH)
  ├─ glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
  ├─ view = camera.view()      // look-at veya identity
  ├─ proj = camera.proj()      // ortho veya perspective
  ├─ viewProj = proj * view
  │
  └─ SpriteBatch::begin(viewProj)
       │  glBindVertexArray(vao)
       │  glUseProgram(spriteProgram)
       │
       └─ drawQuad(tex, x, y, w, h, tint)   // vertex'leri buffer'a yığ
            │  eğer vertex sayısı >= MAX (4096*4) veya texture değişti → flush()
            │
            └─ flush():
                 glBindBuffer(GL_ARRAY_BUFFER, vbo)
                 glBufferSubData(...)        // dynamic VBO
                 glActiveTexture(GL_TEXTURE0)
                 glBindTexture(GL_TEXTURE_2D, tex)
                 glDrawElements(GL_TRIANGLES, quadCount*6, GL_UNSIGNED_SHORT, 0)

Renderer::end()
  ├─ SpriteBatch::flush()       // kalan vertex'leri boşalt
  └─ LineBatch::flush()

Renderer::present()
  └─ SDL_GL_SwapWindow(window)  // vsync
```

**Vertex layout (sprite):** `pos3 + uv2 + col4 = 9 float / 36 byte` —
`layout(location=0) in vec3 aPos; layout(location=1) in vec2 aUV;
layout(location=2) in vec4 aColor;`

**Index buffer:** 65536 vertex için statik IBO, her quad 6 index
(0,1,2, 2,3,0) — quad başına 4 vertex / 6 index.

**Shader fallback:** `shader.cpp` içinde gömülü default kaynaklar var; disk
üzerindeki `assets/shaders/sprite.vert` bulunamazsa gömülü versiyon kullanılır.
Bu sayede editör asset'ler olmadan da açılır.

**Camera mode:** `CameraComponent::ortho=true` ise orthographic projection
(piksel-uyumlu 2B render için), `false` ise perspective. `primary` flag'i
hang kameranın `begin()`'de kullanılacağını belirler.

## 8. Cloud Sync (Render.com)

Editor ↔ Backend akışı:

```
Editor (yaz)                       Backend (Render.com)
  │                                    │
  ├─ POST /api/auth/register  ────────►│  deviceId → JWT token
  │                                    │
  ├─ POST /api/sync/push        ───────►│  projects[] + assets[] (base64)
  │   { projects:[{id,manifest}],      │  upsert by localId↔serverId
  │     assets :[{id,data}] }          │
  │                                    │
  ◄──── POST /api/sync/pull  ──────────┤  { projects, assets }
  │                                    │
  └─ GET  /api/assets/:uuid     ───────►│  CDN-style raw serve
                                       │  (unauth — UUID tahmin edilemez)
```

Detaylar: `docs/RENDER_DEPLOY.md` ve `server/README.md`.
