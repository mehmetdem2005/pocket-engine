// PocketEngine — OpenAL ses motoru uygulaması
// Manuel RIFF/WAV parser + 8 source havuzu.
#include "pocket/audio/audio.h"
#include "pocket/core/log.h"

#include <AL/al.h>
#include <AL/alc.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace pocket::audio {

// ---- WAV parser (RIFF/PCM) ----
struct WavData {
    std::vector<u8> samples;
    ALenum  format = 0;
    ALsizei freq   = 0;
    bool    ok     = false;
};

static u32 le32(const u8* p) {
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}
static u16 le16(const u8* p) {
    return (u16)((u16)p[0] | ((u16)p[1] << 8));
}

static WavData loadWav(const char* path) {
    WavData w;
    FILE* fp = std::fopen(path, "rb");
    if (!fp) {
        PE_ERROR("audio", "WAV açılamadı: %s", path);
        return w;
    }

    u8 header[12];
    if (std::fread(header, 1, 12, fp) != 12) { std::fclose(fp); return w; }
    if (std::memcmp(header, "RIFF", 4) != 0 || std::memcmp(header + 8, "WAVE", 4) != 0) {
        PE_ERROR("audio", "WAV imzası hatalı: %s", path);
        std::fclose(fp);
        return w;
    }

    u16 audioFormat = 0, channels = 0, bitsPerSample = 0;
    u32 sampleRate = 0;
    bool haveFmt = false, haveData = false;

    // Chunk'ları tara
    while (!haveData) {
        u8 chunkHdr[8];
        if (std::fread(chunkHdr, 1, 8, fp) != 8) break;
        u32 chunkSize = le32(chunkHdr + 4);

        if (std::memcmp(chunkHdr, "fmt ", 4) == 0) {
            u8 fmt[16];
            u32 toRead = chunkSize < 16 ? chunkSize : 16;
            if (std::fread(fmt, 1, toRead, fp) != toRead) break;
            if (chunkSize > toRead) std::fseek(fp, chunkSize - toRead, SEEK_CUR);
            // Pad to even
            if (chunkSize & 1) std::fseek(fp, 1, SEEK_CUR);

            audioFormat   = le16(fmt + 0);
            channels      = le16(fmt + 2);
            sampleRate    = le32(fmt + 4);
            bitsPerSample = le16(fmt + 14);
            haveFmt = true;
        } else if (std::memcmp(chunkHdr, "data", 4) == 0) {
            if (!haveFmt) {
                PE_ERROR("audio", "WAV: fmt chunk'tan önce data: %s", path);
                break;
            }
            w.samples.resize(chunkSize);
            if (std::fread(w.samples.data(), 1, chunkSize, fp) != chunkSize) {
                PE_ERROR("audio", "WAV data okunamadı: %s", path);
                break;
            }
            haveData = true;
        } else {
            // Bilinmeyen chunk: atla
            std::fseek(fp, chunkSize + (chunkSize & 1), SEEK_CUR);
        }
    }
    std::fclose(fp);

    if (!haveData) {
        PE_ERROR("audio", "WAV data chunk yok: %s", path);
        return w;
    }
    if (audioFormat != 1) {
        PE_ERROR("audio", "WAV desteklenmeyen format (PCM değil): %s audioFormat=%u", path, audioFormat);
        return w;
    }

    // OpenAL formatını belirle
    if (channels == 1) {
        w.format = (bitsPerSample == 8)  ? AL_FORMAT_MONO8
                 : (bitsPerSample == 16) ? AL_FORMAT_MONO16 : 0;
    } else if (channels == 2) {
        w.format = (bitsPerSample == 8)  ? AL_FORMAT_STEREO8
                 : (bitsPerSample == 16) ? AL_FORMAT_STEREO16 : 0;
    }
    if (w.format == 0) {
        PE_ERROR("audio", "WAV desteklenmeyen kanal/bit: %s ch=%u bps=%u", path, channels, bitsPerSample);
        return w;
    }
    w.freq = (ALsizei)sampleRate;
    w.ok   = true;
    return w;
}

// ---- Impl ----
struct AudioEngine::Impl {
    ALCdevice*  device  = nullptr;
    ALCcontext* context = nullptr;

    // SFX buffer havuzu (id -> AL buffer)
    Vector<ALuint> soundBuffers;
    // Music buffer'ları
    Vector<ALuint> musicBuffers;

    // 8 source havuzu + boşta mı?
    ALuint  sources[SOURCE_POOL_SIZE];
    bool    sourceBusy[SOURCE_POOL_SIZE];

    // Müzik source'u (döngüsel, sabit)
    ALuint  musicSource = 0;
    bool    musicPlaying = false;

