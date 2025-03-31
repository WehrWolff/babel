#!/bin/bash

# Check if Conan is installed
if ! command -v conan &> /dev/null; then
    if ! pip install conan &> /dev/null; then
        pipx install conan && pipx ensurepath
        export PATH=$PATH:$HOME/.local/bin
    fi
fi

# Make sure a default profile is generated
conan profile detect --exist-ok
conan profile show

# Make sure packages are installed
conan install . --output-folder=./build --build=missing --settings=compiler.cppstd=20 --settings=build_type=Release

# Copy required files
cp src/grammar.txt build/grammar.txt
mkdir build/assets && cp assets/parser.dat build/assets/parser.dat

# Build commands
cd build

cmake .. -DCMAKE_TOOLCHAIN_FILE=./build/build/Release/generators/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release #-DCMAKE_CXX_OUTPUT_EXTENSION_REPLACE=ON
cmake --build .

# Run in shell
# ./babel
