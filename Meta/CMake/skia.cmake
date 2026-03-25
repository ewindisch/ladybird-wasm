include_guard(GLOBAL)

# Emscripten: use a pre-built Skia CPU-only static library.
# Build it once with: cd third_party/skia && ./compile.sh cpu
# (skia_enable_ganesh=false skia_use_webgl=false produces libskia.a)
if(EMSCRIPTEN)
    set(SKIA_PREBUILT_DIR "" CACHE PATH "Path to pre-built Skia directory (contains libskia.a and include/)")
    if(NOT SKIA_PREBUILT_DIR)
        message(FATAL_ERROR "WASM build requires SKIA_PREBUILT_DIR pointing to a CPU-only Skia build. "
            "Build with: cd third_party/skia && ./compile.sh cpu")
    endif()
    # Skia headers use two include styles:
    #   #include <core/SkFoo.h>         (root = skia_source/include)
    #   #include "include/core/SkFoo.h" (root = skia_source)
    # Both roots must be on the include path.
    # SKIA_PREBUILT_DIR is e.g. .../third_party/skia/out/wasm-cpu
    # Go up two levels (.../third_party/skia/out/ then .../third_party/skia/)
    get_filename_component(_skia_out_dir "${SKIA_PREBUILT_DIR}" DIRECTORY)
    get_filename_component(_skia_src_dir "${_skia_out_dir}" DIRECTORY)
    add_library(skia STATIC IMPORTED GLOBAL)
    set_target_properties(skia PROPERTIES
        IMPORTED_LOCATION "${SKIA_PREBUILT_DIR}/libskia.a"
        INTERFACE_INCLUDE_DIRECTORIES "${_skia_src_dir};${_skia_src_dir}/include"
    )
    return()
endif()

include(fontconfig)

find_package(unofficial-skia CONFIG)
if(unofficial-skia_FOUND)
    set(SKIA_TARGET unofficial::skia::skia)
    if (HAS_FONTCONFIG AND NOT WIN32)
        set(CMAKE_LINK_GROUP_USING_no_as_needed_SUPPORTED TRUE CACHE BOOL "Link group using no-as-needed supported")
        set(CMAKE_LINK_GROUP_USING_no_as_needed "LINKER:--push-state,--no-as-needed" "LINKER:--pop-state" CACHE STRING "Link group using no-as-needed")
        set_property(TARGET unofficial::skia::skia APPEND PROPERTY INTERFACE_LINK_LIBRARIES "$<LINK_GROUP:no_as_needed,Fontconfig::Fontconfig>")
    endif()
    if (ANDROID)
        # FIXME: Submit a proper patch to vcpkg in order not to bring host's libc++ when compiling for Android
        get_target_property(link_libs unofficial::skia::skia INTERFACE_LINK_LIBRARIES)
        set(filtered_libs)
        foreach(lib ${link_libs})
            if (NOT lib MATCHES "lib/libc\\+\\+.so$")
                list(APPEND filtered_libs ${lib})
            endif()
        endforeach()
        set_property(TARGET unofficial::skia::skia PROPERTY INTERFACE_LINK_LIBRARIES ${filtered_libs})
    endif()
else()
    find_package(PkgConfig)

    # Get skia version from vcpkg.json
    file(READ ${LADYBIRD_SOURCE_DIR}/vcpkg.json VCPKG_DOT_JSON)
    string(JSON VCPKG_OVERRIDES_LENGTH LENGTH ${VCPKG_DOT_JSON} overrides)
    MATH(EXPR VCPKG_OVERRIDES_END_RANGE "${VCPKG_OVERRIDES_LENGTH}-1")
    foreach(IDX RANGE ${VCPKG_OVERRIDES_END_RANGE})
      string(JSON VCPKG_OVERRIDE_NAME GET ${VCPKG_DOT_JSON} overrides ${IDX} name)
      if(VCPKG_OVERRIDE_NAME STREQUAL "skia")
        string(JSON SKIA_REQUIRED_VERSION GET ${VCPKG_DOT_JSON} overrides ${IDX} version)
        string(REGEX MATCH "[0-9]+" SKIA_REQUIRED_VERSION ${SKIA_REQUIRED_VERSION})
      endif()
    endforeach()

    pkg_check_modules(skia skia=${SKIA_REQUIRED_VERSION} REQUIRED IMPORTED_TARGET skia)
    set(SKIA_TARGET PkgConfig::skia)
    set_property(TARGET PkgConfig::skia APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS "SKCMS_DLL")
endif()
add_library(skia ALIAS ${SKIA_TARGET})
