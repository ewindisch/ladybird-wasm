# gperf overlay port — use pre-built system gperf binary.
# The upstream vcpkg portfile runs autoreconf, requiring autoconf-archive.
# This overlay copies the system/pre-built gperf into the vcpkg tools dir.

set(VCPKG_POLICY_EMPTY_INCLUDE_FOLDER enabled)
set(VCPKG_POLICY_EMPTY_PACKAGE enabled)
set(VCPKG_BUILD_TYPE release)

# Find gperf in PATH (our /tmp/gperf-install/bin or system /usr/bin)
find_program(GPERF_EXECUTABLE gperf REQUIRED)
message(STATUS "gperf overlay: using system binary at ${GPERF_EXECUTABLE}")

set(TOOLS_DIR "${CURRENT_PACKAGES_DIR}/tools/${PORT}")
file(MAKE_DIRECTORY "${TOOLS_DIR}")
file(COPY "${GPERF_EXECUTABLE}" DESTINATION "${TOOLS_DIR}")

# Provide empty copyright file
file(WRITE "${CURRENT_PACKAGES_DIR}/share/${PORT}/copyright" "GNU gperf - GPL-3.0-or-later\n")
