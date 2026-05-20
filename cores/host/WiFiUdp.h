#ifndef HOST_ARDUINO_WIFIUDP_H
#define HOST_ARDUINO_WIFIUDP_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t_compat;
#define HOST_CLOSESOCKET closesocket
#define HOST_SOCKET_INVALID INVALID_SOCKET
typedef SOCKET host_socket_t;
inline int host_socket_errno() { return WSAGetLastError(); }
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#define HOST_CLOSESOCKET ::close
#define HOST_SOCKET_INVALID (-1)
typedef int host_socket_t;
inline int host_socket_errno() { return errno; }
#endif

#include "IPAddress.h"
#include "Udp.h"

class WiFiUDP : public UDP
{
public:
    WiFiUDP() : sock_(HOST_SOCKET_INVALID), tx_port_(0), rx_pos_(0), remote_port_(0), last_error_(0) {}
    ~WiFiUDP() override { stop(); }

    uint8_t begin(uint16_t port) override
    {
        stop();
        last_error_ = 0;
        sock_ = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (sock_ == HOST_SOCKET_INVALID)
        {
            last_error_ = host_socket_errno();
            return 0;
        }

        int one = 1;
        ::setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof(one));
        ::setsockopt(sock_, SOL_SOCKET, SO_BROADCAST, (const char *)&one, sizeof(one));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(port);
        if (::bind(sock_, (sockaddr *)&addr, sizeof(addr)) != 0)
        {
            last_error_ = host_socket_errno();
            HOST_CLOSESOCKET(sock_);
            sock_ = HOST_SOCKET_INVALID;
            return 0;
        }
        set_nonblocking(sock_);
        return 1;
    }

    void stop() override
    {
        if (sock_ != HOST_SOCKET_INVALID)
        {
            HOST_CLOSESOCKET(sock_);
            sock_ = HOST_SOCKET_INVALID;
        }
        tx_buf_.clear();
        rx_buf_.clear();
        rx_pos_ = 0;
    }

    int beginPacket(IPAddress ip, uint16_t port) override
    {
        tx_buf_.clear();
        tx_addr_ = sockaddr_in{};
        tx_addr_.sin_family = AF_INET;
        tx_addr_.sin_port = htons(port);
        uint32_t raw;
        memcpy(&raw, ip.raw_address(), 4);
        tx_addr_.sin_addr.s_addr = raw;
        tx_port_ = port;
        return 1;
    }

    int beginPacket(const char *host, uint16_t port) override
    {
        if (!host)
            return 0;
        IPAddress ip;
        if (ip.fromString(host))
        {
            return beginPacket(ip, port);
        }
        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        addrinfo *res = nullptr;
        if (::getaddrinfo(host, nullptr, &hints, &res) != 0 || !res)
        {
            return 0;
        }
        const sockaddr_in *sa = (const sockaddr_in *)res->ai_addr;
        tx_buf_.clear();
        tx_addr_ = *sa;
        tx_addr_.sin_port = htons(port);
        tx_port_ = port;
        ::freeaddrinfo(res);
        return 1;
    }

    int endPacket() override
    {
        if (sock_ == HOST_SOCKET_INVALID)
            return 0;
        const ssize_t n = ::sendto(sock_, (const char *)tx_buf_.data(),
                                   tx_buf_.size(), 0,
                                   (sockaddr *)&tx_addr_, sizeof(tx_addr_));
        if (n < 0)
            last_error_ = host_socket_errno();
        tx_buf_.clear();
        return (n >= 0) ? 1 : 0;
    }

    size_t write(uint8_t b) override
    {
        tx_buf_.push_back(b);
        return 1;
    }
    size_t write(const uint8_t *buffer, size_t size) override
    {
        tx_buf_.insert(tx_buf_.end(), buffer, buffer + size);
        return size;
    }
    using Print::write;

    int parsePacket() override
    {
        if (sock_ == HOST_SOCKET_INVALID)
            return 0;
        rx_pos_ = 0;
        rx_buf_.resize(65535); // max UDP payload — kept allocated as a member

        sockaddr_in from{};
        socklen_t fromlen = sizeof(from);
        const ssize_t n = ::recvfrom(sock_, (char *)rx_buf_.data(), rx_buf_.size(), 0,
                                     (sockaddr *)&from, &fromlen);
        if (n <= 0)
        {
            if (n < 0)
                last_error_ = host_socket_errno();
            rx_buf_.clear();
            return 0;
        }

        rx_buf_.resize((size_t)n);
        memcpy(remote_ip_.raw_address(), &from.sin_addr.s_addr, 4);
        remote_port_ = ntohs(from.sin_port);
        return (int)rx_buf_.size();
    }

    int available() override { return (int)(rx_buf_.size() - rx_pos_); }

    int read() override
    {
        if (rx_pos_ >= rx_buf_.size())
            return -1;
        return rx_buf_[rx_pos_++];
    }
    int read(unsigned char *buffer, size_t len) override
    {
        const size_t remaining = rx_buf_.size() - rx_pos_;
        const size_t n = (len < remaining) ? len : remaining;
        if (n == 0)
            return 0;
        memcpy(buffer, rx_buf_.data() + rx_pos_, n);
        rx_pos_ += n;
        return (int)n;
    }
    int peek() override
    {
        if (rx_pos_ >= rx_buf_.size())
            return -1;
        return rx_buf_[rx_pos_];
    }
    using UDP::read;

    IPAddress remoteIP() override { return remote_ip_; }
    uint16_t remotePort() override { return remote_port_; }

    uint16_t localPort()
    {
        if (sock_ == HOST_SOCKET_INVALID)
            return 0;
        sockaddr_in addr{};
        socklen_t len = sizeof(addr);
        if (::getsockname(sock_, (sockaddr *)&addr, &len) != 0)
            return 0;
        return ntohs(addr.sin_port);
    }

    // errno (POSIX) or WSAGetLastError() (Windows) from the most recent
    // failed begin / endPacket / parsePacket. 0 means no error recorded.
    int lastError() const { return last_error_; }

private:
    static void set_nonblocking(host_socket_t s)
    {
#ifdef _WIN32
        u_long mode = 1;
        ::ioctlsocket(s, FIONBIO, &mode);
#else
        const int flags = ::fcntl(s, F_GETFL, 0);
        ::fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif
    }

    host_socket_t sock_;
    std::vector<uint8_t> tx_buf_;
    sockaddr_in tx_addr_{};
    uint16_t tx_port_;

    std::vector<uint8_t> rx_buf_;
    size_t rx_pos_;
    IPAddress remote_ip_;
    uint16_t remote_port_;
    int last_error_;
};

#endif
