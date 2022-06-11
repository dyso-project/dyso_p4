#pragma once

#include <string>
#include <time.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <utility>


/**
 * Helper functions
 */

// time helper
const std::string getTimeNow() {
    time_t now = time(0);
    struct tm tstruct;
    char buf[20];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "[%m%d_%H%M%S]", &tstruct);
    return buf;
}

// Convert all std::strings to const char* using constexpr if (C++17)
template <typename T>
auto convert(T &&t) {
    if constexpr (std::is_same<std::remove_cv_t<std::remove_reference_t<T>>, std::string>::value) {
        return std::forward<T>(t).c_str();
    } else {
        return std::forward<T>(t);
    }
}

// printf like formatting for C++ with std::string
#pragma GCC diagnostic ignored "-Wformat-security"
template <typename... Args>
std::string stringFormatInternal(const std::string &format, Args &&...args) {
    size_t size = snprintf(nullptr, 0, format.c_str(), std::forward<Args>(args)...) + 1;
    if (size <= 0) {
        throw std::runtime_error("Error: During formatting, something is wrong.");
    }
    std::unique_ptr<char[]> buf(new char[size]);
    snprintf(buf.get(), size, format.c_str(), args...);
    return std::string(buf.get(), buf.get() + size - 1);
}

// custom print helper
template <typename... Args>
void cPrint(std::string tag, std::string fmt, Args &&...args) {
    std::string text = stringFormatInternal(fmt, convert(std::forward<Args>(args))...);
    std::string timeget = "";
    timeget = getTimeNow();
    std::cout << timeget + "[" + tag + "] " << text << std::endl;
    return;
}
