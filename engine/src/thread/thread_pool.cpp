// PocketEngine — thread pool implementasyonu
#include "pocket/thread/thread_pool.h"
#include "pocket/core/log.h"

#include <algorithm>
#include <chrono>
#include <thread>
#include <vector>

namespace pocket::thread {

namespace {
constexpr const char* kTag = "thread";

// Main thread ID'sini call_once ile kaydet.
std::thread::id& mainThreadId() {
    static std::thread::id id;
    return id;
}
std::once_flag& mainOnce() {
    static std::once_flag f;
    return f;
}
bool& mainRegistered() {
    static bool b = false;
    return b;
}
} // namespace

bool isMainThread() {
    if (!mainRegistered()) return true; // kayit yoksa optimistic
    return std::this_thread::get_id() == mainThreadId();
}

MainThreadGuard::MainThreadGuard() {
    std::call_once(mainOnce(), [] {
        mainThreadId() = std::this_thread::get_id();
        mainRegistered() = true;
        PE_INFO(kTag, "main thread kaydedildi");
    });
    if (mainRegistered() && std::this_thread::get_id() != mainThreadId()) {
        PE_WARN(kTag, "MainThreadGuard: main-thread olmayan bir thread'de cagrildi");
    }
}

MainThreadGuard::~MainThreadGuard() = default;

ThreadPool& ThreadPool::instance() {
    static ThreadPool s;
    return s;
}

ThreadPool::~ThreadPool() {
    shutdown();
}

bool ThreadPool::init(size_t workerCount) {
    if (!workers_.empty()) {
        PE_WARN(kTag, "init: zaten calisiyor (%zu worker)", workers_.size());
        return true;
    }
    if (workerCount == 0) {
        workerCount = std::thread::hardware_concurrency();
        if (workerCount == 0) workerCount = 4;
    }
    // Termux/Android'de oversubscription onle: [2, 8].
    workerCount = std::max<size_t>(2, std::min<size_t>(8, workerCount));

    stop_ = false;
    active_ = 0;
    workers_.reserve(workerCount);
    for (size_t i = 0; i < workerCount; ++i) {
        workers_.emplace_back([this] {
            for (;;) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lk(mtx_);
                    cv_.wait(lk, [this] { return stop_ || !tasks_.empty(); });
                    if (stop_ && tasks_.empty()) return;
                    task = std::move(tasks_.front());
                    tasks_.pop();
                }
                active_.fetch_add(1, std::memory_order_relaxed);
                try {
                    task();
                } catch (const std::exception& e) {
                    PE_ERROR(kTag, "task exception: %s", e.what());
                } catch (...) {
                    PE_ERROR(kTag, "task bilinmeyen exception");
                }
                active_.fetch_sub(1, std::memory_order_relaxed);
            }
        });
    }
    PE_INFO(kTag, "thread pool basladi: %zu worker", workerCount);
    return true;
}

void ThreadPool::shutdown() {
    if (workers_.empty()) return;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        stop_ = true;
    }
    cv_.notify_all();
    for (auto& w : workers_) {
        if (w.joinable()) w.join();
    }
    workers_.clear();
    // Kalan tasklari temizle.
    std::queue<std::function<void()>> empty;
    std::swap(tasks_, empty);
    PE_INFO(kTag, "thread pool kapatildi");
}

size_t ThreadPool::workerCount() const {
    return workers_.size();
}

size_t ThreadPool::queueSize() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return tasks_.size();
}

void ThreadPool::parallelFor(size_t begin, size_t end, size_t grainSize,
                             std::function<void(size_t, size_t)> fn) {
    if (begin >= end) return;
    if (grainSize == 0) grainSize = 1;

    if (workers_.empty()) {
        // Pool yoksa ayni thread'de seri calistir.
        fn(begin, end);
        return;
    }

    Vector<std::future<void>> futures;
    for (size_t i = begin; i < end; i += grainSize) {
        size_t e = std::min(i + grainSize, end);
        futures.push_back(submit([fn, i, e]() { fn(i, e); }));
    }
    for (auto& f : futures) {
        if (f.valid()) f.wait();
    }
}

} // namespace pocket::thread
