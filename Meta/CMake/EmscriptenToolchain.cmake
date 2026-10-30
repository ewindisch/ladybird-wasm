# Emscripten Toolchain for Ladybird WASM Port
# This toolchain file configures CMake to build Ladybird for WebAssembly using Emscripten

# CMake has never shipped a Platform/Emscripten module; emsdk provides one.
# Register it so try_compile()-based feature checks (e.g. LIBPNG_HAS_APNG) can
# resolve the system and run their test projects.
list(APPEND CMAKE_MODULE_PATH "$ENV{EMSDK}/upstream/emscripten/cmake/Modules")

set(CMAKE_SYSTEM_NAME Emscripten)
set(CMAKE_SYSTEM_PROCESSOR wasm)

# Compiler configuration
set(CMAKE_C_COMPILER emcc)
set(CMAKE_CXX_COMPILER em++)
set(CMAKE_AR emar)
set(CMAKE_RANLIB emranlib)

# C++23 is required for Ladybird
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type specific flags
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -flto -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g4 -gsource-map -sASSERTIONS=2")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g3 -gsource-map -DNDEBUG")

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")

# Don't search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Platform-specific settings
set(EMSCRIPTEN ON)
add_definitions(-D__EMSCRIPTEN__=1)
