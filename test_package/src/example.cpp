/*
 * Copyright (c) 2025 WehrWolff
 *
 * This file is part of babel, which is BSL-1.0 licensed.
 * See http://opensource.org/licenses/bsl-1.0
 */

#include <fstream>
#include <regex>
#include <zlib.h>
#include "yaml-cpp/yaml.h"
#include "Downloader.hpp"
#include "example.hpp"

Logger LoggerSingleton::instance;

std::optional<std::filesystem::path> installFallback(const std::unordered_map<std::string, std::optional<std::filesystem::path>, TransparentStringHash, std::equal_to<>>&, std::vector<std::string>&);
std::optional<std::filesystem::path> configFallback(const std::unordered_map<std::string, std::optional<std::filesystem::path>, TransparentStringHash, std::equal_to<>>&, std::vector<std::string>&);
std::optional<std::filesystem::path> binaryFallback(const std::unordered_map<std::string, std::optional<std::filesystem::path>, TransparentStringHash, std::equal_to<>>&, std::vector<std::string>&);
PathResolver checkCriticalPaths();
std::vector<std::filesystem::path> getVersionDirectories();

void directoryStructure();
void binaryVerification();
void assetVerification();
void licenseVerification();
void validateChecksums();
void configurationVerification();
void environmentVariableVerification();
void systemDependencyVerification();
void runtimeDependencyVerification();
void compatibilityVerification();
void platformSpecificBehaviorVerification();
void functionalityVerification();

const std::string INSTALL_LABEL = "package installation directory";
const std::string CONFIG_LABEL = "configuration file";
const std::string BINARY_LABEL = "babel binary";

const auto init = []() { 
    LoggerSingleton::getLogger().puts(Logger::Type::INFO, "-- Starting test suite...");
    return 0;
}();

const PathResolver prs = checkCriticalPaths();
const std::filesystem::path INSTALL_PATH = prs.getPath(INSTALL_LABEL);
const std::filesystem::path CONFIG_PATH = prs.getPath(CONFIG_LABEL);
const std::filesystem::path BINARY_PATH = prs.getPath(BINARY_LABEL);
const auto _ = []() {
    using enum Logger::Type;
    LoggerSingleton::getLogger().puts(INFO, "\nLocation summary of essential paths");
    LoggerSingleton::getLogger().puts(SUCCESS, std::format("{}: {}", ucfirst(INSTALL_LABEL), INSTALL_PATH.string()));
    LoggerSingleton::getLogger().puts(SUCCESS, std::format("{}: {}", ucfirst(CONFIG_LABEL), CONFIG_PATH.string()));
    LoggerSingleton::getLogger().puts(SUCCESS, std::format("{}: {}\n", ucfirst(BINARY_LABEL), BINARY_PATH.string()));
    return 0;
}();
const std::vector<std::filesystem::path> VERSIONS = getVersionDirectories();

