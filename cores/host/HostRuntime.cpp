#include "HostRuntime.h"

#include <atomic>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
typedef SOCKET host_socket_t;
static const host_socket_t invalid_socket_value = INVALID_SOCKET;
#else
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
typedef int host_socket_t;
static const host_socket_t invalid_socket_value = -1;
#endif

SerialClass Serial;

namespace {

const char *kChildArg = "--host-arduino-child";
const unsigned long kDefaultConnectTimeoutMs = 10000;
const unsigned long kDefaultParentWaitMs = 5000;
const size_t kDefaultSerialBufferLimit = 65536;

enum LogLevel {
    LOG_ERROR = 0,
    LOG_INFO = 1,
    LOG_DEBUG = 2
};

std::atomic<bool> g_stop(false);
std::atomic<bool> g_connected_once(false);
std::atomic<bool> g_server_started(false);
std::thread g_server_thread;
std::mutex g_serial_mutex;
std::mutex g_log_mutex;
std::string g_output_buffer;
std::string g_input_buffer;
size_t g_output_limit = kDefaultSerialBufferLimit;
host_socket_t g_client_socket = invalid_socket_value;
std::string g_info_path;
std::string g_log_path;
bool g_log_enabled = true;
LogLevel g_log_level = LOG_INFO;
std::string g_exit_reason = "unknown";

unsigned long envUnsignedLong(const char *name, unsigned long fallback)
{
    const char *value = std::getenv(name);
    if (!value || !*value) {
        return fallback;
    }
    char *end = NULL;
    const unsigned long parsed = std::strtoul(value, &end, 10);
    return end && *end == '\0' ? parsed : fallback;
}

size_t envSize(const char *name, size_t fallback)
{
    return static_cast<size_t>(envUnsignedLong(name, static_cast<unsigned long>(fallback)));
}

void closeSocket(host_socket_t sock)
{
    if (sock == invalid_socket_value) {
        return;
    }
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
}

int lastSocketError()
{
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

bool isWouldBlock(int err)
{
#ifdef _WIN32
    return err == WSAEWOULDBLOCK;
#else
    return err == EAGAIN || err == EWOULDBLOCK;
#endif
}

void setNonBlocking(host_socket_t sock)
{
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    const int flags = fcntl(sock, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    }
#endif
}

std::string executablePath()
{
#ifdef _WIN32
    char path[MAX_PATH];
    const DWORD len = GetModuleFileNameA(NULL, path, sizeof(path));
    return len > 0 ? std::string(path, len) : std::string();
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(NULL, &size);
    std::vector<char> path(size + 1);
    if (_NSGetExecutablePath(path.data(), &size) == 0) {
        return std::string(path.data());
    }
    return std::string();
#else
    char path[4096];
    const ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len > 0) {
        path[len] = '\0';
        return std::string(path);
    }
    return std::string();
#endif
}

std::string infoPathForExecutable(const std::string &exe)
{
    return exe + ".host-arduino.json";
}

std::string logPathForExecutable(const std::string &exe)
{
    return exe + ".host-arduino.log";
}

bool envIsFalse(const char *value)
{
    return value && (
        std::strcmp(value, "0") == 0 ||
        std::strcmp(value, "false") == 0 ||
        std::strcmp(value, "FALSE") == 0 ||
        std::strcmp(value, "off") == 0 ||
        std::strcmp(value, "OFF") == 0);
}

LogLevel parseLogLevel()
{
    const char *value = std::getenv("HOST_ARDUINO_LOG_LEVEL");
    if (!value || !*value) {
        return LOG_INFO;
    }
    if (std::strcmp(value, "debug") == 0 || std::strcmp(value, "DEBUG") == 0) {
        return LOG_DEBUG;
    }
    if (std::strcmp(value, "error") == 0 || std::strcmp(value, "ERROR") == 0) {
        return LOG_ERROR;
    }
    return LOG_INFO;
}

const char *logLevelName(LogLevel level)
{
    switch (level) {
    case LOG_ERROR:
        return "error";
    case LOG_DEBUG:
        return "debug";
    case LOG_INFO:
    default:
        return "info";
    }
}

std::string timestampNow()
{
    std::time_t now = std::time(NULL);
    std::tm tm_value;
#ifdef _WIN32
    localtime_s(&tm_value, &now);
#else
    localtime_r(&now, &tm_value);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S%z", &tm_value);
    return std::string(buf);
}

void initLog(const std::string &exe, bool truncate)
{
    const char *log_env = std::getenv("HOST_ARDUINO_LOG");
    if (envIsFalse(log_env)) {
        g_log_enabled = false;
        return;
    }
    g_log_enabled = true;
    g_log_path = (log_env && *log_env) ? std::string(log_env) : logPathForExecutable(exe);
    g_log_level = parseLogLevel();
    if (truncate) {
        std::ofstream clear(g_log_path.c_str(), std::ios::trunc);
    }
}

void logLine(LogLevel level, const std::string &event, const std::string &message)
{
    if (!g_log_enabled || level > g_log_level || g_log_path.empty()) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_log_mutex);
    std::ofstream out(g_log_path.c_str(), std::ios::app);
    if (!out) {
        return;
    }
    out << timestampNow()
        << " " << logLevelName(level)
        << " event=" << event;
    if (!message.empty()) {
        out << " " << message;
    }
    out << "\n";
}

