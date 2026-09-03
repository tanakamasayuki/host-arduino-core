// Acceptance rehearsal for the three extension ports, driven the way a
// verification platform's Arduino middle layer intends to drive them:
//
//   clock port      a virtual clock, and "wait one slice" as the tick
//   lifecycle port  kPreSetup/kPostSetup = fixture and initial dump,
//                   kPreLoop = the director, kPostLoop = advance 1 ms
//   device UART     an AT command answered within one loop() iteration
//   GPIO injection  a pull-up button: LOW, 20 virtual ms, HIGH — and the
//                   sketch's own debounce logic detects one press
//
// The sketch below the line is written as an ordinary application: it
// knows nothing about ticks, directors, or virtual time.
//
// The middle layer installs itself from a global constructor, which is
// both what a real one does (it is a library, not sketch code) and the
// only way to be present for kPreSetup. That also means a virtual clock
// is in force before `main()` calls `runtimeStart()` — so this sketch
// reaching Serial at all is evidence that the runtime's own socket and
// startup waits are still on real time, as documented under "Timeouts
// that stay on real time" in README.md. Virtualizing those would stall
// the accept loop and nothing would ever connect.
#include <Arduino.h>
#include <HostBus.h>
#include <HostClock.h>
#include <HostLifecycle.h>

using namespace HostArduino;

static const uint8_t BTN = 27;

// ---- the mini middle layer ------------------------------------------------
static uint64_t vnow = 0;     // virtual clock, microseconds
static uint32_t tick = 0;
static uint32_t directorCalls = 0;
static bool sawPreSetup = false;
static bool sawPostSetup = false;

// The director: a second loop, separate from the app's. One-shot
// injections only; it counts ticks instead of waiting.
static int phase = 0;
static uint64_t tLow = 0;
static void director() {
  ++directorCalls;

  // UART responder: whatever "AT" arrives, answer "OK". Runs inside
  // waits too (via the wait hook), which is what makes a same-iteration
  // request/response work.
  String tx = Serial1.readTxString();
  if (tx.indexOf("AT") >= 0) Serial1.pushRx("OK\r\n");

  switch (phase) {
    case 0:
      if (tick >= 5) { setPinValue(BTN, LOW); tLow = vnow; phase = 1; }
      break;
    case 1:
      if (vnow - tLow >= 20000) { setPinValue(BTN, HIGH); phase = 2; }
      break;
    default:
      break;
  }
}

static uint64_t nowHook(void *) { return vnow; }
static void waitHook(uint32_t us, void *) {
  vnow += us;      // no real sleep: waits advance the virtual clock
  director();      // and the world keeps moving inside them
}
static void lifeHook(LifecyclePhase p, void *) {
  if (p == kPreSetup)  sawPreSetup = true;   // where the fixture is arranged
  if (p == kPostSetup) sawPostSetup = true;  // where the initial dump goes
  if (p == kPreLoop) { director(); ++tick; }
  if (p == kPostLoop) { vnow += 1000; }      // one loop() iteration = 1 ms
}

// Installed before main() runs, so kPreSetup is not already missed.
struct Installer {
  Installer() {
    setClockHooks(&nowHook, &waitHook, nullptr);
    setLifecycleHook(&lifeHook, nullptr);
  }
};
static Installer installer;

// ---- the application under test (knows nothing of the above) --------------
static uint32_t loopCountApp = 0;
static int presses = 0;
static bool wasLow = false;
static uint32_t lowSince = 0;
static bool atDone = false;
static bool reported = false;
static bool timedOut = false;

void setup() {
  Serial.begin(115200);
  pinMode(BTN, INPUT_PULLUP);
  Serial1.begin(9600);
  Serial1.setTimeout(100);
  Serial.println("TEST start accept_emu_tick");
  Serial.print("overridden=");
  Serial.println(clockOverridden() ? 1 : 0);
}

void loop() {
  ++loopCountApp;

  // The two once-only points, reported from the first iteration because
  // kPostSetup has not happened yet while setup() is still running.
  if (loopCountApp == 1) {
    Serial.printf("phases: preSetup=%d postSetup=%d\n", sawPreSetup ? 1 : 0,
                  sawPostSetup ? 1 : 0);
  }

  // An ordinary debounce: a press is LOW held >= 15 ms, then released.
  const bool low = digitalRead(BTN) == LOW;
  if (low && !wasLow) lowSince = millis();
  if (!low && wasLow && millis() - lowSince >= 15) {
    ++presses;
    Serial.print("PRESS held_ms=");
    Serial.println(millis() - lowSince);
  }
  wasLow = low;

  // Once: write a command and read the reply in this same iteration.
  if (loopCountApp == 3 && !atDone) {
    atDone = true;
    const uint32_t t0 = millis();
    Serial1.print("AT\r\n");
    char buf[5] = {0};
    const size_t n = Serial1.readBytes(buf, 4);
    Serial.print("at_reply_ok=");
    Serial.println((n == 4 && memcmp(buf, "OK\r\n", 4) == 0) ? 1 : 0);
    // Two assertions, deliberately: the exact figure pins the contract
    // that one 1 ms slice of Stream's wait was enough for the director to
    // answer, and the boolean states the weaker thing that actually
    // matters — the reply landed nowhere near the 100 ms timeout, so a
    // change in slice size would not silently turn this into a
    // near-timeout pass.
    const uint32_t at_ms = millis() - t0;
    Serial.print("at_virtual_ms=");
    Serial.println(at_ms);
    Serial.print("at_fast=");
    Serial.println(at_ms < 100 ? 1 : 0);
  }

  // After the press: a long delay must cost virtual time, not real time,
  // and the director must keep running inside it.
  if (presses == 1 && !reported) {
    reported = true;
    const uint64_t real0 = clockRealNowMicros();
    const uint32_t v0 = millis();
    const uint32_t calls0 = directorCalls;
    delay(5000);
    const uint32_t vElapsed = millis() - v0;
    const uint64_t realMs = (clockRealNowMicros() - real0) / 1000;
    Serial.print("delay_virtual_ms=");
    Serial.println(vElapsed);
    Serial.print("delay_real_fast=");
    // Less than half the virtual time: that is the claim, and it leaves
    // room for 5000 director calls to cost something on a slow host
    // without turning this into a stopwatch.
    Serial.println(realMs < 2500 ? 1 : 0);
    // The headline claim of handing the wait over rather than only the
    // clock: 5000 one-millisecond slices, each one a chance for the
    // director to act. Without this the delay would be a dead 5 seconds
    // in which nothing outside the sketch could happen.
    Serial.print("director_in_delay=");
    Serial.println(directorCalls - calls0);
    Serial.print("presses=");
    Serial.println(presses);
    Serial.println("TEST done");
  }

  // Failure path. Deliberately not "TEST done": printing that here would
  // let a broken run satisfy the final expectation. Printed once, because
  // this loop keeps running until pytest tears the process down.
  if (!reported && !timedOut && loopCountApp > 300) {
    timedOut = true;
    Serial.print("TEST timeout loops=");
    Serial.println(loopCountApp);
  }

  // Idle without burning a core. `delay()` would not do it: under a
  // virtual clock the wait hook advances vnow instead of sleeping, so
  // `delay(10)` spins just as hard and only makes the clock run faster.
  // Throttling a sketch is the harness's job once the clock is its own,
  // and the real clock is the only thing that can do it.
  if (reported || timedOut) {
    clockRealWaitMicros(10000);
  }
}
