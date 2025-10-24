#pragma once

#if defined(_MSC_VER)
  #define BABEL_NORETURN __declspec(noreturn)
  #define BABEL_PRINTF_FORMAT(fmt, args)
  #define BABEL_COLD
#elif defined(__GNUC__) || defined(__clang__)
  #define BABEL_NORETURN [[noreturn]]
  #define BABEL_PRINTF_FORMAT(fmt, args) __attribute__((format(printf, fmt, args)))
  #define BABEL_COLD __attribute__((cold))
#else
  #define BABEL_NORETURN [[noreturn]]
  #define BABEL_PRINTF_FORMAT(fmt, args)
  #define BABEL_COLD
#endif