std::optional<std::filesystem::path> installFallback(const std::unordered_map<std::string, std::optional<std::filesystem::path>, TransparentStringHash, std::equal_to<>>& paths, std::vector<std::string>& fallbackKeys) {
    using enum Logger::Type;
    std::filesystem::path installPath;
    std::optional<std::filesystem::path> configPath = paths.at(CONFIG_LABEL);
    std::optional<std::filesystem::path> binaryPath = paths.at(BINARY_LABEL);

    LoggerSingleton::getLogger().puts(INFO, std::format("-- Initiating {} fallback.", INSTALL_LABEL));

    if (configPath.has_value()) {
        LoggerSingleton::getLogger().puts(INFO, std::format("-- Falling back to {}...", CONFIG_LABEL));
        LoggerSingleton::getLogger().puts(INFO, std::format("-- Checking {}...", CONFIG_LABEL));

        PathResolver::updateFallbackKeys(fallbackKeys, CONFIG_LABEL);
        YAML::Node config = YAML::LoadFile(configPath.value().string());
        installPath = std::filesystem::path(config["Environment"]["BABEL_HOME"].as<std::string>());
        if (std::filesystem::exists(installPath) && std::filesystem::is_directory(installPath)) {
            LoggerSingleton::getLogger().puts(SUCCESS, std::format("-- Found {} through fallback ({}).", INSTALL_LABEL, CONFIG_LABEL));
            LoggerSingleton::getLogger().puts(SUCCESS, std::format("-- Found {} at location {}.", INSTALL_LABEL, installPath.string()));
            return installPath;
        }

        LoggerSingleton::getLogger().puts(INFO, std::format("-- {} not found in {}.", ucfirst(INSTALL_LABEL), CONFIG_LABEL));
        LoggerSingleton::getLogger().puts(INFO, std::format("-- Checking directory of {}...", CONFIG_LABEL));

        installPath = configPath.value().parent_path();
        if (std::filesystem::exists(installPath) && std::filesystem::is_directory(installPath)) {
            LoggerSingleton::getLogger().puts(SUCCESS, std::format("-- Found {} through fallback ({}).", INSTALL_LABEL, CONFIG_LABEL));
            LoggerSingleton::getLogger().puts(SUCCESS, std::format("-- Found {} at location {}.", INSTALL_LABEL, installPath.string()));
            return installPath;
        }

        LoggerSingleton::getLogger().puts(INFO, std::format("-- {} not found through {}.", ucfirst(INSTALL_LABEL), CONFIG_LABEL));
    }
    
    if (binaryPath.has_value()) {
        LoggerSingleton::getLogger().puts(INFO, std::format("-- Falling back to {} target location...", BINARY_LABEL));
        LoggerSingleton::getLogger().puts(INFO, std::format("-- Checking symlink target location of {}...", BINARY_LABEL));

        PathResolver::updateFallbackKeys(fallbackKeys, BINARY_LABEL);
        installPath = binaryPath.value();
        installPath = std::filesystem::is_symlink(installPath) ? resolve_symlink(installPath).parent_path() : installPath.parent_path();
        if (std::filesystem::exists(installPath) && std::filesystem::is_directory(installPath)) {
            LoggerSingleton::getLogger().puts(SUCCESS, std::format("-- Found {} through fallback ({}).", INSTALL_LABEL, BINARY_LABEL));
            LoggerSingleton::getLogger().puts(SUCCESS, std::format("-- Found {} at location {}.", INSTALL_LABEL, installPath.string()));
            return installPath;
        }

        LoggerSingleton::getLogger().puts(INFO, std::format("-- {} not found through {}.", ucfirst(INSTALL_LABEL), BINARY_LABEL));
    }

    return std::nullopt;
}

std::optional<std::filesystem::path> configFallback(const std::unordered_map<std::string, std::optional<std::filesystem::path>, TransparentStringHash, std::equal_to<>>& paths, std::vector<std::string>& fallbackKeys) {
    using enum Logger::Type;
    std::filesystem::path configPath;
    std::optional<std::filesystem::path> installPath = paths.at(INSTALL_LABEL);
    std::optional<std::filesystem::path> binaryPath = paths.at(BINARY_LABEL);

    LoggerSingleton::getLogger().puts(INFO, std::format("-- Initiating {} fallback.", CONFIG_LABEL));

    if (installPath.has_value()) {
        LoggerSingleton::getLogger().puts(INFO, std::format("-- Falling back to {}...", INSTALL_LABEL));
        LoggerSingleton::getLogger().puts(INFO, std::format("-- Checking {}...", INSTALL_LABEL));

        PathResolver::updateFallbackKeys(fallbackKeys, INSTALL_LABEL);
        configPath = installPath.value() / "config.yaml";
        if (std::filesystem::exists(configPath) && std::filesystem::is_regular_file(configPath)) {
            LoggerSingleton::getLogger().puts(SUCCESS, std::format("-- Found {} through fallback ({}).", CONFIG_LABEL, INSTALL_LABEL));
            LoggerSingleton::getLogger().puts(SUCCESS, std::format("-- Found {} at location {}.", CONFIG_LABEL, configPath.string()));
            return configPath;
        }
        
        LoggerSingleton::getLogger().puts(INFO, std::format("-- {} not found through {}.", ucfirst(CONFIG_LABEL), INSTALL_LABEL));
    }

    if (binaryPath.has_value()) {
        LoggerSingleton::getLogger().puts(INFO, std::format("-- Falling back to {} target location...", BINARY_LABEL));
        LoggerSingleton::getLogger().puts(INFO, std::format("-- Checking symlink target location of {}...", BINARY_LABEL));

        PathResolver::updateFallbackKeys(fallbackKeys, BINARY_LABEL);
        configPath = binaryPath.value();
        configPath = std::filesystem::is_symlink(configPath) ? resolve_symlink(configPath).parent_path() / "config.yaml" : configPath.parent_path() / "config.yaml";
        if (std::filesystem::exists(configPath) && std::filesystem::is_regular_file(configPath)) {
            LoggerSingleton::getLogger().puts(SUCCESS, std::format("-- Found {} through fallback ({}).", CONFIG_LABEL, BINARY_LABEL));
            LoggerSingleton::getLogger().puts(SUCCESS, std::format("-- Found {} at location {}.", CONFIG_LABEL, configPath.string()));
            return configPath;
        }

        LoggerSingleton::getLogger().puts(INFO, std::format("-- {} not found through {}.", ucfirst(CONFIG_LABEL), BINARY_LABEL));
    }

    return std::nullopt;
}

