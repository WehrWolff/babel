cmake_minimum_required(VERSION 3.12...3.31)
project(babel)

set(BABEL_VERSION_MAJOR 0)
set(BABEL_VERSION_MINOR 0)
set(BABEL_VERSION_PATCH 0)
set(BABEL_VERSION "${BABEL_VERSION_MAJOR}.${BABEL_VERSION_MINOR}.${BABEL_VERSION_PATCH}")

# set(BABEL_VERSION_SPECIFIER "pre-alpha")
# set(BABEL_VERSION "${BABEL_VERSION_MAJOR}.${BABEL_VERSION_MINOR}.${BABEL_VERSION_PATCH}-${BABEL_VERSION_SPECIFIER}")

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
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-instr-generate -fcoverage-mapping")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-instr-generate -fcoverage-mapping")
endif()

enable_testing()
add_test(NAME BabelTests COMMAND babel_tests)


# ----- Coverage Configuration -----

if(CMAKE_COMPILER_IS_GNUCXX)
    # GCC (with lcov)
    add_custom_target(coverage
        COMMAND ${CMAKE_COMMAND} -E remove coverage.info
        COMMAND lcov --capture --directory . --output-file coverage.info --ignore-errors mismatch
        COMMAND lcov --remove coverage.info '/usr/*' --output-file coverage.info
        COMMAND lcov --list coverage.info
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating coverage information..."
    )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # Clang (with llvm-cov and llvm-profdata)
    add_custom_target(coverage
        COMMAND llvm-profdata merge -sparse default.profraw -o default.profdata
        COMMAND llvm-cov show ./babel_tests -instr-profile=default.profdata --format=html -output-dir=coverage_report
        COMMAND llvm-cov export ./babel_tests -instr-profile=default.profdata --format=lcov > coverage.lcov
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating coverage information..."
    )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # MSVC (with Visual Studio built-in coverage tool)
    add_custom_target(coverage
        COMMAND ${CMAKE_COMMAND} -E remove coverage.xml
        COMMAND cmd /c "xcopy /y $(SolutionDir)$(Configuration)\*.pdb $(CMAKE_BINARY_DIR)"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating coverage information..."
    )
else()
    message(WARNING "Unsupported compiler for coverage generation.")
endif()


add_custom_target(test_coverage
    COMMAND ./babel_tests
    COMMAND ${CMAKE_COMMAND} --build . --target coverage
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS babel_tests
    COMMENT "Running tests..."
)


# ----- Package Configuration -----

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
  set(OS_STRING "windows")
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
  set(OS_STRING "macos")
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
  set(OS_STRING "gnu.linux")
elseif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
  set(OS_STRING "freebsd")
else()
  set(OS_STRING "unknown")
endif()

set(PACKAGE_VERSION "${BABEL_VERSION}+${CMAKE_SYSTEM_PROCESSOR}.${OS_STRING}")
set(PACKAGE_VERSION_DIR "${PROJECT_NAME}-${PACKAGE_VERSION}")

if(WIN32)
    set(CMAKE_INSTALL_PREFIX "$ENV{HOMEDRIVE}/$ENV{HOMEPATH}/.babel")
else()
    set(CMAKE_INSTALL_PREFIX "$ENV{HOME}/.babel")
endif()

install(TARGETS babel DESTINATION ${PACKAGE_VERSION_DIR}/bin)
install(CODE "file(MAKE_DIRECTORY \${CMAKE_INSTALL_PREFIX}/${PACKAGE_VERSION_DIR}/core)")
install(CODE "file(MAKE_DIRECTORY \${CMAKE_INSTALL_PREFIX}/${PACKAGE_VERSION_DIR}/include)")
install(CODE "file(MAKE_DIRECTORY \${CMAKE_INSTALL_PREFIX}/${PACKAGE_VERSION_DIR}/lib)")
install(CODE "file(MAKE_DIRECTORY \${CMAKE_INSTALL_PREFIX}/${PACKAGE_VERSION_DIR}/share/docs)")
install(CODE "file(MAKE_DIRECTORY \${CMAKE_INSTALL_PREFIX}/${PACKAGE_VERSION_DIR}/share/man)")
install(CODE "file(MAKE_DIRECTORY \${CMAKE_INSTALL_PREFIX}/${PACKAGE_VERSION_DIR}/stdlib)")
install(FILES LICENSE.md README.md DESTINATION ${PACKAGE_VERSION_DIR})

install(DIRECTORY assets DESTINATION ${PACKAGE_VERSION_DIR}/bin)
install(FILES src/grammar.txt DESTINATION ${PACKAGE_VERSION_DIR}/bin/src)

install(CODE "file(WRITE \${CMAKE_INSTALL_PREFIX}/config.yaml
  \"Default: \\\"${PACKAGE_VERSION}\\\"\n\"
  \"Installed:\n\"
  \"  - \\\"${BABEL_VERSION}\\\"\n\"
  \"Environment:\n\"
  \"  BABEL_HOME: \\\"\${CMAKE_INSTALL_PREFIX}\\\"\n\"
  \"Settings: {}\n\"
  \"Overrides: []\")")