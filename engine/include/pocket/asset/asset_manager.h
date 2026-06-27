#pragma once
// PocketEngine — asset manager (metadata + path yonetimi)
//
// AssetManager sadece METADATA + dosya yollarini yonetir. Gercek GPU upload
// (texture) veya ses buffer yukleme isleri, modul bazli register edilen
// "loader" callback'leri araciligiyla yapilir. Boylece render/audio modulleri
// ile dairesel bagimlilik onlenir.
//
// Async akisi (onemli):
//   - loadAsync(path) bir worker thread'de dosya boyutunu/stat'ini yapar,
//     sonra gercek yuklemeyi "pending upload" kuyruguna atar.
//   - GPU upload main-thread'te yapilmak zorundadir (OpenGL context
//     main-thread-only). Bu yuzden editor main loop her karede
//     pumpPendingUploads() cagirmalidir. Bu metod pending kuyrugundaki
//     isleri register edilen loader callback'leri uzerinden gerceklestirir,
//     onComplete callback'lerini cagirir.
//
//   Ornek akis:
//     assets().registerLoader(AssetType::Texture, myTexLoader, myTexUnloader);
//     assets().loadAsync("assets/player.png", [](AssetMeta& m){ ... });
//     // ... her kare ...
//     assets().pumpPendingUploads();
#include "pocket/core/types.h"
#include "pocket/math/math.h"

#include <functional>
#include <mutex>
#include <queue>
#include <utility>

namespace pocket::asset {

enum class AssetType : int {
    Texture = 0,
    Audio,
    Script,
    Scene,
    Font,
    Unknown
};

struct AssetMeta {
    String    path;
    String    name;
    AssetType type        = AssetType::Unknown;
    u64       sizeBytes   = 0;
    u64       handle      = 0;   // modul-bazli handle (texture id, sound id...)
    bool      loaded      = false;
    bool      loading     = false;
};

class AssetManager {
public:
    using LoaderFn   = std::function<u64(const char* path, AssetType type)>;
    using UnloaderFn = std::function<void(u64 handle, AssetType type)>;

    // Bir asset tipi icin loader/unloader kaydet. Render/audio modulleri
    // init sirasinda cagirir. Ayni tip icin tekrar kayit ustune yazar.
    void registerLoader(AssetType type, LoaderFn load, UnloaderFn unload);

    // Bir dizini recursive tara; her dosya icin AssetMeta olusturur
    // (yuklemez, sadece metadata). Mevcut kayitlar uzerine yazar.
    void scanDirectory(const char* dir);

    // Senkron yukleme. Loader cagirir, handle set eder. AssetMeta* doner
    // (HashMap referans stabilitesi sayesinde sonra eklenen kayitlar
    // pointer'i gecersiz kilmaz; ama unload() sonrasi kullanma).
    AssetMeta* load(const char* path);

    // Tek bir asseti bosalt.
    void unload(const char* path);
    // Tum assetleri bosalt.
    void unloadAll();

    bool isLoaded(const char* path) const;
    u64  handle(const char* path) const;

    // Tum assetlerin snapshot kopyasi. (Mutasyon olunca lazy rebuild.)
    const Vector<AssetMeta>& all() const;

    // Async yukleme. Worker dosya stat'ini yapar, sonra main-thread
    // upload kuyruguna atar. Editor her kare pumpPendingUploads() cagirmali.
    void loadAsync(const char* path,
                   std::function<void(AssetMeta&)> onComplete = {});

    // Main-thread pump: pending uploadlari isle, loader'lari cagir,
    // onComplete callback'leri calistir. maxItems==0 => hepsini bosalt.
    // Editor main loop her karede cagirmali (Texture icin sart).
    void pumpPendingUploads(size_t maxItems = 0);

    // Dosya uzantisindan asset tipini tahmin et.
    AssetType detectType(const char* path) const;

    // Pending kuyrugunda bekleyen upload sayisi (debug/diagnostic).
    size_t pendingUploadCount() const;

private:
    HashMap<String, AssetMeta> assets_;
    struct Loader { LoaderFn load; UnloaderFn unload; };
    HashMap<int, Loader> loaders_;   // keyed by (int)AssetType

    struct PendingUpload {
        String path;
        std::function<void(AssetMeta&)> onComplete;
    };
    // Worker'lar main-thread pump bekleyen uploadlari buraya push eder.
    std::queue<PendingUpload> pendingUploads_;

    // `all()` icin lazy cache.
    mutable Vector<AssetMeta> cachedAll_;
    mutable bool              cacheDirty_ = true;
    mutable std::mutex        mtx_;
};

// Global singleton.
AssetManager& assets();

} // namespace pocket::asset