void setExitReason(const std::string &reason)
{
    std::lock_guard<std::mutex> lock(g_log_mutex);
    if (g_exit_reason == "unknown") {
        g_exit_reason = reason;
    }
}

bool hasChildArg(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], kChildArg) == 0) {
            return true;
        }
    }
    return false;
}

bool startDetachedChild(const std::string &exe)
{
#ifdef _WIN32
    std::string command = "\"" + exe + "\" " + kChildArg;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    std::memset(&si, 0, sizeof(si));
    std::memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);
    BOOL ok = CreateProcessA(
        NULL,
        &command[0],
        NULL,
        NULL,
        FALSE,
        DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP,
        NULL,
        NULL,
        &si,
        &pi);
    if (!ok) {
        logLine(LOG_ERROR, "child_start_failed", "api=CreateProcess");
        return false;
    }
    std::ostringstream oss;
    oss << "pid=" << static_cast<unsigned long>(pi.dwProcessId);
    logLine(LOG_INFO, "child_started", oss.str());
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
#else
    const pid_t pid = fork();
    if (pid < 0) {
        logLine(LOG_ERROR, "child_start_failed", "api=fork");
        return false;
    }
    if (pid > 0) {
        std::ostringstream oss;
        oss << "pid=" << static_cast<long>(pid);
        logLine(LOG_INFO, "child_started", oss.str());
        return true;
    }
    setsid();
    signal(SIGPIPE, SIG_IGN);
    const int devnull = open("/dev/null", O_RDWR);
    if (devnull >= 0) {
        dup2(devnull, STDIN_FILENO);
        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
        if (devnull > STDERR_FILENO) {
            close(devnull);
        }
    }
    execl(exe.c_str(), exe.c_str(), kChildArg, static_cast<char *>(NULL));
    _exit(127);
#endif
}

bool readInfoFile(const std::string &path, std::string *content)
{
    std::ifstream in(path.c_str());
    if (!in) {
        return false;
    }
    std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (data.find("\"port\"") == std::string::npos) {
        return false;
    }
    *content = data;
    return true;
}

int parsePort(const std::string &content)
{
    const std::string key = "\"port\"";
    const size_t pos = content.find(key);
    if (pos == std::string::npos) {
        return 0;
    }
    const size_t colon = content.find(':', pos + key.size());
    if (colon == std::string::npos) {
        return 0;
    }
    return std::atoi(content.c_str() + colon + 1);
}

int currentPid()
{
#ifdef _WIN32
    return static_cast<int>(GetCurrentProcessId());
#else
    return static_cast<int>(getpid());
#endif
}

