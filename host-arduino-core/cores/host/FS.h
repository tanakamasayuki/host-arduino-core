#ifndef HOST_ARDUINO_FS_H
#define HOST_ARDUINO_FS_H

#include <stdint.h>
#include <stdio.h>

#include <cerrno>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <direct.h>
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

#include "HostDiag.h"
#include "Stream.h"
#include "WString.h"

#ifndef FILE_READ
#define FILE_READ "r"
#endif
#ifndef FILE_WRITE
#define FILE_WRITE "w"
#endif
#ifndef FILE_APPEND
#define FILE_APPEND "a"
#endif

namespace fs
{

    enum SeekMode
    {
        SeekSet = SEEK_SET,
        SeekCur = SEEK_CUR,
        SeekEnd = SEEK_END
    };

    namespace detail
    {

        inline bool isSeparator(char ch)
        {
            return ch == '/' || ch == '\\';
        }

        inline std::string executablePath()
        {
#ifdef _WIN32
            char path[MAX_PATH];
            const DWORD len = GetModuleFileNameA(NULL, path, sizeof(path));
            return len > 0 ? std::string(path, len) : std::string();
#elif defined(__APPLE__)
            uint32_t size = 0;
            _NSGetExecutablePath(NULL, &size);
            std::vector<char> path(size + 1);
            if (_NSGetExecutablePath(path.data(), &size) == 0)
            {
                return std::string(path.data());
            }
            return std::string();
#else
            char path[4096];
            const ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
            if (len > 0)
            {
                path[len] = '\0';
                return std::string(path);
            }
            return std::string();
#endif
        }

        inline std::string directoryName(const std::string &path)
        {
            const size_t pos = path.find_last_of("/\\");
            return pos == std::string::npos ? std::string(".") : path.substr(0, pos);
        }

        inline std::string joinPath(const std::string &base, const std::string &name)
        {
            if (base.empty())
            {
                return name;
            }
            if (name.empty())
            {
                return base;
            }
            if (isSeparator(base[base.size() - 1]))
            {
                return base + name;
            }
#ifdef _WIN32
            return base + "\\" + name;
#else
            return base + "/" + name;
#endif
        }

        inline bool makeDirectory(const std::string &path)
        {
            if (path.empty())
            {
                return false;
            }
#ifdef _WIN32
            if (_mkdir(path.c_str()) == 0)
            {
                return true;
            }
#else
            if (mkdir(path.c_str(), 0777) == 0)
            {
                return true;
            }
#endif
            return errno == EEXIST;
        }

        inline bool makeDirectories(const std::string &path)
        {
            if (path.empty())
            {
                return false;
            }
            std::string current;
            size_t index = 0;

#ifdef _WIN32
            if (path.size() >= 2 && path[1] == ':')
            {
                current = path.substr(0, 2);
                index = 2;
            }
#endif
            while (index < path.size() && isSeparator(path[index]))
            {
                current += path[index++];
            }

            for (; index <= path.size(); ++index)
            {
                if (index == path.size() || isSeparator(path[index]))
                {
                    if (!current.empty() && !makeDirectory(current))
                    {
                        return false;
                    }
                    while (index < path.size() && isSeparator(path[index]))
                    {
                        current += path[index++];
                    }
                }
                if (index < path.size())
                {
                    current += path[index];
                }
            }
            return makeDirectory(path);
        }

        inline bool pathExists(const std::string &path)
        {
#ifdef _WIN32
            const DWORD attr = GetFileAttributesA(path.c_str());
            return attr != INVALID_FILE_ATTRIBUTES;
#else
            struct stat st;
            return stat(path.c_str(), &st) == 0;
#endif
        }

        inline bool isDirectoryPath(const std::string &path)
        {
#ifdef _WIN32
            const DWORD attr = GetFileAttributesA(path.c_str());
            return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
#else
            struct stat st;
            return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
#endif
        }

