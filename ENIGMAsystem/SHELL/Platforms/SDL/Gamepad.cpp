#include "Gamepad.h"

#include <string>

namespace enigma {

static std::vector<Gamepad> gamepads;

void Gamepad::clear() {
  for (unsigned i = 0; i < enigma_user::gp_padr; ++i) {
    state.lastButtonStatus[i] = false;
    state.buttonStatus[i] = false;
  }
}

void Gamepad::push() {
  for (unsigned i = 0; i < enigma_user::gp_padr; ++i) {
    state.lastButtonStatus[i] = state.buttonStatus[i];
  }
}

void initGamepads() {
  SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC);
  for (int j = 0; j < SDL_NumJoysticks(); ++j) addGamepad(j);
}

void cleanupGamepads() {
  for (int i = 0; i < SDL_NumJoysticks(); ++i) removeGamepad(i);
}

void pushGamepads() {
  for (int i = 0; i < SDL_NumJoysticks(); ++i) gamepads[i].push();
}

void setGamepadButton(int gamepad, int btn, bool pressed) { gamepads[gamepad].state.buttonStatus[btn] = pressed; }

void addGamepad(unsigned i) {
  if (!SDL_IsGameController(i)) {
    fprintf(stderr, "Could not open gamepad %i: %s\n", i, SDL_GetError());
    return;
  }

  Gamepad g;
  g.clear();
  g.controller = SDL_GameControllerOpen(i);

  if (g.controller == nullptr) {
    fprintf(stderr, "Could not open gamepad %i: %s\n", i, SDL_GetError());
    return;
  }

  printf("Connected gamepad %u %s\n", i, SDL_GameControllerName(g.controller));

  SDL_Joystick* j = SDL_GameControllerGetJoystick(g.controller);

  if (SDL_JoystickIsHaptic(j)) {
    g.haptic = SDL_HapticOpenFromJoystick(j);

    if (SDL_HapticRumbleSupported(g.haptic) && SDL_HapticRumbleInit(g.haptic))
      printf("Rumble Supported: yes\n");
    else
      printf("Rumble Supported: no\n");
  }

  if (i < gamepads.size())
    gamepads[i] = g;
  else
    gamepads.push_back(g);
}

void removeGamepad(unsigned i) {
  printf("Disconnected gamepad %u %s\n", i, SDL_GameControllerName(gamepads[i].controller));
  if (gamepads[i].haptic != nullptr) SDL_HapticClose(gamepads[i].haptic);
  SDL_GameControllerClose(gamepads[i].controller);
}

}  // namespace enigma

using enigma::gamepads;