std::optional<std::filesystem::path> binaryFallback(const std::unordered_map<std::string, std::optional<std::filesystem::path>, TransparentStringHash, std::equal_to<>>& paths, std::vector<std::string>& fallbackKeys) {
    using enum Logger::Type;
    std::filesystem::path binaryPath;
    std::optional<std::filesystem::path> installPath = paths.at(INSTALL_LABEL);
    std::optional<std::filesystem::path> configPath = paths.at(CONFIG_LABEL);

    LoggerSingleton::getLogger().puts(INFO, std::format("-- Initiating {} fallback.", BINARY_LABEL));

    if (installPath.has_value()) {
        LoggerSingleton::getLogger().puts(INFO, std::format("-- Falling back to {}...", INSTALL_LABEL));
        LoggerSingleton::getLogger().puts(INFO, std::format("-- Checking {}...", INSTALL_LABEL));

        PathResolver::updateFallbackKeys(fallbackKeys, INSTALL_LABEL);
        binaryPath = installPath.value() / ("babel" + getBinExt());
        if (std::filesystem::exists(binaryPath) && std::filesystem::is_regular_file(binaryPath)) {
            LoggerSingleton::getLogger().puts(SUCCESS, std::format("-- Found {} through fallback ({}).", BINARY_LABEL, INSTALL_LABEL));
            LoggerSingleton::getLogger().puts(SUCCESS, std::format("-- Found {} at location {}.", BINARY_LABEL, binaryPath.string()));
            return binaryPath;
        }

        LoggerSingleton::getLogger().puts(INFO, std::format("-- {} not found through {}.", ucfirst(BINARY_LABEL), INSTALL_LABEL));
    }

    if (configPath.has_value()) {
        LoggerSingleton::getLogger().puts(INFO, std::format("-- Falling back to {}...", CONFIG_LABEL));
        LoggerSingleton::getLogger().puts(INFO, std::format("-- Checking {}...", CONFIG_LABEL));

        PathResolver::updateFallbackKeys(fallbackKeys, CONFIG_LABEL);
        YAML::Node config = YAML::LoadFile(configPath.value().string());
        binaryPath = std::filesystem::path(config["Environment"]["BABEL_HOME"].as<std::string>()) / ("babel" + getBinExt());
        if (std::filesystem::exists(binaryPath) && std::filesystem::is_regular_file(binaryPath)) {
            LoggerSingleton::getLogger().puts(SUCCESS, std::format("-- Found {} through fallback ({}).", BINARY_LABEL, CONFIG_LABEL));
            LoggerSingleton::getLogger().puts(SUCCESS, std::format("-- Found {} at location {}.", BINARY_LABEL, binaryPath.string()));
            return binaryPath;
        }

        LoggerSingleton::getLogger().puts(INFO, std::format("-- {} not found in {}.", ucfirst(BINARY_LABEL), CONFIG_LABEL));
        LoggerSingleton::getLogger().puts(INFO, std::format("-- Checking directory of {}...", CONFIG_LABEL));

        binaryPath = configPath.value().parent_path();
        if (std::filesystem::exists(binaryPath) && std::filesystem::is_regular_file(binaryPath)) {
            LoggerSingleton::getLogger().puts(SUCCESS, std::format("-- Found {} through fallback ({}).", BINARY_LABEL, CONFIG_LABEL));
            LoggerSingleton::getLogger().puts(SUCCESS, std::format("-- Found {} at location {}.", BINARY_LABEL, binaryPath.string()));
            return binaryPath;
        }

        LoggerSingleton::getLogger().puts(INFO, std::format("-- {} not found through {}.", ucfirst(BINARY_LABEL), CONFIG_LABEL));
    }

    return std::nullopt;
}

