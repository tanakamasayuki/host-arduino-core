// Tests for the lifecycle port (cores/host/HostLifecycle.h).
//
// The four points bracket the Arduino thunk, and what a test driver needs
// from them is that the position and the order never move. So this sketch
// records every phase alongside what the sketch itself was doing, and the
// pytest side compares the whole sequence against a golden list.
//
// `HostArduino::loopCount()` is recorded with each entry, which is what
// pins `kPostLoop` to the iteration it closes out rather than the next
// one.

#include <Arduino.h>

namespace {

// Print on the fourth iteration, so the trace holds three complete
// preLoop -> loop -> postLoop cycles plus the start of a fourth.
constexpr uint8_t kReportOnLoop = 4;
constexpr uint8_t kTraceMax = 24;

struct Trace {
    char line[kTraceMax][32] = {{0}};
    uint8_t count = 0;
    bool overflowed = false;

    void add(const char *what, uint64_t loops)
    {
        if (count >= kTraceMax) {
            overflowed = true;
            return;
        }
        snprintf(line[count], sizeof(line[0]), "%s loops=%u", what, (unsigned)loops);
        ++count;
    }
};

Trace trace;
uint8_t loops_run = 0;
bool recording = true;
bool reported = false;
bool check_pending = false;
uint8_t count_at_clear = 0;
uint64_t loops_at_clear = 0;

const char *phaseName(HostArduino::LifecyclePhase phase)
{
    switch (phase) {
    case HostArduino::kPreSetup:
        return "preSetup";
    case HostArduino::kPostSetup:
        return "postSetup";
    case HostArduino::kPreLoop:
        return "preLoop";
    case HostArduino::kPostLoop:
        return "postLoop";
    }
    return "?";
}

void onPhase(HostArduino::LifecyclePhase phase, void *user)
{
    Trace *t = static_cast<Trace *>(user);
    t->add(phaseName(phase), HostArduino::loopCount());
}

// Registering from a global constructor is the realistic case: nothing
// else has run yet, so `kPreSetup` is genuinely the first thing the driver
// sees. Registering from `setup()` would already have missed it.
struct Installer {
    Installer() { HostArduino::setLifecycleHook(onPhase, &trace); }
};

Installer installer;

} // namespace

void setup()
{
    Serial.begin(115200);
    Serial.println("TEST start lifecycle_hook");

    // The hook was installed before main() ran, so preSetup is already
    // recorded by the time setup() gets here.
    trace.add("setup", HostArduino::loopCount());
    Serial.printf("insetup: recorded=%u loops=%u\n", trace.count,
                  (unsigned)HostArduino::loopCount());
}

void loop()
{
    if (recording) {
        trace.add("loop", HostArduino::loopCount());
        ++loops_run;
    }

    if (!reported && loops_run == kReportOnLoop) {
        Serial.printf("trace=%u overflow=%d\n", trace.count, trace.overflowed ? 1 : 0);
        for (uint8_t i = 0; i < trace.count; ++i) {
            Serial.printf("| %s\n", trace.line[i]);
        }

        // Unregistering leaves the thunk exactly as it was before this
        // port existed: no phases announced, iterations still counted.
        // The sketch stops adding to the trace too, so the delta below
        // measures the hook and nothing else.
        HostArduino::clearLifecycleHook();
        recording = false;
        reported = true;
        check_pending = true;
        count_at_clear = trace.count;
        loops_at_clear = HostArduino::loopCount();
        return;
    }

    if (check_pending) {
        // One full iteration has passed with no hook installed.
        Serial.printf("cleared: delta=%u loops_delta=%u\n", trace.count - count_at_clear,
                      (unsigned)(HostArduino::loopCount() - loops_at_clear));
        Serial.println("TEST done");
        check_pending = false;
    }

    delay(10);
}
