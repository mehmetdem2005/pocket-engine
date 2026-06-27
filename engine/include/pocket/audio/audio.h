#pragma once
// PocketEngine — OpenAL tabanlı ses motoru
// SFX için: WAV yükleyip AL buffer'a koyar, 8 source havuzu üzerinden çalar.
// Music için: Aynı şekilde WAV yükler; ayrı bir müzik source'unda döngüsel çalar.
// OGG desteği opsiyoneldir; bu sürüm yalnızca PCM WAV (RIFF) okur.
#include "pocket/core/types.h"

namespace pocket::audio {

using SoundId = u32;
using MusicId = u32;

constexpr SoundId INVALID_SOUND = 0;
constexpr MusicId INVALID_MUSIC = 0;
constexpr int   SOURCE_POOL_SIZE = 8;

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    // OpenAL device + context aç
    bool init();
    void shutdown();

    // Bir WAV dosyasını SFX buffer'a yükler. Hata durumunda INVALID_SOUND döner.
    SoundId loadSound(const char* path);

    // Bir WAV dosyasını müzik buffer'a yükler (streaming yok, tüm dosya belleğe alınır).
    MusicId loadMusic(const char* path);

    // SFX çal: havuzdan boş source alır, buffer'ı bağlar ve çalar.
    void playSound(SoundId id, float volume = 1.0f, float pitch = 1.0f, bool loop = false);

    // Müzik çal (önceki müziği keser)
    void playMusic(MusicId id, float volume = 1.0f, bool loop = true);
    void stopMusic();

    // Listener master gain
    void setMasterVolume(float v);

    // Tüm sesleri duraklat / devam ettir (source'ları pause/resume eder)
    void pauseAll();
    void resumeAll();

    // Her karede çağrılabilir: çalmayı bitirmiş source'ları serbest bırak
    void update();

    bool isInitialized() const;
private:
    struct Impl;
    Unique<Impl> impl_;
};

// Global singleton (ana thread)
AudioEngine& audio();

} // namespace pocket::audio
