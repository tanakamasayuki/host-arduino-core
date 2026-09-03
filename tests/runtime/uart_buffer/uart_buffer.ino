// Tests for the device-facing UARTs (cores/host/HostUart.h).
//
// `Serial1` / `Serial2` are not wired to anything outside the process:
// both directions are queues that program code drives. A test drains what
// the sketch wrote, works out an answer, and pushes it back.
//
// Two servicing points are exercised, because real drivers need both:
//
//   - from `kPreLoop`, for a sketch that sends in one iteration and reads
//     the reply in a later one
//   - from inside the clock port's wait, for a sketch that sends and
//     reads the reply before returning from `loop()` — every AT-command
//     driver does this, and it is only serviceable because the wait is
//     overridable
//
// The "modem" here answers AT commands. It knows the protocol; the core
// knows only that bytes moved.
//
// A third servicing point is exercised too: the activity hook, which fires
// synchronously before `write()` returns. That is the one that keeps UART
// traffic in order with everything else, and the one a modem can answer
// from without waiting for anything.

#include <Arduino.h>

namespace {

constexpr int PIN_RX = 16;
constexpr int PIN_TX = 17;

// AT traffic is full of CR and LF, which would break one reported line
// into several. Reporting them as '.' keeps a golden comparison to one
// line per event.
String oneLine(const String &text)
{
    String out;
    out.reserve(text.length());
    for (unsigned int i = 0; i < text.length(); ++i) {
        const char c = text.charAt(i);
        out += (c == '\r' || c == '\n') ? '.' : c;
    }
    return out;
}

// Stand-in for the device a test is pretending to be. Same shape as a
// bus-port device model: it reads what the sketch sent and answers.
struct ModemModel {
    uint8_t commands = 0;
    char last[32] = {0};

    // Returns true when it recognized a complete command and answered.
    bool service(HostUart &uart)
    {
        if (uart.txAvailable() == 0) {
            return false;
        }
        const String sent = uart.readTxString();
        snprintf(last, sizeof(last), "%s", oneLine(sent).c_str());
        ++commands;
        if (sent.startsWith("AT+CSQ")) {
            uart.pushRx("+CSQ: 24,0\r\nOK\r\n");
        } else if (sent.startsWith("AT")) {
            uart.pushRx("OK\r\n");
        } else {
            uart.pushRx("ERROR\r\n");
        }
        return true;
    }
};

ModemModel modem;

// --- servicing from the clock port's wait ----------------------------
//
// While the sketch is blocked in readBytesUntil, this runs on every 1 ms
// slice. Real time still passes; the only thing added is the chance to
// answer.
void onWait(uint32_t micros, void *user)
{
    static_cast<ModemModel *>(user)->service(Serial1);
    HostArduino::clockRealWaitMicros(micros);
}

// --- servicing from kPreLoop -----------------------------------------

uint8_t preloop_services = 0;

void onPhase(HostArduino::LifecyclePhase phase, void *user)
{
    if (phase != HostArduino::kPreLoop) {
        return;
    }
    if (static_cast<ModemModel *>(user)->service(Serial1)) {
        ++preloop_services;
    }
}

// --- servicing from the activity hook --------------------------------
//
// Fires before `write()` returns, so the answer is queued before the
// sketch's next statement. No lock is held when this runs, which is what
// makes calling `pushRx` from inside it legal.

struct UartTrace {
    char line[16][40] = {{0}};
    uint8_t count = 0;
    bool overflowed = false;

