/*
 * Stub SDL3/SDL_gamepad.h for EMSCRIPTEN/WASM builds.
 * Provides just enough types and declarations to compile LibWeb Gamepad sources.
 * All functions are no-ops; gamepad support is not available in WASM 2D-canvas builds.
 */

#pragma once

#include <stdint.h>

#ifndef SDLCALL
#    define SDLCALL
#endif

typedef uint32_t Uint32;
typedef Uint32 SDL_JoystickID;
typedef Uint32 SDL_PropertiesID;

typedef struct SDL_Gamepad SDL_Gamepad;
typedef struct SDL_Joystick SDL_Joystick;

typedef enum {
    SDL_GAMEPAD_BUTTON_SOUTH = 0,
    SDL_GAMEPAD_BUTTON_EAST,
    SDL_GAMEPAD_BUTTON_WEST,
    SDL_GAMEPAD_BUTTON_NORTH,
    SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,
    SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
    SDL_GAMEPAD_BUTTON_BACK,
    SDL_GAMEPAD_BUTTON_START,
    SDL_GAMEPAD_BUTTON_LEFT_STICK,
    SDL_GAMEPAD_BUTTON_RIGHT_STICK,
    SDL_GAMEPAD_BUTTON_DPAD_UP,
    SDL_GAMEPAD_BUTTON_DPAD_DOWN,
    SDL_GAMEPAD_BUTTON_DPAD_LEFT,
    SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
    SDL_GAMEPAD_BUTTON_MISC1,
    SDL_GAMEPAD_BUTTON_MISC = SDL_GAMEPAD_BUTTON_MISC1,
    SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1,
    SDL_GAMEPAD_BUTTON_LEFT_PADDLE1,
    SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2,
    SDL_GAMEPAD_BUTTON_LEFT_PADDLE2,
    SDL_GAMEPAD_BUTTON_RIGHT_PADDLE = SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1,
    SDL_GAMEPAD_BUTTON_LEFT_PADDLE = SDL_GAMEPAD_BUTTON_LEFT_PADDLE1,
    SDL_GAMEPAD_BUTTON_MISC2,
    SDL_GAMEPAD_BUTTON_MISC3,
    SDL_GAMEPAD_BUTTON_MISC4,
    SDL_GAMEPAD_BUTTON_MISC5,
    SDL_GAMEPAD_BUTTON_MISC6,
    SDL_GAMEPAD_BUTTON_GUIDE,
    SDL_GAMEPAD_BUTTON_TOUCHPAD,
    SDL_GAMEPAD_BUTTON_MAX
} SDL_GamepadButton;

typedef enum {
    SDL_GAMEPAD_AXIS_LEFTX = 0,
    SDL_GAMEPAD_AXIS_LEFTY,
    SDL_GAMEPAD_AXIS_RIGHTX,
    SDL_GAMEPAD_AXIS_RIGHTY,
    SDL_GAMEPAD_AXIS_LEFT_TRIGGER,
    SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,
    SDL_GAMEPAD_AXIS_MAX
} SDL_GamepadAxis;

#define SDL_JOYSTICK_AXIS_MAX  32767
#define SDL_JOYSTICK_AXIS_MIN  (-32768)

#define SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN          "SDL.gamepad.cap.rumble"
#define SDL_PROP_GAMEPAD_CAP_TRIGGER_RUMBLE_BOOLEAN  "SDL.gamepad.cap.trigger_rumble"

static inline SDL_Gamepad* SDL_OpenGamepad(SDL_JoystickID) { return 0; }
static inline void SDL_CloseGamepad(SDL_Gamepad*) {}
static inline const char* SDL_GetGamepadNameForID(SDL_JoystickID) { return 0; }
static inline SDL_JoystickID* SDL_GetGamepads(int* count) { if (count) *count = 0; return 0; }
static inline bool SDL_GamepadHasAxis(SDL_Gamepad*, SDL_GamepadAxis) { return false; }
static inline bool SDL_GamepadHasButton(SDL_Gamepad*, SDL_GamepadButton) { return false; }
static inline int16_t SDL_GetGamepadAxis(SDL_Gamepad*, SDL_GamepadAxis) { return 0; }
static inline bool SDL_GetGamepadButton(SDL_Gamepad*, SDL_GamepadButton) { return false; }
static inline int16_t SDL_GetJoystickAxis(SDL_Joystick*, int) { return 0; }
static inline bool SDL_IsJoystickVirtual(SDL_JoystickID) { return false; }
static inline bool SDL_RumbleGamepad(SDL_Gamepad*, uint16_t, uint16_t, uint32_t) { return false; }
static inline bool SDL_RumbleGamepadTriggers(SDL_Gamepad*, uint16_t, uint16_t, uint32_t) { return false; }
static inline SDL_PropertiesID SDL_GetGamepadProperties(SDL_Gamepad*) { return 0; }
static inline bool SDL_GetBooleanProperty(SDL_PropertiesID, const char*, bool def) { return def; }
static inline void SDL_UpdateJoysticks(void) {}
static inline void SDL_free(void*) {}
static inline const char* SDL_GetError(void) { return 0; }