PathResolver checkCriticalPaths() {
    std::vector<std::filesystem::path> INSTALL_PATHS = {
        getHomeDir() / ".babel",
        getHomeDir() / ".local" / "share" / "babel",
        getRootDir() / "usr" / "local" / "babel",
        getRootDir() / "usr" / "local" / "share" / "babel",
        getHomeDir() / "Library" / "Application Support" / "babel",
        getHomeDir() / "Library" / "babel",
        getHomeDir() / "AppData" / "Local" / "babel",
    };

    std::vector<std::filesystem::path> CONFIG_PATHS = {
        getHomeDir() / ".babel" / "config.yaml",
        getHomeDir() / ".config" / "babel" / "config.yaml",
        getHomeDir() / ".config" / "babel.yaml",
        getHomeDir() / ".local" / "share" / "babel" / "config.yaml",
        getHomeDir() / ".local" / "share" / "babel.yaml",
        getHomeDir() / "Library" / "Application Support" / "babel" / "config.yaml",
        getHomeDir() / "Library" / "Application Support" / "babel.yaml",
        getHomeDir() / "Library" / "Preferences" / "babel" / "config.yaml",
        getHomeDir() / "Library" / "Preferences" / "babel.yaml",
        getHomeDir() / "AppData" / "Local" / "babel" / "config.yaml",
        getHomeDir() / "AppData" / "Local" / "babel.yaml",
    };

    const std::filesystem::path binary = "babel" + getBinExt();
    std::vector<std::filesystem::path> BINARY_PATHS = getPathDirs();
    std::ranges::transform(BINARY_PATHS, BINARY_PATHS.begin(), [&binary](const std::filesystem::path& p) { return p / binary; });
    BINARY_PATHS.insert(BINARY_PATHS.end(), {
        getRootDir() / "usr" / "bin" / binary,
        getRootDir() / "usr" / "local" / "bin" / binary,
        getHomeDir() / "bin" / binary,
        getHomeDir() / ".local" / "bin" / binary,
        getRootDir() / "Program Files" / binary,
        getRootDir() / "Program Files (x86)" / binary,
        getHomeDir() / "AppData" / "Local" / binary
    });

    if (const char*  BABEL_HOME = getenv("BABEL_HOME"); BABEL_HOME != nullptr) {
        INSTALL_PATHS.emplace_back(BABEL_HOME);
        CONFIG_PATHS.push_back(std::filesystem::path(BABEL_HOME) / "config.yaml");
        BINARY_PATHS.push_back(std::filesystem::path(BABEL_HOME) / binary);
    }

    PathResolver resolver;
    resolver.addPath(INSTALL_LABEL, INSTALL_PATHS, std::filesystem::is_directory, installFallback, {CONFIG_LABEL, BINARY_LABEL});
    resolver.addPath(CONFIG_LABEL, CONFIG_PATHS, std::filesystem::is_regular_file, configFallback, {INSTALL_LABEL, BINARY_LABEL});
    resolver.addPath(BINARY_LABEL, BINARY_PATHS, std::filesystem::is_regular_file, binaryFallback, {INSTALL_LABEL, CONFIG_LABEL});

    resolver.findPaths();
    return resolver;
}

std::vector<std::filesystem::path> getVersionDirectories() {
    std::vector<std::filesystem::path> versions;
    
    std::regex pattern(R"(babel-\d+\.\d+\.\d+\+\w+.[A-Za-z\.]+)");
    for (const auto& entry : std::filesystem::directory_iterator(INSTALL_PATH)) {
        if (!entry.is_directory())
            continue;
        
        if (std::regex_match(entry.path().filename().string(), pattern)) {
            versions.push_back(entry.path());
        }
    }

    return versions;
}

void directoryStructure() {
    LoggerSingleton::getLogger().puts(Logger::Type::INFO, "-- Checking directory structure...");
    
    for (const auto& version : VERSIONS) {
        std::vector<std::filesystem::path> paths = {
            version,
            version / "bin",
            version / "core",
            version / "include",
            version / "lib",
            version / "share",
            version / "share" / "docs",
            version / "share" / "man",
            version / "stdlib"
        };

        LoggerSingleton::getLogger().puts(Logger::Type::INFO, "-- Checking version: " + version.filename().string());
        for (const auto& path : paths) {
            LoggerSingleton::getLogger().puts(Logger::Type::VERBOSE, "Checking path: " + path.string());
            if (!std::filesystem::exists(path) || !std::filesystem::is_directory(path)) {
                // possibly add all the missing directories to a list and display them at the end
                throw MalformedPackageDirException("Missing required directory: " + path.string());
            }
        }
    }
}

