/*
 * Copyright (c) 2025 WehrWolff
 *
 * This file is part of babel, which is BSL-1.0 licensed.
 * See http://opensource.org/licenses/bsl-1.0
 */

#include "Downloader.hpp"
#include "Exceptions.hpp"
#include "Logging.hpp"
#include <format>

// Use before curl 8.13.0 instead of magic value 1L
// Starting from curl 8.13.0 this is already implemented
// Version is represented in hex 0xAABBCC, with major (AA), minor (BB), patch (CC)
#if LIBCURL_VERSION_NUM < 0x080d00
#define CURLFOLLOW_ALL 1L
#endif

size_t write_data(const char* ptr, size_t size, size_t nmemb, std::stringstream* stream) {
    std::string data(ptr, size * nmemb);
    *stream << data << std::endl;
    return size * nmemb;
}

HTTPDownloader::HTTPDownloader() {
    curl = curl_easy_init();
}

HTTPDownloader::~HTTPDownloader() {
    curl_easy_cleanup(curl);
}

std::string HTTPDownloader::download(const std::string& url) {
    if (!curl)
        throw DownloadException("curl_easy_init() failed");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_3); // Force use of strong cryptographic algorithms

    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, CURLFOLLOW_ALL); // We tell libcurl to follow redirection
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1); // Prevent "longjmp causes uninitialized stack frame" bug
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "deflate"); // Compress response using zlib algorithm

    std::stringstream out;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out);

    // Perform the request, get the return code and check for errors
    if (CURLcode res = curl_easy_perform(curl); res != CURLE_OK) {
        throw DownloadException(std::format("curl_easy_perform() failed: {}", curl_easy_strerror(res)));
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    // The request itself can be successful but an error like 404 NotFound can still occur
    // We don't set the option CURLOPT_FAILONERROR, so that the response (error message) is still written to the stream
    if (http_code >= 400) {
        LoggerSingleton::getLogger().puts(Logger::Type::ERROR, std::format("\nError: HTTP code {} returned. Response: {}", http_code, out.str()));
        throw DownloadException("curl_easy_perform() failed: HTTP response code said error");
    }
    
    return out.str();
}