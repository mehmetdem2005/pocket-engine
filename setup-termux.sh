#!/data/data/com.termux/files/usr/bin/bash
# ============================================================================
# PocketEngine — Termux:X11 kurulum scripti
# ----------------------------------------------------------------------------
# Bu script PocketEngine'i Android üzerinde Termux + Termux:X11 ile derlemek
# ve çalıştırmak için gerekli tüm bağımlılıkları kurar, ImGui/sol2 kaynaklarını
# third_party/ altına klonlar ve motoru Ninja ile Release modda derler.
#
# Kullanım:
#   bash setup-termux.sh         # varsayılan
#   bash setup-termux.sh --skip-build   # derleme adımını atla
#
# Idempotent: aynı ortamda tekrar çalıştırılabilir; mevcut klonlar/kurulumlar
# atlanır.
# ============================================================================
set -euo pipefail

# Renkli çıktı (non-tty ise otomatik kapanır)
if [ -t 1 ]; then
    C_GREEN='\033[1;32m'; C_YELLOW='\033[1;33m'; C_RED='\033[1;31m'
    C_BLUE='\033[1;34m';  C_BOLD='\033[1m';      C_RESET='\033[0m'
else
    C_GREEN=''; C_YELLOW=''; C_RED=''; C_BLUE=''; C_BOLD=''; C_RESET=''
fi

info()  { printf "${C_BLUE}[*]${C_RESET} %s\n"  "$*"; }
ok()    { printf "${C_GREEN}[✓]${C_RESET} %s\n" "$*"; }
warn()  { printf "${C_YELLOW}[!]${C_RESET} %s\n" "$*"; }
err()   { printf "${C_RED}[✗]${C_RESET} %s\n"   "$*" >&2; }
die()   { err "$*"; exit 1; }

# --- Kök dizin tespiti (script nereden çağrılırsa çağrılsın) ---
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"
PE_ROOT="$SCRIPT_DIR"

SKIP_BUILD=0
for arg in "$@"; do
    case "$arg" in
        --skip-build) SKIP_BUILD=1 ;;
        -h|--help)
            sed -n '2,20p' "$0"
            exit 0
            ;;
        *) warn "Bilinmeyen argüman: $arg" ;;
    esac
done

printf "${C_BOLD}\n=== PocketEngine Termux Kurulumu ===${C_RESET}\n\n"

# Termux dışında çalıştırılırsa uyar
if [ ! -d "/data/data/com.termux" ]; then
    warn "Bu script Termux ortamı için tasarlandı. Şu an Termux dışındasınız."
    warn "Devam edilecek ama pkg komutları başarısız olabilir."
fi

# ============================================================================
# 1. Paket deposu güncelle
# ============================================================================
info "Paket deposu güncelleniyor (pkg update + upgrade)..."
pkg update -y || warn "pkg update başarısız oldu, devam ediliyor"
pkg upgrade -y || warn "pkg upgrade başarısız oldu, devam ediliyor"

# ============================================================================
# 2. Temel yapı araçları
# ============================================================================
info "Temel yapı araçları kuruluyor..."
# Termux'ta tüm paketler tek paket yöneticisinde; isimler birebir pkg kayıtlı.
pkg install -y git cmake ninja clang make pkg-config wget unzip tar || \
    die "Temel araçlar kurulamadı"

# ============================================================================
# 3. Grafik & X11 (Termux:X11 için)
# ============================================================================
info "Termux:X11 + mesa grafik stack kuruluyor..."
# termux-x11-nightly: Termux:X11 companion (X server)
# mesa: OpenGL yazılım/HW加速 sürücüleri (Termux cihaza göre)
# vulkan-tools-generic: opsiyonel, Mali/Adreno Vulkan yolu (şimdilik opsiyonel)
pkg install -y termux-x11-nightly mesa || \
    warn "Termux:X11/mesa kurulumunda sorun — devam ediliyor"
# Vulkan opsiyonel — bazı cihazlarda paket yok
pkg install -y vulkan-tools-generic 2>/dev/null || \
    warn "vulkan-tools-generic paketi yok (opsiyonel), atlanıyor"

# ============================================================================
# 4. Motor bağımlılıkları (pkg-config üzerinden bulunacak)
# ============================================================================
info "Motor bağımlılıkları kuruluyor (SDL2, OpenGL, Box2D, OpenAL, Lua, SQLite)..."
# Not: pkg isimleri Termux depo adlarıyla birebir eşleşir.
#   sdl2, sdl2-image, sdl2-ttf, sdl2-mixer   → SDL2 + yan kütüphaneler
#   opengl                                    → GL/EGL header+lib (GLESv2)
#   box2d                                     → Box2D v2.4 fizik kütüphanesi
#   openal-soft                               → OpenAL ses
#   lua5.4, lua5.4-dev                        → Lua 5.4 + header
#   sqlite, sqlite-dev                        → SQLite3 + header
pkg install -y sdl2 sdl2-image sdl2-ttf sdl2-mixer || \
    die "SDL2 paketleri kurulamadı"
