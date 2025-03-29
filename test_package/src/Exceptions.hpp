/*
 * Copyright (c) 2025 WehrWolff
 *
 * This file is part of babel, which is BSL-1.0 licensed.
 * See http://opensource.org/licenses/bsl-1.0
 */

#ifndef EXCEPTIONS_HPP
#define EXCEPTIONS_HPP

#include <string>
#include <stdexcept>

class NotFoundException : public std::runtime_error {
private:
    std::string message;
public:
    explicit NotFoundException(const std::string& msg);
    const char* what() const noexcept override;
};

class MalformedPackageDirException : public std::runtime_error {
private:
    std::string message;
public:
    explicit MalformedPackageDirException(const std::string& msg);
    const char* what() const noexcept override;
};

class MissingAssetException : public std::runtime_error {
private:
    std::string message;
public:
    explicit MissingAssetException(const std::string& msg);
    const char* what() const noexcept override;
};

class LicenseException : public std::runtime_error {
private:
    std::string message;
public:
    explicit LicenseException(const std::string& msg);
    const char* what() const noexcept override;
};

class PermissionException : public std::runtime_error {
private:
    std::string message;
public:
    explicit PermissionException(const std::string& msg);
    const char* what() const noexcept override;
};

class MissingConfigEntryException : public std::runtime_error {
private:
    std::string message;
public:
    explicit MissingConfigEntryException(const std::string& msg);
    const char* what() const noexcept override;
};

class MissingVersionException : public std::runtime_error {
private:
    std::string message;
public:
    explicit MissingVersionException(const std::string& msg);
    const char* what() const noexcept override;
};

class MismatchedVersionException : public std::runtime_error {
private:
    std::string message;
public:
    explicit MismatchedVersionException(const std::string& msg);
    const char* what() const noexcept override;
};

class MissingBinaryException : public std::runtime_error {
private:
    std::string message;
public:
    explicit MissingBinaryException(const std::string& msg);
    const char* what() const noexcept override;
};

class MissingMainBinaryException : public std::runtime_error {
private:
    std::string message;
public:
    explicit MissingMainBinaryException(const std::string& msg);
    const char* what() const noexcept override;
};

class SystemCompatibilityException : public std::runtime_error {
private:
    std::string message;
public:
    explicit SystemCompatibilityException(const std::string& msg);
    const char* what() const noexcept override;
};

class BinaryExecutionException : public std::runtime_error {
private:
    std::string message;
public:
    explicit BinaryExecutionException(const std::string& msg);
    const char* what() const noexcept override;
};

class LibraryException : public std::runtime_error {
private:
    std::string message;
public:
    explicit LibraryException(const std::string& msg);
    const char* what() const noexcept override;
};

class EnvironmentVariableException : public std::runtime_error {
private:
    std::string message;
public:
    explicit EnvironmentVariableException(const std::string& msg);
    const char* what() const noexcept override;
};

class ChecksumValidationException : public std::runtime_error {
private:
    std::string message;
public:
    explicit ChecksumValidationException(const std::string& msg);
    const char* what() const noexcept override;
};

class DownloadException : public std::runtime_error {
private:
    std::string message;
public:
    explicit DownloadException(const std::string& msg);
    const char* what() const noexcept override;
};

#endif  /* EXCEPTIONS_HPP */