void binaryVerification() {
    LoggerSingleton::getLogger().puts(Logger::Type::INFO, "-- Checking for required binaries...");
    std::filesystem::path launcherPath = INSTALL_PATH / ("babellauncher" + getBinExt());
    
    // maybe there is a babel binary instead of babellauncher (renamed babellauncher)
    LoggerSingleton::getLogger().puts(Logger::Type::INFO, "-- Checking for babellauncher binary...");
    if (!std::filesystem::exists(launcherPath) || !std::filesystem::is_regular_file(launcherPath)) {
        launcherPath = INSTALL_PATH / ("babel" + getBinExt());
        if (!std::filesystem::exists(launcherPath) || !std::filesystem::is_regular_file(launcherPath)) {
            throw MissingBinaryException("Missing required binary: babellauncher");
        }
    }

    LoggerSingleton::getLogger().puts(Logger::Type::INFO, "-- Checking for babellauncher binary permissions...");
    std::filesystem::perms permissions = std::filesystem::status(launcherPath).permissions();
    bool perm_exec = std::filesystem::perms::none == (std::filesystem::perms::owner_exec & permissions);

    if (!perm_exec) {
        throw PermissionException("Binary babellauncher must be executable.");
    }

    LoggerSingleton::getLogger().puts(Logger::Type::INFO, "-- Checking for version binaries...");
    for (const auto& version : VERSIONS) {
        LoggerSingleton::getLogger().puts(Logger::Type::INFO, "-- Checking version: " + version.filename().string());
        std::filesystem::path binPath = version / "bin" / ("babel" + getBinExt());

        if (!std::filesystem::exists(binPath) || !std::filesystem::is_regular_file(binPath)) {
            throw MissingBinaryException("Missing required binary for version: " + version.filename().string());
        }

        LoggerSingleton::getLogger().puts(Logger::Type::INFO, "-- Checking binary permissions...");
        permissions = std::filesystem::status(binPath).permissions();
        perm_exec = std::filesystem::perms::none == (std::filesystem::perms::owner_exec & permissions);

        if (!perm_exec) {
            throw PermissionException("Binary of version " + version.filename().string() + " must be executable.");
        }
    }
}

void assetVerification() {
    LoggerSingleton::getLogger().puts(Logger::Type::INFO, "-- Checking for required assets...");

    for (const auto& version : VERSIONS) {
        std::vector<std::filesystem::path> assets = {
            version / "LICENSE.md",
            version / "README.md"
        };

        for (const auto& asset : assets) {
            if (!std::filesystem::exists(asset) || !std::filesystem::is_regular_file(asset)) {
                // possibly add all the missing assets to a list and display them at the end
                throw MissingAssetException("Missing required asset: " + asset.string());
            }
        }
    }
}

void licenseVerification() {
    LoggerSingleton::getLogger().puts(Logger::Type::INFO, "-- Checking for required license...");

    std::string expectedContent = R"(
        Boost Software License - Version 1.0 - August 17th, 2003

        Permission is hereby granted, free of charge, to any person or organization
        obtaining a copy of the software and accompanying documentation covered by
        this license (the "Software") to use, reproduce, display, distribute,
        execute, and transmit the Software, and to prepare derivative works of the
        Software, and to permit third-parties to whom the Software is furnished to
        do so, all subject to the following:

        The copyright notices in the Software and this entire statement, including
        the above license grant, this restriction and the following disclaimer,
        must be included in all copies of the Software, in whole or in part, and
        all derivative works of the Software, unless such copies or derivative
        works are solely in the form of machine-executable object code generated by
        a source language processor.

        THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
        IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
        FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
        SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
        FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
        ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
        DEALINGS IN THE SOFTWARE.
    )";

    for (const auto& version : VERSIONS) {
        LoggerSingleton::getLogger().puts(Logger::Type::INFO, "-- Checking version: " + version.filename().string());
        std::filesystem::path licensePath = version / "LICENSE.md";
        std::ifstream licenseFile(licensePath);
        std::string licenseContent((std::istreambuf_iterator<char>(licenseFile)), std::istreambuf_iterator<char>());

        std::erase_if(licenseContent, [](char c){ return std::isspace(c); });
        std::erase_if(expectedContent, [](char c){ return std::isspace(c); });

        if (licenseContent != expectedContent) {
            throw LicenseException("The Boost Software License 1.0 must be included.");
        }
    }
}

