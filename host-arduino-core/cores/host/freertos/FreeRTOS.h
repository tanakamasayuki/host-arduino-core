#ifndef HOST_ARDUINO_FREERTOS_H
#define HOST_ARDUINO_FREERTOS_H

// FreeRTOS subset for the host runtime.
//
// Scope: enough of the API surface that ESP32 sketches built around tasks,
// queues, semaphores, mutexes, and task notifications compile and run on the
// host without source changes. Implemented on top of std::thread,
// std::mutex, std::condition_variable.
//
// Out of scope (silently ignored on host):
//   - Task priority and core affinity (priority/coreID arguments accepted,
//     but the underlying std::thread is scheduled by the OS).
//   - Stack size (we let the OS pick).
//   - True task killing — vTaskDelete(NULL) sets an exit flag and returns;
//     the std::thread completes when the task function returns. Calling
//     vTaskDelete on another task is a no-op aside from the flag (we can't
//     forcibly terminate a std::thread portably).
//   - ISR variants (xQueueSendFromISR etc.) just forward to the non-ISR
//     version; there are no interrupts to defer.

#include <stdint.h>
#include <stddef.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

typedef long      BaseType_t;
typedef unsigned long UBaseType_t;
typedef uint32_t  TickType_t;
typedef void *    StackType_t;
typedef void (*TaskFunction_t)(void *);

#ifndef pdFALSE
#define pdFALSE ((BaseType_t)0)
#endif
#ifndef pdTRUE
#define pdTRUE  ((BaseType_t)1)
#endif
#ifndef pdPASS
#define pdPASS  pdTRUE
#endif
#ifndef pdFAIL
#define pdFAIL  pdFALSE
#endif

#ifndef portTICK_PERIOD_MS
#define portTICK_PERIOD_MS ((TickType_t)1U)
#endif
#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif
#ifndef configTICK_RATE_HZ
#define configTICK_RATE_HZ 1000U
#endif
#ifndef portMAX_DELAY
#define portMAX_DELAY ((TickType_t)0xFFFFFFFFU)
#endif

#ifndef pdMS_TO_TICKS
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#endif

#ifndef tskNO_AFFINITY
#define tskNO_AFFINITY 0x7FFFFFFF
#endif

// Critical sections collapse to a global recursive mutex. portMUX_TYPE is a
// dummy so user code that declares `portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;`
// compiles unchanged.
typedef struct {
    int _unused;
} portMUX_TYPE;
#ifndef portMUX_INITIALIZER_UNLOCKED
#define portMUX_INITIALIZER_UNLOCKED { 0 }
#endif

namespace host_freertos {

inline std::recursive_mutex &critical_mutex() {
    static std::recursive_mutex m;
    return m;
}

struct TaskControl {
    std::atomic<bool> should_exit{false};
    std::atomic<uint32_t> notify_value{0};
    std::mutex notify_mutex;
    std::condition_variable notify_cv;
    std::thread thread;
    std::string name;
};

inline TaskControl *&current_task_slot() {
    thread_local TaskControl *slot = nullptr;
    return slot;
}

} // namespace host_freertos

typedef host_freertos::TaskControl *TaskHandle_t;

// ---- Task ----------------------------------------------------------------

inline BaseType_t xTaskCreatePinnedToCore(TaskFunction_t fn,
                                          const char *name,
                                          uint32_t /*stack*/,
                                          void *param,
                                          UBaseType_t /*priority*/,
                                          TaskHandle_t *out_handle,
                                          BaseType_t /*coreID*/) {
    if (!fn) return pdFAIL;
    auto *ctrl = new host_freertos::TaskControl();
    if (name) ctrl->name = name;
    ctrl->thread = std::thread([ctrl, fn, param]() {
        host_freertos::current_task_slot() = ctrl;
        fn(param);
        host_freertos::current_task_slot() = nullptr;
        ctrl->should_exit.store(true);
    });
    ctrl->thread.detach();
    if (out_handle) *out_handle = ctrl;
    return pdPASS;
}

inline BaseType_t xTaskCreate(TaskFunction_t fn,
                              const char *name,
                              uint32_t stack,
                              void *param,
                              UBaseType_t priority,
                              TaskHandle_t *out_handle) {
    return xTaskCreatePinnedToCore(fn, name, stack, param, priority, out_handle, tskNO_AFFINITY);
}

