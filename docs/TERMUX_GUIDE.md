# Termux:X11 Setup Guide

Bu doküman PocketEngine'i Android'de Termux + Termux:X11 ile çalıştırmak için
adım adım rehber sunar. Sorun giderme ve performans ipuçları dahildir.

## İçindekiler

1. [Termux:X11 nedir, ne değildir?](#1-termuxx11-nedir-ne-değildir)
2. [Gereksinimler](#2-gereksinimler)
3. [Adım adım kurulum](#3-adım-adım-kurulum)
4. [Editörü çalıştırma](#4-editörü-çalıştırma)
5. [Troubleshooting](#5-troubleshooting)
6. [Performans ipuçları](#6-performans-ipuçları)

---

## 1. Termux:X11 nedir, ne değildir?

**Termux**, Android üzerinde bir Linux terminali sağlayan uygulamadır. Kendi
`pkg` paket yöneticisi vardır, Android'in sandbox'ında çalışır, **root
gerektirmez**.

**Termux:X11**, Termux'a eklenen bir **X11 sunucusudur**. Cihazda ayrı bir
Android uygulaması (`.apk`) olarak çalışır, Termux içinden `DISPLAY=:0`
üzerinden X11 istemcilerini (SDL2, ImGui, GTK uygulamaları) çizer.

| Özellik | Termux (sadece) | Termux:X11 ile |
|---------|-----------------|----------------|
| CLI araçlar (git, clang, vim) | ✅ | ✅ |
| GUI uygulama (SDL2, ImGui) | ❌ | ✅ |
| OpenGL ES context | ❌ (headless) | ✅ |
| Fare/klavye dokunmatik eşleme | — | ✅ |
| Android native dialog | — | ❌ (X11 penceresi) |

> **Önemli:** Termux:X11, **proot** veya **chroot** değildir. Gerçek X11
> sunucusudur; Termux process'leri doğrudan Xlib/SDL ile bağlanır.

---

## 2. Gereksinimler

- **Android 7.0+** (Termux minimum)
- **~1.5 GB boş yer** (paketler + ImGui derlemesi)
- **F-Droid** (Play Store sürümleri **güncel değil**, kullanmayın)
- İnternet bağlantısı (kurulum sırasında)

Aşağıdaki üç APK'yı **F-Droid'ten** kurun:

1. **Termux** — `com.termux`
2. **Termux:X11** — `com.termux.x11` (X server, Android penceresi)
3. (Otomatik `pkg install termux-x11-nightly` ile gelen companion paket)

> Termux:X11 ile Termux aynı process grubunda olmalı. Genelde Termux önce,
> sonra Termux:X11 açılır.

---

## 3. Adım adım kurulum

### 3.1. F-Droid'ten uygulamaları kur

```
F-Droid → "Termux" ara → Install
F-Droid → "Termux:X11" ara → Install
```

### 3.2. Termux'u aç, depoyu güncelle

```bash
pkg update -y && pkg upgrade -y
```

### 3.3. Git kur, repoyu klonla

```bash
pkg install -y git
git clone https://github.com/<user>/pocket-engine.git
cd pocket-engine
```

### 3.4. Setup scriptini çalıştır

```bash
bash setup-termux.sh
```

Script şunları yapar:

1. `pkg update` + `upgrade`
2. Temel araçlar: `git cmake ninja clang make pkg-config wget unzip`
3. Grafik stack: `termux-x11-nightly mesa` (+ opsiyonel `vulkan-tools-generic`)
4. Motor deps: `sdl2 sdl2-image sdl2-ttf sdl2-mixer opengl box2d openal-soft lua5.4 lua5.4-dev sqlite sqlite-dev`
5. `third_party/imgui` (v1.91.5) klonu — idempotent
6. `third_party/sol2` (v3.3.1) klonu — idempotent
7. `assets/fonts/Roboto-Medium.ttf` indir (yedek URL'lerle)
8. `build/` altında CMake + Ninja ile Release derleme

> İlk çalıştırma 5–10 dk. Tekrar çalıştırırsan idempotent: mevcut klonlar
> atlanır, sadece değişen dosyalar yeniden derlenir.

### 3.5. (Opsiyonel) İlk test — X11 çalışıyor mu?

```bash
pkg install -y x11-utils
# Termux:X11 uygulamasını Android'de aç (siyah ekran görünene kadar)
# Sonra Termux'a dön:
DISPLAY=:0 xdpyinfo | head -20
```

`name of display:    :0` satırını görüyorsan X11 hazır.

---

## 4. Editörü çalıştırma

### 4.1. Termux:X11'i aç

Android'de **Termux:X11** uygulamasını aç. Siyah bir ekran görmelisin.

### 4.2. run.sh

Termux'a dön:

```bash
bash run.sh
```

`run.sh` otomatik olarak:

- `DISPLAY=:0` set eder
- `POCKET_LANDSCAPE=1` ve `SDL_VIDEO_X11_FORCE_LANDSCAPE=1` set eder
- `pgrep -f termux-x11` boşsa arka planda `termux-x11 :0` başlatır
- `xdpyinfo` (varsa) ile X sunucusunu doğrular
- `exec ./build/bin/pocket_editor "$@"` ile editörü başlatır

Editör Termux:X11 penceresinde landscape modda açılır.

### 4.3. Cihazı döndür

Eğer editör portrait'te açılırsa:

1. Android auto-rotate kapat
2. Cihazı landscape'e çevir
3. `run.sh` ile yeniden başlat

Bazı ROM'larda Termux:X11 penceresi her zaman cihaz oryantasyonunu takip eder;
bu durumda telefonu yatay tutman yeterli.

---

## 5. Troubleshooting

### 5.1. `Cannot open display :0`

**Belirti:** `run.sh` sonrası SDL2 hata: `Couldn't open X11 display`.

**Çözüm:**

1. Termux:X11 Android uygulamasının **açık** olduğundan emin ol (siyah ekran).
2. `DISPLAY` env değişkeni doğru: `echo $DISPLAY` → `:0`
3. `termux-x11 :0` çalışıyor mu?
   ```bash
   pgrep -af termux-x11
   ```
   Boşsa manuel başlat:
   ```bash
   termux-x11 :0 &
   sleep 2
   bash run.sh
   ```
4. `xdpyinfo` ile test (yukarıda 3.5).

### 5.2. `termux-x11: command not found`

Companion paketi kurulu değil:

```bash
pkg install -y termux-x11-nightly
```

### 5.3. GL context oluşturulamıyor (`SDL_GL_CreateContext failed`)

**Sebep:** mesa/GL driver yok ya da cihazın GPU'su desteklemiyor.

**Çözüm:**

```bash
pkg install -y mesa
# Test:
DISPLAY=:0 glxinfo | head -10   # pkg install mesa-demos
```

Bazı cihazlarda `mesa` yazılım rasterizer'ı yüklemek gerekiyor. Mali/Adreno
GPU'lar için Termux'un kendi `mesa` paketi yeterli olmalı.

### 5.4. ImGui dokunmatik tıklama offset'li

**Sebep:** Termux:X11 penceresi ile SDL2'nin anladığı ekran koordinatları
arasında ölçek farkı.

**Çözüm:**

- `run.sh` içinde `POCKET_LANDSCAPE=1` set edildiğinden emin ol
- SDL2 `SDL_HINT_MOUSE_TOUCH_EVENTS` ve `SDL_HINT_TOUCH_MOUSE_EVENTS` ile
  dokunmatik → fare eşlemesi yapılır (window.cpp içinde set ediliyor)
- Multi-touch için ImGui'nin `io.AddInputCharacterForEvent` yerine
  doğrudan `io.AddMouseButtonEvent` kullan

### 5.5. Derleme sırasında `box2d/box2d.h: No such file`

```bash
pkg install -y box2d
# Doğrula:
ls $PREFIX/include/box2d/box2d.h
```

### 5.6. `sol/sol.hpp: No such file or directory`

`third_party/sol2` klonu eksik:

```bash
cd third_party
git clone --depth 1 --branch v3.3.1 https://github.com/ThePhD/sol2.git
cd ..
```

### 5.7. Font yüklenmedi, ImGui'de kareler görünüyor

`assets/fonts/Roboto-Medium.ttf` yok. Setup script'i indiremedi demektir:

```bash
# Elle indir (alternatif URL'lerden biri):
wget -O assets/fonts/Roboto-Medium.ttf \
  https://github.com/googlefonts/roboto/raw/main/src/hinted/Roboto-Medium.ttf
```

### 5.8. Editör açılıyor ama ekran boş (siyah)

GL context oluştulmuş ama hiçbir şey çizilmiyor. Olası nedenler:

- Asset path'leri yanlış: `./build/bin/pocket_editor` çalışma dizini
  `pocket-engine/` olmalı (`run.sh` zaten `cd` ile oraya geçiyor)
- Shader derleme hatası: log'u kontrol et
- Camera entity yok: `assets/scenes/default.scene`'i argüman olarak ver:
  ```bash
  ./run.sh assets/scenes/default.scene
  ```

### 5.9. `pkg install` paket adı değişti

Termux depo isimleri zaman zaman değişiyor. Mevcut adı bulmak için:

```bash
pkg search mesa
pkg search sdl2
pkg search openal
```

### 5.10. Android bataryası hızlı bitiyor

Editör açıkken CPU/GPU sürekli meşgul. Olası nedenler:

- VSync kapalı (`SDL_GL_SetSwapInterval(0)`) — `run.sh`'te vsync açık
- ImGui 60 FPS'de sürekli redraw — editör idle'da 30 FPS'e düşürmeli
- Box2D fixed-tick çok sık — `time().setFixedDelta(1/60)` yeterli

---

## 6. Performans ipuçları

### 6.1. Build hızı

```bash
# Ninja zaten varsayılan. CPU çekirdek sayısı:
nproc              # örn. 8
# Setup script nproc kullanır. Manuel derleme:
ninja -C build -j$(nproc)
```

### 6.2. Editör runtime

- **Release build** (varsayılan): `-O2 -flto`
- **Sprite batch** 4096 quad/flush — daha büyük sahnelerde flush sayısı artar
- **Asset cache** ref-counted — aynı texture iki kez yüklenmez
- **Async asset load** worker thread'lerde dosya okur, sadece GPU upload
  ana thread'te

### 6.3. RAM

Termux Android sandbox'ında ~1.5–3 GB heap kullanılabilir. Büyük sahnelerde:

- Asset cache cap koy (TODO)
- Arena allocator frame sonunda reset
- ComponentPool HashMap grow factor'ü izle

### 6.4. GPU

- OpenGL ES 3.0 — Adreno 6xx+, Mali-G72+ cihazlar sorunsuz
- Eski cihazlarda GLES 2.0 fallback henüz YOK
- Yazılım rasterizer (mesa llvmpipe) çok yavaş — sadece debug için

### 6.5. Storage

- `~/.pocketengine/` altında SQLite DB + project JSON'ları
- Asset cache disk'e yazılmaz (runtime-only)
- Build artifacts `build/` altında ~200 MB — `uninstall.sh` ile temizle

---

## 7. Ekran görüntüleri (TODO)

```
docs/screenshots/
├── termux-x11-black.png        # Termux:X11 açılış siyah ekran
├── editor-landscape.png        # Editör landscape modda
├── ecs-hierarchy.png           # Entity hierarchy panel
└── play-mode.png               # Play mode
```

Bunlar ilk stabil release ile eklenecek.
