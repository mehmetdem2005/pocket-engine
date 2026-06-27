#pragma once
// PocketEngine — cloud sync config
// Render.com üzerinde çalışan PocketEngine backend'inin URL'si.
// Editör bu sunucuya auth/project sync/asset upload için bağlanır.
//
// Backend kaynak kodu: server/ dizininde (Node.js/Express).
// Deploy: https://pocket-engine-server.onrender.com (Render.com free tier)
//
// Not: Gerçek HTTP istemcisi için libcurl gereklidir (Termux: pkg install curl).
//      pocket::net::CloudSync sınıfı bu bağımlılık yüklenince aktifleşir.
#include "pocket/core/types.h"

namespace pocket::net {

// Canlı backend URL'si (Render.com)
constexpr const char* DEFAULT_SERVER_URL = "https://pocket-engine-server.onrender.com";

// API uç noktaları (göreceli path'ler)
constexpr const char* EP_AUTH_REGISTER = "/api/auth/register";
constexpr const char* EP_AUTH_VERIFY   = "/api/auth/verify";
constexpr const char* EP_PROJECTS      = "/api/projects";
constexpr const char* EP_SYNC_PUSH     = "/api/sync/push";
constexpr const char* EP_SYNC_PULL     = "/api/sync/pull";
constexpr const char* EP_ASSET_UPLOAD  = "/api/assets/upload";
constexpr const char* EP_HEALTH        = "/api/health";

// Cihaz kimliği — Termux'ta `getprop ro.serialno` veya rastgele UUID
// ilk açılışta $HOME/.pocketengine/device_id dosyasına yazılır.
String getDeviceId();

// Sunucu URL'sini override etmek için (env POCKET_SERVER_URL)
String getServerUrl();

} // namespace pocket::net
