#pragma once
// PocketEngine — bellek yönetimi
// Arena allocator: hızlı bump-allocation, toplu free
// Pool allocator: sabit boyutlu nesneler için, free-list
#include "pocket/core/types.h"
#include "pocket/core/log.h"
#include <cstdlib>
#include <cstring>

namespace pocket {

// ---- Arena ----
// Büyük blok ayır, içine sıralı dağıt. Reset() ile hepsi bir anda serbest.
class Arena {
public:
    explicit Arena(size_t bytes = 1024 * 1024) {
        base_ = (u8*)std::malloc(bytes);
        PE_ASSERT(base_, "arena malloc failed");
        capacity_ = bytes;
        offset_ = 0;
    }
    ~Arena() { std::free(base_); }

    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;

    void* alloc(size_t size, size_t align = 16) {
        size_t aligned = (offset_ + align - 1) & ~(align - 1);
        if (aligned + size > capacity_) {
            PE_ERROR("arena", "out of arena memory: need %zu, have %zu", size, capacity_ - aligned);
            return nullptr;
        }
        offset_ = aligned + size;
        return base_ + aligned;
    }

    template <typename T, typename... Args>
    T* create(Args&&... args) {
        void* p = alloc(sizeof(T), alignof(T));
        return new (p) T(std::forward<Args>(args)...);
    }

    void reset() { offset_ = 0; }
    size_t used() const { return offset_; }
    size_t capacity() const { return capacity_; }

private:
    u8* base_;
    size_t capacity_;
    size_t offset_;
};

// ---- Pool ----
// Sabit boyutlu bloklar için free-list tabanlı ayırıcı.
class Pool {
public:
    Pool(size_t blockSize, size_t count) {
        block_size_ = (blockSize + 15) & ~size_t(15); // 16-byte hizalı
        capacity_ = count;
        base_ = (u8*)std::malloc(block_size_ * count);
        PE_ASSERT(base_, "pool malloc failed");
        free_list_ = (void**)std::malloc(count * sizeof(void*));
        for (size_t i = 0; i < count; ++i) {
            free_list_[i] = base_ + i * block_size_;
        }
        free_count_ = count;
    }
    ~Pool() {
        std::free(base_);
        std::free(free_list_);
    }

    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;

    void* alloc() {
        if (free_count_ == 0) {
            PE_ERROR("pool", "exhausted (block=%zu)", block_size_);
            return nullptr;
        }
        return free_list_[--free_count_];
    }

    void free(void* p) {
        if (!p) return;
        free_list_[free_count_++] = p;
    }

    size_t freeBlocks() const { return free_count_; }
    size_t capacity() const { return capacity_; }

private:
    u8* base_;
    void** free_list_;
    size_t block_size_;
    size_t capacity_;
    size_t free_count_;
};

// ---- Scoped arena (frame allocator tarzı) ----
class ScopedArena {
public:
    explicit ScopedArena(Arena& a) : arena_(a), mark_(a.used()) {}
    ~ScopedArena() { arena_.reset(); /*NOTE: arena reset hepsini sıfırlar; gerçek frame alloc'da mark korunurdu*/ (void)mark_; }
    void* alloc(size_t s, size_t a = 16) { return arena_.alloc(s, a); }
    template <typename T, typename... Args> T* create(Args&&... args) { return arena_.create<T>(std::forward<Args>(args)...); }
private:
    Arena& arena_;
    size_t mark_;
};

// ---- Global istatistik ----
struct MemoryStats {
    std::atomic<size_t> allocations{0};
    std::atomic<size_t> deallocations{0};
    std::atomic<size_t> bytesAllocated{0};
    std::atomic<size_t> peakBytes{0};
};

inline MemoryStats& memStats() {
    static MemoryStats s;
    return s;
}

} // namespace pocket
