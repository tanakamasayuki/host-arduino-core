#ifndef HOST_ARDUINO_ESP_TIMER_H
#define HOST_ARDUINO_ESP_TIMER_H

// Host implementation of the ESP-IDF esp_timer API.
//
// esp_timer_get_time() returns microseconds since some fixed reference point
// (here: the first call). The create / start_periodic / start_once / stop /
// delete surface is backed by std::thread + std::condition_variable — same
// trade-offs as our FreeRTOS layer: no priority, no real-time guarantees,
// the OS scheduler picks when the callback fires.

#include <stdint.h>
#include <stddef.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "freertos/FreeRTOS.h"

typedef int esp_err_t;
#ifndef ESP_OK
#define ESP_OK          0
#define ESP_FAIL       -1
#define ESP_ERR_INVALID_ARG  0x102
#define ESP_ERR_INVALID_STATE 0x103
#endif

typedef void (*esp_timer_cb_t)(void *arg);

typedef enum {
    ESP_TIMER_TASK,
    ESP_TIMER_ISR,
} esp_timer_dispatch_t;

typedef struct {
    esp_timer_cb_t callback;
    void *arg;
    esp_timer_dispatch_t dispatch_method;
    const char *name;
    bool skip_unhandled_events;
} esp_timer_create_args_t;

namespace host_esp_timer {

inline std::chrono::steady_clock::time_point &epoch() {
    static std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    return t0;
}

struct Timer {
    esp_timer_cb_t cb = nullptr;
    void *arg = nullptr;
    std::string name;
    std::atomic<bool> running{false};
    std::atomic<bool> stop_requested{false};
    std::atomic<bool> periodic{false};
    std::atomic<uint64_t> period_us{0};
    std::atomic<uint64_t> next_fire_us{0};
    std::mutex mu;
    std::condition_variable cv;
    std::thread th;

    ~Timer() {
        stop_requested.store(true);
        running.store(false);
        cv.notify_all();
        if (th.joinable()) th.join();
    }
};

} // namespace host_esp_timer

typedef host_esp_timer::Timer *esp_timer_handle_t;

inline int64_t esp_timer_get_time(void) {
    auto &t0 = host_esp_timer::epoch();
    const auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(now - t0).count();
}

inline esp_err_t esp_timer_init(void) { (void)host_esp_timer::epoch(); return ESP_OK; }
inline esp_err_t esp_timer_deinit(void) { return ESP_OK; }

inline esp_err_t esp_timer_create(const esp_timer_create_args_t *args,
                                  esp_timer_handle_t *out_handle) {
    if (!args || !args->callback || !out_handle) return ESP_ERR_INVALID_ARG;
    auto *t = new host_esp_timer::Timer();
    t->cb = args->callback;
    t->arg = args->arg;
    if (args->name) t->name = args->name;
    *out_handle = t;
    return ESP_OK;
}

namespace host_esp_timer {

inline void run_loop(Timer *t) {
    std::unique_lock<std::mutex> lk(t->mu);
    while (!t->stop_requested.load()) {
        if (!t->running.load()) {
            t->cv.wait(lk, [t]() {
                return t->running.load() || t->stop_requested.load();
            });
            if (t->stop_requested.load()) break;
        }
        const uint64_t target = t->next_fire_us.load();
        const int64_t now = esp_timer_get_time();
        const int64_t remain = static_cast<int64_t>(target) - now;
        if (remain > 0) {
            t->cv.wait_for(lk, std::chrono::microseconds(remain),
                [t, target]() {
                    return t->stop_requested.load() ||
                           !t->running.load() ||
                           t->next_fire_us.load() != target;
                });
            if (t->stop_requested.load()) break;
            if (!t->running.load()) continue;
            if (t->next_fire_us.load() != target) continue;
            if (static_cast<int64_t>(t->next_fire_us.load()) > esp_timer_get_time()) continue;
        }
        const auto cb = t->cb;
        void *arg = t->arg;
        const bool periodic = t->periodic.load();
        const uint64_t period = t->period_us.load();
        lk.unlock();
        if (cb) cb(arg);
        lk.lock();
        if (periodic && t->running.load()) {
            t->next_fire_us.store(esp_timer_get_time() + period);
        } else {
            t->running.store(false);
        }
    }
}

inline void ensure_thread(Timer *t) {
    if (!t->th.joinable()) {
        t->th = std::thread(run_loop, t);
    }
}

} // namespace host_esp_timer

inline esp_err_t esp_timer_start_once(esp_timer_handle_t t, uint64_t timeout_us) {
    if (!t) return ESP_ERR_INVALID_ARG;
    {
        std::lock_guard<std::mutex> g(t->mu);
        t->periodic.store(false);
        t->period_us.store(0);
        t->next_fire_us.store(esp_timer_get_time() + timeout_us);
        t->running.store(true);
        host_esp_timer::ensure_thread(t);
    }
    t->cv.notify_all();
    return ESP_OK;
}

inline esp_err_t esp_timer_start_periodic(esp_timer_handle_t t, uint64_t period_us) {
    if (!t || period_us == 0) return ESP_ERR_INVALID_ARG;
    {
        std::lock_guard<std::mutex> g(t->mu);
        t->periodic.store(true);
        t->period_us.store(period_us);
        t->next_fire_us.store(esp_timer_get_time() + period_us);
        t->running.store(true);
        host_esp_timer::ensure_thread(t);
    }
    t->cv.notify_all();
    return ESP_OK;
}

inline esp_err_t esp_timer_stop(esp_timer_handle_t t) {
    if (!t) return ESP_ERR_INVALID_ARG;
    {
        std::lock_guard<std::mutex> g(t->mu);
        t->running.store(false);
    }
    t->cv.notify_all();
    return ESP_OK;
}

inline esp_err_t esp_timer_delete(esp_timer_handle_t t) {
    if (!t) return ESP_ERR_INVALID_ARG;
    delete t;
    return ESP_OK;
}

inline bool esp_timer_is_active(esp_timer_handle_t t) {
    return t ? t->running.load() : false;
}

#endif // HOST_ARDUINO_ESP_TIMER_H