        inline size_t fileSize(const std::string &path)
        {
#ifdef _WIN32
            WIN32_FILE_ATTRIBUTE_DATA data;
            if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &data))
            {
                return 0;
            }
            ULARGE_INTEGER value;
            value.HighPart = data.nFileSizeHigh;
            value.LowPart = data.nFileSizeLow;
            return static_cast<size_t>(value.QuadPart);
#else
            struct stat st;
            return stat(path.c_str(), &st) == 0 ? static_cast<size_t>(st.st_size) : 0;
#endif
        }

        inline std::string sanitizePath(const char *path)
        {
            std::string input(path ? path : "");
            std::string out;
            size_t index = 0;
            while (index < input.size() && isSeparator(input[index]))
            {
                ++index;
            }
            while (index < input.size())
            {
                while (index < input.size() && isSeparator(input[index]))
                {
                    ++index;
                }
                const size_t start = index;
                while (index < input.size() && !isSeparator(input[index]))
                {
                    ++index;
                }
                const std::string part = input.substr(start, index - start);
                if (part.empty() || part == ".")
                {
                    continue;
                }
                if (part == "..")
                {
                    return std::string();
                }
                out = out.empty() ? part : joinPath(out, part);
            }
            return out;
        }

        inline bool modeWrites(const char *mode)
        {
            const char *m = mode ? mode : FILE_READ;
            return std::strchr(m, 'w') || std::strchr(m, 'a') || std::strchr(m, '+');
        }

    } // namespace detail

    class File : public Stream
    {
    public:
        using Print::write;

        File() : handle_(NULL), directory_(false), size_(0), position_(0)
#ifdef _WIN32
                 ,
                 dir_index_(0)
#else
                 ,
                 dir_(NULL)
#endif
        {
        }

        File(const std::string &hostPath, const std::string &displayPath, const char *mode, bool directory)
            : handle_(NULL), host_path_(hostPath), display_path_(displayPath),
              name_(baseName(displayPath)), directory_(directory), size_(0), position_(0)
#ifdef _WIN32
              ,
              dir_index_(0)
#else
              ,
              dir_(NULL)
#endif
        {
            if (directory_)
            {
#ifdef _WIN32
                dir_index_ = 0;
#else
                dir_ = opendir(host_path_.c_str());
#endif
                return;
            }

            handle_ = std::fopen(host_path_.c_str(), mode ? mode : FILE_READ);
            if (handle_)
            {
                size_ = detail::fileSize(host_path_);
            }
            else if (detail::modeWrites(mode))
            {
                // Read failures are routinely how sketches probe for files; only
                // hint on unexpected write/append failures (e.g. permissions,
                // missing parent dir, full disk).
                HOST_DIAG_ONCE("File::open() failed for write/append; check path and permissions");
            }
        }

        File(const File &other)
            : Stream(), handle_(NULL), host_path_(other.host_path_), display_path_(other.display_path_),
              name_(other.name_), directory_(other.directory_), size_(other.size_),
              position_(other.position_)
#ifdef _WIN32
              ,
              dir_entries_(other.dir_entries_), dir_index_(other.dir_index_)
#else
              ,
              dir_(NULL)
#endif
        {
            if (!directory_ && other.handle_)
            {
                handle_ = std::fopen(host_path_.c_str(), "rb");
                if (handle_)
                {
                    std::fseek(handle_, static_cast<long>(position_), SEEK_SET);
                }
            }
        }

        File(File &&other)
            : Stream(), handle_(other.handle_), host_path_(other.host_path_),
              display_path_(other.display_path_), name_(other.name_), directory_(other.directory_),
              size_(other.size_), position_(other.position_),
#ifdef _WIN32
              dir_entries_(other.dir_entries_), dir_index_(other.dir_index_)
#else
              dir_(other.dir_)
#endif
        {
            other.handle_ = NULL;
#ifndef _WIN32
            other.dir_ = NULL;
#endif
            other.directory_ = false;
            other.size_ = 0;
            other.position_ = 0;
        }

        File &operator=(const File &other)
        {
            if (this == &other)
            {
                return *this;
            }
            close();
            host_path_ = other.host_path_;
            display_path_ = other.display_path_;
            name_ = other.name_;
            directory_ = other.directory_;
            size_ = other.size_;
            position_ = other.position_;
#ifdef _WIN32
            dir_entries_ = other.dir_entries_;
            dir_index_ = other.dir_index_;
#endif
            if (!directory_ && other.handle_)
            {
                handle_ = std::fopen(host_path_.c_str(), "rb");
                if (handle_)
                {
                    std::fseek(handle_, static_cast<long>(position_), SEEK_SET);
                }
            }
            return *this;
        }

        File &operator=(File &&other)
        {
            if (this == &other)
            {
                return *this;
            }
            close();
            handle_ = other.handle_;
            host_path_ = other.host_path_;
            display_path_ = other.display_path_;
            name_ = other.name_;
            directory_ = other.directory_;
            size_ = other.size_;
            position_ = other.position_;
#ifdef _WIN32
            dir_entries_ = other.dir_entries_;
            dir_index_ = other.dir_index_;
#else
            dir_ = other.dir_;
#endif
            other.handle_ = NULL;
#ifndef _WIN32
            other.dir_ = NULL;
#endif
            other.directory_ = false;
            other.size_ = 0;
            other.position_ = 0;
            return *this;
        }

        ~File()
        {
            close();
        }

        size_t write(uint8_t value)
        {
            return write(&value, 1);
        }

        size_t write(const uint8_t *buffer, size_t size)
        {
            if (!handle_ || !buffer || directory_)
            {
                return 0;
            }
            const size_t written = std::fwrite(buffer, 1, size, handle_);
            position_ += written;
            if (position_ > size_)
            {
                size_ = position_;
            }
            return written;
        }

        int available()
        {
            if (!handle_ || directory_ || position_ >= size_)
            {
                return 0;
            }
            return static_cast<int>(size_ - position_);
        }

        int read()
        {
            if (!handle_ || directory_)
            {
                return -1;
            }
            const int c = std::fgetc(handle_);
            if (c >= 0)
            {
                ++position_;
            }
            return c;
        }

        size_t read(uint8_t *buffer, size_t size)
        {
            if (!handle_ || !buffer || directory_)
            {
                return 0;
            }
            const size_t count = std::fread(buffer, 1, size, handle_);
            position_ += count;
            return count;
        }

        int peek()
        {
            if (!handle_ || directory_)
            {
                return -1;
            }
            const int c = std::fgetc(handle_);
            if (c >= 0)
            {
                std::ungetc(c, handle_);
            }
            return c;
        }

        void flush()
        {
            if (handle_)
            {
                std::fflush(handle_);
            }
        }

        bool seek(uint32_t pos, SeekMode mode = SeekSet)
        {
            if (!handle_ || directory_)
            {
                return false;
            }
            if (std::fseek(handle_, static_cast<long>(pos), mode) != 0)
            {
                return false;
            }
            const long current = std::ftell(handle_);
            position_ = current < 0 ? 0 : static_cast<size_t>(current);
            return true;
        }

        size_t position() const
        {
            return position_;
        }

        size_t size() const
        {
            return size_;
        }

        void close()
        {
            if (handle_)
            {
                std::fclose(handle_);
                handle_ = NULL;
            }
#ifndef _WIN32
            if (dir_)
            {
                closedir(dir_);
                dir_ = NULL;
            }
#endif
        }

        const char *name() const
        {
            return name_.c_str();
        }

        const char *path() const
        {
            return display_path_.c_str();
        }

        bool isDirectory() const
        {
            return directory_;
        }

        File openNextFile(const char *mode = FILE_READ)
        {
            if (!directory_)
            {
                return File();
            }
#ifdef _WIN32
            if (dir_entries_.empty())
            {
                WIN32_FIND_DATAA data;
                const std::string pattern = detail::joinPath(host_path_, "*");
                HANDLE find = FindFirstFileA(pattern.c_str(), &data);
                if (find == INVALID_HANDLE_VALUE)
                {
                    return File();
                }
                do
                {
                    const std::string entry(data.cFileName);
                    if (entry != "." && entry != "..")
                    {
                        dir_entries_.push_back(entry);
                    }
                } while (FindNextFileA(find, &data));
                FindClose(find);
            }
            if (dir_index_ >= dir_entries_.size())
            {
                return File();
            }
            const std::string entry = dir_entries_[dir_index_++];
#else
            if (!dir_)
            {
                return File();
            }
            struct dirent *ent = NULL;
            std::string entry;
            while ((ent = readdir(dir_)) != NULL)
            {
                entry = ent->d_name;
                if (entry != "." && entry != "..")
                {
                    break;
                }
                entry.clear();
            }
            if (entry.empty())
            {
                return File();
            }
#endif
            const std::string childHostPath = detail::joinPath(host_path_, entry);
            const std::string childDisplayPath = detail::joinPath(display_path_, entry);
            return File(childHostPath, childDisplayPath, mode, detail::isDirectoryPath(childHostPath));
        }

        void rewindDirectory()
        {
#ifdef _WIN32
            dir_index_ = 0;
#else
            if (dir_)
            {
                rewinddir(dir_);
            }
#endif
        }

        operator bool() const
        {
            return directory_ ? detail::isDirectoryPath(host_path_) : handle_ != NULL;
        }

    private:
        FILE *handle_;
        std::string host_path_;
        std::string display_path_;
        std::string name_;
        bool directory_;
        size_t size_;
        size_t position_;
#ifdef _WIN32
        std::vector<std::string> dir_entries_;
        size_t dir_index_;
#else
        DIR *dir_;
#endif

        static std::string baseName(const std::string &path)
        {
            const size_t pos = path.find_last_of("/\\");
            return pos == std::string::npos ? path : path.substr(pos + 1);
        }
    };

    class FS
    {
    public:
        FS() : mounted_(false) {}
        explicit FS(const char *name) : name_(name ? name : ""), mounted_(false) {}

        bool begin(bool = true, const char * = NULL, uint8_t = 10, const char * = NULL)
        {
            if (name_.empty())
            {
                return false;
            }
            mounted_ = detail::makeDirectories(rootPath());
            return mounted_;
        }

        void end()
        {
            mounted_ = false;
        }

        File open(const char *path, const char *mode = FILE_READ, const bool = false)
        {
            if (!ensureMounted())
            {
                return File();
            }
            const std::string clean = detail::sanitizePath(path);
            if (clean.empty() && path && path[0] != '/')
            {
                return File();
            }
            const std::string hostPath = clean.empty() ? rootPath() : detail::joinPath(rootPath(), clean);
            if (detail::modeWrites(mode))
            {
                const std::string parent = detail::directoryName(hostPath);
                if (!detail::makeDirectories(parent))
                {
                    return File();
                }
            }
            const std::string displayPath = clean.empty() ? std::string("/") : std::string("/") + clean;
            return File(hostPath, displayPath, mode, detail::isDirectoryPath(hostPath));
        }

        bool exists(const char *path)
        {
            if (!ensureMounted())
            {
                return false;
            }
            const std::string clean = detail::sanitizePath(path);
            const std::string hostPath = clean.empty() ? rootPath() : detail::joinPath(rootPath(), clean);
            return detail::pathExists(hostPath);
        }

        bool remove(const char *path)
        {
            if (!ensureMounted())
            {
                return false;
            }
            const std::string clean = detail::sanitizePath(path);
            if (clean.empty())
            {
                return false;
            }
            return std::remove(detail::joinPath(rootPath(), clean).c_str()) == 0;
        }

        bool rename(const char *pathFrom, const char *pathTo)
        {
            if (!ensureMounted())
            {
                return false;
            }
            const std::string from = detail::sanitizePath(pathFrom);
            const std::string to = detail::sanitizePath(pathTo);
            if (from.empty() || to.empty())
            {
                return false;
            }
            const std::string toHost = detail::joinPath(rootPath(), to);
            if (!detail::makeDirectories(detail::directoryName(toHost)))
            {
                return false;
            }
            return std::rename(detail::joinPath(rootPath(), from).c_str(), toHost.c_str()) == 0;
        }

        bool mkdir(const char *path)
        {
            if (!ensureMounted())
            {
                return false;
            }
            const std::string clean = detail::sanitizePath(path);
            if (clean.empty())
            {
                return false;
            }
            return detail::makeDirectories(detail::joinPath(rootPath(), clean));
        }

        bool rmdir(const char *path)
        {
            if (!ensureMounted())
            {
                return false;
            }
            const std::string clean = detail::sanitizePath(path);
            if (clean.empty())
            {
                return false;
            }
#ifdef _WIN32
            return _rmdir(detail::joinPath(rootPath(), clean).c_str()) == 0;
#else
            return ::rmdir(detail::joinPath(rootPath(), clean).c_str()) == 0;
#endif
        }

        const char *name() const
        {
            return name_.c_str();
        }

        const std::string &root() const
        {
            root_cache_ = detail::joinPath(detail::directoryName(detail::executablePath()), name_);
            return root_cache_;
        }

        operator bool() const
        {
            return mounted_;
        }

    private:
        std::string name_;
        bool mounted_;
        mutable std::string root_cache_;

        bool ensureMounted()
        {
            return mounted_ || begin();
        }

        std::string rootPath() const
        {
            const std::string exe = detail::executablePath();
            const std::string base = exe.empty() ? std::string(".") : detail::directoryName(exe);
            return detail::joinPath(base, name_);
        }
    };

} // namespace fs

using fs::File;
using fs::FS;
using fs::SeekCur;
using fs::SeekEnd;
using fs::SeekMode;
using fs::SeekSet;

#endif
