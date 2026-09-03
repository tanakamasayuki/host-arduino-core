// Tests for the clock port (cores/host/HostClock.h).
//
// Three things are being verified, in order:
//
//   1. with nothing installed, time is the real monotonic clock and
//      `delay` really sleeps — the pre-existing behavior
//   2. overriding only the wait keeps real time and gives a driver a
//      heartbeat inside every wait: the "tick callback" shape
//   3. overriding both makes time virtual, so `delay(5000)` returns at
//      host speed with the clock 5000 ms further on, and the `Stream`
//      timeouts that `readBytes` is built on move with it
//
// (2) and (3) are the same mechanism with different bodies, which is the
// point of handing over the wait rather than only the clock.

#include <Arduino.h>

namespace {

// A Stream with nothing in it, so `readBytes` always runs its timeout out
// to the end. Under a virtual clock that has to cost no real time.
class SilentStream : public Stream {
public:
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    size_t write(uint8_t) override { return 1; }
};

SilentStream silent;

// --- (2) the heartbeat shape: real time, own work in the gaps ---------

uint32_t ticks = 0;
uint32_t tick_micros_total = 0;

void onTickWait(uint32_t micros, void *user)
{
    (void)user;
    ++ticks;
    tick_micros_total += micros;
    // Still sleep for real: this is the shape for a driver that wants to
    // be woken periodically without changing what time it is.
    HostArduino::clockRealWaitMicros(micros);
}

// --- (3) the virtual clock -------------------------------------------

struct VirtualClock {
    uint64_t micros = 0;
    uint32_t waits = 0;
};

VirtualClock vclock;

uint64_t virtualNow(void *user)
{
    return static_cast<VirtualClock *>(user)->micros;
}

void virtualWait(uint32_t micros, void *user)
{
    VirtualClock *c = static_cast<VirtualClock *>(user);
    ++c->waits;
    // Advance instead of sleeping. This is what stops a sketch full of
    // `delay` from running in real time.
    c->micros += micros;
}

} // namespace

void setup()
{
    Serial.begin(115200);
    Serial.println("TEST start clock_hook");

    // --- 1. nothing installed ----------------------------------------

    Serial.printf("default: overridden=%d\n", HostArduino::clockOverridden() ? 1 : 0);

    // Real time really passes, measured against the real clock so the
    // assertion holds however the rest of this sketch messes with time.
    const uint64_t real_before = HostArduino::clockRealNowMicros();
    const uint32_t millis_before = millis();
    delay(30);
    const uint64_t real_slept = HostArduino::clockRealNowMicros() - real_before;
    const uint32_t millis_slept = millis() - millis_before;
    Serial.printf("real: slept=%d advanced=%d\n", real_slept >= 25000 ? 1 : 0,
                  millis_slept >= 25 ? 1 : 0);

    // millis() and micros() are one reading truncated two ways, so the
    // microsecond figure must fall between the two millisecond readings
    // that bracket it. Comparing a single pair would be a coin flip
    // whenever the two calls straddle a millisecond boundary.
    const uint32_t ms_before = millis();
    const uint32_t us = micros();
    const uint32_t ms_after = millis();
    Serial.printf("agree: %d\n", (us / 1000 >= ms_before && us / 1000 <= ms_after) ? 1 : 0);

    // --- 2. wait only: the heartbeat ---------------------------------

    HostArduino::setClockHooks(nullptr, onTickWait);
    Serial.printf("tickmode: overridden=%d\n", HostArduino::clockOverridden() ? 1 : 0);

    const uint64_t tick_real_before = HostArduino::clockRealNowMicros();
    const uint32_t tick_millis_before = millis();
    delay(20);
    const uint64_t tick_real_slept = HostArduino::clockRealNowMicros() - tick_real_before;
    // `fired` rather than a slice count: how many slices a 20 ms delay
    // takes is a property of the host's sleep granularity, not of this
    // port. Windows rounds a 1 ms sleep up to the ~15 ms timer tick, so
    // the same delay is 2 slices there and 20 on Linux. What the port
    // guarantees is that the hook is reached at all, and with the slice
    // size the core asked for.
    Serial.printf("tick: fired=%d slice=%u realtime=%d advanced=%d\n", ticks >= 1 ? 1 : 0,
                  ticks ? tick_micros_total / ticks : 0, tick_real_slept >= 20000 ? 1 : 0,
                  millis() - tick_millis_before >= 20 ? 1 : 0);

    // yield() goes through the port as a zero-length wait, which is where
    // a busy-waiting sketch gives the driver a chance to run.
    const uint32_t before_yield = ticks;
    yield();
    Serial.printf("yield: ticks_delta=%u\n", ticks - before_yield);

    HostArduino::clearClockHooks();

    // --- 3. both: virtual time ---------------------------------------

    // Start the virtual clock where the real one left off, so millis()
    // does not appear to jump backwards across the handover.
    vclock.micros = HostArduino::clockRealNowMicros();
    HostArduino::setClockHooks(virtualNow, virtualWait, &vclock);

    const uint32_t v_before = millis();
    const uint64_t v_real_before = HostArduino::clockRealNowMicros();
    delay(5000);
    const uint64_t v_real_spent = HostArduino::clockRealNowMicros() - v_real_before;
    // "Less than half the virtual time" rather than an arbitrary budget:
    // that is exactly the claim being made, and it leaves room for 5000
    // hook calls to cost something on a slow host without the assertion
    // becoming a stopwatch.
    Serial.printf("virtual: advanced=%u realtime_fast=%d waits=%u\n", millis() - v_before,
                  v_real_spent < 2500000 ? 1 : 0, vclock.waits);

    // delayMicroseconds is one wait with no loop around it, so it lands
    // exactly on the requested amount.
    const uint32_t before_us = micros();
    delayMicroseconds(1234);
    Serial.printf("micros_delay: advanced=%u\n", micros() - before_us);

    // The Stream timeouts move with the clock: readBytes on a stream that
    // never produces a byte burns its whole timeout and no real time.
    silent.setTimeout(2000);
    char buffer[4] = {0};
    const uint32_t s_before = millis();
    const uint64_t s_real_before = HostArduino::clockRealNowMicros();
    const size_t got = silent.readBytes(buffer, sizeof(buffer));
    const uint64_t s_real_spent = HostArduino::clockRealNowMicros() - s_real_before;
    Serial.printf("stream: got=%u advanced=%u realtime_fast=%d\n", (unsigned)got,
                  millis() - s_before, s_real_spent < 1000000 ? 1 : 0); // half of 2000 ms

    // Handing the clock back leaves the real one where it always was —
    // the virtual excursion did not move it.
    HostArduino::clearClockHooks();
    Serial.printf("restored: overridden=%d sane=%d\n", HostArduino::clockOverridden() ? 1 : 0,
                  millis() < 5000 ? 1 : 0);

    Serial.println("TEST done");
}

void loop()
{
    delay(10);
}