inline void vTaskDelete(TaskHandle_t handle) {
    auto *ctrl = handle ? handle : host_freertos::current_task_slot();
    if (!ctrl) return;
    ctrl->should_exit.store(true);
    // We cannot leak less without portably terminating the thread.
    // The task function is expected to observe should_exit and return.
}

inline void vTaskDelay(const TickType_t ticks) {
    if (ticks == 0) {
        std::this_thread::yield();
        return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(ticks * portTICK_PERIOD_MS));
}

inline void vTaskDelayUntil(TickType_t *prev_wake, const TickType_t increment) {
    if (!prev_wake) {
        vTaskDelay(increment);
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    static const auto t0 = now;
    const auto target_ms = static_cast<int64_t>(*prev_wake) + static_cast<int64_t>(increment);
    *prev_wake += increment;
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - t0).count();
    const auto remain = target_ms - elapsed_ms;
    if (remain > 0) std::this_thread::sleep_for(std::chrono::milliseconds(remain));
}

inline TickType_t xTaskGetTickCount() {
    static const auto t0 = std::chrono::steady_clock::now();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return static_cast<TickType_t>(ms);
}
inline TickType_t xTaskGetTickCountFromISR() { return xTaskGetTickCount(); }

inline TaskHandle_t xTaskGetCurrentTaskHandle() {
    return host_freertos::current_task_slot();
}

inline void taskYIELD_impl() { std::this_thread::yield(); }
#ifndef taskYIELD
#define taskYIELD() taskYIELD_impl()
#endif
#ifndef portYIELD
#define portYIELD() taskYIELD_impl()
#endif
#ifndef portYIELD_FROM_ISR
#define portYIELD_FROM_ISR() taskYIELD_impl()
#endif

// ---- Task notify ---------------------------------------------------------

inline BaseType_t xTaskNotifyGive(TaskHandle_t task) {
    if (!task) return pdFAIL;
    {
        std::lock_guard<std::mutex> g(task->notify_mutex);
        task->notify_value.fetch_add(1);
    }
    task->notify_cv.notify_all();
    return pdPASS;
}
inline BaseType_t xTaskNotifyGiveFromISR(TaskHandle_t task, BaseType_t * /*woken*/) {
    return xTaskNotifyGive(task);
}

inline uint32_t ulTaskNotifyTake(BaseType_t clear_on_exit, TickType_t ticks_to_wait) {
    auto *self = host_freertos::current_task_slot();
    if (!self) return 0;
    std::unique_lock<std::mutex> lk(self->notify_mutex);
    auto has_value = [self]() { return self->notify_value.load() > 0; };
    if (!has_value()) {
        if (ticks_to_wait == 0) return 0;
        if (ticks_to_wait == portMAX_DELAY) {
            self->notify_cv.wait(lk, has_value);
        } else {
            self->notify_cv.wait_for(lk,
                std::chrono::milliseconds(ticks_to_wait * portTICK_PERIOD_MS),
                has_value);
        }
    }
    const uint32_t v = self->notify_value.load();
    if (v == 0) return 0;
    if (clear_on_exit) {
        self->notify_value.store(0);
    } else {
        self->notify_value.fetch_sub(1);
    }
    return v;
}

// ---- Queue ---------------------------------------------------------------

namespace host_freertos {
struct Queue {
    std::mutex mu;
    std::condition_variable not_full;
    std::condition_variable not_empty;
    std::deque<std::vector<uint8_t>> items;
    size_t capacity;
    size_t item_size;
};
} // namespace host_freertos

typedef host_freertos::Queue *QueueHandle_t;

inline QueueHandle_t xQueueCreate(UBaseType_t length, UBaseType_t item_size) {
    auto *q = new host_freertos::Queue();
    q->capacity = length;
    q->item_size = item_size;
    return q;
}

inline void vQueueDelete(QueueHandle_t q) { delete q; }

inline BaseType_t xQueueSendGeneric(QueueHandle_t q, const void *item, TickType_t ticks_to_wait, bool to_front) {
    if (!q || !item) return pdFAIL;
    std::unique_lock<std::mutex> lk(q->mu);
    auto has_room = [q]() { return q->items.size() < q->capacity; };
    if (!has_room()) {
        if (ticks_to_wait == 0) return pdFAIL;
        if (ticks_to_wait == portMAX_DELAY) {
            q->not_full.wait(lk, has_room);
        } else {
            if (!q->not_full.wait_for(lk,
                    std::chrono::milliseconds(ticks_to_wait * portTICK_PERIOD_MS),
                    has_room)) {
                return pdFAIL;
            }
        }
    }
    std::vector<uint8_t> buf(q->item_size);
    std::memcpy(buf.data(), item, q->item_size);
    if (to_front) q->items.push_front(std::move(buf));
    else          q->items.push_back(std::move(buf));
    lk.unlock();
    q->not_empty.notify_one();
    return pdPASS;
}

