// PocketEngine — asset manager implementasyonu
// NOTE: <cstring> once include ediliyor cunku asset_manager.h -> math.h
// chain'i std::memset kullaniyor ama math.h <cstring> include etmiyor.
// (Baska modullere dokunma politikasi geregi workaround burada.)
#include <cstring>

#include "pocket/asset/asset_manager.h"
#include "pocket/thread/thread_pool.h"
#include "pocket/core/log.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <system_error>
#include <utility>

namespace pocket::asset {

namespace {

constexpr const char* kTag = "asset";

String toLower(String s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

String extOf(const String& path) {
    auto pos = path.find_last_of('.');
    if (pos == String::npos) return "";
    return toLower(path.substr(pos));
}

String nameOf(const String& path) {
    std::filesystem::path p(path);
    return p.stem().string();
}

} // namespace

AssetManager& assets() {
    static AssetManager s;
    return s;
}

void AssetManager::registerLoader(AssetType type, LoaderFn load, UnloaderFn unload) {
    std::lock_guard<std::mutex> lk(mtx_);
    loaders_[(int)type] = {std::move(load), std::move(unload)};
    PE_INFO(kTag, "loader kaydedildi: type=%d", (int)type);
}

AssetType AssetManager::detectType(const char* path) const {
    if (!path) return AssetType::Unknown;
    String ext = extOf(path);
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
        ext == ".bmp" || ext == ".tga" || ext == ".webp" || ext == ".gif") {
        return AssetType::Texture;
    }
    if (ext == ".wav" || ext == ".ogg" || ext == ".mp3" || ext == ".flac") {
        return AssetType::Audio;
    }
    if (ext == ".lua") {
        return AssetType::Script;
    }
    if (ext == ".scene" || ext == ".scn") {
        return AssetType::Scene;
    }
    if (ext == ".ttf" || ext == ".otf") {
        return AssetType::Font;
    }
    return AssetType::Unknown;
}

void AssetManager::scanDirectory(const char* dir) {
    if (!dir || !dir[0]) {
        PE_WARN(kTag, "scanDirectory: bos yol");
        return;
    }
    std::error_code ec;
    std::filesystem::path root(dir);
    if (!std::filesystem::exists(root, ec) ||
        !std::filesystem::is_directory(root, ec)) {
        PE_WARN(kTag, "scanDirectory: yok veya dizin degil: %s", dir);
        return;
    }

    size_t found = 0;
    std::lock_guard<std::mutex> lk(mtx_);
    for (auto it = std::filesystem::recursive_directory_iterator(
             root, std::filesystem::directory_options::skip_permission_denied, ec);
         it != std::filesystem::recursive_directory_iterator(); ++it) {
        if (ec) { ec.clear(); continue; }
        const auto& entry = *it;
        if (!entry.is_regular_file()) continue;

        String p = entry.path().string();
        AssetType t = detectType(p.c_str());
        if (t == AssetType::Unknown) continue;

        AssetMeta meta;
        meta.path = p;
        meta.name = nameOf(p);
        meta.type = t;
        std::error_code sec;
        auto sz = std::filesystem::file_size(entry, sec);
        meta.sizeBytes = sec ? 0 : (u64)sz;
        meta.loaded    = false;
        meta.loading   = false;
        meta.handle    = 0;

        // Eger daha once yuklenmisse, handle koru.
        auto existing = assets_.find(p);
        if (existing != assets_.end() && existing->second.loaded) {
            meta.loaded = true;
            meta.handle = existing->second.handle;
        }
        assets_[p] = std::move(meta);
        ++found;
    }
    cacheDirty_ = true;
    PE_INFO(kTag, "scanDirectory(%s): %zu asset bulundu", dir, found);
}

AssetMeta* AssetManager::load(const char* path) {
    if (!path || !path[0]) return nullptr;
    String p = path;
    AssetType type = detectType(p.c_str());

    // Oncelikle loader var mi kontrol et (lock altinda).
    Loader loader{nullptr, nullptr};
    bool alreadyLoaded = false;
    bool loading = false;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = assets_.find(p);
        if (it != assets_.end()) {
            if (it->second.loaded) {
                return &it->second;
            }
            loading = it->second.loading;
        }
        auto lit = loaders_.find((int)type);
        if (lit == loaders_.end() || !lit->second.load) {
            PE_ERROR(kTag, "load: type %d icin loader yok (%s)", (int)type, p.c_str());
            return nullptr;
        }
        loader = lit->second;
    }

