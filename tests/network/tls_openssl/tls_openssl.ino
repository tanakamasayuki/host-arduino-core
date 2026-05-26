// Probe sketch for the `tls=openssl` board menu option.
//
// Confirms that:
//   1. `HOST_ARDUINO_HAVE_OPENSSL` is defined by the board menu.
//   2. OpenSSL headers are reachable at compile time
//      (compile-time version macro is printed).
//   3. OpenSSL is actually linked at runtime
//      (a runtime version query and a tiny API call succeed).

#include <Arduino.h>

#ifndef HOST_ARDUINO_HAVE_OPENSSL
#error "tls_openssl probe must be built with TLS=OpenSSL menu option"
#endif

#include <openssl/opensslv.h>
#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

void setup()
{
    Serial.begin(115200);

    Serial.print("OPENSSL_HEADER=");
    Serial.println(OPENSSL_VERSION_TEXT);

    Serial.print("OPENSSL_RUNTIME=");
    Serial.println(OpenSSL_version(OPENSSL_VERSION));

    // Smoke: create and destroy an SSL_CTX. If linking is wrong this
    // either fails to link, or crashes at the first symbol resolution.
    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    if (ctx == nullptr)
    {
        Serial.println("CTX_NEW_FAIL");
        return;
    }
    SSL_CTX_free(ctx);
    Serial.println("CTX_NEW_OK");

    Serial.println("TLS_PROBE_DONE");
}

void loop()
{
    delay(1000);
}