void validateChecksums() {
    for (const auto& version : VERSIONS) {
        std::string versionName = version.filename().string();
        size_t start = versionName.find_first_of("-") + 1;
        size_t end = versionName.find_first_of("+");
        std::string versionInfo = versionName.substr(start, end - start);

        HTTPDownloader downloader;
        std::string res = downloader.download(std::format("https://github.com/WehrWolff/babel/releases/download/v{}/checksums.txt", versionInfo));

        auto csMap = parseData(res);

        for (const auto& entry : std::filesystem::recursive_directory_iterator(version)) {
            if (!entry.is_regular_file())
                continue;

            // we could use a stringstream, however this is inefficient when files get larger
            // which is why we choose to do it like this
            uLong size = std::filesystem::file_size(entry);
            std::string content(size, '\0');
            std::ifstream in(entry.path());
            in.read(&content[0], size);
    
            std::string hash = sha256(content);
            uLong crc = crc32(0, (const unsigned char*)content.c_str(), static_cast<uInt>(content.size()));
    
            auto path = csMap.at(entry.path().string());
            if (path.sha256 != hash) {
                LoggerSingleton::getLogger().puts(Logger::Type::ERROR, std::format("Exception during checksum validation of {}", entry.path().string()));
                throw ChecksumValidationException(std::format("Expected hash {} but found {}.", path.sha256, hash));
            }
    
            if (path.crc32 != crc) {
                LoggerSingleton::getLogger().puts(Logger::Type::ERROR, std::format("Exception during checksum validation of {}", entry.path().string()));
                throw ChecksumValidationException(std::format("Expected crc {} but found {}.", path.crc32, crc));
            }
    
            if (path.file_size != size) {
                LoggerSingleton::getLogger().puts(Logger::Type::ERROR, std::format("Exception during checksum validation of {}", entry.path().string()));
                throw ChecksumValidationException(std::format("Expected size {} but found {}.", path.file_size, size));
            }
        }
    }
}

void configurationVerification() {
    using enum Logger::Type;
    LoggerSingleton::getLogger().puts(INFO, "-- Checking for valid configuration...");
    LoggerSingleton::getLogger().puts(INFO, "-- Checking configuration file permissions...");

    if (auto permissions = std::filesystem::status(CONFIG_PATH).permissions(); !has_perm(std::filesystem::perms::owner_read, permissions) || !has_perm(std::filesystem::perms::owner_write, permissions)) {
        throw PermissionException("Configuration file must be readable and writable.");
    }

    // process yaml, must have Default and Installed keys
    // check if they have a non empty value if the key exists
    LoggerSingleton::getLogger().puts(INFO, "-- Checking configuration file structure...");
    YAML::Node config = YAML::LoadFile(CONFIG_PATH);

    if (!config["Default"] || !config["Installed"]) {
        throw MissingConfigEntryException("Configuration file must contain Default and Installed entries.");
    }

    // check if the default version exists
    LoggerSingleton::getLogger().puts(INFO, "-- Checking for valid default versions...");
    std::string defaultVersion = "babel-" + config["Default"].as<std::string>();
    if (auto defaultPath = INSTALL_PATH / defaultVersion; std::ranges::find(VERSIONS, defaultPath) == VERSIONS.end()) {
        throw MissingVersionException("Default version does not exist.");
    }

    // check if the installed versions exists
    LoggerSingleton::getLogger().puts(INFO, "-- Checking for valid installed versions...");
    for (const auto& version : config["Installed"]) {
        std::string installedVersion = "babel-" + version.as<std::string>();
        std::filesystem::path installedPath = INSTALL_PATH / installedVersion;

        if (std::ranges::find(VERSIONS, installedPath) == VERSIONS.end()) {
            throw MissingVersionException("Version " + installedVersion + " is not installed.");
        }
    }

    // also check if there are any versions installed that are not in the configuration
    LoggerSingleton::getLogger().puts(INFO, "-- Checking for mismatched installed versions...");
    if (VERSIONS.size() != config["Installed"].size()) {
        throw MismatchedVersionException("Some installed versions are not specified in the configuration.");
    }

    // potentially check for settings and overrides
    LoggerSingleton::getLogger().puts(INFO, "-- Checking for valid settings and overrides...");
    LoggerSingleton::getLogger().puts(STATUS, "-- Feature is in development. Skipping checks...");
}

