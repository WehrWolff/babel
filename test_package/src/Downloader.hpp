/*
 * Copyright (c) 2025 WehrWolff
 *
 * This file is part of babel, which is BSL-1.0 licensed.
 * See http://opensource.org/licenses/bsl-1.0
 */

#ifndef HTTPDOWNLOADER_HPP
#define HTTPDOWNLOADER_HPP

#include <string>
#include <curl/curl.h>
#include <curl/easy.h>
#include <sstream>
#include <iostream>

// A non-threadsafe simple libcURL-easy based HTTP downloader
class HTTPDownloader {
public:
    HTTPDownloader();
    ~HTTPDownloader();
    /**
     * Download a file using HTTP GET and store in in a std::string
     * @param url The URL to download
     * @return The download result
     */
    std::string download(const std::string& url);
private:
    CURL* curl;
};

#endif  /* HTTPDOWNLOADER_HPP */