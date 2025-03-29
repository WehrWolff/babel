cmake_minimum_required(VERSION 3.31)
project(BabelPackageTest CXX)

set(SOURCE_FILES
  src/example.cpp
  src/Downloader.cpp
  src/Exceptions.cpp
  src/Logging.cpp
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

FetchContent_Declare(
  yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG 0.8.0
)
FetchContent_MakeAvailable(yaml-cpp)

find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

add_executable(example ${SOURCE_FILES})
target_link_libraries(example yaml-cpp)
target_link_libraries(example crypto)
target_link_libraries(example ${CURL_LIBRARIES})
target_link_libraries(example ${ZLIB_LIBRARIES})