void writeInfoFile(int port)
{
    std::ofstream out(g_info_path.c_str(), std::ios::trunc);
    out << "{\n";
    out << "  \"pid\": " << currentPid() << ",\n";
    out << "  \"port\": " << port << "\n";
    out << "}\n";
    std::ostringstream oss;
    oss << "path=" << g_info_path << " port=" << port << " pid=" << currentPid();
    logLine(LOG_INFO, "info_file_written", oss.str());
}

void appendOutputLocked(const char *data, size_t len)
{
    if (g_output_limit == 0) {
        return;
    }
    if (len >= g_output_limit) {
        g_output_buffer.assign(data + (len - g_output_limit), g_output_limit);
        return;
    }
    if (g_output_buffer.size() + len > g_output_limit) {
        g_output_buffer.erase(0, g_output_buffer.size() + len - g_output_limit);
    }
    g_output_buffer.append(data, len);
}

void sendToClient(const char *data, size_t len)
{
    if (g_client_socket == invalid_socket_value || len == 0) {
        return;
    }
    size_t offset = 0;
    unsigned would_block_retries = 0;
    while (offset < len && !g_stop) {
#ifdef _WIN32
        const int flags = 0;
#else
        const int flags = MSG_NOSIGNAL;
#endif
        const int chunk_len = static_cast<int>(
            std::min(len - offset, static_cast<size_t>(1024)));
        const int sent = send(g_client_socket, data + offset, chunk_len, flags);
        if (sent > 0) {
            offset += static_cast<size_t>(sent);
            would_block_retries = 0;
            continue;
        }
        const int err = lastSocketError();
        if (sent < 0 && isWouldBlock(err) && would_block_retries++ < 1000) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        setExitReason("tcp_send_error");
        std::ostringstream oss;
        oss << "error=" << err << " sent_bytes=" << offset << " total_bytes=" << len;
        logLine(LOG_ERROR, "tcp_send_error", oss.str());
        g_stop = true;
        return;
    }
    std::ostringstream oss;
    oss << "bytes=" << offset;
    logLine(LOG_DEBUG, "tcp_send", oss.str());
}

void serverLoop(host_socket_t server, unsigned long connect_timeout_ms)
{
    g_server_started = true;
    {
        std::ostringstream oss;
        oss << "connect_timeout_ms=" << connect_timeout_ms;
        logLine(LOG_INFO, "tcp_accept_wait", oss.str());
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(connect_timeout_ms);

    while (!g_stop) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(server, &rfds);
        timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        const int ready = select(static_cast<int>(server + 1), &rfds, NULL, NULL, &tv);
        if (ready > 0 && FD_ISSET(server, &rfds)) {
            sockaddr_in client_addr;
#ifdef _WIN32
            int len = sizeof(client_addr);
#else
            socklen_t len = sizeof(client_addr);
#endif
            g_client_socket = accept(server, reinterpret_cast<sockaddr *>(&client_addr), &len);
            if (g_client_socket != invalid_socket_value) {
                setNonBlocking(g_client_socket);
                g_connected_once = true;
                logLine(LOG_INFO, "tcp_connected", "remote=127.0.0.1");
                break;
            }
        }
        if (!g_connected_once && std::chrono::steady_clock::now() >= deadline) {
            setExitReason("connect_timeout");
            logLine(LOG_INFO, "connect_timeout", "");
            g_stop = true;
            closeSocket(server);
            return;
        }
    }

    closeSocket(server);

    {
        std::lock_guard<std::mutex> lock(g_serial_mutex);
        if (!g_output_buffer.empty()) {
            std::ostringstream oss;
            oss << "bytes=" << g_output_buffer.size();
            logLine(LOG_DEBUG, "serial_buffer_flush", oss.str());
            sendToClient(g_output_buffer.data(), g_output_buffer.size());
        }
    }

    char buf[1024];
    while (!g_stop && g_client_socket != invalid_socket_value) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(g_client_socket, &rfds);
        timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        const int ready = select(static_cast<int>(g_client_socket + 1), &rfds, NULL, NULL, &tv);
        if (ready > 0 && FD_ISSET(g_client_socket, &rfds)) {
            const int received = recv(g_client_socket, buf, sizeof(buf), 0);
            if (received > 0) {
                std::lock_guard<std::mutex> lock(g_serial_mutex);
                g_input_buffer.append(buf, received);
                std::ostringstream oss;
                oss << "bytes=" << received;
                logLine(LOG_DEBUG, "serial_rx", oss.str());
            } else if (received == 0 || !isWouldBlock(lastSocketError())) {
                setExitReason(received == 0 ? "tcp_disconnected" : "tcp_recv_error");
                std::ostringstream oss;
                oss << "received=" << received;
                if (received < 0) {
                    oss << " error=" << lastSocketError();
                }
                logLine(LOG_INFO, "tcp_disconnected", oss.str());
                g_stop = true;
                break;
            }
        }
    }

    closeSocket(g_client_socket);
    g_client_socket = invalid_socket_value;
    setExitReason("tcp_disconnected");
    g_stop = true;
}

