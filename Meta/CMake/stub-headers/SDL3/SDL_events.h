/*
 * Stub SDL3/SDL_events.h for EMSCRIPTEN/WASM builds.
 */

#pragma once

#include <stdint.h>
#include "SDL_joystick.h"

typedef enum {
    SDL_EVENT_GAMEPAD_ADDED           = 0x650,
    SDL_EVENT_GAMEPAD_REMOVED         = 0x651,
    SDL_EVENT_GAMEPAD_UPDATE_COMPLETE = 0x660,
} SDL_EventType;

typedef struct {
    uint32_t type;
    uint32_t reserved;
    uint64_t timestamp;
    SDL_JoystickID which;
} SDL_GamepadDeviceEvent;

typedef union SDL_Event {
    uint32_t type;
    SDL_GamepadDeviceEvent gdevice;
    uint8_t padding[128];
} SDL_Event;

static inline int SDL_PollEvent(SDL_Event*) { return 0; }
