#ifndef HOST_ARDUINO_WIFISERVER_H
#define HOST_ARDUINO_WIFISERVER_H

// host-arduino-core WiFiServer implementation (plain TCP listener).
//
// Mirrors the ESP32 API: construct with a port, call `begin()`, then
// poll `available()` from `loop()`. Accepted connections are returned
// as `WiFiClient` values that share the underlying socket via
// shared_ptr — copying or assigning them does not duplicate the fd.

#include <stdint.h>
#include <stddef.h>

#include "HostDiag.h"
#include "HostSocket.h"
#include "Server.h"
#include "WiFiClient.h"

class WiFiServer : public Server
{
public:
    WiFiServer() : listen_sock_(HOST_SOCKET_INVALID), port_(0), last_error_(0) {}
    explicit WiFiServer(uint16_t port) : listen_sock_(HOST_SOCKET_INVALID), port_(port), last_error_(0) {}
    ~WiFiServer() override { end(); }

    void begin(uint16_t port = 0) override
    {
        if (port != 0)
            port_ = port;
        end();
        last_error_ = 0;

        listen_sock_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (listen_sock_ == HOST_SOCKET_INVALID)
        {
            last_error_ = host_socket_errno();
            HOST_DIAG_ONCE("WiFiServer::begin() socket() failed; see lastError()");
            return;
        }

        int one = 1;
        ::setsockopt(listen_sock_, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof(one));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(port_);
        if (::bind(listen_sock_, (sockaddr *)&addr, sizeof(addr)) != 0)
        {
            last_error_ = host_socket_errno();
            HOST_CLOSESOCKET(listen_sock_);
            listen_sock_ = HOST_SOCKET_INVALID;
            HOST_DIAG_ONCE("WiFiServer::begin() bind() failed; see lastError()");
            return;
        }
        if (::listen(listen_sock_, 4) != 0)
        {
            last_error_ = host_socket_errno();
            HOST_CLOSESOCKET(listen_sock_);
            listen_sock_ = HOST_SOCKET_INVALID;
            HOST_DIAG_ONCE("WiFiServer::begin() listen() failed; see lastError()");
            return;
        }
        host_socket_set_nonblocking(listen_sock_);

        if (port_ == 0)
        {
            // Resolve OS-assigned port.
            sockaddr_in actual{};
            socklen_t len = sizeof(actual);
            if (::getsockname(listen_sock_, (sockaddr *)&actual, &len) == 0)
                port_ = ntohs(actual.sin_port);
        }
    }

    // Accept the next pending connection. Returns a connected WiFiClient
    // if one was pending, otherwise an unconnected client (operator bool
    // returns false). Non-blocking — does not wait.
    WiFiClient available()
    {
        if (listen_sock_ == HOST_SOCKET_INVALID)
        {
            HOST_DIAG_ONCE("WiFiServer::available() called before begin()");
            return WiFiClient();
        }
        sockaddr_in addr{};
        socklen_t len = sizeof(addr);
        host_socket_t s = ::accept(listen_sock_, (sockaddr *)&addr, &len);
        if (s == HOST_SOCKET_INVALID)
        {
            const int err = host_socket_errno();
            if (!host_socket_would_block(err))
            {
                last_error_ = err;
                HOST_DIAG_ONCE("WiFiServer::available() accept() failed; see lastError()");
            }
            return WiFiClient();
        }
        return WiFiClient(s);
    }

    WiFiClient accept() { return available(); }

    bool hasClient()
    {
        if (listen_sock_ == HOST_SOCKET_INVALID)
            return false;
        // Peek without consuming: poll-style check via select with zero timeout.
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(listen_sock_, &rfds);
        timeval tv{};
        const int n = ::select((int)listen_sock_ + 1, &rfds, nullptr, nullptr, &tv);
        return n > 0 && FD_ISSET(listen_sock_, &rfds);
    }

    void end()
    {
        if (listen_sock_ != HOST_SOCKET_INVALID)
        {
            HOST_CLOSESOCKET(listen_sock_);
            listen_sock_ = HOST_SOCKET_INVALID;
        }
    }
    void stop() { end(); }
    void close() { end(); }

    operator bool() const { return listen_sock_ != HOST_SOCKET_INVALID; }

    uint16_t port() const { return port_; }
    int lastError() const { return last_error_; }

    // Server::Print interface — broadcast to all clients is not modeled
    // by this minimal server; provided so the class is concrete.
    size_t write(uint8_t) override { return 0; }
    size_t write(const uint8_t *, size_t size) override { return size ? 0 : 0; }
    using Print::write;

private:
    host_socket_t listen_sock_;
    uint16_t port_;
    int last_error_;
};

#endif
