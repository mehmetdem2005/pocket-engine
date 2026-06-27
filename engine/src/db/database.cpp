// PocketEngine — SQLite veri katmani implementasyonu
#include "pocket/db/database.h"
#include "pocket/core/log.h"

#include <sqlite3.h>

#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>

namespace pocket::db {

namespace {

constexpr const char* kTag = "db";

// SQLite hata kodunu logla ve false don.
bool logErr(sqlite3* db, const char* what) {
    PE_ERROR(kTag, "%s: %s (code=%d)", what, db ? sqlite3_errmsg(db) : "no db",
             db ? sqlite3_errcode(db) : 0);
    return false;
}

// Su anki zamani "YYYY-MM-DD HH:MM:SS" formatinda dondur.
String nowIso() {
    std::time_t t = std::time(nullptr);
    std::tm tmv{};
    localtime_r(&t, &tmv);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                  tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
                  tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
    return String(buf);
}

// Varsayilan DB yolu: $HOME/.pocketengine/projects.db
String defaultPath() {
    const char* home = std::getenv("HOME");
    if (!home || home[0] == '\0') home = ".";
    std::filesystem::path dir = std::string(home) + "/.pocketengine";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec); // hata olsa bile devam et
    return (dir / "projects.db").string();
}

// Tek satirlik exec (parametre yok) — schema olusturma gibi.
bool execSimple(sqlite3* db, const char* sql) {
    char* err = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        PE_ERROR(kTag, "exec failed: %s — sql: %s", err ? err : "?", sql);
        if (err) sqlite3_free(err);
        return false;
    }
    return true;
}

} // namespace

Database::~Database() {
    close();
}

bool Database::open(const char* path) {
    if (db_) {
        PE_WARN(kTag, "open: zaten acik, once close() cagrilmali");
        close();
    }

    String p = (path && path[0]) ? String(path) : defaultPath();
    // sqlite3_open path'in parent dizinini olusturmaz; biz olusturduk.
    int rc = sqlite3_open(p.c_str(), &db_);
    if (rc != SQLITE_OK) {
        logErr(db_, "sqlite3_open");
        // aciksa kapat
        if (db_) { sqlite3_close(db_); db_ = nullptr; }
        return false;
    }

    // Performans: WAL + normal sync.
    execSimple(db_, "PRAGMA journal_mode=WAL;");
    execSimple(db_, "PRAGMA synchronous=NORMAL;");
    execSimple(db_, "PRAGMA foreign_keys=ON;");

    PE_INFO(kTag, "DB acildi: %s", p.c_str());
    return true;
}

void Database::close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
        PE_DEBUG(kTag, "DB kapatildi");
    }
}

bool Database::initSchema() {
    if (!db_) {
        PE_ERROR(kTag, "initSchema: DB acik degil");
        return false;
    }

    const char* sql =
        "CREATE TABLE IF NOT EXISTS projects ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name TEXT NOT NULL,"
        "  path TEXT NOT NULL,"
        "  created_at TEXT NOT NULL,"
        "  updated_at TEXT NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS settings ("
        "  key TEXT PRIMARY KEY,"
        "  value TEXT NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS assets ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  project TEXT NOT NULL,"
        "  path TEXT NOT NULL,"
        "  type TEXT NOT NULL"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_assets_project ON assets(project);";

    if (!execSimple(db_, sql)) return false;
    PE_INFO(kTag, "schema hazir");
    return true;
}

// ---- Projeler ----

bool Database::createProject(const String& name, const String& path, ProjectRecord& out) {
    if (!db_) return false;

    const char* sql = "INSERT INTO projects(name, path, created_at, updated_at) VALUES(?,?,?,?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return logErr(db_, "createProject: prepare");

    String ts = nowIso();
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, ts.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, ts.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    i64 id = sqlite3_last_insert_rowid(db_);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return logErr(db_, "createProject: step");

    out.id        = id;
    out.name      = name;
    out.path      = path;
    out.createdAt = ts;
    out.updatedAt = ts;
    return true;
}

