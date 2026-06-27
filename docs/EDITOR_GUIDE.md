# PocketEngine — Editor Guide

PocketEngine editörü, Dear ImGui üzerine kurulu, **dockable** panel
sistemine sahip, **landscape modda** optimize edilmiş bir oyun editörüdür.
Bu doküman panel kullanımını, sahne oluşturmayı, entity/component eklemeyi,
Lua scripting örneğini ve play mode'u açıklar.

## İçindekiler

1. [Editöre giriş](#1-editöre-giriş)
2. [Paneller](#2-paneller)
3. [Sahne oluşturma](#3-sahne-oluşturma)
4. [Entity & Component ekleme](#4-entity--component-ekleme)
5. [Lua scripting örneği](#5-lua-scripting-örneği)
6. [Play mode](#6-play-mode)
7. [Kısayollar](#7-kısayollar)

---

## 1. Editöre giriş

Editörü başlat:

```bash
bash run.sh
```

Açılışta:

- Termux:X11 penceresinde **landscape** modda açılır
- Default layout: sol üstte Hierarchy + Inspector, ortada Viewport, altta
  Asset Browser + Console
- ImGui dockable — panelleri sürükleyip bırakarak kendi düzenini kurabilirsin

> İlk açılışta `assets/scenes/default.scene` otomatik yüklenir (bir kamera
> + bir kare sprite içerir).

---

## 2. Paneller

### 2.1. Hierarchy

Sahnedeki tüm entity'leri ağaç yapısında listeler.

- **+ Entity** butonu — yeni boş entity oluştur
- **Sağ tık → Delete** — entity'i sil
- **Sağ tık → Duplicate** — kopyala (tag hariç tüm component'lerle)
- **Drag & drop** — parent/child ilişkisi kur (TODO)

### 2.2. Inspector

Seçili entity'nin component'lerini gösterir/düzenler.

Her component için özelleştirilmiş ImGui widget'ları:

- **Tag** — text input
- **Transform** — pos (3 float), rot (3 float), scale (3 float)
- **SpriteComponent** — size (2 float), tint (color picker), texture (path)
- **CameraComponent** — ortho (checkbox), zoom (slider), primary (checkbox),
  clearColor (color picker)
- **Rigidbody2D** — bodyType (Static/Kinematic/Dynamic enum), gravityScale,
  damping, fixedRotation
- **Collider2D** — shape (Box/Circle), density, friction, restitution, sensor
- **ScriptComponent** — path (text input + browse button), started (read-only)

**+ Add Component** dropdown'ından yeni component eklenir.

### 2.3. Viewport

Sahnenin render çıktısı. ImGui `Image` widget'ı içinde bir framebuffer'a
çizilir.

- **Sol tık** — entity seç (raycast)
- **Sağ tık + sürükle** — kamera pan
- **Tekerlek** — zoom (camera.zoom değiştirir)
- **Shift + tekerlek** — hızlı zoom
- **F** tuşu — seçili entity'ye odaklan (TODO)

Viewport üst köşede **play/pause/stop** butonları.

### 2.4. Asset Browser

`assets/` altındaki tüm dosyaları tarar (AssetManager.scanDirectory):

- **Textures** — `.png .jpg .jpeg .bmp .tga .webp .gif`
- **Audio** — `.wav .ogg .mp3 .flac`
- **Scripts** — `.lua`
- **Scenes** — `.scene .scn`
- **Fonts** — `.ttf .otf`

Sürükleyip Inspector'daki ilgili alana bırakarak asset atayabilirsin.

### 2.5. Console

Motor log çıktısı (`PE_LOG_*` makroları). Filtre:

- Trace / Debug / Info / Warn / Error seviyeleri (checkbox)
- Text filter (substring)
- **Clear** butonu

### 2.6. Profiler (TODO)

Frame time grafiği, subsystem breakdown (render/physics/script/db ms).

---

## 3. Sahne oluşturma

### 3.1. Boş sahne

`File → New Scene` (Ctrl+N). Tüm entity'ler temizlenir, sadece boş bir
kamera entity'si eklenir.

### 3.2. Kaydet

`File → Save Scene` (Ctrl+S). `.scene` JSON dosyası olarak kaydedilir.
Format:

```json
{
  "name": "My Scene",
  "entities": [
    {
      "tag": "MainCamera",
      "transform": { "pos": [0,0,10], "rot": [0,0,0], "scale": [1,1,1] },
      "camera":    { "ortho": true, "zoom": 1.0, "primary": true,
                      "clearColor": [0.15, 0.15, 0.17, 1.0] }
    }
  ]
}
```

`assets/scenes/default.scene` örnek bir şablondur.

### 3.3. Yükle

`File → Open Scene` (Ctrl+O) ya da CLI'dan:

```bash
./run.sh path/to/my.scene
```

---

## 4. Entity & Component ekleme

### 4.1. Yeni entity

Hierarchy → **+ Entity** → boş entity oluşur, otomatik seçilir, Inspector'da
açılır.

### 4.2. Tag ekle

Inspector → **+ Add Component → Tag** → text input'a isim yaz (örn. "Player").

### 4.3. Transform

Tüm entity'ler Transform içermeli (manuel eklenmeli). Inspector →
**+ Add Component → Transform**. Pos/Rot/Scale 3'er float ile düzenle.

### 4.4. Sprite

**+ Add Component → SpriteComponent**:

- **Size** — dünya biriminde (1.0 = 1m)
- **Tint** — RGBA color picker (default beyaz)
- **Texture** — Asset Browser'dan .png sürükle ya da path yaz

### 4.5. Camera

Her sahnenin en az bir **primary** kameraya ihtiyacı var:

- **Ortho** true → 2B piksel-uyumlu render
- **Zoom** → viewport dünya boyutu = ekran boyutu / zoom
- **Clear color** → her frame başı ekranı bu renge boşalt

### 4.6. Physics

**+ Add Component → Rigidbody2D**:

- **BodyType**: Static (duvar), Kinematic (platform), Dynamic (oyuncu)
- **GravityScale** → 0 ise yerçekimi etkisiz
- **FixedRotation** → true ise dönmesin

**+ Add Component → Collider2D**:

- **Shape**: Box (size) veya Circle (radius)
- **Density / Friction / Restitution** → fizik malzeme parametreleri
- **Sensor** → true ise çarpışma değil tetikleme

### 4.7. Script

**+ Add Component → ScriptComponent** → path alanına `.lua` dosyası ver.

---

## 5. Lua scripting örneği

`scripts/example_player.lua` örnek bir hareket scriptidir:

```lua
-- example_player.lua
-- Ok tuşları ile entity'yi 5 birim/saniye hareket ettir

function on_start()
    pocket.log("Player script başladı, entity_id=" .. tostring(entity_id))
end

function on_update(dt)
    local t = pocket.getTransform(entity_id)
    local speed = 5.0  -- birim/saniye

    -- SDL scancode'lar: 79=Right, 80=Left, 81=Down, 82=Up
    if pocket.input.keyDown(79) then t.x = t.x + speed * dt end
    if pocket.input.keyDown(80) then t.x = t.x - speed * dt end
    if pocket.input.keyDown(81) then t.y = t.y - speed * dt end
    if pocket.input.keyDown(82) then t.y = t.y + speed * dt end

    pocket.setTransform(entity_id, t.x, t.y, t.z)
end
```

### 5.1. Lua API

Tüm API `pocket` global tablosu altında:

| Fonksiyon | Açıklama |
|-----------|----------|
| `pocket.log(msg)` | Log (info seviyesi) |
| `pocket.input.keyDown(scancode)` | Tuş basılı mı? (SDL scancode) |
| `pocket.getTransform(entityId)` | `→ {x, y, z}` table |
| `pocket.setTransform(entityId, x, y, z)` | Transform güncelle |
| `pocket.spawn(tag)` | Yeni entity oluştur, `→ entityId` |
| `pocket.audio.playSound(path)` | Ses çal (load yoksa otomatik) |
| `pocket.audio.playMusic(path)` | Müzik stream |
| `pocket.audio.stopMusic()` | Müzik durdur |

### 5.2. Lifecycle

- `on_start()` — entity ilk kez update döngüsüne girdiğinde bir kez
- `on_update(dt)` — her frame, dt saniye cinsinden delta time

Her script kendi `sol::environment` sandbox'unda çalışır — global çakışması
yok. `entity_id` globali script'e özel olarak set edilir.

### 5.3. Hata yakalama

Script hatası log'a yazılır, diğer script'ler etkilenmez. `sol::protected_function`
+ `sol::script_pass_on_error` kullanılır.

---

## 6. Play mode

Viewport üstündeki **▶ Play** butonuna bas → oyun moduna geçer.

### Play mode davranışı

- **Editör snapshot alınır** — mevcut sahne state'i kaydedilir
- **ScriptComponent.onStart** çağrılır (her script için bir kez)
- **Physics world createBodies** — tüm Rigidbody2D+Collider2D için Box2D body
- **Main loop**:
  - `input().beginFrame()` → `pollEvents()` → `time().update()`
  - Fixed tick: `physics2d().step(fixedDelta)` + sync
  - Variable tick: `scripts().onUpdate(delta)`
  - Render
- **Stop** → snapshot geri yüklenir, fizik world temizlenir, script state sıfırlanır

### Play mode sırasında edit

Play mode'da Inspector'da değişiklik yapabilirsin ama **stop**'a basınca
kaybolur. Kalıcı değişiklik için **stop → edit → save**.

### Pause

**⏸ Pause** → main loop donar (render devam eder, update çağrılmaz).
Frame-step için (TODO) **⏭ Step** butonu.

---

## 7. Kısayollar

| Tuş | Aksiyon |
|-----|---------|
| `Ctrl+N` | New Scene |
| `Ctrl+O` | Open Scene |
| `Ctrl+S` | Save Scene |
| `Ctrl+Shift+S` | Save As |
| `Ctrl+P` | Toggle Play |
| `Ctrl+D` | Duplicate entity |
| `Delete` | Delete entity |
| `F` | Focus selected (TODO) |
| `Esc` | Deselect |

> Termux:X11 üzerinde fiziksel klavye yoksa, Android soft keyboard
> kullanılabilir; ImGui text input'lar otomatik IME açar.

---

## 8. İpuçları

- **Landscape zorunlu** — `run.sh` `POCKET_LANDSCAPE=1` set eder
- **Touch → mouse** — SDL2 `SDL_HINT_MOUSE_TOUCH_EVENTS=1` ile tek dokunuş
  sol tık olarak maplenir (window.cpp içinde)
- **Multi-touch** — ImGui şu an tek-touch; pinch-zoom için TODO
- **Asset cache** — aynı dosya iki kez yüklenmez; unload için `assets().unload(path)`
- **DB** — `~/.pocketengine/projects.db` altında; her save otomatik kayıt

Sorun mu var? `docs/TERMUX_GUIDE.md` → Troubleshooting bölümüne bak.
