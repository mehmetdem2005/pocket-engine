#!/data/data/com.termux/files/usr/bin/bash
# ============================================================================
# PocketEngine editör başlatıcı — Termux:X11 üzerinde landscape modda açar
# ----------------------------------------------------------------------------
# Kullanım:
#   ./run.sh                # editörü başlat
#   ./run.sh scene.scene    # belirli bir sahneyi yükle
#   ./run.sh --no-x11       # termux-x11'i otomatik başlatma
# ============================================================================
set -euo pipefail

# --- Kök dizin ---
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Renkli çıktı
if [ -t 1 ]; then
    C_GREEN='\033[1;32m'; C_YELLOW='\033[1;33m'; C_RED='\033[1;31m'
    C_BLUE='\033[1;34m';  C_RESET='\033[0m'
else
    C_GREEN=''; C_YELLOW=''; C_RED=''; C_BLUE=''; C_RESET=''
fi
info() { printf "${C_BLUE}[*]${C_RESET} %s\n" "$*"; }
ok()   { printf "${C_GREEN}[✓]${C_RESET} %s\n" "$*"; }
warn() { printf "${C_YELLOW}[!]${C_RESET} %s\n" "$*"; }
die()  { printf "${C_RED}[✗]${C_RESET} %s\n" "$*" >&2; exit 1; }

AUTO_X11=1
for arg in "$@"; do
    case "$arg" in
        --no-x11) AUTO_X11=0; shift ;;
        -h|--help)
            sed -n '2,12p' "$0"
            exit 0
            ;;
    esac
done

# ============================================================================
# 1. Derlenmiş mi kontrol et
# ============================================================================
EDITOR_BIN="build/bin/pocket_editor"
if [ ! -x "$EDITOR_BIN" ]; then
    die "Editör binary'si yok: $EDITOR_BIN
Önce kurulumu çalıştırın: bash setup-termux.sh"
fi

# ============================================================================
# 2. DISPLAY ayarla (Termux:X11 her zaman :0 kullanır)
# ============================================================================
export DISPLAY="${DISPLAY:-:0}"
info "DISPLAY=$DISPLAY"

# ============================================================================
# 3. Landscape zorla (bazı SDL sürümleri bunu okur)
# ============================================================================
export POCKET_LANDSCAPE=1
export SDL_VIDEO_X11_FORCE_LANDSCAPE=1
# SDL'in oryantasyon ipucu (window.cpp içinde de SDL_HINT_ORIENTATIONS set ediliyor)
export SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS=0

# ============================================================================
# 4. Termux:X11 sunucusunu kontrol et, gerekirse başlat
# ============================================================================
if [ "$AUTO_X11" -eq 1 ]; then
    # pgrep ile termux-x11 süreci var mı?
    if pgrep -f "termux-x11" >/dev/null 2>&1; then
        ok "termux-x11 zaten çalışıyor"
    else
        info "termux-x11 başlatılıyor (com.termux.x11 paketini açın)..."
        # termux-x11-nightly paketinin sağladığı başlatıcı
        if command -v termux-x11 >/dev/null 2>&1; then
            # Arka planda başlat — çıktıyı logla
            (termux-x11 :0 >/tmp/termux-x11.log 2>&1 &) || \
                warn "termux-x11 başlatılamadı, elle açmayı deneyin"
            # X server'ın hazır olması için kısa bekle
            sleep 2
        else
            warn "termux-x11 komutu bulunamadı. 'pkg install termux-x11-nightly' ile kurun."
            warn "Devam edilecek — DISPLAY'i elle açtığınız Termux:X11'e yönlendirin."
        fi
        # Cihazda Termux:X11 uygulamasını açmak için kullanıcıya hatırlat
        warn "Android'de Termux:X11 uygulamasını açın (siyah ekran görünene kadar bekleyin)."
    fi
fi

# ============================================================================
# 5. X sunucusunun hazır olduğunu doğrula (xdpyinfo varsa)
# ============================================================================
if command -v xdpyinfo >/dev/null 2>&1; then
    if ! xdpyinfo -display "$DISPLAY" >/dev/null 2>&1; then
        warn "X sunucusu henüz yanıt vermiyor ($DISPLAY).
Termux:X11 uygulamasını açtıktan sonra 'bash run.sh' komutunu tekrar çalıştırın."
    fi
fi

# ============================================================================
# 6. Editörü başlat (exec — shell'in yerini alır, sinyaller editöre gider)
# ============================================================================
info "Editör başlatılıyor: $EDITOR_BIN $*"
exec "$EDITOR_BIN" "$@"
