#ifndef HOST_ARDUINO_HOST_SOCKET_H
#define HOST_ARDUINO_HOST_SOCKET_H

// Shared POSIX / Winsock abstraction for host-arduino-core network classes.

#ifdef _WIN32
// Defensive: same windows.h-collision guards as Arduino.h, in case this
// header is reached through an include chain that doesn't go via
// Arduino.h. Idempotent — duplicates with Arduino.h are #ifndef-guarded.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOUSER
#define NOUSER
#endif
#ifndef NOGDI
#define NOGDI
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#define HOST_CLOSESOCKET closesocket
#define HOST_SOCKET_INVALID INVALID_SOCKET
typedef SOCKET host_socket_t;
inline int host_socket_errno() { return WSAGetLastError(); }
inline bool host_socket_would_block(int err) { return err == WSAEWOULDBLOCK; }
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#define HOST_CLOSESOCKET ::close
#define HOST_SOCKET_INVALID (-1)
typedef int host_socket_t;
inline int host_socket_errno() { return errno; }
inline bool host_socket_would_block(int err) { return err == EAGAIN || err == EWOULDBLOCK; }
#endif

inline void host_socket_set_nonblocking(host_socket_t s)
{
#ifdef _WIN32
    u_long mode = 1;
    ::ioctlsocket(s, FIONBIO, &mode);
#else
    const int flags = ::fcntl(s, F_GETFL, 0);
    ::fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif
}

inline int host_socket_bytes_available(host_socket_t s)
{
#ifdef _WIN32
    u_long n = 0;
    if (::ioctlsocket(s, FIONREAD, &n) != 0) return 0;
    return (int)n;
#else
    int n = 0;
    if (::ioctl(s, FIONREAD, &n) != 0) return 0;
    return n;
#endif
}

#endif