    float   masterVolume = 1.0f;
    bool    paused = false;
};

// ---- Ctor/Dtor ----
AudioEngine::AudioEngine() : impl_(nullptr) {}
AudioEngine::~AudioEngine() { shutdown(); }

// ---- init ----
bool AudioEngine::init() {
    if (impl_) shutdown();
    impl_ = makeUnique<Impl>();

    impl_->device = alcOpenDevice(nullptr);
    if (!impl_->device) {
        PE_ERROR("audio", "alcOpenDevice failed");
        impl_.reset();
        return false;
    }
    impl_->context = alcCreateContext(impl_->device, nullptr);
    if (!impl_->context || !alcMakeContextCurrent(impl_->context)) {
        PE_ERROR("audio", "alcCreateContext/MakeCurrent failed");
        if (impl_->context) alcDestroyContext(impl_->context);
        alcCloseDevice(impl_->device);
        impl_.reset();
        return false;
    }

    // Source havuzu
    alGenSources(SOURCE_POOL_SIZE, impl_->sources);
    for (int i = 0; i < SOURCE_POOL_SIZE; ++i) impl_->sourceBusy[i] = false;

    // Müzik source
    alGenSources(1, &impl_->musicSource);
    alSourcef(impl_->musicSource, AL_GAIN, 1.0f);

    alListenerf(AL_GAIN, impl_->masterVolume);

    PE_INFO("audio", "AudioEngine init: device=%s",
            alcGetString(impl_->device, ALC_DEVICE_SPECIFIER));
    return true;
}

// ---- shutdown ----
void AudioEngine::shutdown() {
    if (!impl_) return;
    // Çalan source'ları durdur
    for (int i = 0; i < SOURCE_POOL_SIZE; ++i) {
        if (impl_->sources[i]) {
            alSourceStop(impl_->sources[i]);
            alSourcei(impl_->sources[i], AL_BUFFER, 0);
        }
    }
    if (impl_->musicSource) {
        alSourceStop(impl_->musicSource);
        alSourcei(impl_->musicSource, AL_BUFFER, 0);
        alDeleteSources(1, &impl_->musicSource);
        impl_->musicSource = 0;
    }
    if (impl_->sources[0]) {
        alDeleteSources(SOURCE_POOL_SIZE, impl_->sources);
    }
    // Buffer'ları sil
    for (ALuint b : impl_->soundBuffers) if (b) alDeleteBuffers(1, &b);
    for (ALuint b : impl_->musicBuffers)  if (b) alDeleteBuffers(1, &b);
    impl_->soundBuffers.clear();
    impl_->musicBuffers.clear();

    alcMakeContextCurrent(nullptr);
    if (impl_->context) alcDestroyContext(impl_->context);
    if (impl_->device)  alcCloseDevice(impl_->device);
    impl_.reset();
    PE_INFO("audio", "AudioEngine shutdown");
}

bool AudioEngine::isInitialized() const { return impl_ && impl_->context; }

// ---- loadSound ----
SoundId AudioEngine::loadSound(const char* path) {
    if (!isInitialized()) return INVALID_SOUND;
    WavData w = loadWav(path);
    if (!w.ok) return INVALID_SOUND;

    ALuint buf = 0;
    alGenBuffers(1, &buf);
    alBufferData(buf, w.format, w.samples.data(),
                 (ALsizei)w.samples.size(), w.freq);
    if (alGetError() != AL_NO_ERROR) {
        PE_ERROR("audio", "alBufferData failed: %s", path);
        alDeleteBuffers(1, &buf);
        return INVALID_SOUND;
    }
    impl_->soundBuffers.push_back(buf);
    SoundId id = (SoundId)impl_->soundBuffers.size(); // 1-tabanlı
    PE_INFO("audio", "Sound loaded: %s -> id=%u (buf=%u)", path, id, buf);
    return id;
}

// ---- loadMusic ----
MusicId AudioEngine::loadMusic(const char* path) {
    if (!isInitialized()) return INVALID_MUSIC;
    WavData w = loadWav(path);
    if (!w.ok) return INVALID_MUSIC;

    ALuint buf = 0;
    alGenBuffers(1, &buf);
    alBufferData(buf, w.format, w.samples.data(),
                 (ALsizei)w.samples.size(), w.freq);
    if (alGetError() != AL_NO_ERROR) {
        PE_ERROR("audio", "alBufferData(music) failed: %s", path);
        alDeleteBuffers(1, &buf);
        return INVALID_MUSIC;
    }
    impl_->musicBuffers.push_back(buf);
    MusicId id = (MusicId)impl_->musicBuffers.size(); // 1-tabanlı
    PE_INFO("audio", "Music loaded: %s -> id=%u (buf=%u)", path, id, buf);
    return id;
}

