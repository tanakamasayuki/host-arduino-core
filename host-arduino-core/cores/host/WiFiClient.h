#ifndef HOST_ARDUINO_WIFICLIENT_H
#define HOST_ARDUINO_WIFICLIENT_H

// host-arduino-core WiFiClient implementation (plain TCP).
//
// Copy semantics: WiFiClient is value-copyable in Arduino-land — copies
// must share the underlying socket so that
// `WiFiClient c = server.available();` works. We store state in a
// shared_ptr; closing one copy closes them all.
//
// I/O model: socket is non-blocking after connect, mirroring `WiFiUDP`.
// `read()` returns -1 with no data so `Stream::timedRead()` polls until
// the configured timeout.

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <memory>

#include "Client.h"
#include "HostDiag.h"
#include "HostSocket.h"
#include "IPAddress.h"

class WiFiClient : public Client
{
public:
    WiFiClient() : state_(std::make_shared<State>()) {}

    // Wrap an already-accepted socket (used by WiFiServer::available()).
    explicit WiFiClient(host_socket_t accepted_sock) : state_(std::make_shared<State>())
    {
        state_->sock = accepted_sock;
        if (accepted_sock != HOST_SOCKET_INVALID)
        {
            host_socket_set_nonblocking(accepted_sock);
            capture_peer(accepted_sock);
        }
    }

