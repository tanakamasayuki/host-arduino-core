# TODO

## Optional Display UX

- Consider a `console` menu for `lang-ship:host:display` if Windows users need to hide the extra stdout console window.

## Runtime / API

- WiFi.h: keep state-tracking connection stubs; `begin()` should be able to complete as connected.
- `touchRead` / `touchAttachInterrupt`: the next candidates for the observation port's read
  direction. A touch count is a per-pin scalar like an ADC reading, so `setTouchValue` would
  mirror `setAnalogValue`; the interrupt half needs a decision on how a model fires a callback
  (which thread, and whether `attachInterrupt` should become firable at all) and is the reason
  this was not folded into the analog pass.
- `analogContinuous*`: not provided. Open for contribution when a concrete sketch needs it.

## Clock port coverage

The waits listed under "Timeouts that stay on real time" in README.md are outside
the clock port. Two groups, with different reasons:

- `condition_variable::wait_for` in `cores/host/freertos/FreeRTOS.h` (queue,
  semaphore, task-notify timeouts) and `cores/host/esp_timer.h` (timer firing).
  These wake early on notify *and* give up on a deadline, which a single
  "wait a slice" cannot express; they would have to become deadline-plus-predicate
  loops. On top of that they run on FreeRTOS task threads, so virtualizing them
  needs an answer to "who advances the clock" that the loop thread cannot give
  alone. They are self-consistent and always expire today, so this is a
  consistency gap rather than a hang.
- `WiFiClientSecure`'s 5 s handshake budget. Single-threaded and easy to route;
  left out only because no test needed it yet.

`cores/host/HostRuntime.cpp`'s startup and socket waits stay on real time
permanently — virtualizing them would stall the runtime before the test harness
had connected over a real socket.