// ---- playSound ----
void AudioEngine::playSound(SoundId id, float volume, float pitch, bool loop) {
    if (!isInitialized() || id == INVALID_SOUND) return;
    if (id < 1 || id > impl_->soundBuffers.size()) return;
    ALuint buf = impl_->soundBuffers[id - 1];

    // Boş source bul
    int slot = -1;
    for (int i = 0; i < SOURCE_POOL_SIZE; ++i) {
        if (!impl_->sourceBusy[i]) { slot = i; break; }
    }
    if (slot < 0) {
        // Steal: çalmayı biten ya da en eski source'u al
        for (int i = 0; i < SOURCE_POOL_SIZE; ++i) {
            ALint state = 0;
            alGetSourcei(impl_->sources[i], AL_SOURCE_STATE, &state);
            if (state != AL_PLAYING && state != AL_PAUSED) { slot = i; break; }
        }
    }
    if (slot < 0) {
        // Hepsinde çalıyor: 0'yı çal
        slot = 0;
        alSourceStop(impl_->sources[0]);
    }

    ALuint src = impl_->sources[slot];
    alSourcei(src, AL_BUFFER, (ALint)buf);
    alSourcef(src, AL_GAIN, volume * impl_->masterVolume);
    alSourcef(src, AL_PITCH, pitch);
    alSourcei(src, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
    alSourcePlay(src);
    impl_->sourceBusy[slot] = true;
}

// ---- playMusic / stopMusic ----
void AudioEngine::playMusic(MusicId id, float volume, bool loop) {
    if (!isInitialized() || id == INVALID_MUSIC) return;
    if (id < 1 || id > impl_->musicBuffers.size()) return;
    ALuint buf = impl_->musicBuffers[id - 1];

    alSourceStop(impl_->musicSource);
    alSourcei(impl_->musicSource, AL_BUFFER, (ALint)buf);
    alSourcef(impl_->musicSource, AL_GAIN, volume);
    alSourcei(impl_->musicSource, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
    alSourcePlay(impl_->musicSource);
    impl_->musicPlaying = true;
}
void AudioEngine::stopMusic() {
    if (!isInitialized() || !impl_->musicSource) return;
    alSourceStop(impl_->musicSource);
    impl_->musicPlaying = false;
}

// ---- setMasterVolume ----
void AudioEngine::setMasterVolume(float v) {
    if (!isInitialized()) return;
    impl_->masterVolume = v;
    alListenerf(AL_GAIN, v);
}

// ---- pauseAll / resumeAll ----
void AudioEngine::pauseAll() {
    if (!isInitialized() || impl_->paused) return;
    for (int i = 0; i < SOURCE_POOL_SIZE; ++i) {
        ALint state = 0;
        alGetSourcei(impl_->sources[i], AL_SOURCE_STATE, &state);
        if (state == AL_PLAYING) alSourcePause(impl_->sources[i]);
    }
    if (impl_->musicPlaying) alSourcePause(impl_->musicSource);
    impl_->paused = true;
}
void AudioEngine::resumeAll() {
    if (!isInitialized() || !impl_->paused) return;
    for (int i = 0; i < SOURCE_POOL_SIZE; ++i) {
        ALint state = 0;
        alGetSourcei(impl_->sources[i], AL_SOURCE_STATE, &state);
        if (state == AL_PAUSED) alSourcePlay(impl_->sources[i]);
    }
    if (impl_->musicPlaying) alSourcePlay(impl_->musicSource);
    impl_->paused = false;
}

// ---- update ----
void AudioEngine::update() {
    if (!isInitialized()) return;
    // Çalmayı biten source'ları boşalt
    for (int i = 0; i < SOURCE_POOL_SIZE; ++i) {
        if (!impl_->sourceBusy[i]) continue;
        ALint state = 0;
        alGetSourcei(impl_->sources[i], AL_SOURCE_STATE, &state);
        if (state == AL_STOPPED || state == AL_INITIAL) {
            alSourcei(impl_->sources[i], AL_BUFFER, 0);
            impl_->sourceBusy[i] = false;
        }
    }
    // Müzik bitti mi?
    if (impl_->musicPlaying) {
        ALint state = 0;
        alGetSourcei(impl_->musicSource, AL_SOURCE_STATE, &state);
        if (state == AL_STOPPED) impl_->musicPlaying = false;
    }
}

// ---- Singleton ----
AudioEngine& audio() {
    static AudioEngine s;
    return s;
}

} // namespace pocket::audio
