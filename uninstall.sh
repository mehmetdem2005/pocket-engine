#!/data/data/com.termux/files/usr/bin/bash
# ============================================================================
# PocketEngine uninstall — build çıktılarını ve kurulu verileri temizler
# ----------------------------------------------------------------------------
# Kullanım:
#   ./uninstall.sh             # build/ + ~/.pocketengine verisi
#   ./uninstall.sh --all       # yukarıdakiler + third_party klonları
#   ./uninstall.sh --keep-data # build/ temizle ama kullanıcı verisini koru
# ============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

if [ -t 1 ]; then
    C_GREEN='\033[1;32m'; C_YELLOW='\033[1;33m'; C_RED='\033[1;31m'
    C_BLUE='\033[1;34m';  C_RESET='\033[0m'
else
    C_GREEN=''; C_YELLOW=''; C_RED=''; C_BLUE=''; C_RESET=''
fi
info() { printf "${C_BLUE}[*]${C_RESET} %s\n" "$*"; }
ok()   { printf "${C_GREEN}[✓]${C_RESET} %s\n" "$*"; }
warn() { printf "${C_YELLOW}[!]${C_RESET} %s\n" "$*"; }

REMOVE_THIRD_PARTY=0
KEEP_DATA=0
for arg in "$@"; do
    case "$arg" in
        --all)         REMOVE_THIRD_PARTY=1 ;;
        --keep-data)   KEEP_DATA=1 ;;
        -h|--help)
            sed -n '2,12p' "$0"
            exit 0
            ;;
        *) warn "Bilinmeyen argüman: $arg" ;;
    esac
done

printf "\n\033[1m=== PocketEngine Uninstall ===\033[0m\n\n"

# --- 1. Build çıktıları ---
if [ -d build ]; then
    info "build/ siliniyor..."
    rm -rf build
    ok "build/ silindi"
else
    info "build/ yok, atlanıyor"
fi

# --- 2. CMake cache dosyaları ---
for f in CMakeCache.txt CMakeFiles compile_commands.json; do
    if [ -e "$f" ]; then
        rm -rf "$f"
        ok "$f silindi"
    fi
done

# --- 3. Editör/canlı süreçler var mı? ---
if pgrep -f "pocket_editor" >/dev/null 2>&1; then
    warn "pocket_editor çalışıyor — kapatılıyor..."
    pkill -f "pocket_editor" 2>/dev/null || true
    sleep 1
fi

# --- 4. Kullanıcı verisi (~/.pocketengine) ---
PE_DATA_DIR="${HOME}/.pocketengine"
if [ "$KEEP_DATA" -eq 1 ]; then
    info "--keep-data verildi, $PE_DATA_DIR korunuyor"
elif [ -d "$PE_DATA_DIR" ]; then
    warn "Kullanıcı verisi siliniyor: $PE_DATA_DIR"
    warn "  (projeler, ayarlar, yerel SQLite DB — geri alınamaz!)"
    read -r -p "Devam edilsin mi? [e/H] " ans </dev/tty 2>/dev/null || ans="h"
    case "$ans" in
        e|E|y|Y)
            rm -rf "$PE_DATA_DIR"
            ok "$PE_DATA_DIR silindi"
            ;;
        *)
            info "İptal edildi, veri korundu"
            ;;
    esac
else
    info "$PE_DATA_DIR yok, atlanıyor"
fi

# --- 5. (Opsiyonel) third_party klonları ---
if [ "$REMOVE_THIRD_PARTY" -eq 1 ] && [ -d third_party ]; then
    info "--all verildi, third_party/ siliniyor..."
    warn "Bu bir sonraki kurulumda ImGui/sol2'nin yeniden klonlanmasını gerektirir."
    rm -rf third_party/imgui third_party/sol2
    # third_party/CMakeLists.txt'yi koru
    ok "third_party/imgui ve third_party/sol2 silindi"
fi

# --- 6. pkg paketleri ---
cat <<EOF

${C_YELLOW}[!]${C_RESET} Not: pkg ile kurulmuş paketler (sdl2, mesa, lua5.4, vs.)
       bu script tarafından kaldırılmaz. Tamamen temizlemek için:

           pkg uninstall -y sdl2 sdl2-image sdl2-ttf sdl2-mixer \\
                          box2d openal-soft lua5.4 sqlite mesa \\
                          termux-x11-nightly cmake ninja clang

       (Diğer projelerde de kullanıyor olabilirsiniz — dikkat edin.)

${C_GREEN}=== Uninstall tamam ===${C_RESET}
EOF