namespace enigma_user {
bool gamepad_is_supported() {
  //SDL supports gamepads
  return true;
}

int gamepad_get_device_count() { return gamepads.size(); }

bool gamepad_is_connected(int id) { return id >= 0 && id < SDL_NumJoysticks() && SDL_IsGameController(id); }

std::string gamepad_get_description(int id) { return SDL_GameControllerNameForIndex(id); }

int gamepad_axis_count(int id) {
#ifdef DEBUG_MODE
  if (id < 0 || id >= static_cast<int>(gamepads.size())) return 0;
#endif

  SDL_GameController* c = gamepads[id].controller;

#ifdef DEBUG_MODE
  if (c == nullptr) return 0;
#endif

  SDL_Joystick* j = SDL_GameControllerGetJoystick(c);
  return SDL_JoystickNumAxes(j);
}

float gamepad_axis_value(int id, int axis) {
#ifdef DEBUG_MODE
  if (id < 0 || id >= static_cast<int>(gamepads.size())) return 0;
#endif

  SDL_GameController* c = gamepads[id].controller;

#ifdef DEBUG_MODE
  if (c == nullptr) return 0;
#endif

  SDL_GameControllerAxis SDLaxis;

  switch (axis) {
    case gp_axislh:
      SDLaxis = SDL_CONTROLLER_AXIS_LEFTX;
      break;
    case gp_axislv:
      SDLaxis = SDL_CONTROLLER_AXIS_LEFTY;
      break;
    case gp_axisrh:
      SDLaxis = SDL_CONTROLLER_AXIS_RIGHTX;
      break;
    case gp_axisrv:
      SDLaxis = SDL_CONTROLLER_AXIS_RIGHTY;
      break;
    case gp_shoulderlb:
      SDLaxis = SDL_CONTROLLER_AXIS_TRIGGERLEFT;
      break;
    case gp_shoulderrb:
      SDLaxis = SDL_CONTROLLER_AXIS_TRIGGERRIGHT;
      break;

    default:
      return 0;
  }

  //The state is a value ranging from -32768 to 32767.
  return SDL_GameControllerGetAxis(c, SDLaxis) / 32768;
}

bool gamepad_button_check(int id, int btn) {
#ifdef DEBUG_MODE
  if (id < 0 || id >= static_cast<int>(gamepads.size())) return false;
#endif

  return gamepads[id].state.buttonStatus[btn];
}

bool gamepad_button_check_pressed(int id, int btn) {
#ifdef DEBUG_MODE
  if (id < 0 || id >= static_cast<int>(gamepads.size()) || btn > gp_padr) return false;
#endif

  auto g = gamepads[id].state;

  if (btn == gp_any || btn == gp_none) {
    for (int i = 0; i < gp_axisrv; i++)
      if (g.buttonStatus[i] && !g.lastButtonStatus[i]) return (btn == gp_any);

    return !(btn == gp_any);
  }

  return g.buttonStatus[btn] && !g.lastButtonStatus[btn];
}

bool gamepad_button_check_released(int id, int btn) {
#ifdef DEBUG_MODE
  if (id < 0 || id >= static_cast<int>(gamepads.size()) || btn > gp_padr) return false;
#endif

  auto g = gamepads[id].state;

  if (btn == gp_any || btn == gp_none) {
    for (int i = 0; i < gp_axisrv; i++)
      if (!g.buttonStatus[i] && g.lastButtonStatus[i]) return (btn == gp_any);

    return !(btn == gp_any);
  }

  return !g.buttonStatus[btn] && g.lastButtonStatus[btn];
}

int gamepad_button_count(int id) {
#ifdef DEBUG_MODE
  if (id < 0 || id >= static_cast<int>(gamepads.size())) return 0;
#endif

  SDL_GameController* c = gamepads[id].controller;

#ifdef DEBUG_MODE
  if (c == nullptr) return 0;
#endif

  SDL_Joystick* j = SDL_GameControllerGetJoystick(c);
  return SDL_JoystickNumButtons(j);
}

float gamepad_button_value(int id, int btn) {
  //analog buttons are axes
  return gamepad_axis_value(id, btn);
}

void gamepad_set_vibration(int id, double left, double right) {
#ifdef DEBUG_MODE
  if (id < 0 || id >= static_cast<int>(gamepads.size())) return;
#endif

  if (gamepads[id].haptic != nullptr) {
    if (left == 0)
      SDL_HapticRumbleStop(gamepads[id].haptic);
    else
      SDL_HapticRumblePlay(gamepads[id].haptic, left, SDL_HAPTIC_INFINITY);
  }
}

float gamepad_get_button_threshold(int id) {
#ifdef DEBUG_MODE
  if (id < 0 || id >= static_cast<int>(gamepads.size())) return 0;
#endif

  return gamepads[id].thresh;
}

void gamepad_set_button_threshold(int id, double threshold) {
#ifdef DEBUG_MODE
  if (id < 0 || id >= static_cast<int>(gamepads.size())) return;
#endif

  gamepads[id].thresh = threshold;
}

float gamepad_get_axis_deadzone(int id, double deadzone) {
#ifdef DEBUG_MODE
  if (id < 0 || id >= static_cast<int>(gamepads.size())) return 0;
#endif

  return gamepads[id].deadzone;
}

void gamepad_set_axis_deadzone(int id, double deadzone) {
#ifdef DEBUG_MODE
  if (id < 0 || id >= static_cast<int>(gamepads.size())) return;
#endif

  gamepads[id].deadzone = deadzone;
}

//gamepad_set_colour is playstation 4 only so why bother?

}  // namespace enigma_user