    if (loading) {
        PE_WARN(kTag, "load: %s zaten async yukleniyor, senkron load atlandi", p.c_str());
        return nullptr;
    }

    // Loader cagir (lock disinda; GPU isleri olabilir ama caller main thread'de
    // olmali — bu yuzden load() editor main thread'den cagrilmali).
    u64 h = loader.load(p.c_str(), type);

    std::lock_guard<std::mutex> lk(mtx_);
    auto& meta = assets_[p];
    if (meta.path.empty()) {
        meta.path = p;
        meta.name = nameOf(p);
        meta.type = type;
    }
    meta.handle  = h;
    meta.loaded  = (h != 0);
    meta.loading = false;
    if (h == 0) {
        PE_ERROR(kTag, "load: loader 0 dondurdu: %s", p.c_str());
        return &meta;  // yine de meta don (loaded=false)
    }
    cacheDirty_ = true;
    return &meta;
}

void AssetManager::unload(const char* path) {
    if (!path) return;
    String p = path;
    UnloaderFn unloadFn = nullptr;
    u64 h = 0;
    AssetType t = AssetType::Unknown;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = assets_.find(p);
        if (it == assets_.end()) return;
        if (!it->second.loaded) return;
        h = it->second.handle;
        t = it->second.type;
        auto lit = loaders_.find((int)t);
        if (lit != loaders_.end()) unloadFn = lit->second.unload;
        it->second.loaded = false;
        it->second.handle = 0;
        it->second.loading = false;
        cacheDirty_ = true;
    }
    if (unloadFn && h != 0) {
        unloadFn(h, t);
    }
}

void AssetManager::unloadAll() {
    // Once tum handler'lari topla, lock disinda unload cagir.
    struct Item { u64 h; AssetType t; UnloaderFn fn; };
    Vector<Item> items;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        items.reserve(assets_.size());
        for (auto& kv : assets_) {
            AssetMeta& m = kv.second;
            if (m.loaded && m.handle != 0) {
                auto lit = loaders_.find((int)m.type);
                if (lit != loaders_.end() && lit->second.unload) {
                    items.push_back({m.handle, m.type, lit->second.unload});
                }
                m.loaded  = false;
                m.handle  = 0;
                m.loading = false;
            } else {
                m.loaded  = false;
                m.loading = false;
                m.handle  = 0;
            }
        }
        cacheDirty_ = true;
    }
    for (auto& it : items) {
        if (it.fn) it.fn(it.h, it.t);
    }
    PE_INFO(kTag, "unloadAll: %zu asset bosaltildi", items.size());
}

bool AssetManager::isLoaded(const char* path) const {
    if (!path) return false;
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = assets_.find(path);
    return it != assets_.end() && it->second.loaded;
}

u64 AssetManager::handle(const char* path) const {
    if (!path) return 0;
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = assets_.find(path);
    if (it == assets_.end()) return 0;
    return it->second.handle;
}

const Vector<AssetMeta>& AssetManager::all() const {
    std::lock_guard<std::mutex> lk(mtx_);
    if (cacheDirty_) {
        cachedAll_.clear();
        cachedAll_.reserve(assets_.size());
        for (const auto& kv : assets_) {
            cachedAll_.push_back(kv.second);
        }
        cacheDirty_ = false;
    }
    return cachedAll_;
}