pkg install -y opengl || warn "opengl paketi kurulamadı — GL headerlar eksik olabilir"
pkg install -y box2d  || warn "box2d kurulamadı — fizik modülü derlenmeyecek"
pkg install -y openal-soft || warn "openal-soft kurulamadı — ses modülü derlenmeyecek"
pkg install -y lua5.4 lua5.4-dev || warn "lua5.4 kurulamadı — script modülü derlenmeyecek"
pkg install -y sqlite sqlite-dev || warn "sqlite kurulamadı — db modülü derlenmeyecek"

# ============================================================================
# 5. ImGui kaynak (vendor) — third_party/imgui
# ============================================================================
info "ImGui kaynak kodu hazırlanıyor..."
mkdir -p third_party
cd third_party

IMGUI_TAG="v1.91.5"
if [ -d imgui ] && [ -f imgui/imgui.cpp ]; then
    ok "ImGui zaten klonlanmış, atlanıyor"
else
    info "ImGui $IMGUI_TAG klonlanıyor..."
    rm -rf imgui
    git clone --depth 1 --branch "$IMGUI_TAG" \
        https://github.com/ocornut/imgui.git \
        || die "ImGui klonlanamadı (internet bağlantısını kontrol edin)"
    ok "ImGui klonlandı"
fi

# ============================================================================
# 6. sol2 (Lua C++ binding — header-only)
# ============================================================================
SOL2_TAG="v3.3.1"
if [ -d sol2 ] && [ -f sol2/sol/sol.hpp ]; then
    ok "sol2 zaten klonlanmış, atlanıyor"
else
    info "sol2 $SOL2_TAG klonlanıyor..."
    rm -rf sol2
    git clone --depth 1 --branch "$SOL2_TAG" \
        https://github.com/ThePhD/sol2.git \
        || die "sol2 klonlanamadı"
    ok "sol2 klonlandı"
fi

cd "$PE_ROOT"

# ============================================================================
# 7. Font indir (ImGui + SDL2_ttf için Roboto-Medium.ttf)
# ============================================================================
info "Font hazırlanıyor..."
mkdir -p assets/fonts
FONT_PATH="assets/fonts/Roboto-Medium.ttf"
if [ -f "$FONT_PATH" ] && [ "$(stat -c%s "$FONT_PATH" 2>/dev/null || echo 0)" -gt 10000 ]; then
    ok "Roboto-Medium.ttf zaten mevcut, atlanıyor"
else
    # Birden fazla yedek URL dene — Google Fonts mirror'ları
    FONT_URLS=(
        "https://github.com/googlefonts/roboto-3-classic/raw/main/src/hinted/Roboto-Medium.ttf"
        "https://raw.githubusercontent.com/googlefonts/roboto/main/src/hinted/Roboto-Medium.ttf"
        "https://github.com/googlefonts/roboto/raw/main/src/hinted/Roboto-Medium.ttf"
    )
    FONT_OK=0
    for url in "${FONT_URLS[@]}"; do
        info "Font deneniyor: $url"
        if wget -q --tries=2 --timeout=20 -O "$FONT_PATH" "$url" \
           && [ "$(stat -c%s "$FONT_PATH" 2>/dev/null || echo 0)" -gt 10000 ]; then
            ok "Font indirildi"
            FONT_OK=1
            break
        fi
    done
    if [ "$FONT_OK" -eq 0 ]; then
        rm -f "$FONT_PATH"
        warn "Font indirilemedi. ImGui varsayılan fontu kullanılacak."
        warn "Elle indirip assets/fonts/Roboto-Medium.ttf olarak yerleştirin."
    fi
fi

# ============================================================================
# 8. Build
# ============================================================================
if [ "$SKIP_BUILD" -eq 1 ]; then
    warn "--skip-build verildi, derleme atlanıyor"
else
    info "CMake + Ninja ile Release derlemesi başlıyor..."
    mkdir -p build
    cd build
    # Ninja üreticiyi kullan; Termux'ta Make de çalışır ama Ninja daha hızlı
    cmake -G Ninja \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
          .. \
        || die "CMake konfigürasyonu başarısız"

    NPROC="$(nproc 2>/dev/null || echo 4)"
    info "ninja -j${NPROC} (bu birkaç dakika sürebilir)..."
    ninja -j"$NPROC" || die "Derleme başarısız"

    cd "$PE_ROOT"
    ok "Derleme tamam: build/bin/pocket_editor"
fi

# ============================================================================
# 9. Scriptleri çalıştırılabilir yap (idempotent)
# ============================================================================
info "Shell scriptleri çalıştırılabilir yapılıyor..."
chmod +x setup-termux.sh run.sh uninstall.sh 2>/dev/null || true

# ============================================================================
# Bitiş
# ============================================================================
printf "\n${C_GREEN}${C_BOLD}=== Kurulum tamam! ===${C_RESET}\n\n"
cat <<'EOF'

Sonraki adımlar:
  1. Termux:X11 uygulamasını Android'de açın (com.termux.x11).
  2. Termux'a geri dönün ve çalıştırın:
        ./run.sh
  3. Editör landscape modda Termux:X11 ekranında açılacak.

İlk kurulumdan sonra güncelleme için:
  git pull && bash setup-termux.sh --skip-build && ninja -C build

Sorun mu var? docs/TERMUX_GUIDE.md içinde troubleshooting bölümüne bakın.
EOF
