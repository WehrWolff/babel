#ifndef UTIL_HPP
#define UTIL_HPP

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <functional>
#include <string>

__attribute__ ((format (printf, 1, 2))) __attribute__ ((cold)) [[noreturn]] void babel_panic(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    abort();
}

__attribute__ ((cold)) [[noreturn]] static inline void babel_unreachable(void) {
    babel_panic("Reached unreachable code");
}

__attribute__ ((cold)) [[noreturn]] static inline void babel_stub(void) {
    babel_panic("Reached a stub. Not yet implemented");
}

struct TransparentStringHash {
    using is_transparent = void; // Enables heterogeneous operations

    std::size_t operator()(const std::string& str) const noexcept {
        return std::hash<std::string>{}(str);
    }

    std::size_t operator()(std::string_view str) const noexcept {
        return std::hash<std::string_view>{}(str);
    }

    std::size_t operator()(const char* str) const noexcept {
        return std::hash<std::string_view>{}(str);
    }
};

#endif /* UTIL_HPP */