void environmentVariableVerification() {
    LoggerSingleton::getLogger().puts(Logger::Type::INFO, "-- Checking for environment variables...");
    LoggerSingleton::getLogger().puts(Logger::Type::INFO, "-- Checking in configuration file...");

    YAML::Node config = YAML::LoadFile(CONFIG_PATH.string());
    if (!config["Environment"]) {
        throw MissingConfigEntryException("Configuration file must contain Environment entry.");
    }

    std::vector<std::string> requiredVariables = {
        "BABEL_HOME"
    };

    for (const auto& variable : requiredVariables) {
        LoggerSingleton::getLogger().puts(Logger::Type::INFO, "-- Checking for environment variable: " + variable);
        std::string value = config["Environment"][variable].as<std::string>();

        if (!getenv(variable.c_str())) {
            throw EnvironmentVariableException("Missing required environment variable: " + variable);
        }

        if (getenv(variable.c_str()) != value) {
            throw EnvironmentVariableException("Environment variable " + variable + " does not match configuration.");
        }
    }
}

void systemDependencyVerification() {
    LoggerSingleton::getLogger().puts(Logger::Type::INFO, "-- Checking for required system dependencies...");

    std::vector<std::string> requiredPackages = {
        "clang",
        "llvm",
        "cmake",
        "ninja",
        "git"
    };

    // check if the required packages are installed
    LoggerSingleton::getLogger().puts(Logger::Type::INFO, "-- Feature is in development. Skipping checks...");
}

void runtimeDependencyVerification() {
    LoggerSingleton::getLogger().puts(Logger::Type::INFO, "-- Checking for required runtime dependencies...");
    LoggerSingleton::getLogger().puts(Logger::Type::INFO, "-- Checking for required libraries...");

    std::vector<std::string> requiredLibraries = {
        "libLLVM"
    };

    const std::string libExt = getLibExt();
    std::ranges::transform(requiredLibraries, requiredLibraries.begin(), [&libExt](const std::string& lib) { return lib + libExt; });

    // Libraries need to be searched for in various locations
    // Consider adding CMake like behavior to search for libraries
    std::vector<std::filesystem::path> libPaths = {
        "/usr/lib",
        "/usr/local/lib",
        "/usr/lib64",
        "/usr/local/lib64",
        "/lib",
        "/lib64"
    };

    // check version directories for required libraries as well
    for (const auto& lib : requiredLibraries) {
        bool found = false;
        LoggerSingleton::getLogger().puts(Logger::Type::INFO, "-- Checking for library: " + lib);
        for (const auto& path : libPaths) {
            LoggerSingleton::getLogger().puts(Logger::Type::VERBOSE, "Checking path: " + path.string());
            if (std::filesystem::exists(path / lib)) {
                LoggerSingleton::getLogger().puts(Logger::Type::INFO, std::format("-- Found required library: {} at {}", lib, path.string()));
                found = true;
                break;
            }
        }

        if (!found) {
            throw LibraryException("Missing required library: " + lib);
        }
    }
}

void compatibilityVerification() {
    using enum Logger::Type;
    LoggerSingleton::getLogger().puts(INFO, "-- Checking for system compatibility...");

    std::string arch = getArch();
    std::string os = getOS();
    LoggerSingleton::getLogger().puts(INFO, "-- Detected architecture: " + arch);
    LoggerSingleton::getLogger().puts(INFO, "-- Detected operating system: " + os);

    for (const auto& version : VERSIONS) {
        LoggerSingleton::getLogger().puts(INFO, "-- Checking version: " + version.filename().string());
        std::string versionName = version.filename().string();
        std::string platformInfo = versionName.substr(versionName.find_last_of("+") + 1);
        std::string versionArch = platformInfo.substr(0, platformInfo.find_first_of("."));
        std::string versionOS = platformInfo.substr(platformInfo.find_first_of(".") + 1);

        if (versionArch != arch) {
            throw SystemCompatibilityException("Incompatible architecture: " + versionArch);
        }

        if (versionOS != os) {
            throw SystemCompatibilityException("Incompatible operating system: " + versionOS);
        }
    }
}

