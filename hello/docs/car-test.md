<!-- SPDX-License-Identifier: MPL-2.0 -->
# Car test: Hello before a release

Parked, ignition on. About 15 minutes, all from the phone: no adapter, no
computer.

Run it on a candidate before tagging a release, and whenever the car-side
behaviour changed. Hello is a **module**: NBT Manager's `nbt-runner` loads it
into its own process and draws for it.

## What you need

- NBT Manager 0.3.1 or later on the car, and its phone app, paired.
- From the kit folder: the candidate `hello-<version>.nbt`, a second one with
  a higher sequence for part C, and their `SHA256SUMS` file. Copy them to the
  phone.
- A USB gamepad (Xbox or DualShock 4 / DualSense), for B.4.

## A. Install

1. In the phone app's library, add the candidate and open it. Under the file
   name: **Project: NBTFoundry examples**, **Licence: MPL-2.0**, then the
   text, ending with the repository's address.
2. Send it to the car (an install, or an update of **hello**) and approve on
   the car. The health check (about 10 s) must pass.
3. Hold the push on **hello**: 3 settings (Colour, Speed, Debug log).

## B. Use

1. Select **hello**: a dark screen, "Hello" at the top left, a blue box in
   the centre.
2. iDrive controller: tilt moves the box, turning the knob resizes it, push
   centres it. The phone shows its status ("Size 96 at 640,240").
3. MENU, or BACK held: the help, naming the iDrive controller. Again: gone.
4. Plug the gamepad in: D-pad or left stick moves, the shoulder buttons
   resize, the bottom button centres; the help names the pad and its buttons.
   Unplug it: the iDrive controller works again.
5. While it runs, change **Colour** from the phone: it applies at once.
6. BACK, long hold (1.4 s): it closes and the screen returns.

## C. Stop, start again, update

1. Start **hello**, then hold the push on it → **Stop**: the screen returns.
   Start it again.
2. While it runs, send the second package from the phone and approve. The
   update stops it, runs the health check and installs.

## Logs

The phone app: **hello** → **Show log** → **Share** (more with **Debug log:
on**). A crash shows as `Process … terminated SIG… ip=…` and the application
ending by a signal: copy both lines.

## Back

Uninstall **hello** from the car's page, or install the last release from
GitHub (a test package with a lower sequence is refused).

## Report

| Step | Result | Notes |
| --- | --- | --- |
| A.1 information shown | | |
| A.2 install or update + health check | | |
| A.3 3 settings | | |
| B.1 starts, box shown | | |
| B.2 iDrive controls, status on the phone | | |
| B.3 help | | |
| B.4 gamepad named, controls, unplug | | |
| B.5 colour from the phone, live | | |
| B.6 long BACK hold closes | | |
| C.1 stop, start again | | |
| C.2 update while running | | |

Add the car's software version and the pad model.
