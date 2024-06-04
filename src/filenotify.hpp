#ifndef __FILENOTIFY_HPP
#define __FILENOTIFY_HPP

#include <string>

#ifdef _WIN32
#include <Windows.h>
#endif

#ifdef _WIN32
using filenotify_handle_t = HANDLE;
// constexpr = expression did not evaluate to a constant
const filenotify_handle_t filenotify_handle_nullptr{INVALID_HANDLE_VALUE};
#else
using filenotify_handle_t = int;
constexpr filenotify_handle_t filenotify_handle_nullptr{-1};
#endif

class filenotify {
private:
    std::string filename{};
    std::string full_path{};
    filenotify_handle_t handle{filenotify_handle_nullptr};

#ifndef _WIN32
    filenotify_handle_t file_handle{filenotify_handle_nullptr};
#endif
public:
    filenotify(const std::string filepath);
    ~filenotify();
    bool wait();
};

#endif