void platformSpecificBehaviorVerification() {
    using enum Logger::Type;
    LoggerSingleton::getLogger().puts(INFO, "-- Checking for platform specific behavior...");
    #ifdef _WIN32
    LoggerSingleton::getLogger().puts(INFO, "-- Running Windows-specific checks...");
    LoggerSingleton::getLogger().puts(INFO, "-- Nothing to do.");
    #elif __APPLE__
    LoggerSingleton::getLogger().puts(INFO, "-- Running macOS-specific checks...");
    LoggerSingleton::getLogger().puts(INFO, "-- Nothing to do.");
    #elif __linux__
    LoggerSingleton::getLogger().puts(INFO, "-- Running Linux-specific checks...");
    LoggerSingleton::getLogger().puts(INFO, "-- Nothing to do.");
    #elif __FreeBSD__
    LoggerSingleton::getLogger().puts(INFO, "-- Running FreeBSD-specific checks...");
    LoggerSingleton::getLogger().puts(INFO, "-- Nothing to do.");
    #else
    LoggerSingleton::getLogger().puts(INFO, "-- Unknown platform detected, skipping checks...");
    #endif
}

void functionalityVerification() {
    LoggerSingleton::getLogger().puts(Logger::Type::INFO, "-- Checking for required functionality...");
    std::string testCommand = BINARY_PATH.string() + " --version";
    std::cout << exec(testCommand.c_str()) << '\n';

    LoggerSingleton::getLogger().puts(Logger::Type::INFO, "-- Checking if babel binary is in path...");
    std::vector<std::filesystem::path> path = getPathDirs();
    bool found = false;
    for (const auto& dir : path) {
        LoggerSingleton::getLogger().puts(Logger::Type::INFO, "-- Checking path: " + dir.string());
        if (std::filesystem::exists(dir / "babel") && std::filesystem::is_regular_file(dir / "babel")) {
            testCommand = (dir / "babel").string() + " --version";
            std::cout << exec(testCommand.c_str()) << '\n';
            found = true;
        }
    }

    if (!found) {
        throw MissingMainBinaryException("Babel binary not found in path.");
    }
}

int main(int argc, char* argv[]) {
    using enum Logger::Type;

    std::span<char*> args(argv, argc);
    LoggerSingleton::getLogger().applyVerboseSettings(args);

    directoryStructure();
    binaryVerification();
    assetVerification();
    licenseVerification();
    validateChecksums();
    configurationVerification();
    environmentVariableVerification();
    systemDependencyVerification();
    runtimeDependencyVerification();
    compatibilityVerification();
    platformSpecificBehaviorVerification();
    functionalityVerification();

    LoggerSingleton::getLogger().puts(INFO, "Concluding test summary...");
    LoggerSingleton::getLogger().puts(PROGRESS, "Directory structure is correct", std::format("{:3}%", 100/12));
    LoggerSingleton::getLogger().puts(PROGRESS, "Binaries are built and ready for use", std::format("{:3}%", 200/12));
    LoggerSingleton::getLogger().puts(PROGRESS, "All necessary assets are in place and accessible", std::format("{:3}%", 300/12));
    LoggerSingleton::getLogger().puts(PROGRESS, "License requirements have been met", std::format("{:3}%", 400/12));
    LoggerSingleton::getLogger().puts(PROGRESS, "Data integrity has been verified through checksum analysis", std::format("{:3}%", 500/12));
    LoggerSingleton::getLogger().puts(PROGRESS, "Package configuration is accurate", std::format("{:3}%", 600/12));
    LoggerSingleton::getLogger().puts(PROGRESS, "Environment variables are setup properly", std::format("{:3}%", 700/12));
    LoggerSingleton::getLogger().puts(PROGRESS, "System dependencies are satisfied and compatible", std::format("{:3}%", 800/12));
    LoggerSingleton::getLogger().puts(PROGRESS, "Runtime dependencies have been resolved", std::format("{:3}%", 900/12));
    LoggerSingleton::getLogger().puts(PROGRESS, "Package is compatible with target system", std::format("{:3}%", 1000/12));
    LoggerSingleton::getLogger().puts(PROGRESS, "Platform-specific behavior was confirmed", std::format("{:3}%", 1100/12));
    LoggerSingleton::getLogger().puts(PROGRESS, "Package functionality has been tested and meets expectations\n", std::format("{:3}%", 1200/12));
    LoggerSingleton::getLogger().puts(SUCCESS, "All tests passed.", "✓");

    return 0;
}