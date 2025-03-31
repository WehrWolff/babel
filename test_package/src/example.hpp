/*
 * Copyright (c) 2025 WehrWolff
 *
 * This file is part of babel, which is BSL-1.0 licensed.
 * See http://opensource.org/licenses/bsl-1.0
 */

#include <optional>
#include <functional>
#include <filesystem>
#include <format>
#include "Logging.hpp"
#include "Exceptions.hpp"
#include <pwd.h>
#include <unistd.h>
#include <openssl/evp.h>

std::string getBinExt() {
    #ifdef _WIN32
    return ".exe";
    #else
    return "";
    #endif
}

std::string getLibExt() {
    #ifdef _WIN32
    return ".dll";
    #else
    return ".so";
    #endif
}

std::string getArch() { //Get current architecture, detects nearly every architecture. Coded by Freak
    #if defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
    #elif defined(i386) || defined(__i386__) || defined(__i386) || defined(_M_IX86)
    return "x86_32";
    #elif defined(__ARM_ARCH_2__)
    return "ARM2";
    #elif defined(__ARM_ARCH_3__) || defined(__ARM_ARCH_3M__)
    return "ARM3";
    #elif defined(__ARM_ARCH_4T__) || defined(__TARGET_ARM_4T)
    return "ARM4T";
    #elif defined(__ARM_ARCH_5_) || defined(__ARM_ARCH_5E_)
    return "ARM5"
    #elif defined(__ARM_ARCH_6T2_) || defined(__ARM_ARCH_6T2_)
    return "ARM6T2";
    #elif defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) || defined(__ARM_ARCH_6K__) || defined(__ARM_ARCH_6Z__) || defined(__ARM_ARCH_6ZK__)
    return "ARM6";
    #elif defined(__ARM_ARCH_7__) || defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7S__)
    return "ARM7";
    #elif defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7S__)
    return "ARM7A";
    #elif defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7S__)
    return "ARM7R";
    #elif defined(__ARM_ARCH_7M__)
    return "ARM7M";
    #elif defined(__ARM_ARCH_7S__)
    return "ARM7S";
    #elif defined(__aarch64__) || defined(_M_ARM64)
    return "ARM64";
    #elif defined(mips) || defined(__mips__) || defined(__mips)
    return "MIPS";
    #elif defined(__sh__)
    return "SUPERH";
    #elif defined(__powerpc) || defined(__powerpc__) || defined(__powerpc64__) || defined(__POWERPC__) || defined(__ppc__) || defined(__PPC__) || defined(_ARCH_PPC)
    return "POWERPC";
    #elif defined(__PPC64__) || defined(__ppc64__) || defined(_ARCH_PPC64)
    return "POWERPC64";
    #elif defined(__sparc__) || defined(__sparc)
    return "SPARC";
    #elif defined(__m68k__)
    return "M68K";
    #else
    return "UNKNOWN";
    #endif
}

std::string getOS() {
    #ifdef _WIN32
    return "windows";
    #elif __APPLE__
    return "macos";
    #elif __linux__
    return "gnu.linux";
    #elif __FreeBSD__
    return "freebsd";
    #else
    return "unknown";
    #endif
}

std::filesystem::path getHomeDir() {
    // Check for Windows (_WIN32 works for both 32 and 64 bit)
    // make it more c like with checking against nullptr
    #ifdef _WIN32
    const char* homeEnv = getenv("USERPROFILE");
    std::string homedir = homeEnv == nullptr ? getenv("HOMEDRIVE") + getenv("HOMEPATH") : homeEnv;
    #else
    const char* homeEnv = getenv("HOME");
    std::string homedir = homeEnv == nullptr ? getpwuid(getuid())->pw_dir : homeEnv;
    #endif

    std::filesystem::path home(homedir);
    return home;
}

std::filesystem::path getRootDir() {
    #ifdef _WIN32
    std::filesystem::path root = "C:";
    #else
    std::filesystem::path root = "/";
    #endif
    return root;
}


