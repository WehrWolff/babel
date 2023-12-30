@echo off

:: Install conan
python -m pip install conan
conan profile detect || exit /b

:: Install packages
conan install . --output-folder=.\build --build=missing --settings=compiler.cppstd=20 --settings=build_type=Release

:: Copy required files
copy src\grammar.txt build\build\grammar.txt

:: Build commands
cd build

:: Run cmake and make
cmake .. -DCMAKE_TOOLCHAIN_FILE=.\build\build\Release\generators\conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
nmake

:: Run in shell
:: babel
