# Build

## First time

1. Need a Conan profile to set the compile, build configuration, architecture...
`conan profile detect --force`

2. Download and install dependencies specified in conanfile.txt
`conan install . --build=missing -s build_type=Release`

3. Build and run application

```bash
cmake -B build/Release -S . -DCMAKE_TOOLCHAIN_FILE=build/Release/generators/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build/Release
./build/Release/spb
```