std::vector<std::filesystem::path> getPathDirs() {
    std::vector<std::filesystem::path> paths;

    const char* pathEnv = getenv("PATH");
    if (pathEnv == nullptr) {
        LoggerSingleton::getLogger().puts(Logger::Type::WARNING, "PATH environment variable not found.");
        return paths;
    }

    #ifdef _WIN32
    std::string delimiter = ";";
    #else
    std::string delimiter = ":";
    #endif

    std::string path(pathEnv);
    size_t start;
    size_t end = 0;
    while ((start = path.find_first_not_of(delimiter, end)) != std::string::npos) {
        end = path.find(delimiter, start);
        paths.emplace_back(path.substr(start, end - start));
        LoggerSingleton::getLogger().puts(Logger::Type::VERBOSE, "Found path: " + paths.back().string());
    }

    return paths;
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

std::string ucfirst(const std::string&);

class PathResolver {
public:

    using FallbackFunc = std::function<std::optional<std::filesystem::path>(const std::unordered_map<std::string, std::optional<std::filesystem::path>, TransparentStringHash, std::equal_to<>>&, std::vector<std::string>& fallbackKeys)>;
    using ConditionFunc = bool (*)(const std::filesystem::path&);

    struct PathInfo {
        std::optional<std::filesystem::path> path;
        std::vector<std::filesystem::path> commonLocations;
        ConditionFunc condition;
        FallbackFunc fallback;
        std::vector<std::string> fallbackKeys;
    };

    PathResolver() = default;

    void addPath(const std::string& name, const std::vector<std::filesystem::path>& commonLocations, const ConditionFunc& condition = [](const auto&) { return true; }, const FallbackFunc& fallback = nullptr, const std::vector<std::string>& fallbackKeys = {}) noexcept {
        paths[name] = {std::nullopt, commonLocations, condition, fallback, fallbackKeys};
    }

    std::filesystem::path getPath(const std::string& name) const {
        if (paths.at(name).path.has_value())
            return paths.at(name).path.value();

        LoggerSingleton::getLogger().puts(Logger::Type::ERROR, std::format("\nAll known paths have been checked. All attempts to find the {} have failed.", name));
        LoggerSingleton::getLogger().puts(Logger::Type::ERROR, std::format("All fallbacks have failed. All ways to find the {} have been exhausted.\n", name));
        throw NotFoundException(std::format("{} not found.", ucfirst(name)));
    }

    void findPaths() {
        for (const auto& [pathName, _] :paths) {
            LoggerSingleton::getLogger().puts(Logger::Type::INFO, std::format("-- Checking for {}...", pathName));
            checkPath(pathName);
        }

        bool recheck = false;
        do {
            for (auto& [_, pathInfo] : paths) {
                if (pathInfo.path.has_value() || !pathInfo.fallback) {
                    continue;
                }
                
                auto newPath = applyFallback(pathInfo);
                if (newPath.has_value()) {
                    pathInfo.path = newPath;
                    // New path found during fallback, maybe now we can resolve others
                    recheck = true;
                }
            }
        } while (recheck);
    }

    static void updateFallbackKeys(std::vector<std::string>& fallbackKeys, const std::string& key) noexcept {
        if (std::ranges::find(fallbackKeys, key) != fallbackKeys.end()) {
            fallbackKeys.erase(std::ranges::find(fallbackKeys, key));
        }
    }

private:

    std::unordered_map<std::string, PathInfo, TransparentStringHash, std::equal_to<>> paths;

    void checkPath(const std::string& pathName) noexcept {
        using enum Logger::Type;

        PathInfo& pathInfo = paths.at(pathName);
        for (const auto& location : pathInfo.commonLocations) {
            LoggerSingleton::getLogger().puts(VERBOSE, std::format("Searching in {}", location.string()));
            if (std::filesystem::exists(location) && pathInfo.condition(location)) {
                LoggerSingleton::getLogger().puts(PROGRESS, std::format("-- Found {} at location {}", pathName, location.string()));
                pathInfo.path = location;
                return;
            }
        }

        LoggerSingleton::getLogger().puts(INFO, std::format("-- {} not found.", ucfirst(pathName)));
    }

    [[nodiscard]] std::optional<std::filesystem::path> applyFallback(PathInfo& pathInfo) {
        if (!pathInfo.fallback) {
            return std::nullopt;
        }

        std::unordered_map<std::string, std::optional<std::filesystem::path>, TransparentStringHash, std::equal_to<>> fallbackInput;
        for (const auto& [pathName, _] : paths) {
            fallbackInput[pathName] = std::nullopt;
        }

        for (const auto& dep : pathInfo.fallbackKeys) {
            if (paths.at(dep).path.has_value()) {
                fallbackInput[dep] = paths.at(dep).path.value();
            }
        }

        if (const auto& newPath = pathInfo.fallback(fallbackInput, pathInfo.fallbackKeys); newPath.has_value() && std::filesystem::exists(newPath.value())) {
            return newPath;
        }

        return std::nullopt;
    }
};

std::string ucfirst(const std::string& str) {
    std::string result = str;

    if (!result.empty())
        result[0] = static_cast<char>(toupper(result[0]));
    
    return result;
}

bool has_perm(std::filesystem::perms permType, std::filesystem::perms permissions) {
    return (permissions & permType) != std::filesystem::perms::none;
}

struct ChecksumDetails {
    std::string sha256;
    uLong crc32;
    uLong file_size;
};

std::unordered_map<std::string, ChecksumDetails, TransparentStringHash, std::equal_to<>> parseData(const std::string& data) {
    std::unordered_map<std::string, ChecksumDetails, TransparentStringHash, std::equal_to<>> csMap;
    std::istringstream dataStream(data);
    std::string line;

    while (std::getline(dataStream, line)) {
        std::ranges::replace(line, ',', ' ');
        std::istringstream lineStream(line);
        std::string path;
        std::string sha256;
        uLong crc32;
        uLong file_size;

        if (lineStream >> path >> sha256 >> crc32 >> file_size) {
            csMap[path] = { sha256, crc32, file_size };
        }
    }

    return csMap;
}

template <typename T>
struct OpenSSLFree {
    void operator()(T* ptr) const {
        EVP_MD_CTX_free((EVP_MD_CTX*)ptr);
    }
};

template <typename T>
using OpenSSLPointer = std::unique_ptr<T, OpenSSLFree<T>>;

[[nodiscard]] std::string sha256(const std::string &str) {
    OpenSSLPointer<EVP_MD_CTX> context(EVP_MD_CTX_new());

    if (context.get() == nullptr) throw ChecksumValidationException("Failed to generate hash, validation cannot proceed.");
    if (!EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr)) throw ChecksumValidationException("Failed to generate hash, validation cannot proceed.");
    if (!EVP_DigestUpdate(context.get(), str.c_str(), str.size())) throw ChecksumValidationException("Failed to generate hash, validation cannot proceed.");

    unsigned char hash[EVP_MAX_MD_SIZE];

    if (uint32_t hashSize = 0; !EVP_DigestFinal_ex(context.get(), hash, &hashSize)) throw ChecksumValidationException("Failed to generate hash, validation cannot proceed.");

    std::string out;
    for (unsigned char c : hash) {
        out += std::format("{:02x}", (int)c);
    }

    return out;
}

[[nodiscard]] std::filesystem::path resolve_symlink(const std::filesystem::path& path) noexcept {
    auto target = path;
    while (std::filesystem::is_symlink(target)) {
        target = std::filesystem::read_symlink(target);
    }
    return target;
}

[[nodiscard]] std::string exec(const char* cmd) {
    const size_t buffer_size = 128;
    std::array<char, buffer_size> buffer;
    std::string result;

    std::unique_ptr<FILE, std::function<void(FILE*)>> pipe(popen(cmd, "r"), 
        [cmd](FILE* f) {
            int status = pclose(f);
            if (status != 0) {
                throw BinaryExecutionException(std::format("Command '{}' failed with exit code {}", cmd, status));
            }
        });

    if (!pipe) {
        throw BinaryExecutionException("Opening Process has failed during call of popen().");
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    return result;
}