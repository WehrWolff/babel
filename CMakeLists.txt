cmake_minimum_required(VERSION 3.12)
project(babel)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SOURCE_FILES
    src/shell.cpp
)

find_package(Boost 1.83.0 REQUIRED COMPONENTS algorithm)

add_executable(babel ${SOURCE_FILES})
target_include_directories(babel PRIVATE src)
target_link_libraries(babel PRIVATE ${Boost_LIBRARIES})


# ----- Testing Configuration -----

set(TEST_FILES
    tests/test_shell.cpp
)

include_directories(src test)
find_package(GTest REQUIRED)

add_executable(babel_tests ${TEST_FILES})
target_include_directories(babel_tests PRIVATE ${GTEST_INCLUDE_DIRS})
target_link_libraries(babel_tests PRIVATE GTest::GTest GTest::Main)
target_link_libraries(babel_tests PRIVATE ${Boost_LIBRARIES})

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-fprofile-arcs -ftest-coverage)
    add_link_options(-lgcov)
endif()

enable_testing()
add_test(NAME BabelTests COMMAND babel_tests)

add_custom_target(coverage
    COMMAND ${CMAKE_COMMAND} -E remove coverage.info
    COMMAND lcov --capture --directory . --output-file coverage.info
    COMMAND lcov --remove coverage.info '/usr/*' --output-file coverage.info
    COMMAND lcov --list coverage.info
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS babel_tests
    COMMENT "Generating coverage information..."
)
