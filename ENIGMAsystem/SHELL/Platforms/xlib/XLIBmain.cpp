/** Copyright (C) 2008-2017 Josh Ventura
*** Copyright (C) 2008-2011 IsmAvatar <cmagicj@nni.com>
*** Copyright (C) 2013 Robert B. Colton
*** Copyright (C) 2014 Seth N. Hetu
*** Copyright (C) 2015 Harijs Grinbergs
***
*** This file is a part of the ENIGMA Development Environment.
***
*** ENIGMA is free software: you can redistribute it and/or modify it under the
*** terms of the GNU General Public License as published by the Free Software
*** Foundation, version 3 of the license or any later version.
***
*** This application and its source code is distributed AS-IS, WITHOUT ANY
*** WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
*** FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
*** details.
***
*** You should have received a copy of the GNU General Public License along
*** with this code. If not, see <http://www.gnu.org/licenses/>
**/

#include "XLIBmain.h"
#include "LINUXjoystick.h"
#include "XLIBwindow.h"

#include "Platforms/General/PFmain.h"
#include "Platforms/General/PFsystem.h"
#include "Platforms/platforms_mandatory.h"

#include "Universal_System/roomsystem.h"
#include "Universal_System/var4.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrandr.h> // for display_set_size

#include <sys/types.h>  //getpid
#include <unistd.h>

using namespace enigma::x11;

namespace enigma_user {
const int os_type = os_linux;
}  // namespace enigma_user

namespace enigma {

void (*WindowResizedCallback)();
void WindowResized();

int handleEvents() {
  while (XQLength(enigma::x11::disp) || XPending(enigma::x11::disp)) {
    XEvent e;
    XNextEvent(disp, &e);
    int gk;
    unsigned char actualKey;
    switch (e.type) {
      case KeyPress: {
        gk = XLookupKeysym(&e.xkey, 0);
        if (gk == NoSymbol) return 0;

        if (!(gk & 0xFF00))
          actualKey = enigma_user::keyboard_get_map((int)enigma::keymap[gk & 0xFF]);
        else
          actualKey = enigma_user::keyboard_get_map((int)enigma::keymap[gk & 0x1FF]);
        {  // Set keyboard_lastchar. Seems to work without
          char str[1];
          int len = XLookupString(&e.xkey, str, 1, NULL, NULL);
          if (len > 0) {
            enigma_user::keyboard_lastchar = string(1, str[0]);
            if (actualKey == enigma_user::vk_backspace) {
              enigma_user::keyboard_string =
                  enigma_user::keyboard_string.substr(0, enigma_user::keyboard_string.length() - 1);
            } else {
              enigma_user::keyboard_string += enigma_user::keyboard_lastchar;
            }
          }
        }
        enigma_user::keyboard_lastkey = actualKey;
        enigma_user::keyboard_key = actualKey;
        if (enigma::last_keybdstatus[actualKey] == 1 && enigma::keybdstatus[actualKey] == 0) {
          enigma::keybdstatus[actualKey] = 1;
          return 0;
        }
        enigma::last_keybdstatus[actualKey] = enigma::keybdstatus[actualKey];
        enigma::keybdstatus[actualKey] = 1;
        return 0;
      }
      case KeyRelease: {
        enigma_user::keyboard_key = 0;
        gk = XLookupKeysym(&e.xkey, 0);
        if (gk == NoSymbol) return 0;

        if (!(gk & 0xFF00))
          actualKey = enigma_user::keyboard_get_map((int)enigma::keymap[gk & 0xFF]);
        else
          actualKey = enigma_user::keyboard_get_map((int)enigma::keymap[gk & 0x1FF]);

        enigma::last_keybdstatus[actualKey] = enigma::keybdstatus[actualKey];
        enigma::keybdstatus[actualKey] = 0;
        return 0;
      }
      case ButtonPress: {
        if (e.xbutton.button < 4)
          enigma::mousestatus[e.xbutton.button == 1 ? 0 : 4 - e.xbutton.button] = 1;
        else
          switch (e.xbutton.button) {
            case 4:
              enigma_user::mouse_vscrolls++;
              break;
            case 5:
              enigma_user::mouse_vscrolls--;
              break;
            case 6:
              enigma_user::mouse_hscrolls++;
              break;
            case 7:
              enigma_user::mouse_hscrolls--;
              break;
            default:;
          }
        return 0;
      }
      case ButtonRelease: {
        if (e.xbutton.button < 4)
          enigma::mousestatus[e.xbutton.button == 1 ? 0 : 4 - e.xbutton.button] = 0;
        else
          switch (e.xbutton.button) {
            case 4:
              enigma_user::mouse_vscrolls++;
              break;
            case 5:
              enigma_user::mouse_vscrolls--;
              break;
            case 6:
              enigma_user::mouse_hscrolls++;
              break;
            case 7:
              enigma_user::mouse_hscrolls--;
              break;
            default:;
          }
        return 0;
      }
      case ConfigureNotify: {
        enigma::windowWidth = e.xconfigure.width;
        enigma::windowHeight = e.xconfigure.height;

        if (WindowResizedCallback != NULL) {
          WindowResizedCallback();
        }
        return 0;
      }
      case FocusIn:
        input_initialize();
        init_joysticks();
        game_window_focused = true;
        pausedSteps = 0;
        return 0;
      case FocusOut:
        game_window_focused = false;
        return 0;
      case ClientMessage:
        if ((Atom)e.xclient.data.l[0] ==
            wm_delwin)  //For some reason, this line warns whether we cast to unsigned or not.
          return 1;
        //else fall through
      default:
#ifdef DEBUG_MODE
        printf("Unhandled xlib event: %d\n", e.type);
#endif
        return 0;
    }
    //Move/Resize = ConfigureNotify
    //Min = UnmapNotify
    //Restore = MapNotify
  }

  return 0;
}

void initInput() {
  initkeymap();
  init_joysticks();
}

void handleInput() {
  handle_joysticks();
  input_push();
}

}  // namespace enigma

namespace enigma_user {

int display_get_width() { return XWidthOfScreen(enigma::x11::screen); }
int display_get_height() { return XHeightOfScreen(enigma::x11::screen); }

bool display_set_size(int w, int h) {
  XRRScreenConfiguration *conf = XRRGetScreenInfo(enigma::x11::disp, enigma::x11::win);
  Rotation original_rotation;
  SizeID original_size_id = XRRConfigCurrentConfiguration(conf, &original_rotation);
  SizeID compatible_size_id = -1;

  int num_sizes;
  XRRScreenSize *xrrs;
  xrrs = XRRSizes(enigma::x11::disp, 0, &num_sizes);
  for (int i = 0; i < num_sizes; i++) {
    if (xrrs[i].width == w && xrrs[i].height == h) {
      compatible_size_id = i;
      break;
    }
  }
  if (compatible_size_id == -1)
    return false;

  return XRRSetScreenConfig(enigma::x11::disp, conf, enigma::x11::win, compatible_size_id, original_rotation, CurrentTime);
}

}  // namespace enigma_user