    int connect(IPAddress ip, uint16_t port) override
    {
        stop();
        state_->last_error = 0;

        host_socket_t s = ::socket(AF_INET, SOCK_STREAM, 0);
        if (s == HOST_SOCKET_INVALID)
        {
            state_->last_error = host_socket_errno();
            HOST_DIAG_ONCE("WiFiClient::connect() socket() failed; see lastError()");
            return 0;
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        uint32_t raw;
        memcpy(&raw, ip.raw_address(), 4);
        addr.sin_addr.s_addr = raw;

        if (::connect(s, (sockaddr *)&addr, sizeof(addr)) != 0)
        {
            state_->last_error = host_socket_errno();
            HOST_CLOSESOCKET(s);
            HOST_DIAG_ONCE("WiFiClient::connect() connect() failed; see lastError()");
            return 0;
        }

        state_->sock = s;
        state_->remote_ip = ip;
        state_->remote_port = port;
        host_socket_set_nonblocking(s);
        return 1;
    }

    int connect(const char *host, uint16_t port) override
    {
        if (!host)
            return 0;

        IPAddress ip;
        if (ip.fromString(host))
            return connect(ip, port);

        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        addrinfo *res = nullptr;
        if (::getaddrinfo(host, nullptr, &hints, &res) != 0 || !res)
        {
            HOST_DIAG_ONCE("WiFiClient::connect(host) getaddrinfo failed");
            return 0;
        }
        const sockaddr_in *sa = (const sockaddr_in *)res->ai_addr;
        IPAddress resolved;
        memcpy(resolved.raw_address(), &sa->sin_addr.s_addr, 4);
        ::freeaddrinfo(res);
        return connect(resolved, port);
    }

    size_t write(uint8_t b) override { return write(&b, 1); }

    size_t write(const uint8_t *buf, size_t size) override
    {
        if (state_->sock == HOST_SOCKET_INVALID || size == 0)
            return 0;
        size_t total = 0;
        while (total < size)
        {
            const ssize_t n = ::send(state_->sock, (const char *)(buf + total), size - total, 0);
            if (n > 0)
            {
                total += (size_t)n;
                continue;
            }
            if (n < 0)
            {
                const int err = host_socket_errno();
                if (host_socket_would_block(err))
                    continue; // retry until non-blocking sendable
                state_->last_error = err;
                HOST_DIAG_ONCE("WiFiClient::write() send() failed; see lastError()");
            }
            break;
        }
        return total;
    }
    using Print::write;

    int available() override
    {
        if (state_->sock == HOST_SOCKET_INVALID)
            return state_->peek_byte >= 0 ? 1 : 0;
        const int n = host_socket_bytes_available(state_->sock);
        return n + (state_->peek_byte >= 0 ? 1 : 0);
    }

    int read() override
    {
        if (state_->peek_byte >= 0)
        {
            const int b = state_->peek_byte;
            state_->peek_byte = -1;
            return b;
        }
        if (state_->sock == HOST_SOCKET_INVALID)
            return -1;
        uint8_t b;
        const ssize_t n = ::recv(state_->sock, (char *)&b, 1, 0);
        if (n == 1)
            return b;
        if (n == 0)
        {
            close_socket();
            return -1;
        }
        const int err = host_socket_errno();
        if (!host_socket_would_block(err))
        {
            state_->last_error = err;
            HOST_DIAG_ONCE("WiFiClient::read() recv() failed; see lastError()");
            close_socket();
        }
        return -1;
    }

    int read(uint8_t *buf, size_t size) override
    {
        if (!buf || size == 0)
            return 0;
        size_t out = 0;
        if (state_->peek_byte >= 0)
        {
            buf[out++] = (uint8_t)state_->peek_byte;
            state_->peek_byte = -1;
        }
        if (out >= size || state_->sock == HOST_SOCKET_INVALID)
            return (int)out;
        const ssize_t n = ::recv(state_->sock, (char *)(buf + out), size - out, 0);
        if (n > 0)
            return (int)(out + (size_t)n);
        if (n == 0)
        {
            close_socket();
            return out > 0 ? (int)out : 0;
        }
        const int err = host_socket_errno();
        if (!host_socket_would_block(err))
        {
            state_->last_error = err;
            HOST_DIAG_ONCE("WiFiClient::read(buf) recv() failed; see lastError()");
            close_socket();
        }
        return (int)out;
    }

    int peek() override
    {
        if (state_->peek_byte >= 0)
            return state_->peek_byte;
        if (state_->sock == HOST_SOCKET_INVALID)
            return -1;
        uint8_t b;
        const ssize_t n = ::recv(state_->sock, (char *)&b, 1, 0);
        if (n == 1)
        {
            state_->peek_byte = b;
            return b;
        }
        if (n == 0)
            close_socket();
        return -1;
    }

    void flush() override {}

    void stop() override
    {
        close_socket();
        state_->peek_byte = -1;
    }

    uint8_t connected() override
    {
        if (state_->peek_byte >= 0)
            return 1;
        if (state_->sock == HOST_SOCKET_INVALID)
            return 0;
        // Probe: if the peer has closed and rx buffer is empty, recv()
        // returns 0; cache otherwise.
        uint8_t b;
        const ssize_t n = ::recv(state_->sock, (char *)&b, 1, 0);
        if (n == 1)
        {
            state_->peek_byte = b;
            return 1;
        }
        if (n == 0)
        {
            close_socket();
            return 0;
        }
        const int err = host_socket_errno();
        if (host_socket_would_block(err))
            return 1; // still connected, just no data
        state_->last_error = err;
        close_socket();
        return 0;
    }

    operator bool() override { return state_->sock != HOST_SOCKET_INVALID || state_->peek_byte >= 0; }

    IPAddress remoteIP() const { return state_->remote_ip; }
    uint16_t remotePort() const { return state_->remote_port; }
    int lastError() const { return state_->last_error; }
    void clearError() { state_->last_error = 0; }

    // For WiFiServer's convenience: expose the underlying socket fd.
    host_socket_t fd() const { return state_->sock; }

private:
    struct State
    {
        host_socket_t sock = HOST_SOCKET_INVALID;
        int peek_byte = -1;
        int last_error = 0;
        IPAddress remote_ip;
        uint16_t remote_port = 0;
        ~State()
        {
            if (sock != HOST_SOCKET_INVALID)
                HOST_CLOSESOCKET(sock);
        }
    };

    void close_socket()
    {
        if (state_->sock != HOST_SOCKET_INVALID)
        {
            HOST_CLOSESOCKET(state_->sock);
            state_->sock = HOST_SOCKET_INVALID;
        }
    }

    void capture_peer(host_socket_t s)
    {
        sockaddr_in addr{};
        socklen_t len = sizeof(addr);
        if (::getpeername(s, (sockaddr *)&addr, &len) == 0)
        {
            memcpy(state_->remote_ip.raw_address(), &addr.sin_addr.s_addr, 4);
            state_->remote_port = ntohs(addr.sin_port);
        }
    }

    std::shared_ptr<State> state_;
};

#endif
