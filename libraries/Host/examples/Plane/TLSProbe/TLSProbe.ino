// TLSProbe — verifies that the `TLS=OpenSSL` board menu option links
// OpenSSL correctly on the host.
//
// How to use:
//   1. Open this sketch in the Arduino IDE (or via `arduino-cli`).
//   2. Select board: lang-ship:host:host.
//   3. Set the "TLS" menu to "OpenSSL".
//   4. Make sure OpenSSL development files are installed:
//        Linux:        sudo apt install libssl-dev      (or distro equivalent)
//        Windows MSYS2: pacman -S mingw-w64-ucrt-x86_64-openssl
//        macOS:        brew install openssl@3
//                      (additional -I / -L flags may be required;
//                       macOS is not officially supported by this menu yet.)
//   5. Compile and upload — the executable runs locally.
//   6. Connect to the TCP-backed Serial endpoint printed by upload and
//      read the output. Each line is one PASS/FAIL signal.
//
// What it prints:
//   TLS_MENU=<state>          — whether HOST_ARDUINO_HAVE_OPENSSL is defined.
//   OPENSSL_HEADER=...        — version macro from <openssl/opensslv.h>.
//   OPENSSL_RUNTIME=...       — what the linked libssl reports at runtime.
//   CTX_NEW_OK / CTX_NEW_FAIL — result of SSL_CTX_new(TLS_client_method()).
//   PROBE_RESULT=PASS|FAIL    — final verdict.
//
// If you see PROBE_RESULT=PASS on your platform, please report success on
// the issue tracker so the support matrix in README.md can be updated.

#include <Arduino.h>

#ifdef HOST_ARDUINO_HAVE_OPENSSL
#include <openssl/opensslv.h>
#include <openssl/crypto.h>
#include <openssl/ssl.h>
#endif

static bool g_passed = false;

void setup()
{
    Serial.begin(115200);

#ifndef HOST_ARDUINO_HAVE_OPENSSL
    Serial.println("TLS_MENU=disabled");
    Serial.println("HINT: select Tools > TLS > OpenSSL in the IDE,");
    Serial.println("HINT: or set tls=openssl on the FQBN, then rebuild.");
    Serial.println("PROBE_RESULT=FAIL");
    return;
#else
    Serial.println("TLS_MENU=openssl");

    Serial.print("OPENSSL_HEADER=");
    Serial.println(OPENSSL_VERSION_TEXT);

    Serial.print("OPENSSL_RUNTIME=");
    Serial.println(OpenSSL_version(OPENSSL_VERSION));

    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    if (ctx == nullptr)
    {
        Serial.println("CTX_NEW_FAIL");
        Serial.println("PROBE_RESULT=FAIL");
        return;
    }
    SSL_CTX_free(ctx);
    Serial.println("CTX_NEW_OK");

    g_passed = true;
    Serial.println("PROBE_RESULT=PASS");
#endif
}

void loop()
{
    // Heartbeat so log readers can tell the process is alive after the
    // probe finished. Quiet enough not to spam — once per 5 seconds.
    static uint32_t last = 0;
    const uint32_t now = millis();
    if (now - last >= 5000)
    {
        last = now;
        Serial.print("alive ");
        Serial.println(g_passed ? "PASS" : "FAIL");
    }
    delay(50);
}
