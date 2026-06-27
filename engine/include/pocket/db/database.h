#pragma once
// PocketEngine — SQLite veri katmani
// Proje kayitlari, ayarlar (key-value) ve asset manifest'i yonetir.
// Varsayilan DB konumu: $HOME/.pocketengine/projects.db
#include "pocket/core/types.h"

struct sqlite3;

namespace pocket::db {

// ---- Proje kaydi ----
struct ProjectRecord {
    i64 id = 0;
    String name;
    String path;          // dosya sistemi yolu
    String createdAt;     // ISO "YYYY-MM-DD HH:MM:SS"
    String updatedAt;
};

class Database {
public:
    Database() = default;
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // path == nullptr ise $HOME/.pocketengine/projects.db kullanilir.
    // Gerekli dizin(.pocketengine) yoksa olusturulur.
    bool open(const char* path);
    void close();

    // Tablolari yoksa olusturur: projects, settings, assets
    bool initSchema();

    // ---- Projeler ----
    bool createProject(const String& name, const String& path, ProjectRecord& out);
    bool listProjects(Vector<ProjectRecord>& out);
    bool deleteProject(i64 id);
    bool updateProject(i64 id, const String& name);

    // ---- Ayarlar (key-value) ----
    String getSetting(const String& key, const String& fallback = "");
    bool   setSetting(const String& key, const String& value);

    // ---- Asset manifest ----
    struct AssetRow {
        i64    id = 0;
        String path;
        String type;
        String project;
    };
    bool addAsset(const String& project, const String& path, const String& type);
    bool listAssets(const String& project, Vector<AssetRow>& out);

    // Ham sqlite3 handle (ileri duzey kullanim icin; dikkatli ol).
    sqlite3* raw() { return db_; }
    bool     isOpen() const { return db_ != nullptr; }

private:
    sqlite3* db_ = nullptr;
};

// Global singleton (editorden kolay erisim).
Database& database();

} // namespace pocket::db
