"""Tests for xSemaphoreCreateMutex / Take / Give and the binary and
counting semaphores.

The claim is that the mutex loses no updates: 3 tasks x 1000 increments
must be exactly 3000. How long that takes is not part of the claim, so
`wait_timeout` is asserted separately: a slow runner then reports itself
instead of looking like a lost update. That distinction was not academic —
a bad first `millis()` reading once made the wait loop exit immediately
and this test reported `shared=2611`, which reads like a broken mutex and
was not one.
"""


def test_freertos_mutex(dut):
    dut.expect("TEST start freertos_mutex", timeout=10)
    dut.expect("mutex_create=ok", timeout=10)

    # All three tasks finished inside the budget. If this is the failure,
    # the wait was too short — not the mutex.
    dut.expect("workers_done=3", timeout=20)
    dut.expect("wait_timeout=0", timeout=10)

    # And nothing was lost under contention. This is the actual claim.
    dut.expect("shared=3000", timeout=10)

    dut.expect("bin_empty=ok", timeout=10)
    dut.expect("bin_after_give=ok", timeout=10)
    dut.expect("cnt_taken=3", timeout=10)
    dut.expect("TEST done", timeout=10)