    void add(const char *what, const uint8_t *data, size_t len)
    {
        if (count >= 16) {
            overflowed = true;
            return;
        }
        char text[24] = {0};
        const size_t copy = len < sizeof(text) - 1 ? len : sizeof(text) - 1;
        for (size_t i = 0; i < copy; ++i) {
            const char c = static_cast<char>(data[i]);
            text[i] = (c == '\r' || c == '\n') ? '.' : c;
        }
        snprintf(line[count], sizeof(line[0]), "%s len=%u %s", what, (unsigned)len, text);
        ++count;
    }
};

UartTrace utrace;
bool answer_from_hook = false;

void onActivity(HostUart::ActivityEvent event, HostUart &uart, const uint8_t *data, size_t len,
                void *user)
{
    (void)user;
    switch (event) {
    case HostUart::kUartBegin:
        utrace.add("begin", nullptr, 0);
        break;
    case HostUart::kUartEnd:
        utrace.add("end", nullptr, 0);
        break;
    case HostUart::kUartConfig:
        utrace.add("config", nullptr, 0);
        break;
    case HostUart::kUartTx:
        utrace.add("tx", data, len);
        // Answering from the notification itself. This would deadlock if
        // the core held its mutex across the callback.
        if (answer_from_hook) {
            uart.pushRx("OK\r\n");
        }
        break;
    case HostUart::kUartRx:
        utrace.add("rx", data, len);
        break;
    case HostUart::kUartRxDiscard:
        utrace.add("discard", data, len);
        break;
    }
}

uint8_t stage = 0;

} // namespace

