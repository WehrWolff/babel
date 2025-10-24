#ifndef UTIL_HPP
#define UTIL_HPP

#include "platform.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <functional>
#include <string>

BABEL_PRINTF_FORMAT(1, 2) BABEL_COLD BABEL_NORETURN void babel_panic(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    abort();
}

BABEL_COLD BABEL_NORETURN static inline void babel_unreachable(void) {
    babel_panic("Reached unreachable code");
}

BABEL_COLD BABEL_NORETURN static inline void babel_stub(void) {
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