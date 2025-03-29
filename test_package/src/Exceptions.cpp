/*
 * Copyright (c) 2025 WehrWolff
 *
 * This file is part of babel, which is BSL-1.0 licensed.
 * See http://opensource.org/licenses/bsl-1.0
 */

#include "Exceptions.hpp"
#include "Logging.hpp"

using Type = Logger::Type;

NotFoundException::NotFoundException(const std::string& msg) : std::runtime_error(msg) {
    message = Logger::formatMsg(Type::ERROR, "NotFoundException: " + msg);
}

const char* NotFoundException::what() const noexcept {
    return message.c_str();
}


MalformedPackageDirException::MalformedPackageDirException(const std::string& msg) : std::runtime_error(msg) {
    message = Logger::formatMsg(Type::ERROR, "MalformedPackageDirException: " + msg);
}

const char* MalformedPackageDirException::what() const noexcept {
    return message.c_str();
}


MissingAssetException::MissingAssetException(const std::string& msg) : std::runtime_error(msg) {
    message = Logger::formatMsg(Type::ERROR, "MissingAssetException: " + msg);
}

const char* MissingAssetException::what() const noexcept {
    return message.c_str();
}


LicenseException::LicenseException(const std::string& msg) : std::runtime_error(msg) {
    message = Logger::formatMsg(Type::ERROR, "LicenseException: " + msg);
}

const char* LicenseException::what() const noexcept {
    return message.c_str();
}


PermissionException::PermissionException(const std::string& msg) : std::runtime_error(msg) {
    message = Logger::formatMsg(Type::ERROR, "PermissionException: " + msg);
}

const char* PermissionException::what() const noexcept {
    return message.c_str();
}


MissingConfigEntryException::MissingConfigEntryException(const std::string& msg) : std::runtime_error(msg) {
    message = Logger::formatMsg(Type::ERROR, "MissingConfigEntryException: " + msg);
}

const char* MissingConfigEntryException::what() const noexcept {
    return message.c_str();
}


MissingVersionException::MissingVersionException(const std::string& msg) : std::runtime_error(msg) {
    message = Logger::formatMsg(Type::ERROR, "MissingVersionException: " + msg);
}

const char* MissingVersionException::what() const noexcept {
    return message.c_str();
}


MismatchedVersionException::MismatchedVersionException(const std::string& msg) : std::runtime_error(msg) {
    message = Logger::formatMsg(Type::ERROR, "MismatchedVersionException: " + msg);
}

const char* MismatchedVersionException::what() const noexcept {
    return message.c_str();
}


MissingBinaryException::MissingBinaryException(const std::string& msg) : std::runtime_error(msg) {
    message = Logger::formatMsg(Type::ERROR, "MissingBinaryException: " + msg);
}

const char* MissingBinaryException::what() const noexcept {
    return message.c_str();
}


MissingMainBinaryException::MissingMainBinaryException(const std::string& msg) : std::runtime_error(msg) {
    message = Logger::formatMsg(Type::ERROR, "MissingMainBinaryException: " + msg);
}

const char* MissingMainBinaryException::what() const noexcept {
    return message.c_str();
}


SystemCompatibilityException::SystemCompatibilityException(const std::string& msg) : std::runtime_error(msg) {
    message = Logger::formatMsg(Type::ERROR, "SystemCompatibilityException: " + msg);
}

const char* SystemCompatibilityException::what() const noexcept {
    return message.c_str();
}


BinaryExecutionException::BinaryExecutionException(const std::string& msg) : std::runtime_error(msg) {
    message = Logger::formatMsg(Type::ERROR, "BinaryExecutionException: " + msg);
}

const char* BinaryExecutionException::what() const noexcept {
    return message.c_str();
}


LibraryException::LibraryException(const std::string& msg) : std::runtime_error(msg) {
    message = Logger::formatMsg(Type::ERROR, "LibraryException: " + msg);
}

const char* LibraryException::what() const noexcept {
    return message.c_str();
}


EnvironmentVariableException::EnvironmentVariableException(const std::string& msg) : std::runtime_error(msg) {
    message = Logger::formatMsg(Type::ERROR, "EnvironmentVariableException: " + msg);
}

const char* EnvironmentVariableException::what() const noexcept {
    return message.c_str();
}


ChecksumValidationException::ChecksumValidationException(const std::string& msg) : std::runtime_error(msg) {
    message = Logger::formatMsg(Type::ERROR, "ChecksumValidationException: " + msg);
}

const char* ChecksumValidationException::what() const noexcept {
    return message.c_str();
}


DownloadException::DownloadException(const std::string& msg) : std::runtime_error(msg) {
    message = Logger::formatMsg(Type::ERROR, "DownloadException: " + msg);
}

const char* DownloadException::what() const noexcept {
    return message.c_str();
}
