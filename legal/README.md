<!-- SPDX-License-Identifier: MPL-2.0 -->
# License notices

The repository's original work is licensed as described in its root README and LICENSE. Third-party components retain their own licenses and copyright notices, collected for each example in its `legal/NOTICE.txt` (written by `make notice`, `tools/make_notice.py`). Original NBTFoundry artwork is MPL-2.0.

| Component | In | Licence | Text |
| --- | --- | --- | --- |
| The examples (`src/`, `tests/`), tools and build files | both | MPL-2.0 | [LICENSE](../LICENSE) |
| LVGL (`third_party/lvgl`) | `lvgl` | MIT | [MIT](../LICENSES/MIT.txt); NOTICE.txt |
| Montserrat, LVGL's built-in text fonts | `lvgl` | OFL-1.1 | [OFL-1.1](../LICENSES/OFL-1.1.txt); NOTICE.txt |
| Font Awesome Free, the symbols in LVGL's built-in fonts | `lvgl` | OFL-1.1 (fonts) | NOTICE.txt |
| NBT App SDK and its third-party parts | both | MPL-2.0; MIT, public domain | [MPL-2.0](../LICENSES/MPL-2.0.txt); NOTICE.txt |
| picolibc (the NBT toolchain's C library, linked into a module or a guest) | both | BSD-style | NOTICE.txt |
| compiler-rt, libc++, libc++abi, libunwind (linked in) | both | Apache-2.0 WITH LLVM-exception | no attribution owed in binaries |

The native form links the car's own C library when it starts instead of picolibc; the notices cover every form.

The notices are a snapshot of the current dependencies. Update them when changing a bundled dependency, the SDK commit or the toolchain.

See [DISCLAIMER.md](../DISCLAIMER.md) for head-unit risks and warranty and liability terms.
