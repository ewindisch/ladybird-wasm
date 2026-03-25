include_guard()

if(EMSCRIPTEN)
    # FFmpeg is not available for WASM; provide empty stub targets so LibMedia
    # compiles without error (media playback is a no-op in the WASM build).
    foreach(_pkg AVCODEC AVFORMAT AVUTIL LIBSWRESAMPLE)
        if(NOT TARGET PkgConfig::${_pkg})
            add_library(PkgConfig::${_pkg} INTERFACE IMPORTED)
        endif()
    endforeach()
elseif(NOT ANDROID)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(AVCODEC REQUIRED IMPORTED_TARGET libavcodec)
    pkg_check_modules(AVFORMAT REQUIRED IMPORTED_TARGET libavformat)
    pkg_check_modules(AVUTIL REQUIRED IMPORTED_TARGET libavutil)
    pkg_check_modules(LIBSWRESAMPLE REQUIRED IMPORTED_TARGET libswresample)
else()
    find_package(FFMPEG REQUIRED)
endif()
