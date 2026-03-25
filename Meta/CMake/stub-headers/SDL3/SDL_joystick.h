/*
 * Stub SDL3/SDL_joystick.h for EMSCRIPTEN/WASM builds.
 */

#pragma once

#include <stdint.h>
#include "SDL_gamepad.h"

typedef struct SDL_Joystick SDL_Joystick;

typedef enum {
    SDL_JOYSTICK_TYPE_UNKNOWN = 0,
    SDL_JOYSTICK_TYPE_GAMEPAD,
    SDL_JOYSTICK_TYPE_WHEEL,
    SDL_JOYSTICK_TYPE_ARCADE_STICK,
    SDL_JOYSTICK_TYPE_FLIGHT_STICK,
    SDL_JOYSTICK_TYPE_DANCE_PAD,
    SDL_JOYSTICK_TYPE_GUITAR,
    SDL_JOYSTICK_TYPE_DRUM_KIT,
    SDL_JOYSTICK_TYPE_ARCADE_PAD,
    SDL_JOYSTICK_TYPE_THROTTLE,
    SDL_JOYSTICK_TYPE_COUNT
} SDL_JoystickType;

typedef struct SDL_VirtualJoystickDesc {
    uint32_t version;
    uint16_t type;
    uint16_t padding;
    uint16_t vendor_id;
    uint16_t product_id;
    uint16_t naxes;
    uint16_t nbuttons;
    uint16_t nballs;
    uint16_t nhats;
    uint16_t ntouchpads;
    uint16_t nsensors;
    uint16_t padding2[2];
    uint32_t button_mask;
    uint32_t axis_mask;
    const char* name;
    const void* touchpads;
    const void* sensors;
    void* userdata;
    void (*Update)(void* userdata);
    void (*SetPlayerIndex)(void* userdata, int player_index);
    bool (*Rumble)(void* userdata, uint16_t low_frequency_rumble, uint16_t high_frequency_rumble);
    bool (*RumbleTriggers)(void* userdata, uint16_t left_rumble, uint16_t right_rumble);
    int (*SetLED)(void* userdata, uint8_t red, uint8_t green, uint8_t blue);
    int (*SendEffect)(void* userdata, const void* data, int size);
} SDL_VirtualJoystickDesc;

/* SDL_INIT_INTERFACE macro — no-op for stub (caller does {} init already) */
#define SDL_INIT_INTERFACE(iface) ((void)(iface))

static inline SDL_JoystickID SDL_AttachVirtualJoystick(const SDL_VirtualJoystickDesc*) { return 0; }
static inline int SDL_DetachVirtualJoystick(SDL_JoystickID) { return 0; }
static inline SDL_Joystick* SDL_OpenJoystick(SDL_JoystickID) { return 0; }
static inline void SDL_CloseJoystick(SDL_Joystick*) {}
static inline int SDL_SetJoystickVirtualAxis(SDL_Joystick*, int, int16_t) { return 0; }
static inline int SDL_SetJoystickVirtualButton(SDL_Joystick*, int, int8_t) { return 0; }
