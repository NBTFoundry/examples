#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Records an example for its README: the host build under Xvfb, played
through the bridge as NBT Manager would feed it (the iDrive controller's
socket, a settings file changed while it runs), captured as a looping
animated WebP.

    capture.py <hello|lvgl> <host binary> <out.webp>

`make anim` in an example runs it. Each demo below is a list of (seconds,
action, value): "keys" (the NBT_CONTROLLER_* held), "turn" (knob detents) or
"settings" (the values, applied by the application within a second). A demo
ends as it starts, so the animation loops. Needs Xvfb and ffmpeg with x11grab
and libwebp.
"""
import os
from pathlib import Path
import select
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import time

PUSH, MENU = 1 << 0, 1 << 1
UP, DOWN, LEFT, RIGHT = 1 << 8, 1 << 9, 1 << 10, 1 << 11
WIDTH, HEIGHT, FPS = 1280, 480, 15


def press(at, key, held=0.12):
    return [(at, "keys", key), (at + held, "keys", 0)]


def turns(start, detents, every=0.1):
    step = 1 if detents > 0 else -1
    return [(start + i * every, "turn", step) for i in range(abs(detents))]


DEMOS = {
    # The box moves, grows, takes another colour; the help; back to the start.
    "hello": {
        "settings": {"colour": "blue", "speed": "normal"},
        "before": [],
        "seconds": 10.0,
        "steps": [
            (0.5, "keys", RIGHT), (1.5, "keys", 0),
            (1.6, "keys", UP), (2.0, "keys", 0),
            *turns(2.3, 4),
            (3.0, "keys", LEFT | DOWN), (4.2, "keys", 0),
            (4.3, "settings", {"colour": "green", "speed": "normal"}),
            *press(5.6, MENU), *press(7.2, MENU),
            (7.3, "settings", {"colour": "blue", "speed": "normal"}),
            *turns(7.6, -4),
            *press(8.6, PUSH),
        ],
    },
    # The dial changed with the knob, the button pressed, the light theme.
    "lvgl": {
        "settings": {"theme": "dark"},
        "before": [*turns(0.2, 1), *turns(0.6, -1)],  # the focus shown, on the dial
        "seconds": 11.0,
        "steps": [
            *press(0.5, PUSH),
            *turns(1.0, 32, 0.05), *turns(3.0, -14, 0.05),
            *press(4.2, PUSH),
            *turns(4.8, 1),
            *press(5.6, PUSH),
            (6.2, "settings", {"theme": "light"}),
            (8.2, "settings", {"theme": "dark"}),
            *turns(9.8, -1),
        ],
    },
}


class Bridge:
    """The controller's socket and the settings file, as the manager's."""

    def __init__(self, folder):
        self.app_end, self.end = socket.socketpair(socket.AF_UNIX, socket.SOCK_DGRAM)
        self.settings = folder / "settings"
        self.sequence = self.keys = self.detents = 0

    def send(self):
        self.sequence += 1
        self.end.send(struct.pack("<BBBBIIi", 0x43, 1, 1, 0, self.sequence, self.keys, self.detents))

    def write_settings(self, values):
        text = "NBTVALUES1\n" + "".join(f"{k}={v}\n" for k, v in values.items())
        fd = os.open(self.settings, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
        with open(fd, "w") as f:
            f.write(text)

    def play(self, steps):
        start = time.monotonic()
        for at, action, value in sorted(steps, key=lambda step: step[0]):
            time.sleep(max(0.0, start + at - time.monotonic()))
            if action == "settings":
                self.write_settings(value)
                continue
            if action == "keys":
                self.keys = value
            else:
                self.detents += value
            self.send()
        return start


def xvfb():
    r, w = os.pipe()
    proc = subprocess.Popen(
        ["Xvfb", "-displayfd", str(w), "-screen", "0", f"{WIDTH}x{HEIGHT}x24", "-nolisten", "tcp"],
        pass_fds=(w,), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    os.close(w)
    number = b""
    while not number.endswith(b"\n"):
        ready, _, _ = select.select([r], [], [], 10)
        chunk = os.read(r, 1) if ready else b""
        if not chunk:
            sys.exit("Xvfb did not start")
        number += chunk
    os.close(r)
    return proc, ":" + number.decode().strip()


def main():
    if len(sys.argv) != 4 or sys.argv[1] not in DEMOS:
        sys.exit(__doc__)
    demo, binary, out = DEMOS[sys.argv[1]], Path(sys.argv[2]), Path(sys.argv[3])
    for tool in ("Xvfb", "ffmpeg"):
        if not shutil.which(tool):
            sys.exit(f"{tool} is required")
    if not os.access(binary, os.X_OK):
        sys.exit(f"build {binary} first (make host)")
    work = Path(tempfile.mkdtemp(prefix="nbt-capture-"))
    display_proc, display = xvfb()
    app = None
    try:
        bridge = Bridge(work)
        bridge.write_settings(demo["settings"])
        env = {k: v for k, v in os.environ.items() if not k.startswith("NBT_")}
        env.update({
            "NBT_LAUNCH": "user", "DISPLAY": display, "LIBGL_ALWAYS_SOFTWARE": "1", "NBT_NO_AUDIO": "1",
            "NBT_DATA_DIR": str(work), "NBT_BRIDGE_CAPS": "show,controller,live-settings",
            "NBT_CONTROLLER_FD": str(bridge.app_end.fileno()), "NBT_SETTINGS_FILE": str(bridge.settings),
        })
        app = subprocess.Popen([str(binary)], env=env, pass_fds=(bridge.app_end.fileno(),),
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        time.sleep(1.5)  # the window is up
        if app.poll() is not None:
            sys.exit(f"{binary} ended with status {app.returncode}")
        bridge.send()  # keys count once all were seen up
        end = bridge.play(demo["before"]) + 1.2
        time.sleep(max(0.0, end - time.monotonic()))

        raw = work / "raw.mkv"
        grab = subprocess.Popen(
            ["ffmpeg", "-loglevel", "error", "-y", "-f", "x11grab", "-draw_mouse", "0",
             "-framerate", "30", "-video_size", f"{WIDTH}x{HEIGHT}", "-i", display,
             "-t", str(demo["seconds"]), "-c:v", "libx264rgb", "-crf", "0", "-preset", "ultrafast",
             str(raw)])
        time.sleep(0.3)  # ffmpeg is recording
        bridge.play(demo["steps"])
        if grab.wait(timeout=60) != 0:
            sys.exit("ffmpeg could not record the screen")
        out.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(
            ["ffmpeg", "-loglevel", "error", "-y", "-i", str(raw), "-vf", f"fps={FPS}",
             "-c:v", "libwebp_anim", "-lossless", "1", "-compression_level", "6", "-loop", "0",
             str(out)], check=True)
        print(f"{out}: {out.stat().st_size // 1024} KiB, {demo['seconds']:.0f} s at {FPS} frames a second")
    finally:
        if app and app.poll() is None:
            app.terminate()
            app.wait(timeout=10)
        display_proc.terminate()
        display_proc.wait(timeout=5)
        shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    main()