bool startServer()
{
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        logLine(LOG_ERROR, "wsa_startup_failed", "");
        return false;
    }
#else
    signal(SIGPIPE, SIG_IGN);
#endif

    const host_socket_t server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == invalid_socket_value) {
        logLine(LOG_ERROR, "socket_failed", "");
        return false;
    }

    int yes = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&yes), sizeof(yes));

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(0);
    if (bind(server, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
        std::ostringstream oss;
        oss << "error=" << lastSocketError();
        logLine(LOG_ERROR, "tcp_bind_failed", oss.str());
        closeSocket(server);
        return false;
    }
    if (listen(server, 1) != 0) {
        std::ostringstream oss;
        oss << "error=" << lastSocketError();
        logLine(LOG_ERROR, "tcp_listen_failed", oss.str());
        closeSocket(server);
        return false;
    }

    sockaddr_in bound;
#ifdef _WIN32
    int bound_len = sizeof(bound);
#else
    socklen_t bound_len = sizeof(bound);
#endif
    if (getsockname(server, reinterpret_cast<sockaddr *>(&bound), &bound_len) != 0) {
        std::ostringstream oss;
        oss << "error=" << lastSocketError();
        logLine(LOG_ERROR, "tcp_getsockname_failed", oss.str());
        closeSocket(server);
        return false;
    }
    const int port = ntohs(bound.sin_port);
    {
        std::ostringstream oss;
        oss << "host=127.0.0.1 port=" << port;
        logLine(LOG_INFO, "tcp_listen", oss.str());
    }
    writeInfoFile(port);

    const unsigned long timeout_ms = envUnsignedLong("HOST_ARDUINO_CONNECT_TIMEOUT_MS", kDefaultConnectTimeoutMs);
    g_server_thread = std::thread(serverLoop, server, timeout_ms);
    return true;
}

