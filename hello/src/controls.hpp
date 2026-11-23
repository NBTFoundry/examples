// SPDX-License-Identifier: MPL-2.0
/* The application's actions (scene.hpp) on the iDrive controller, a USB
 * gamepad of any family and the desktop keyboard (README, Controls). */
#ifndef NBT_HELLO_CONTROLS_HPP
#define NBT_HELLO_CONTROLS_HPP

#include "nbt/frontend/pad_profiles.hpp"

namespace hello {

const nbt::frontend::Profiles& controls();

}  // namespace hello

#endif /* NBT_HELLO_CONTROLS_HPP */
