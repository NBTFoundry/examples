// SPDX-License-Identifier: MPL-2.0
#include "controls.hpp"

#include <stddef.h>

#include "nbt/app/video.hpp"
#include "scene.hpp"

namespace hello {

namespace {

using nbt::frontend::Binding;
using nbt::frontend::ControllerProfile;
using nbt::frontend::KeyboardProfile;
using nbt::frontend::PadProfile;
using nbt::frontend::Profiles;

/* NBT_GAMEPAD_* name buttons by position, so one layout serves every family:
 * the bottom face button is Xbox A, DualShock and DualSense Cross. */
const Binding kPadButtons[] = {{NBT_GAMEPAD_UP, kActionUp},       {NBT_GAMEPAD_DOWN, kActionDown},
                               {NBT_GAMEPAD_LEFT, kActionLeft},   {NBT_GAMEPAD_RIGHT, kActionRight},
                               {NBT_GAMEPAD_RB, kActionBigger},   {NBT_GAMEPAD_LB, kActionSmaller},
                               {NBT_GAMEPAD_SOUTH, kActionCentre}};

/* The left stick moves too, the triggers resize too. View and Menu held
 * together (Share/Create and Options), or the Xbox or PS button, show the
 * help. */
const PadProfile kPad = {kPadButtons,
                         sizeof(kPadButtons) / sizeof(kPadButtons[0]),
                         kActionUp,
                         kActionDown,
                         kActionLeft,
                         kActionRight,
                         kActionSmaller,
                         kActionBigger,
                         NBT_GAMEPAD_VIEW | NBT_GAMEPAD_MENU,
                         NBT_GAMEPAD_GUIDE};

/* The iDrive controller: tilts move, the knob resizes (each detent holds its
 * action for 100 ms), push centres. MENU or a BACK hold shows the help, a
 * long BACK hold quits; a BACK tap does nothing here. */
const Binding kControllerKeys[] = {{NBT_CONTROLLER_TILT_UP, kActionUp},
                                   {NBT_CONTROLLER_TILT_DOWN, kActionDown},
                                   {NBT_CONTROLLER_TILT_LEFT, kActionLeft},
                                   {NBT_CONTROLLER_TILT_RIGHT, kActionRight},
                                   {NBT_CONTROLLER_PUSH, kActionCentre}};
const ControllerProfile kController = {kControllerKeys,
                                       sizeof(kControllerKeys) / sizeof(kControllerKeys[0]),
                                       kActionSmaller,
                                       kActionBigger,
                                       100,
                                       nbt::frontend::kNoAction,
                                       NBT_CONTROLLER_MENU};

/* The desktop: the arrows, W and Q, Return; Escape shows the help. */
const Binding kKeys[] = {{nbt::app::kHostKeyUp, kActionUp},
                         {nbt::app::kHostKeyDown, kActionDown},
                         {nbt::app::kHostKeyLeft, kActionLeft},
                         {nbt::app::kHostKeyRight, kActionRight},
                         {'w', kActionBigger},
                         {'q', kActionSmaller},
                         {nbt::app::kHostKeyReturn, kActionCentre}};
const KeyboardProfile kKeyboard = {kKeys, sizeof(kKeys) / sizeof(kKeys[0]),
                                   nbt::app::kHostKeyEscape};

const Profiles kProfiles = {{&kPad, &kPad, &kPad, &kPad}, &kController, &kKeyboard};

}  // namespace

const nbt::frontend::Profiles& controls() {
  return kProfiles;
}

}  // namespace hello