void AssetManager::loadAsync(const char* path,
                             std::function<void(AssetMeta&)> onComplete) {
    if (!path || !path[0]) return;
    String p = path;
    AssetType type = detectType(p.c_str());

    {
        std::lock_guard<std::mutex> lk(mtx_);
        auto& meta = assets_[p];
        if (meta.path.empty()) {
            meta.path = p;
            meta.name = nameOf(p);
            meta.type = type;
        }
        if (meta.loaded) {
            // Zaten yuklu: callback'i main-thread pump'ta cagir ki UI thread
            // disinda asset yuklemeyelim.
            pendingUploads_.push({p, std::move(onComplete)});
            return;
        }
        if (meta.loading) {
            // Zaten kuyrukta: callback'i sona ekle, ilk yukleme bitince cagrilir.
            pendingUploads_.push({p, std::move(onComplete)});
            return;
        }
        meta.loading = true;
        cacheDirty_ = true;
    }

    // Worker: dosyayi stat et, sonra pending kuyruguna push.
    // GPU upload main-thread pump'ta yapilacak.
    auto& pool = thread::ThreadPool::instance();
    pool.submit([this, p, onComplete = std::move(onComplete)]() mutable {
        u64 sz = 0;
        std::error_code ec;
        if (std::filesystem::exists(p, ec) &&
            std::filesystem::is_regular_file(p, ec)) {
            auto s = std::filesystem::file_size(p, ec);
            sz = ec ? 0 : (u64)s;
        } else {
            PE_WARN(kTag, "loadAsync: dosya yok: %s", p.c_str());
        }
        {
            std::lock_guard<std::mutex> lk(mtx_);
            auto it = assets_.find(p);
            if (it != assets_.end()) {
                it->second.sizeBytes = sz;
            }
            pendingUploads_.push({p, std::move(onComplete)});
        }
    });
}

void AssetManager::pumpPendingUploads(size_t maxItems) {
    std::queue<PendingUpload> local;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (pendingUploads_.empty()) return;
        size_t n = (maxItems == 0) ? pendingUploads_.size()
                                   : std::min(maxItems, pendingUploads_.size());
        for (size_t i = 0; i < n && !pendingUploads_.empty(); ++i) {
            local.push(std::move(pendingUploads_.front()));
            pendingUploads_.pop();
        }
    }

    // Her item icin: gerekirse loader cagir (GPU upload, main-thread),
    // sonra onComplete callback calistir.
    struct Cb { AssetMeta* meta; std::function<void(AssetMeta&)> fn; };
    Vector<Cb> callbacks;
    callbacks.reserve(local.size());

    {
        std::lock_guard<std::mutex> lk(mtx_);
        while (!local.empty()) {
            PendingUpload pu = std::move(local.front());
            local.pop();

            auto it = assets_.find(pu.path);
            if (it == assets_.end()) {
                PE_WARN(kTag, "pump: asset kaybolmus: %s", pu.path.c_str());
                continue;
            }
            AssetMeta& meta = it->second;

            // Ilk defa yuklenecekse loader cagir (GPU upload main-thread).
            if (!meta.loaded) {
                auto lit = loaders_.find((int)meta.type);
                if (lit == loaders_.end() || !lit->second.load) {
                    PE_ERROR(kTag, "pump: loader yok type=%d (%s)",
                             (int)meta.type, pu.path.c_str());
                    meta.loading = false;
                    if (pu.onComplete) callbacks.push_back({&meta, std::move(pu.onComplete)});
                    continue;
                }
                u64 h = lit->second.load(pu.path.c_str(), meta.type);
                meta.handle  = h;
                meta.loaded  = (h != 0);
                meta.loading = false;
                cacheDirty_  = true;
            } else {
                // Zaten yuklu (birden fazla callback ayni asset icin): sadece
                // loading flag'ini kaldir, callback'i cagir.
                meta.loading = false;
            }
            if (pu.onComplete) {
                callbacks.push_back({&meta, std::move(pu.onComplete)});
            }
        }
    }

    // Callback'leri lock disinda cagir (callback icinde tekrar asset API
    // cagrilabilir).
    for (auto& c : callbacks) {
        c.fn(*c.meta);
    }
}

size_t AssetManager::pendingUploadCount() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return pendingUploads_.size();
}

} // namespace pocket::asset
