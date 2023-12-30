#!/bin/bash

#Verify conan is installed and make sure a default profile is generated
pip install conan
conan profile detect || true

# Make sure packages are installed
conan install . --output-folder=./build --build=missing --settings=compiler.cppstd=20 --settings=build_type=Release

# Create build dir if it does not exist
# mkdir build

# Copy required files
cp src/grammar.txt build/grammar.txt

# Build commands
ls
cd build
ls

cmake .. -DCMAKE_TOOLCHAIN_FILE=./build/build/Release/generators/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
make

# Run in shell
./babel
