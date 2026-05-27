#ifndef HOST_ARDUINO_HTTPCLIENT_H
#define HOST_ARDUINO_HTTPCLIENT_H

// Minimal host-side HTTPClient that mirrors the ESP32 API surface most
// commonly used for "talk to an external HTTP/HTTPS API" sketches:
//
//   - begin(url) / begin(client, url): supports http:// and https://.
//     Internal-client form auto-selects WiFiClient or WiFiClientSecure
//     based on the URL scheme; the latter only works when the
//     `TLS=OpenSSL` board menu option is selected.
//   - addHeader(name, value): accumulate request headers.
//   - GET() / POST(payload) / sendRequest(verb, payload).
//   - getString(): drain the response body. Both Content-Length and
//     `Transfer-Encoding: chunked` bodies are decoded.
//   - getStream() / getStreamPtr(): stream-style body access.
//
// Scope explicitly NOT covered:
//   - Automatic redirect following — check the status code (3xx) and
//     getLocation(), then re-issue.
//   - Keep-alive / connection reuse — every request opens a fresh
//     connection (`Connection: close`).
//   - Multipart, gzip, cookies, basic-auth helpers.
//
// Sketches that need richer behavior should drop to the raw
// WiFiClient / WiFiClientSecure layer.

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <memory>
#include <vector>

#include "Client.h"
#include "HostDiag.h"
#include "Stream.h"
#include "WString.h"
#include "WiFiClient.h"
#include "WiFiClientSecure.h"

#define HTTPC_ERROR_CONNECTION_REFUSED (-1)
#define HTTPC_ERROR_SEND_HEADER_FAILED (-2)
#define HTTPC_ERROR_SEND_PAYLOAD_FAILED (-3)
#define HTTPC_ERROR_NOT_CONNECTED (-4)
#define HTTPC_ERROR_CONNECTION_LOST (-5)
#define HTTPC_ERROR_NO_STREAM (-6)
#define HTTPC_ERROR_NO_HTTP_SERVER (-7)
#define HTTPC_ERROR_ENCODING (-9)
#define HTTPC_ERROR_READ_TIMEOUT (-11)
#define HTTPC_ERROR_TLS_BACKEND_MISSING (-100)

class HTTPClient
{
public:
    HTTPClient() : timeout_ms_(5000), content_length_(-1) {}
    ~HTTPClient() { end(); }

    bool begin(const String &url)
    {
        end();
        if (!parseURL(url))
            return false;
        if (use_https_)
        {
#ifdef HOST_ARDUINO_HAVE_OPENSSL
            WiFiClientSecure *cs = new WiFiClientSecure();
            cs->setInsecure();
            owned_client_.reset(cs);
            client_ = owned_client_.get();
#else
            HOST_DIAG_ONCE("HTTPClient: https:// URL requires TLS=OpenSSL board menu option");
            return false;
#endif
        }
        else
        {
            owned_client_.reset(new WiFiClient());
            client_ = owned_client_.get();
        }
        return true;
    }

    bool begin(Client &client, const String &url)
    {
        end();
        if (!parseURL(url))
            return false;
        owned_client_.reset();
        client_ = &client;
        return true;
    }

    bool begin(const String &host, uint16_t port, const String &uri = "/", bool https = false)
    {
        String url = (https ? String("https://") : String("http://")) + host;
        if ((https && port != 443) || (!https && port != 80))
        {
            url += ":";
            url += String((unsigned long)port);
        }
        url += uri;
        return begin(url);
    }

    void end()
    {
        if (client_)
            client_->stop();
        owned_client_.reset();
        client_ = nullptr;
        request_headers_.clear();
        location_ = String();
        content_length_ = -1;
    }

    void setTimeout(uint16_t timeout_ms) { timeout_ms_ = timeout_ms; }
    void setUserAgent(const String &ua) { user_agent_ = ua; }
    void setAuthorization(const char *auth)
    {
        if (auth && *auth)
            addHeader(F_AUTH, String(auth), true, true);
    }

    void addHeader(const String &name, const String &value, bool first = false, bool replace = true)
    {
        if (replace)
        {
            for (size_t i = 0; i < request_headers_.size(); ++i)
            {
                if (equalsIgnoreCase(request_headers_[i].name, name))
                {
                    request_headers_[i].value = value;
                    return;
                }
            }
        }
        Header h{name, value};
        if (first)
            request_headers_.insert(request_headers_.begin(), h);
        else
            request_headers_.push_back(h);
    }

