#include <ostream>
#include <format>

namespace color {
    enum FORMAT_CODE {
        DEFAULT = 39,
        BLACK = 30,
        RED = 31,
        GREEN = 32,
        YELLOW = 33,
        BLUE = 34,
        MAGENTA = 35,
        CYAN = 36,
        LIGHT_GRAY = 37,
        DARK_GRAY = 90,
        LIGHT_RED = 91,
        LIGHT_GREEN = 92,
        LIGHT_YELLOW = 93,
        LIGHT_BLUE = 94,
        LIGHT_MAGENTA = 95,
        LIGHT_CYAN = 96,
        WHITE = 97,
        BG_DEFAULT = 49,
        BG_BLACK = 40,
        BG_RED = 41,
        BG_GREEN = 42,
        BG_YELLOW = 43,
        BG_BLUE = 44,
        BG_MAGENTA = 45,
        BG_CYAN = 46,
        BG_LIGHT_GRAY = 47,
        BG_DARK_GRAY = 100,
        BG_LIGHT_RED = 101,
        BG_LIGHT_GREEN = 102,
        BG_LIGHT_YELLOW = 103,
        BG_LIGHT_BLUE = 104,
        BG_LIGHT_MAGENTA = 105,
        BG_LIGHT_CYAN = 106,
        BG_WHITE = 107,
        RESET = 0,
        BOLD = 1,
        UNDERLINE = 4,
        INVERSE = 7,
        HIDDEN = 8,
        RESET_BOLD = 21,
        RESET_UNDERLINE = 24,
        RESET_INVERSE = 27,
        RESET_HIDDEN = 28
    };

    std::ostream& operator<<(std::ostream& os, const FORMAT_CODE& code) {
        os << "\033[" << static_cast<int>(code) << "m";
        return os;
    }

    std::string rize(const std::string& text, FORMAT_CODE format, FORMAT_CODE foreground, FORMAT_CODE background) {
        std::string s = "\033[" + std::to_string(format) + ";" + std::to_string(foreground) + ";" + std::to_string(background) + "m" + text + "\033[0m";
        return s;
    }

    std::string rize(const std::string& text, FORMAT_CODE format, FORMAT_CODE foreground) {
        std::string s = "\033[" + std::to_string(format) + ";" + std::to_string(foreground) + "m" + text + "\033[0m";
        return s;
    }

    std::string rize(const std::string& text, FORMAT_CODE single_format) {
        std::string s = "\033[" + std::to_string(single_format) + "m" + text + "\033[0m";
        return s;
    }


}