inline BaseType_t xQueueSend(QueueHandle_t q, const void *item, TickType_t ticks) {
    return xQueueSendGeneric(q, item, ticks, false);
}
inline BaseType_t xQueueSendToBack(QueueHandle_t q, const void *item, TickType_t ticks) {
    return xQueueSendGeneric(q, item, ticks, false);
}
inline BaseType_t xQueueSendToFront(QueueHandle_t q, const void *item, TickType_t ticks) {
    return xQueueSendGeneric(q, item, ticks, true);
}
inline BaseType_t xQueueSendFromISR(QueueHandle_t q, const void *item, BaseType_t * /*woken*/) {
    return xQueueSendGeneric(q, item, 0, false);
}
inline BaseType_t xQueueSendToBackFromISR(QueueHandle_t q, const void *item, BaseType_t * /*woken*/) {
    return xQueueSendGeneric(q, item, 0, false);
}
inline BaseType_t xQueueSendToFrontFromISR(QueueHandle_t q, const void *item, BaseType_t * /*woken*/) {
    return xQueueSendGeneric(q, item, 0, true);
}

inline BaseType_t xQueueReceive(QueueHandle_t q, void *out, TickType_t ticks_to_wait) {
    if (!q || !out) return pdFAIL;
    std::unique_lock<std::mutex> lk(q->mu);
    auto has_item = [q]() { return !q->items.empty(); };
    if (!has_item()) {
        if (ticks_to_wait == 0) return pdFAIL;
        if (ticks_to_wait == portMAX_DELAY) {
            q->not_empty.wait(lk, has_item);
        } else {
            if (!q->not_empty.wait_for(lk,
                    std::chrono::milliseconds(ticks_to_wait * portTICK_PERIOD_MS),
                    has_item)) {
                return pdFAIL;
            }
        }
    }
    std::memcpy(out, q->items.front().data(), q->item_size);
    q->items.pop_front();
    lk.unlock();
    q->not_full.notify_one();
    return pdPASS;
}
inline BaseType_t xQueueReceiveFromISR(QueueHandle_t q, void *out, BaseType_t * /*woken*/) {
    return xQueueReceive(q, out, 0);
}

inline BaseType_t xQueuePeek(QueueHandle_t q, void *out, TickType_t ticks_to_wait) {
    if (!q || !out) return pdFAIL;
    std::unique_lock<std::mutex> lk(q->mu);
    auto has_item = [q]() { return !q->items.empty(); };
    if (!has_item()) {
        if (ticks_to_wait == 0) return pdFAIL;
        if (ticks_to_wait == portMAX_DELAY) {
            q->not_empty.wait(lk, has_item);
        } else {
            if (!q->not_empty.wait_for(lk,
                    std::chrono::milliseconds(ticks_to_wait * portTICK_PERIOD_MS),
                    has_item)) {
                return pdFAIL;
            }
        }
    }
    std::memcpy(out, q->items.front().data(), q->item_size);
    return pdPASS;
}

inline UBaseType_t uxQueueMessagesWaiting(QueueHandle_t q) {
    if (!q) return 0;
    std::lock_guard<std::mutex> g(q->mu);
    return static_cast<UBaseType_t>(q->items.size());
}
inline UBaseType_t uxQueueSpacesAvailable(QueueHandle_t q) {
    if (!q) return 0;
    std::lock_guard<std::mutex> g(q->mu);
    return static_cast<UBaseType_t>(q->capacity - q->items.size());
}
inline BaseType_t xQueueReset(QueueHandle_t q) {
    if (!q) return pdFAIL;
    std::lock_guard<std::mutex> g(q->mu);
    q->items.clear();
    q->not_full.notify_all();
    return pdPASS;
}

// ---- Semaphore / Mutex ---------------------------------------------------

namespace host_freertos {
struct Semaphore {
    std::mutex mu;
    std::condition_variable cv;
    uint32_t count = 0;
    uint32_t max_count = 1;
    bool is_mutex = false;
    bool is_recursive = false;
    std::thread::id owner{};
    uint32_t recursion = 0;
};
} // namespace host_freertos

typedef host_freertos::Semaphore *SemaphoreHandle_t;