    int GET() { return sendRequest("GET", (const uint8_t *)nullptr, 0); }
    int POST(const String &payload) { return sendRequest("POST", (const uint8_t *)payload.c_str(), payload.length()); }
    int POST(const uint8_t *payload, size_t size) { return sendRequest("POST", payload, size); }
    int PUT(const String &payload) { return sendRequest("PUT", (const uint8_t *)payload.c_str(), payload.length()); }
    int PATCH(const String &payload) { return sendRequest("PATCH", (const uint8_t *)payload.c_str(), payload.length()); }

    int sendRequest(const char *verb, const String &payload)
    {
        return sendRequest(verb, (const uint8_t *)payload.c_str(), payload.length());
    }

    int sendRequest(const char *verb, const uint8_t *payload, size_t size)
    {
        if (!client_)
            return HTTPC_ERROR_NOT_CONNECTED;
        if (!client_->connected())
        {
            if (!client_->connect(host_.c_str(), port_))
                return HTTPC_ERROR_CONNECTION_REFUSED;
        }

        String req;
        req.reserve(256);
        req += verb;
        req += " ";
        req += uri_.length() ? uri_ : String("/");
        req += " HTTP/1.1\r\nHost: ";
        req += host_;
        if ((use_https_ && port_ != 443) || (!use_https_ && port_ != 80))
        {
            req += ":";
            req += String((unsigned long)port_);
        }
        req += "\r\nUser-Agent: ";
        req += user_agent_.length() ? user_agent_ : String("host-arduino-core/1");
        req += "\r\nConnection: close\r\n";
        if (size > 0)
        {
            req += "Content-Length: ";
            req += String((unsigned long)size);
            req += "\r\n";
        }
        for (size_t i = 0; i < request_headers_.size(); ++i)
        {
            req += request_headers_[i].name;
            req += ": ";
            req += request_headers_[i].value;
            req += "\r\n";
        }
        req += "\r\n";

        const size_t wrote_h = client_->write((const uint8_t *)req.c_str(), req.length());
        if (wrote_h != req.length())
            return HTTPC_ERROR_SEND_HEADER_FAILED;
        if (size > 0)
        {
            const size_t wrote_b = client_->write(payload, size);
            if (wrote_b != size)
                return HTTPC_ERROR_SEND_PAYLOAD_FAILED;
        }
        return readResponseHead();
    }

    int getSize() const { return content_length_; }
    const String &getLocation() const { return location_; }

    String getString()
    {
        String body;
        if (!client_)
            return body;

        if (chunked_)
        {
            // Transfer-Encoding: chunked decoding.
            while (true)
            {
                const String size_line = readHTTPLine();
                if (size_line.length() == 0)
                {
                    // Some servers emit a stray blank line after the
                    // terminator; treat as end of stream.
                    break;
                }
                const long chunk_size = strtol(size_line.c_str(), nullptr, 16);
                if (chunk_size <= 0)
                {
                    // Terminator chunk. Drain optional trailing headers
                    // (rare) until a blank line.
                    while (readHTTPLine().length() > 0)
                    {
                    }
                    break;
                }
                long remaining = chunk_size;
                while (remaining > 0)
                {
                    if (!waitData())
                        return body;
                    uint8_t buf[256];
                    const size_t want = (size_t)((remaining < (long)sizeof(buf)) ? remaining : (long)sizeof(buf));
                    const int got = client_->read(buf, want);
                    if (got <= 0)
                        return body;
                    appendBytes(body, buf, (size_t)got);
                    remaining -= got;
                }
                // Discard the trailing \r\n after each chunk.
                (void)readHTTPLine();
            }
            return body;
        }

        if (content_length_ >= 0)
            body.reserve((size_t)content_length_);

        long remaining = content_length_; // -1 means "read until EOF"
        while (remaining != 0)
        {
            if (!waitData())
                break;
            uint8_t buf[256];
            size_t want = sizeof(buf);
            if (remaining > 0 && (long)want > remaining)
                want = (size_t)remaining;
            const int got = client_->read(buf, want);
            if (got <= 0)
            {
                if (!client_->connected())
                    break;
                continue;
            }
            appendBytes(body, buf, (size_t)got);
            if (remaining > 0)
                remaining -= got;
        }
        return body;
    }

    Stream &getStream()
    {
        // If somebody calls getStream() without begin(), they get a
        // reference to the dummy client, which behaves like a closed
        // socket. Same shape as ESP32 returns.
        if (!client_)
        {
            HOST_DIAG_ONCE("HTTPClient::getStream() called without begin()");
        }
        return client_ ? *static_cast<Stream *>(client_) : *static_cast<Stream *>(&dummy_);
    }
    Client *getStreamPtr() { return client_; }
    Client *client() { return client_; }

    bool connected() { return client_ && client_->connected(); }

private:
    struct Header
    {
        String name;
        String value;
    };

    std::unique_ptr<Client> owned_client_;
    Client *client_ = nullptr;
    WiFiClient dummy_; // returned from getStream() when not configured

