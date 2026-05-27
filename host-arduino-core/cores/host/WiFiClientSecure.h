#ifndef HOST_ARDUINO_WIFICLIENTSECURE_H
#define HOST_ARDUINO_WIFICLIENTSECURE_H

// host-arduino-core WiFiClientSecure implementation.
//
// Backend selection: this header is always available so sketches compile
// regardless of TLS configuration. The actual TLS plumbing is enabled
// only when `HOST_ARDUINO_HAVE_OPENSSL` is defined (set by the board
// menu `TLS=OpenSSL`). With no backend, `connect()` returns 0 and emits
// a one-shot `[HostCore]` hint over `Serial`; everything else is benign
// no-op so existing sketches at least link and run.
//
// Policy: certificate verification is *always* skipped on host. Cert
// configuration APIs (`setCACert`, `setCertificate`, `setPrivateKey`,
// `setInsecure`, `loadCACert`, ...) accept input and discard it. Cert
// correctness is a real-device test concern; host tests verify request
// shape, not chain-of-trust.
//
// Include-path policy: this header lives in `cores/host` (not in an
// external library) so `#include <WiFiClientSecure.h>` resolves the
// same way as on ESP32 / ESP8266 and there is no risk of an Arduino IDE
// library shadowing the core header.

#include <stddef.h>
#include <stdint.h>

#include "HostDiag.h"
#include "Stream.h"
#include "WiFiClient.h"

#ifdef HOST_ARDUINO_HAVE_OPENSSL
#include <memory>

#include <openssl/err.h>
#include <openssl/ssl.h>

#include "HostSocket.h"
#endif

class WiFiClientSecure : public WiFiClient
{
public:
#ifdef HOST_ARDUINO_HAVE_OPENSSL
    WiFiClientSecure() : tls_(std::make_shared<TlsState>()) {}
#else
    WiFiClientSecure() {}
#endif
    ~WiFiClientSecure() { stop(); }

    int connect(IPAddress ip, uint16_t port) override
    {
#ifndef HOST_ARDUINO_HAVE_OPENSSL
        (void)ip;
        (void)port;
        HOST_DIAG_ONCE("WiFiClientSecure::connect() — TLS backend not compiled in. Select Tools > TLS > OpenSSL and rebuild.");
        return 0;
#else
        if (!WiFiClient::connect(ip, port))
            return 0;
        return handshake();
#endif
    }

    int connect(const char *host, uint16_t port) override
    {
#ifndef HOST_ARDUINO_HAVE_OPENSSL
        (void)host;
        (void)port;
        HOST_DIAG_ONCE("WiFiClientSecure::connect(host) — TLS backend not compiled in. Select Tools > TLS > OpenSSL and rebuild.");
        return 0;
#else
        if (!WiFiClient::connect(host, port))
            return 0;
        return handshake();
#endif
    }

    // Cert configuration API — no-op on host. Cert correctness is a
    // real-device concern; the host build always skips verification.
    void setInsecure() {}
    void setCACert(const char *) {}
    void setCertificate(const char *) {}
    void setPrivateKey(const char *) {}
    void setPreSharedKey(const char *, const char *) {}
    bool setCACertBundle(const uint8_t *, size_t) { return true; }

    // Stream-loading variants drain the requested bytes so subsequent
    // reads on the same Stream start in a sane place.
    bool loadCACert(Stream &s, size_t len)
    {
        while (len--)
            (void)s.read();
        return true;
    }
    bool loadCertificate(Stream &s, size_t len) { return loadCACert(s, len); }
    bool loadPrivateKey(Stream &s, size_t len) { return loadCACert(s, len); }

#ifdef HOST_ARDUINO_HAVE_OPENSSL
    size_t write(uint8_t b) override { return write(&b, 1); }

    size_t write(const uint8_t *buf, size_t size) override
    {
        if (!tls_->ssl || size == 0)
            return 0;
        size_t total = 0;
        while (total < size)
        {
            const int n = SSL_write(tls_->ssl, buf + total, (int)(size - total));
            if (n > 0)
            {
                total += (size_t)n;
                continue;
            }
            const int err = SSL_get_error(tls_->ssl, n);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
            {
                if (wait_socket(err, 200))
                    continue;
                break;
            }
            HOST_DIAG_ONCE("WiFiClientSecure::write() SSL_write failed; see lastError()");
            break;
        }
        return total;
    }
    using Print::write;

    int available() override
    {
        if (!tls_->ssl)
            return 0;
        const int pending = SSL_pending(tls_->ssl);
        if (pending > 0)
            return pending;
        // Surface kernel-buffered data via a 1-byte peek into the SSL
        // record layer. Returns >0 if at least 1 plaintext byte is now
        // decryptable.
        unsigned char b;
        const int n = SSL_peek(tls_->ssl, &b, 1);
        if (n == 1)
        {
            const int extra = SSL_pending(tls_->ssl);
            return 1 + (extra > 0 ? extra : 0);
        }
        return 0;
    }

