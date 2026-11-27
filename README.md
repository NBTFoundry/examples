<!-- SPDX-License-Identifier: MPL-2.0 -->
<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="docs/assets/logo-dark.svg">
    <img src="docs/assets/logo-light.svg" alt="NBTFoundry" height="44">
  </picture>
</p>

<h1 align="center">NBT App SDK examples</h1>

> **Warning: installing or modifying this software can brick your head unit. Recovery may require professional service or replacement, at your expense. Provided AS IS; use at your own risk, subject to applicable law.** [Read the risk and liability disclaimer](DISCLAIMER.md).

<p align="center">
  <b>Complete small applications for the iDrive screen,</b><br>
  to read, run on the desktop and copy from.
</p>

Each example is a whole application on the [NBT App SDK](https://github.com/NBTFoundry/sdk),
as NBT Manager installs and runs one: its sources, its build for the desktop
and for the head unit, its tests and its package description.

<table>
<tr>
<td width="50%"><a href="hello/"><img src="hello/docs/hello.webp" alt="Hello, animated: a box moved and resized with the iDrive controller, its colour changed from the settings, the help"></a><br>
<a href="hello/"><b><code>hello/</code></b></a>: the SDK alone. The window, the iDrive controller, a USB gamepad by pad family and the desktop keyboard, settings that apply while it runs, a status line, the health check.</td>
<td width="50%"><a href="lvgl/"><img src="lvgl/docs/hello-lvgl.webp" alt="Hello LVGL, animated: a dial changed with the knob, a button pressed, the light theme"></a><br>
<a href="lvgl/"><b><code>lvgl/</code></b></a>: a screen made of <a href="https://lvgl.io">LVGL</a> widgets, with the iDrive controller as LVGL's encoder: turn to choose, push to change or press.</td>
</tr>
</table>

To start an application of your own, use the
[template](https://github.com/NBTFoundry/template): the bare minimum, ready to
build. To port an existing program (a game, an emulator, a tool), follow the
SDK's [porting guide](https://github.com/NBTFoundry/sdk/blob/main/docs/porting.md).

**Supported head units:** BMW NBT Evo (iDrive 6). Support for the earlier NBT
may come later.

Part of [NBTFoundry](https://github.com/NBTFoundry):
[manager](https://github.com/NBTFoundry/manager) ·
[sdk](https://github.com/NBTFoundry/sdk) ·
[template](https://github.com/NBTFoundry/template) ·
[mobile-app](https://github.com/NBTFoundry/mobile-app) ·
[installer](https://github.com/NBTFoundry/installer) ·
[toolchains](https://github.com/NBTFoundry/toolchains)

Not affiliated with or endorsed by BMW AG. BMW, iDrive and NBT are trademarks
of BMW AG.

## Build

Coding agents: [AGENTS.md](AGENTS.md).

Needs the NBT App SDK next to this repository (`../sdk`, or `NBT_SDK_DIR`),
gcc, Python 3, and the X11, EGL and GLESv2 development files; Xvfb for the
tests. `git submodule update --init` fetches LVGL for the `lvgl` example. In
an example's folder:

```sh
make -j"$(nproc)" host test test-asan check-static-init   # host binary and tests
./build/nbt-hello-host                                    # on the desktop (X11); lvgl: nbt-hello-lvgl-host
```

The desktop build runs on its own; the tests start it as NBT Manager does: a
health check without a window, then launches under Xvfb driven through the
controller and gamepad sockets, the status line and a settings file. The
package test needs NBT Manager's checkout (`../manager`, or `NBT_MGR_DIR`)
with `nbt-verify` built (`make -C ../manager host`); it is skipped without.

`make anim` records the example's animation for its README: the desktop build
under Xvfb, played through the controller's socket and a settings file
(`tools/capture.py`; needs ffmpeg with libwebp).

## The head unit: with and without the runner

Every example builds with the NBT toolchain (`make -C ../sdk toolchain` once)
in the three forms an application can take. `FORM` names the one `vm-test`
and `package` take; an application of your own keeps one.

| Form | `make` | For | Package |
| --- | --- | --- | --- |
| Module, with the runner | `module` | code that is not GPL or LGPL; NBT Manager's `nbt-runner` loads it into its own process | bridge 1.7, `runner` |
| Guest, with the runner | `guest` | GPL or LGPL code, even one file; `nbt-runner` starts it as its child process | bridge 1.7, `runner` |
| Native, without the runner | `native native-check` | a program that needs the car's own libraries, other processes or sockets; NBT Manager starts it itself | bridge 1.5, no `runner` (`release/native.args`) |

```sh
make -j"$(nproc)" module                              # build/nbt-hello.so
make -j"$(nproc)" guest                               # build/nbt-hello.guest
make -j"$(nproc)" native native-check                 # build/nbt-hello-native
make vm-test                                          # the module in the SDK's VM (FORM=guest, FORM=native)
make APP_KEY=/path/to/apps-signing-key.pem package    # build/hello-<version>.nbt (FORM=native: without the runner)
```

`native-check` verifies that the native program imports only what the car has
(`CAR_SYSROOT` names a sysroot of the release the car runs; skipped without);
a module or a guest is checked when it is linked. `vm-test` runs the health
check, and with the runner a launch, in the SDK's virtual machine (its
`nbt-runner` build needs what the SDK's `docs/native-apps.md` lists).
`package` signs the payload with NBT Manager's packer
(`../manager/tools/package.py`), described by the example's
`release/package.args`, with `release/native.args` after it for the native
form. `make notice` writes the example's `legal/NOTICE.txt`; `make test`
fails when it lags.

Our code is C++11 without exceptions or RTTI, and nothing of it runs before
`main` (`check-static-init`). `release/sdk.ref` is the SDK commit CI builds
every example against (`.github/workflows/ci.yml`: the host tests, the same
under ASan/UBSan, and the three head-unit forms).

## Releases

Each example is released on its own, from a tag `hello-vX.Y.Z` or
`hello-lvgl-vX.Y.Z`, with its notes written first
(`hello/release/notes/X.Y.Z.md`, `lvgl/release/notes/X.Y.Z.md`: the build
fails without them), two ways:

- **Here, with a developer key:** signed with this repository's own key, as a
  third-party application would be. Add `release/developer-public.pem` on the
  car first: in the NBT Manager app, Car › Manage › Developer keys (fingerprint
  `57f3 7c30 e437 198f`).
- **In [community-apps](https://github.com/NBTFoundry/community-apps):** built
  from these sources and signed with NBTFoundry's apps key, like any submitted
  application; it installs with no key to add.

A car keeps an example with the key that installed it first: remove it before
switching to the other.

## Layout

| Path | Contents |
| --- | --- |
| `hello/`, `lvgl/` | The examples: each with its `Makefile`, `src/`, `tests/`, `release/` (package description) and `legal/NOTICE.txt` |
| `third_party/lvgl/` | LVGL 9.6, unmodified (submodule) |
| `tools/make_notice.py`, `tools/capture.py` | Write an example's notices; record its animation |
| `release/sdk.ref` | The SDK commit to build against |
| `legal/`, `LICENSES/` | Notices and licence texts |

## License

Copyright (c) 2026 Fabien Poussin. Original code, tests, build scripts and
documentation are licensed under **MPL-2.0**; see [LICENSE](LICENSE).

LVGL keeps its own licence ([LICENSES/MIT.txt](LICENSES/MIT.txt)); its
built-in Montserrat fonts are under the SIL Open Font License
([LICENSES/OFL-1.1.txt](LICENSES/OFL-1.1.txt)). Original NBTFoundry logos and
artwork are MPL-2.0; copyright permission for artwork does not grant trademark
rights or imply endorsement by BMW AG.

See [legal/README.md](legal/README.md) for third-party license notices.
