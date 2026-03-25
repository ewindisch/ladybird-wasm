include_guard(GLOBAL)

if(EMSCRIPTEN)
    # SDL3 is not used in the WASM 2D-canvas build; provide a stub so targets
    # that conditionally link SDL3::SDL3 can still be configured.
    # Also provide stub headers so LibWeb Gamepad sources compile (they include SDL3/SDL_gamepad.h).
    if(NOT TARGET SDL3::SDL3)
        add_library(SDL3::SDL3 INTERFACE IMPORTED GLOBAL)
        set(_sdl3_stub_headers "${CMAKE_CURRENT_LIST_DIR}/stub-headers")
        set_target_properties(SDL3::SDL3 PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${_sdl3_stub_headers}"
        )
    endif()
else()
    find_package(SDL3 CONFIG REQUIRED)
endif()
