<!-- SPDX-License-Identifier: MPL-2.0 -->
# Hello

A box to move and resize: an application on the NBT App SDK alone. It opens
the window, reads the iDrive controller, a gamepad and the desktop keyboard,
follows its settings while it runs, reports a status line, passes NBT
Manager's health check and stops when asked. Build and forms: the
[repository's README](../README.md).

`src/main.cpp` is the order every application follows: the bridge, the
window, what can be prepared without the screen, ready, the loop, a clean
end.

## Controls

Bindings are chosen for the pad plugged in, by its family; there is nothing to
set. Everything works with the iDrive controller alone.

| Action | iDrive controller | Xbox | DualShock 4 / DualSense | Desktop |
| --- | --- | --- | --- | --- |
| Move the box | Tilt | D-pad or left stick | D-pad or left stick | Arrows |
| Bigger / smaller | Turn the knob right / left | RB / LB, or RT / LT | R1 / L1, or R2 / L2 | W / Q |
| Centre | Push | A | Cross | Return |
| Help | MENU, or BACK held | Xbox button, or View + Menu | PS button, or Share / Create + Options | Escape |
| Quit | BACK, long hold | on the iDrive controller | on the iDrive controller | Close the window |

The help names the buttons of the input in use. The knob holds its action for
100 ms per detent. Which pads are supported is up to NBT Manager
(`NBTFoundry/manager: docs/gamepad-bridge.md`).

## Settings

Changed from the phone app or NBT Manager's settings page; they apply at once.

| Setting | Choices | Default |
| --- | --- | --- |
| Colour | blue, green, red | blue |
| Speed | slow, normal, fast | normal |
| Debug log | off, on | off |

## Bridge

With the runner the package declares bridge 1.7: `show`, `controller` and
`runner` required, `status`, `live-settings` and `gamepad` optional; the
native form's package declares bridge 1.5 and no `runner`. It also carries
what the phone app shows before an install (the project, the licence, a few
lines), so it needs NBT Manager 0.3.1 or later. Hello reports the box's size
and place as its status. NBT Manager keeps its standard error as its log
(shown, shared and cleared from the phone app); the Debug log setting adds the
SDK's tracing.

## Layout

| Path | Contents |
| --- | --- |
| `src/main.cpp` | The application: the bridge, the window, the loop |
| `src/scene.*` | What it shows and its settings, free of window and input code |
| `src/controls.*` | The actions on each pad family, the iDrive controller and the keyboard |
| `tests/` | Unit test (`hello_test.cpp`); launch, package and notice tests (`test_hello.py`) |
| `release/` | The package description (`package.args`; `native.args` for the native form) |
| `docs/car-test.md` | A procedure for the car, as an application writes one before a release |