inline SemaphoreHandle_t xSemaphoreCreateBinary() {
    auto *s = new host_freertos::Semaphore();
    s->count = 0;
    s->max_count = 1;
    return s;
}
inline SemaphoreHandle_t xSemaphoreCreateCounting(UBaseType_t max_count, UBaseType_t initial_count) {
    auto *s = new host_freertos::Semaphore();
    s->count = static_cast<uint32_t>(initial_count);
    s->max_count = static_cast<uint32_t>(max_count);
    return s;
}
inline SemaphoreHandle_t xSemaphoreCreateMutex() {
    auto *s = new host_freertos::Semaphore();
    s->count = 1;
    s->max_count = 1;
    s->is_mutex = true;
    return s;
}
inline SemaphoreHandle_t xSemaphoreCreateRecursiveMutex() {
    auto *s = new host_freertos::Semaphore();
    s->count = 1;
    s->max_count = 1;
    s->is_mutex = true;
    s->is_recursive = true;
    return s;
}

inline void vSemaphoreDelete(SemaphoreHandle_t s) { delete s; }

inline BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t ticks_to_wait) {
    if (!s) return pdFAIL;
    std::unique_lock<std::mutex> lk(s->mu);
    const auto self = std::this_thread::get_id();
    if (s->is_recursive && s->owner == self && s->recursion > 0) {
        s->recursion++;
        return pdPASS;
    }
    auto available = [s]() { return s->count > 0; };
    if (!available()) {
        if (ticks_to_wait == 0) return pdFAIL;
        if (ticks_to_wait == portMAX_DELAY) {
            s->cv.wait(lk, available);
        } else {
            if (!s->cv.wait_for(lk,
                    std::chrono::milliseconds(ticks_to_wait * portTICK_PERIOD_MS),
                    available)) {
                return pdFAIL;
            }
        }
    }
    s->count--;
    if (s->is_mutex) {
        s->owner = self;
        s->recursion = 1;
    }
    return pdPASS;
}
inline BaseType_t xSemaphoreTakeRecursive(SemaphoreHandle_t s, TickType_t ticks) {
    return xSemaphoreTake(s, ticks);
}
inline BaseType_t xSemaphoreTakeFromISR(SemaphoreHandle_t s, BaseType_t * /*woken*/) {
    return xSemaphoreTake(s, 0);
}

inline BaseType_t xSemaphoreGive(SemaphoreHandle_t s) {
    if (!s) return pdFAIL;
    std::unique_lock<std::mutex> lk(s->mu);
    if (s->is_recursive && s->recursion > 1) {
        s->recursion--;
        return pdPASS;
    }
    if (s->count >= s->max_count) return pdFAIL;
    s->count++;
    if (s->is_mutex) {
        s->owner = std::thread::id();
        s->recursion = 0;
    }
    lk.unlock();
    s->cv.notify_one();
    return pdPASS;
}
inline BaseType_t xSemaphoreGiveRecursive(SemaphoreHandle_t s) {
    return xSemaphoreGive(s);
}
inline BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t s, BaseType_t * /*woken*/) {
    return xSemaphoreGive(s);
}

// ---- Critical sections ---------------------------------------------------

inline void host_freertos_enter_critical(portMUX_TYPE * /*mux*/) {
    host_freertos::critical_mutex().lock();
}
inline void host_freertos_exit_critical(portMUX_TYPE * /*mux*/) {
    host_freertos::critical_mutex().unlock();
}

#ifndef portENTER_CRITICAL
#define portENTER_CRITICAL(mux) host_freertos_enter_critical(mux)
#endif
#ifndef portEXIT_CRITICAL
#define portEXIT_CRITICAL(mux)  host_freertos_exit_critical(mux)
#endif
#ifndef portENTER_CRITICAL_ISR
#define portENTER_CRITICAL_ISR(mux) host_freertos_enter_critical(mux)
#endif
#ifndef portEXIT_CRITICAL_ISR
#define portEXIT_CRITICAL_ISR(mux)  host_freertos_exit_critical(mux)
#endif
#ifndef portENTER_CRITICAL_SAFE
#define portENTER_CRITICAL_SAFE(mux) host_freertos_enter_critical(mux)
#endif
#ifndef portEXIT_CRITICAL_SAFE
#define portEXIT_CRITICAL_SAFE(mux)  host_freertos_exit_critical(mux)
#endif

#endif // HOST_ARDUINO_FREERTOS_H