void setup()
{
    Serial.begin(115200);
    Serial.println("TEST start uart_buffer");

    // --- what begin() records ----------------------------------------

    Serial.printf("before: begun=%d bool=%d num=%u\n", Serial1.begun() ? 1 : 0, Serial1 ? 1 : 0,
                  Serial1.uartNum());
    Serial1.begin(9600, SERIAL_8N1, PIN_RX, PIN_TX);
    Serial.printf("begin: begun=%d baud=%u config=%X rx=%d tx=%d\n", Serial1.begun() ? 1 : 0,
                  (unsigned)Serial1.baudRate(), (unsigned)Serial1.config(), Serial1.rxPin(),
                  Serial1.txPin());

    // --- the plain queues --------------------------------------------

    // Nothing consumes the tx queue but a driver, so what the sketch
    // wrote is still there, in order.
    const size_t written = Serial1.print("hello");
    Serial.printf("tx: written=%u waiting=%u total=%u\n", (unsigned)written,
                  (unsigned)Serial1.txAvailable(), (unsigned)Serial1.txTotal());

    uint8_t drained[8] = {0};
    const size_t took = Serial1.readTx(drained, sizeof(drained));
    Serial.printf("drain: took=%u text=%.*s left=%u\n", (unsigned)took, (int)took,
                  (const char *)drained, (unsigned)Serial1.txAvailable());

    // Pushed bytes are what the sketch reads back.
    Serial1.pushRx("world");
    Serial.printf("rx: available=%d peek=%c total=%u\n", Serial1.available(), Serial1.peek(),
                  (unsigned)Serial1.rxTotal());
    const String read_back = Serial1.readString();
    Serial.printf("rx: text=%s available=%d read=%d\n", read_back.c_str(), Serial1.available(),
                  Serial1.read());

    // --- answering inside one iteration ------------------------------
    //
    // The sketch sends and reads the reply without returning to loop().
    // Only the clock port can service that.
    HostArduino::setClockHooks(nullptr, onWait, &modem);
    Serial1.print("AT+CSQ\r\n");
    Serial1.setTimeout(500);
    const String reply = Serial1.readStringUntil('\n');
    HostArduino::clearClockHooks();
    Serial.printf("sameiter: cmd=%s reply=%s commands=%u\n", modem.last,
                  oneLine(reply).c_str(), modem.commands);

    // Without a servicing point the same call just times out, which is
    // the honest answer for a device nobody is playing.
    Serial1.clearRx();
    Serial1.print("AT\r\n");
    Serial1.setTimeout(20);
    const String silent = Serial1.readStringUntil('\n');
    Serial.printf("unserviced: len=%u waiting=%u\n", (unsigned)silent.length(),
                  (unsigned)Serial1.txAvailable());
    Serial1.clearTx();

    // --- overflow ----------------------------------------------------

    // The tx queue drops what will not fit and says so, rather than
    // growing until a test runs out of memory.
    Serial1.setTxBufferSize(8);
    uint8_t blob[16];
    for (uint8_t i = 0; i < sizeof(blob); ++i) {
        blob[i] = i;
    }
    const size_t accepted = Serial1.write(blob, sizeof(blob));
    Serial.printf("txoverflow: accepted=%u room=%d flag=%d\n", (unsigned)accepted,
                  Serial1.availableForWrite(), Serial1.txOverflowed() ? 1 : 0);
    Serial1.clearTx();
    Serial1.clearOverflow();

    // And the rx queue does the same when the sketch is not reading.
    Serial1.setRxBufferSize(4);
    const size_t pushed = Serial1.pushRx(blob, sizeof(blob));
    Serial.printf("rxoverflow: pushed=%u available=%d flag=%d\n", (unsigned)pushed,
                  Serial1.available(), Serial1.rxOverflowed() ? 1 : 0);

    // flush() drops what the sketch has not read; the tx queue is the
    // driver's and is left alone.
    Serial1.print("keep");
    Serial1.flush();
    Serial.printf("flush: available=%d waiting=%u\n", Serial1.available(),
                  (unsigned)Serial1.txAvailable());

    // --- Serial2 is a separate bus -----------------------------------

    Serial2.begin(115200);
    Serial2.print("two");
    Serial.printf("serial2: num=%u baud=%u waiting=%u other=%u\n", Serial2.uartNum(),
                  (unsigned)Serial2.baudRate(), (unsigned)Serial2.txAvailable(),
                  (unsigned)Serial1.txAvailable());

    // --- the activity hook -------------------------------------------
    //
    // The third servicing point. Unlike the two above it needs no wait and
    // no next iteration: the answer is queued before `print()` returns.
    Serial1.end();
    Serial1.clearOverflow();
    Serial1.setRxBufferSize(HostUart::kDefaultBufferSize);
    Serial1.setTxBufferSize(HostUart::kDefaultBufferSize);
    Serial1.setActivityHook(onActivity);
    answer_from_hook = true;
    Serial1.begin(9600, SERIAL_8N1, PIN_RX, PIN_TX);
    Serial1.print("AT\r\n");
    // Already there — no wait, no loop() iteration.
    Serial.printf("activity: answered=%d\n", Serial1.available());
    const int first = Serial1.read();
    answer_from_hook = false;
    Serial1.pushRx("XY");
    Serial1.flush();
    Serial1.updateBaudRate(19200);
    Serial.printf("activity: first=%c trace=%u overflow=%d\n", first, utrace.count,
                  utrace.overflowed ? 1 : 0);
    for (uint8_t i = 0; i < utrace.count; ++i) {
        Serial.printf("# %s\n", utrace.line[i]);
    }

    // Releasing the slot stops the notifications; the queues keep working.
    Serial1.clearActivityHook();
    const uint8_t before_uart_clear = utrace.count;
    Serial1.print("Z");
    Serial.printf("activity: cleared_delta=%u waiting=%u\n", utrace.count - before_uart_clear,
                  (unsigned)Serial1.txAvailable());

    // Reset for the loop-side section.
    Serial1.end();
    Serial1.clearOverflow();
    Serial1.resetTotals();
    Serial1.setTxBufferSize(HostUart::kDefaultBufferSize);
    Serial1.setRxBufferSize(HostUart::kDefaultBufferSize);
    Serial1.begin(9600, SERIAL_8N1, PIN_RX, PIN_TX);
    Serial.printf("reset: begun=%d waiting=%u available=%d total=%u\n", Serial1.begun() ? 1 : 0,
                  (unsigned)Serial1.txAvailable(), Serial1.available(), (unsigned)Serial1.txTotal());

    modem.commands = 0;
    HostArduino::setLifecycleHook(onPhase, &modem);
}

void loop()
{
    switch (stage) {
    case 0:
        // Send now; the driver runs at the next kPreLoop, so the reply
        // cannot be here yet.
        Serial1.print("AT\r\n");
        Serial.printf("preloop: sent available=%d\n", Serial1.available());
        ++stage;
        break;

    case 1: {
        // One kPreLoop has run since, so the answer is waiting.
        const String reply = Serial1.readStringUntil('\n');
        Serial.printf("preloop: reply=%s services=%u commands=%u\n", oneLine(reply).c_str(),
                      preloop_services, modem.commands);
        HostArduino::clearLifecycleHook();
        ++stage;
        break;
    }

    case 2:
        Serial.println("TEST done");
        ++stage;
        break;

    default:
        break;
    }

    delay(10);
}
