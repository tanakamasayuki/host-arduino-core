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
