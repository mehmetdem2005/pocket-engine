// PocketEngine — cloud config impl
#include "pocket/net/cloud_config.h"
#include "pocket/core/log.h"

#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace pocket::net {

String getDeviceId() {
    // 1. Önce $HOME/.pocketengine/device_id dosyasından oku
    const char* home = std::getenv("HOME");
    if (!home || !home[0]) home = "/tmp";
    std::filesystem::path dir = std::string(home) + "/.pocketengine";
    std::filesystem::path file = dir / "device_id";
    try { std::filesystem::create_directories(dir); } catch (...) {}

    std::ifstream ifs(file);
    if (ifs.is_open()) {
        std::stringstream ss; ss << ifs.rdbuf();
        String id = ss.str();
        if (!id.empty()) return id;
    }

    // 2. Yoksa rastgele UUID üret (zaman + rand tabanlı)
    unsigned long long seed = (unsigned long long)std::time(nullptr);
    seed ^= (unsigned long long)std::rand() << 16;
    char buf[64];
    std::snprintf(buf, sizeof(buf), "pe-%016llx", seed);

    // 3. Dosyaya yaz (kalıcı)
    std::ofstream ofs(file);
    if (ofs.is_open()) ofs << buf;
    PE_INFO("net", "Generated device id: %s", buf);
    return buf;
}

String getServerUrl() {
    const char* env = std::getenv("POCKET_SERVER_URL");
    if (env && env[0]) return String(env);
    return String(DEFAULT_SERVER_URL);
}

} // namespace pocket::net
