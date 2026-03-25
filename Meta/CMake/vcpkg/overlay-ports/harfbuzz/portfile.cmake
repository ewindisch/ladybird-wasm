vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO harfbuzz/harfbuzz
    REF 10.2.0
    HEAD_REF main
    SHA512 697205a571bb3d52d83598e8511e2e21e7cd15630aac32d8deb4354e462efda6a5ce46510cf4a1c18365dffe935cf2e4f1fda65d1779f17c9bb60c503315bf5c
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DHB_BUILD_TESTS=OFF
        -DHB_BUILD_UTILS=OFF
        -DHB_BUILD_SUBSET=OFF
        -DHB_HAVE_FREETYPE=OFF
        -DHB_HAVE_ICU=OFF
        -DHB_HAVE_GRAPHITE2=OFF
        -DHB_HAVE_GLIB=OFF
        -DHB_HAVE_GOBJECT=OFF
        -DHB_HAVE_INTROSPECTION=OFF
        -DHB_HAVE_CORETEXT=OFF
        -DHB_HAVE_DIRECTWRITE=OFF
        -DBUILD_SHARED_LIBS=OFF
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(CONFIG_PATH "lib/cmake/harfbuzz")

vcpkg_fixup_pkgconfig()

# harfbuzz's cmake export records "/harfbuzz" (without the install prefix) as
# the include directory. Fix it to use the relative ${_IMPORT_PREFIX} so the
# installed config is relocatable.
set(_hb_cfg "${CURRENT_PACKAGES_DIR}/share/harfbuzz/harfbuzzConfig.cmake")
if(EXISTS "${_hb_cfg}")
    vcpkg_replace_string("${_hb_cfg}"
        "INTERFACE_INCLUDE_DIRECTORIES \"/harfbuzz\""
        "INTERFACE_INCLUDE_DIRECTORIES \"\${_IMPORT_PREFIX}/include/harfbuzz\""
    )
endif()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/COPYING")