    String host_;
    String uri_;
    uint16_t port_ = 80;
    bool use_https_ = false;

    String user_agent_;
    std::vector<Header> request_headers_;

    uint16_t timeout_ms_;
    long content_length_;
    bool chunked_ = false;
    String location_;

    static constexpr const char *F_AUTH = "Authorization";

    static bool equalsIgnoreCase(const String &a, const String &b)
    {
        if (a.length() != b.length())
            return false;
        for (size_t i = 0; i < a.length(); ++i)
        {
            char ca = a.charAt(i);
            char cb = b.charAt(i);
            if (ca >= 'A' && ca <= 'Z')
                ca = (char)(ca - 'A' + 'a');
            if (cb >= 'A' && cb <= 'Z')
                cb = (char)(cb - 'A' + 'a');
            if (ca != cb)
                return false;
        }
        return true;
    }

    static void appendBytes(String &dst, const uint8_t *buf, size_t n)
    {
        // String::concat is null-terminated only; copy through a small
        // null-terminated staging buffer. HTTP bodies are usually text;
        // for binary use getStream() / read() directly.
        char stage[257];
        while (n > 0)
        {
            const size_t take = n < (sizeof(stage) - 1) ? n : (sizeof(stage) - 1);
            memcpy(stage, buf, take);
            stage[take] = '\0';
            dst.concat(stage);
            buf += take;
            n -= take;
        }
    }

    bool parseURL(const String &url)
    {
        host_.remove(0);
        uri_.remove(0);
        port_ = 80;
        use_https_ = false;

        int scheme_end = url.indexOf("://");
        if (scheme_end < 0)
            return false;
        String scheme = url.substring(0, scheme_end);
        scheme.toLowerCase();
        if (scheme == "https")
        {
            use_https_ = true;
            port_ = 443;
        }
        else if (scheme != "http")
        {
            return false;
        }
        String rest = url.substring(scheme_end + 3);
        int slash = rest.indexOf('/');
        String authority = slash < 0 ? rest : rest.substring(0, slash);
        uri_ = slash < 0 ? String("/") : rest.substring(slash);
        int colon = authority.indexOf(':');
        if (colon >= 0)
        {
            host_ = authority.substring(0, colon);
            port_ = (uint16_t)authority.substring(colon + 1).toInt();
        }
        else
        {
            host_ = authority;
        }
        return host_.length() > 0;
    }

    int readResponseHead()
    {
        content_length_ = -1;
        chunked_ = false;
        location_.remove(0);

        const String status = readHTTPLine();
        if (!status.startsWith("HTTP/"))
            return HTTPC_ERROR_NO_HTTP_SERVER;
        const int sp1 = status.indexOf(' ');
        if (sp1 < 0)
            return HTTPC_ERROR_NO_HTTP_SERVER;
        const int sp2 = status.indexOf(' ', sp1 + 1);
        const String code_str = sp2 < 0 ? status.substring(sp1 + 1) : status.substring(sp1 + 1, sp2);
        const int code = code_str.toInt();
        if (code <= 0)
            return HTTPC_ERROR_NO_HTTP_SERVER;

        while (true)
        {
            const String line = readHTTPLine();
            if (line.length() == 0)
                break;
            const int colon = line.indexOf(':');
            if (colon < 0)
                continue;
            String name = line.substring(0, colon);
            String value = line.substring(colon + 1);
            value.trim();
            if (equalsIgnoreCase(name, String("Content-Length")))
                content_length_ = value.toInt();
            else if (equalsIgnoreCase(name, String("Transfer-Encoding")))
            {
                String lv = value;
                lv.toLowerCase();
                if (lv.indexOf("chunked") >= 0)
                    chunked_ = true;
            }
            else if (equalsIgnoreCase(name, String("Location")))
                location_ = value;
        }
        return code;
    }

    String readHTTPLine()
    {
        String out;
        while (true)
        {
            if (!waitData())
                break;
            const int c = client_->read();
            if (c < 0)
                break;
            if (c == '\n')
            {
                if (out.length() > 0 && out.charAt(out.length() - 1) == '\r')
                    out.remove(out.length() - 1);
                break;
            }
            out += (char)c;
        }
        return out;
    }

    // Wait for at least one byte to be available, or for the connection
    // to clearly close. Returns true if data is ready, false on timeout
    // or final EOF.
    bool waitData()
    {
        if (!client_)
            return false;
        if (client_->available() > 0)
            return true;
        const uint32_t deadline = millis() + timeout_ms_;
        while (millis() < deadline)
        {
            if (client_->available() > 0)
                return true;
            if (!client_->connected())
                return client_->available() > 0;
            delay(1);
        }
        return false;
    }
};

#endif