bool Database::listProjects(Vector<ProjectRecord>& out) {
    out.clear();
    if (!db_) return false;

    const char* sql = "SELECT id, name, path, created_at, updated_at FROM projects ORDER BY id;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return logErr(db_, "listProjects: prepare");

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ProjectRecord r;
        r.id        = sqlite3_column_int64(stmt, 0);
        const unsigned char* n = sqlite3_column_text(stmt, 1);
        const unsigned char* p = sqlite3_column_text(stmt, 2);
        const unsigned char* c = sqlite3_column_text(stmt, 3);
        const unsigned char* u = sqlite3_column_text(stmt, 4);
        r.name      = n ? reinterpret_cast<const char*>(n) : "";
        r.path      = p ? reinterpret_cast<const char*>(p) : "";
        r.createdAt = c ? reinterpret_cast<const char*>(c) : "";
        r.updatedAt = u ? reinterpret_cast<const char*>(u) : "";
        out.push_back(std::move(r));
    }
    sqlite3_finalize(stmt);
    return true;
}

bool Database::deleteProject(i64 id) {
    if (!db_) return false;
    const char* sql = "DELETE FROM projects WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return logErr(db_, "deleteProject: prepare");
    sqlite3_bind_int64(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return logErr(db_, "deleteProject: step");
    return true;
}

bool Database::updateProject(i64 id, const String& name) {
    if (!db_) return false;
    const char* sql = "UPDATE projects SET name=?, updated_at=? WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return logErr(db_, "updateProject: prepare");

    String ts = nowIso();
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, ts.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return logErr(db_, "updateProject: step");
    return true;
}

// ---- Ayarlar ----

String Database::getSetting(const String& key, const String& fallback) {
    if (!db_) return fallback;
    const char* sql = "SELECT value FROM settings WHERE key=?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        logErr(db_, "getSetting: prepare");
        return fallback;
    }
    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    String result = fallback;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* v = sqlite3_column_text(stmt, 0);
        if (v) result = reinterpret_cast<const char*>(v);
    }
    sqlite3_finalize(stmt);
    return result;
}

bool Database::setSetting(const String& key, const String& value) {
    if (!db_) return false;
    const char* sql = "INSERT INTO settings(key, value) VALUES(?,?) "
                      "ON CONFLICT(key) DO UPDATE SET value=excluded.value;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return logErr(db_, "setSetting: prepare");
    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return logErr(db_, "setSetting: step");
    return true;
}

// ---- Asset manifest ----

bool Database::addAsset(const String& project, const String& path, const String& type) {
    if (!db_) return false;
    const char* sql = "INSERT INTO assets(project, path, type) VALUES(?,?,?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return logErr(db_, "addAsset: prepare");
    sqlite3_bind_text(stmt, 1, project.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, type.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return logErr(db_, "addAsset: step");
    return true;
}

bool Database::listAssets(const String& project, Vector<AssetRow>& out) {
    out.clear();
    if (!db_) return false;
    const char* sql = "SELECT id, path, type, project FROM assets WHERE project=? ORDER BY id;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return logErr(db_, "listAssets: prepare");
    sqlite3_bind_text(stmt, 1, project.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        AssetRow r;
        r.id = sqlite3_column_int64(stmt, 0);
        const unsigned char* p = sqlite3_column_text(stmt, 1);
        const unsigned char* t = sqlite3_column_text(stmt, 2);
        const unsigned char* pr = sqlite3_column_text(stmt, 3);
        r.path    = p ? reinterpret_cast<const char*>(p) : "";
        r.type    = t ? reinterpret_cast<const char*>(t) : "";
        r.project = pr ? reinterpret_cast<const char*>(pr) : "";
        out.push_back(std::move(r));
    }
    sqlite3_finalize(stmt);
    return true;
}

// ---- Singleton ----
Database& database() {
    static Database s;
    return s;
}

} // namespace pocket::db
