<div align="center">

# PocketEngine

**C++17 game engine that runs natively on Android via Termux:X11**

*Unity/Godot tarzı bir editör — telefonunuzun içinde, landscape modda.*

![build](https://img.shields.io/badge/build-WIP-yellow?style=flat-square)
![platform](https://img.shields.io/badge/platform-Android%20%7C%20Termux%3AX11-22c55e?style=flat-square)
![lang](https://img.shields.io/badge/C%2B%2B-17-00599c?style=flat-square)
![render](https://img.shields.io/badge/render-OpenGL%20ES%203.0-ff5722?style=flat-square)
![license](https://img.shields.io/badge/license-MIT-blue?style=flat-square)

</div>

---

## 🇹🇷 Türkçe

PocketEngine, **Android telefonunuzda doğrudan çalışan** C++ tabanlı bir 2B/3B
oyun motorudur. Root gerekmez — sadece [Termux](https://termux.dev) +
[Termux:X11](https://github.com/termux/termux-x11) kurarsınız, bu repoyu
klonlarsınız ve editör landscape modda açılır.

### Özellikler

- **ECS** — archetype-tabanlı, SoA, cache-friendly component pool'ları
- **Render** — OpenGL ES 3.0, dynamic sprite batch (4096 quad/flush), line batch, camera
- **Fizik** — Box2D v2.4 (2B rigidbody + collider)
- **Ses** — OpenAL, 8 source havuzu, WAV loader, müzik stream
- **Script** — Lua 5.4 + sol2 v3.3.1 (korumalı çağrı, sandbox environment)
- **Veritabanı** — SQLite3 (WAL mode, projects/settings/assets manifest)
- **Threading** — Thread pool + job queue, `parallelFor`, main-thread guard
- **Asset** — Ref-counted asset cache, async load + main-thread GPU upload pump
- **Editör** — Dear ImGui tabanlı, dockable panel, landscape optimize
- **Bulut senkron** — Render.com backend (JWT auth, project sync, asset CDN)

### Hızlı Başlangıç (Termux)

1. **F-Droid'ten** Termux + Termux:X11 + Termux:X11 companion paketini kurun
   (Play Store sürümünü **kullanmayın** — güncel değildir).
2. Termux'u açın:
   ```bash
   pkg install git
   git clone https://github.com/<kullanici>/pocket-engine.git
   cd pocket-engine
   bash setup-termux.sh
   ```
3. Android'de **Termux:X11** uygulamasını açın (siyah ekran görünene kadar).
4. Termux'a geri dönün:
   ```bash
   bash run.sh
   ```
5. Editör landscape modda Termux:X11 ekranında açılır.

> İlk kurulum 5–10 dk sürer (paketler + ImGui derlemesi). Sonraki derlemeler
> 30 sn civarı.

### Dokümanlar

- 📐 [Mimari](docs/ARCHITECTURE.md) — katmanlar, modüller, ECS, render pipeline
- 📱 [Termux:X11 Kurulumu](docs/TERMUX_GUIDE.md) — adım adım + troubleshooting
- 🎮 [Editör Rehberi](docs/EDITOR_GUIDE.md) — panel kullanımı, sahne, Lua scripting
- ☁️ [Render.com Backend Deploy](docs/RENDER_DEPLOY.md) — bulut senkron sunucusu

### Proje Yapısı

```
pocket-engine/
├── setup-termux.sh          # Tek komut kurulum
├── run.sh                   # Editör başlatıcı (landscape)
├── uninstall.sh             # Temizlik
├── CMakeLists.txt           # Kök CMake
├── engine/                  # Statik kütüphane (motor çekirdeği)
│   ├── include/pocket/
│   │   ├── core/            # window, input, time, log, memory, types, math
│   │   ├── ecs/             # Registry, Entity, ComponentPool<T>
│   │   ├── render/          # Renderer, Camera, Texture, Shader, SpriteBatch
│   │   ├── physics/         # PhysicsWorld2D (Box2D)
│   │   ├── audio/           # AudioEngine (OpenAL)
│   │   ├── script/          # ScriptEngine (Lua + sol2)
│   │   ├── db/              # Database (SQLite)
│   │   ├── thread/          # ThreadPool + parallelFor
│   │   └── asset/           # AssetManager (ref-counted cache)
│   └── src/
├── editor/                  # pocket_editor executable (ImGui)
│   ├── include/
│   └── src/
├── server/                  # Render.com backend (Node/Express)
│   ├── src/
│   ├── render.yaml          # Render Blueprint
│   └── README.md
├── assets/
│   ├── shaders/             # sprite.vert/frag, line.vert/frag (GLSL ES 3.00)
│   ├── fonts/               # Roboto-Medium.ttf (setup indirir)
│   ├── sprites/             # kullanıcı sprite'ları
│   └── scenes/              # default.scene (örnek)
├── third_party/
│   ├── CMakeLists.txt       # ImGui derleme hedefi
│   ├── imgui/               # klonlanır (v1.91.5)
│   └── sol2/                # klonlanır (v3.3.1, header-only)
├── scripts/                 # örnek Lua scriptleri
└── docs/                    # bu dokümanlar
```

### Yol Haritası

- [x] ECS, render, fizik, ses, script, db, thread, asset
- [x] ImGui editör iskeleti + landscape layout
- [x] Render.com backend (auth, sync, CDN)
- [ ] Editöre sahne kaydet/yükle (`.scene` JSON)
- [ ] Asset browser paneli
- [ ] Sprite animator
- [ ] 3D render yolu (Bullet3 opsiyonel)
- [ ] Touch input → ImGui mapping kalibrasyonu

### Katkıda Bulunma

PR'ler memnuniyetle karşılanır. Lütfen önce bir issue açın. Kod stili:
4-space indent, `snake_case`, `pocket::` namespace, `#pragma once`,
PIMPL pattern. Commit mesajları İngilizce, conventional-commits önerilir.

### Lisans

MIT — bkz. `LICENSE` (eklenecek).

---

## 🇬🇧 English

PocketEngine is a **C++17 game engine that runs natively on Android** through
Termux:X11 — no root, no Docker, no cloud build. Just install Termux + Termux:X11
from F-Droid, clone this repo, run `setup-termux.sh`, then `run.sh` and the
editor opens in landscape.

### Features

- **ECS** — archetype-based, SoA, cache-friendly component pools
- **Render** — OpenGL ES 3.0, dynamic sprite batch, line batch, camera
- **Physics** — Box2D v2.4 (2D rigidbody + collider)
- **Audio** — OpenAL, 8-source pool, WAV loader, music stream
- **Scripting** — Lua 5.4 + sol2 v3.3.1 (sandboxed, protected calls)
- **DB** — SQLite3 (WAL, projects / settings / asset manifest)
- **Threading** — Thread pool + job queue, `parallelFor`, main-thread guard
- **Assets** — Ref-counted cache, async load + main-thread GPU upload pump
- **Editor** — Dear ImGui, dockable panels, landscape-optimized layout
- **Cloud sync** — Render.com backend (JWT auth, project sync, asset CDN)

### Quick Start (Termux)

1. Install from **F-Droid** (not Play Store): Termux, Termux:X11, and the
   Termux:X11 companion package.
2. In Termux:
   ```bash
   pkg install git
   git clone https://github.com/<user>/pocket-engine.git
   cd pocket-engine
   bash setup-termux.sh
   ```
3. Open the **Termux:X11** app on Android (until you see a black screen).
4. Back in Termux:
   ```bash
   bash run.sh
   ```
5. The editor launches in landscape inside the Termux:X11 window.

### Documentation

- 📐 [Architecture](docs/ARCHITECTURE.md)
- 📱 [Termux:X11 Setup Guide](docs/TERMUX_GUIDE.md)
- 🎮 [Editor Guide](docs/EDITOR_GUIDE.md)
- ☁️ [Render.com Backend Deploy](docs/RENDER_DEPLOY.md)

### Screenshots

> _Screenshots will be added here once the editor ships its first playable
> build. Place `docs/screenshots/editor-landscape.png` etc._

```
docs/screenshots/
├── editor-landscape.png   (TODO)
├── ecs-hierarchy.png      (TODO)
└── play-mode.png          (TODO)
```

### Project Layout

See the tree above (same for both languages).

### Roadmap

See Turkish section above.

### Contributing

PRs welcome — please open an issue first. Code style: 4-space indent,
`snake_case`, `pocket::` namespace, `#pragma once`, PIMPL pattern.

### License

MIT — see `LICENSE` (to be added).

<div align="center">
<sub>Built for fun, on a phone, in landscape. 📱🎮</sub>
</div>
