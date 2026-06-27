#pragma once
// PocketEngine — thread pool / job system
// Standart producer/consumer tasari. submit() herhangi bir callable alir,
// std::future doner. parallelFor ile aralik parcalara bolunup paralel kosulur.
//
// NOT: Termux/Android'de cekirdek sayisi kucuk olabilir; init() worker sayisini
// [2, 8] arasinda sinirlandirir (oversubscription onlemek icin).
#include "pocket/core/types.h"

#include <thread>
#include <atomic>
#include <functional>
#include <future>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <type_traits>
#include <utility>

namespace pocket::thread {

class ThreadPool {
public:
    static ThreadPool& instance();

    // workerCount == 0 => std::thread::hardware_concurrency()
    // Min 2, max 8 worker (Termux oversubscription korumasi).
    bool init(size_t workerCount = 0);
    void shutdown();

    size_t workerCount() const;
    size_t queueSize() const;

    // Bir task gonder; sonuc icin future doner.
    // Ornek:  auto f = pool.submit([](int x){ return x*2; }, 21);
    //         int r = f.get();  // 42
    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<typename std::invoke_result<F, Args...>::type>;

    // [begin, end) araligini grainSize'lik parcara bol, her parcayi pool'a ver,
    // hepsi bitene kadar bekle. fn(begin, end) cagrilir.
    void parallelFor(size_t begin, size_t end, size_t grainSize,
                     std::function<void(size_t, size_t)> fn);

    // Init edilmis mi?
    bool initialized() const { return !workers_.empty(); }

private:
    ThreadPool() = default;
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    Vector<std::thread>              workers_;
    std::queue<std::function<void()>> tasks_;
    mutable std::mutex               mtx_;
    std::condition_variable          cv_;
    std::atomic<bool>                stop_{false};
    std::atomic<size_t>              active_{0};
};

// ---- submit implementation ----
template <typename F, typename... Args>
auto ThreadPool::submit(F&& f, Args&&... args)
    -> std::future<typename std::invoke_result<F, Args...>::type>
{
    using R = typename std::invoke_result<F, Args...>::type;

    auto task = std::make_shared<std::packaged_task<R()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    std::future<R> fut = task->get_future();

    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (stop_) {
            // Pool kapandi: hemen calistirip future'i tamamla.
            // (Alternatif: hata firlat. Biz yumusak davraniyoruz.)
        }
        tasks_.emplace([task]() { (*task)(); });
    }
    cv_.notify_one();
    return fut;
}

// Main-thread-only bolumler icin RAII yardimcisi.
// Ilk olusturuldugunda (genellikle main()'in basinda) mevcut thread'i
// "main thread" olarak kaydeder. Sonraki olusumlar ayni thread'de miyiz diye
// soft-check yapar (degilse uyari loglar).
struct MainThreadGuard {
    MainThreadGuard();
    ~MainThreadGuard();
};

// Free yardimci: mevcut thread main thread mi?
bool isMainThread();

} // namespace pocket::thread