    int read() override
    {
        if (!tls_->ssl)
            return -1;
        unsigned char b;
        const int n = SSL_read(tls_->ssl, &b, 1);
        if (n == 1)
            return b;
        return -1;
    }

    int read(uint8_t *buf, size_t size) override
    {
        if (!tls_->ssl || !buf || size == 0)
            return 0;
        const int n = SSL_read(tls_->ssl, buf, (int)size);
        return n > 0 ? n : 0;
    }

    int peek() override
    {
        if (!tls_->ssl)
            return -1;
        unsigned char b;
        const int n = SSL_peek(tls_->ssl, &b, 1);
        return n == 1 ? b : -1;
    }

    void flush() override {}

    void stop() override
    {
        if (tls_)
        {
            if (tls_->ssl)
            {
                SSL_shutdown(tls_->ssl);
                SSL_free(tls_->ssl);
                tls_->ssl = nullptr;
            }
            if (tls_->ctx)
            {
                SSL_CTX_free(tls_->ctx);
                tls_->ctx = nullptr;
            }
        }
        WiFiClient::stop();
    }

    uint8_t connected() override
    {
        // Never delegate to WiFiClient::connected() — that does a raw
        // 1-byte recv() and caches it in the parent's peek_byte, which
        // steals a ciphertext byte out of the TLS record stream and
        // corrupts subsequent SSL_read() calls. Probe via SSL_peek
        // instead.
        if (!tls_ || !tls_->ssl)
            return 0;
        if (SSL_pending(tls_->ssl) > 0)
            return 1;
        unsigned char b;
        const int n = SSL_peek(tls_->ssl, &b, 1);
        if (n > 0)
            return 1;
        if (n == 0)
            return 0; // peer closed cleanly
        const int err = SSL_get_error(tls_->ssl, n);
        return (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) ? 1 : 0;
    }

    operator bool() override { return tls_ && tls_->ssl != nullptr; }

private:
    struct TlsState
    {
        SSL_CTX *ctx = nullptr;
        SSL *ssl = nullptr;
        ~TlsState()
        {
            if (ssl)
            {
                SSL_shutdown(ssl);
                SSL_free(ssl);
            }
            if (ctx)
                SSL_CTX_free(ctx);
        }
    };

    std::shared_ptr<TlsState> tls_;

    bool wait_socket(int ssl_err, int timeout_ms)
    {
        const host_socket_t s = WiFiClient::fd();
        if (s == HOST_SOCKET_INVALID)
            return false;
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(s, &fds);
        timeval tv{};
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        if (ssl_err == SSL_ERROR_WANT_READ)
            return ::select((int)s + 1, &fds, nullptr, nullptr, &tv) > 0;
        if (ssl_err == SSL_ERROR_WANT_WRITE)
            return ::select((int)s + 1, nullptr, &fds, nullptr, &tv) > 0;
        return false;
    }

    int handshake()
    {
        tls_->ctx = SSL_CTX_new(TLS_client_method());
        if (!tls_->ctx)
        {
            HOST_DIAG_ONCE("WiFiClientSecure: SSL_CTX_new() failed");
            WiFiClient::stop();
            return 0;
        }
        SSL_CTX_set_verify(tls_->ctx, SSL_VERIFY_NONE, nullptr);

        tls_->ssl = SSL_new(tls_->ctx);
        if (!tls_->ssl)
        {
            HOST_DIAG_ONCE("WiFiClientSecure: SSL_new() failed");
            SSL_CTX_free(tls_->ctx);
            tls_->ctx = nullptr;
            WiFiClient::stop();
            return 0;
        }
        SSL_set_fd(tls_->ssl, (int)WiFiClient::fd());

        // Non-blocking socket — drive SSL_connect via select() until
        // it returns 1 (success), 0 (controlled shutdown), or <0 with a
        // fatal error. ~5s budget for the entire handshake.
        const uint32_t deadline_ms = 5000;
        const auto start = std::chrono::steady_clock::now();
        while (true)
        {
            const int rc = SSL_connect(tls_->ssl);
            if (rc == 1)
                return 1;
            const int err = SSL_get_error(tls_->ssl, rc);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
            {
                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                         std::chrono::steady_clock::now() - start)
                                         .count();
                if ((uint32_t)elapsed >= deadline_ms)
                {
                    HOST_DIAG_ONCE("WiFiClientSecure: SSL_connect timed out");
                    break;
                }
                wait_socket(err, 200);
                continue;
            }
            HOST_DIAG_ONCE("WiFiClientSecure: SSL_connect failed");
            break;
        }
        SSL_free(tls_->ssl);
        tls_->ssl = nullptr;
        SSL_CTX_free(tls_->ctx);
        tls_->ctx = nullptr;
        WiFiClient::stop();
        return 0;
    }
#endif // HOST_ARDUINO_HAVE_OPENSSL
};

#endif