bool waitForClient()
{
    const unsigned long timeout_ms = envUnsignedLong("HOST_ARDUINO_CONNECT_TIMEOUT_MS", kDefaultConnectTimeoutMs);
    {
        std::ostringstream oss;
        oss << "timeout_ms=" << timeout_ms;
        logLine(LOG_INFO, "runtime_wait_client", oss.str());
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (!g_connected_once.load() && !g_stop.load()) {
        if (std::chrono::steady_clock::now() >= deadline) {
            setExitReason("connect_timeout");
            logLine(LOG_INFO, "runtime_wait_client_timeout", "");
            g_stop = true;
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (!g_connected_once.load()) {
        return false;
    }
    logLine(LOG_INFO, "runtime_client_ready", "");
    return true;
}

int runLauncher(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    const std::string exe = executablePath();
    if (exe.empty()) {
        std::cerr << "HOST_ARDUINO_ERROR=executable_path_unavailable\n";
        return 1;
    }
    initLog(exe, true);
    logLine(LOG_INFO, "launcher_start", "exe=" + exe);
    const std::string info_path = infoPathForExecutable(exe);
    std::remove(info_path.c_str());
    if (!startDetachedChild(exe)) {
        std::cerr << "HOST_ARDUINO_ERROR=child_start_failed\n";
        logLine(LOG_ERROR, "launcher_exit", "reason=child_start_failed");
        return 1;
    }

    const unsigned long wait_ms = envUnsignedLong("HOST_ARDUINO_PARENT_WAIT_MS", kDefaultParentWaitMs);
    {
        std::ostringstream oss;
        oss << "info=" << info_path << " timeout_ms=" << wait_ms;
        logLine(LOG_INFO, "parent_wait_info", oss.str());
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(wait_ms);
    std::string content;
    while (std::chrono::steady_clock::now() < deadline) {
        if (readInfoFile(info_path, &content)) {
            const int port = parsePort(content);
            if (port > 0) {
                std::cout << "HOST_ARDUINO_PORT=" << port << "\n";
                std::cout << "HOST_ARDUINO_INFO=" << info_path << "\n";
                std::ostringstream oss;
                oss << "reason=ok port=" << port;
                logLine(LOG_INFO, "launcher_exit", oss.str());
                return 0;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    std::cerr << "HOST_ARDUINO_ERROR=port_file_timeout\n";
    std::cerr << "HOST_ARDUINO_INFO=" << info_path << "\n";
    logLine(LOG_ERROR, "launcher_exit", "reason=port_file_timeout");
    return 1;
}

} // namespace

namespace HostArduino {

bool runtimeStart(int argc, char **argv)
{
    if (!hasChildArg(argc, argv)) {
        std::exit(runLauncher(argc, argv));
    }

    g_output_limit = envSize("HOST_ARDUINO_SERIAL_BUFFER_SIZE", kDefaultSerialBufferLimit);
    const std::string exe = executablePath();
    initLog(exe, false);
    logLine(LOG_INFO, "runtime_start", "exe=" + exe);
    g_info_path = infoPathForExecutable(exe);
    const bool ok = startServer();
    if (!ok) {
        setExitReason("runtime_start_failed");
        logLine(LOG_ERROR, "runtime_start_failed", "");
        return false;
    }
    if (!waitForClient()) {
        logLine(LOG_ERROR, "runtime_start_failed", "reason=client_not_connected");
        return false;
    }
    return true;
}

void runtimeStop()
{
    g_stop = true;
    if (g_server_thread.joinable()) {
        g_server_thread.join();
    }
#ifdef _WIN32
    WSACleanup();
#endif
    std::string reason;
    {
        std::lock_guard<std::mutex> lock(g_log_mutex);
        reason = g_exit_reason;
    }
    logLine(LOG_INFO, "runtime_stop", "reason=" + reason);
}

bool runtimeShouldStop()
{
    return g_stop.load();
}

void runtimePoll()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(0));
}

void runtimeLogInfo(const char *event, const char *message)
{
    logLine(LOG_INFO, event ? event : "runtime_note", message ? message : "");
}

size_t serialWrite(const char *data, size_t len)
{
    if (!data || len == 0) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(g_serial_mutex);
    appendOutputLocked(data, len);
    sendToClient(data, len);
    std::ostringstream oss;
    oss << "bytes=" << len;
    logLine(LOG_DEBUG, "serial_tx", oss.str());
    return len;
}

int serialAvailable()
{
    std::lock_guard<std::mutex> lock(g_serial_mutex);
    return static_cast<int>(g_input_buffer.size());
}

int serialRead()
{
    std::lock_guard<std::mutex> lock(g_serial_mutex);
    if (g_input_buffer.empty()) {
        return -1;
    }
    const unsigned char ch = static_cast<unsigned char>(g_input_buffer[0]);
    g_input_buffer.erase(0, 1);
    return static_cast<int>(ch);
}

int serialPeek()
{
    std::lock_guard<std::mutex> lock(g_serial_mutex);
    if (g_input_buffer.empty()) {
        return -1;
    }
    const unsigned char ch = static_cast<unsigned char>(g_input_buffer[0]);
    return static_cast<int>(ch);
}

} // namespace HostArduino
