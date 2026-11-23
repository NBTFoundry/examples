# SPDX-License-Identifier: MPL-2.0
"""The desktop build as NBT Manager starts it: the health check without a
window, then user launches under Xvfb driven through the bridge (the iDrive
controller, a gamepad, the status line, live settings); the signed package's
declaration; the notices being current."""

import os
from pathlib import Path
import re
import select
import shutil
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]
TOP = ROOT.parent  # the repository
APP = Path(os.environ.get("NBT_HELLO_APP", ROOT / "build/nbt-hello-host"))
MGR_DIR = Path(os.environ.get("NBT_MGR_DIR", TOP.parent / "manager"))
SDK_DIR = Path(os.environ.get("NBT_SDK_DIR", TOP.parent / "sdk"))

# The iDrive controller's keys and a pad's buttons (the SDK's
# nbt/controller_wire.h and nbt/gamepad_wire.h).
MENU, BACK, TILT_RIGHT = 1 << 1, 1 << 2, 1 << 11
PAD_LEFT = 1 << 13
XBOX = 1


def controller_state(sequence, keys=0, rotation=0):
    return struct.pack("<BBBBIIi", 0x43, 1, 1, 0, sequence, keys, rotation)


def gamepad_state(sequence, buttons=0, family=XBOX):
    return struct.pack("<BBBBIIhhhhHH", 1, 1, 1, family, sequence, buttons, 0, 0, 0, 0, 0, 0)


def xvfb():
    r, w = os.pipe()
    proc = subprocess.Popen(
        ["Xvfb", "-displayfd", str(w), "-screen", "0", "1280x480x24", "-nolisten", "tcp"],
        pass_fds=(w,), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    os.close(w)
    number = b""
    while not number.endswith(b"\n"):
        ready, _, _ = select.select([r], [], [], 10)
        chunk = os.read(r, 1) if ready else b""
        if not chunk:
            break
        number += chunk
    os.close(r)
    return proc, ":" + number.decode().strip()


class App:
    """The application, its log (standard error) read by line."""

    def __init__(self, env, args=(), pass_fds=()):
        full = {k: v for k, v in os.environ.items() if not k.startswith("NBT_")}
        full.update(env)
        self.proc = subprocess.Popen([str(APP), *args], stdout=subprocess.DEVNULL,
                                     stderr=subprocess.PIPE, env=full, pass_fds=pass_fds)
        self.lines, self.seen, self.buf = [], [], b""

    def wait(self, pattern, timeout=20.0):
        regex, deadline = re.compile(pattern), time.monotonic() + timeout
        while True:
            while b"\n" in self.buf:
                raw, self.buf = self.buf.split(b"\n", 1)
                line = raw.decode(errors="replace")
                self.lines.append(line)
                self.seen.append(line)
            for i, line in enumerate(self.lines):
                if regex.search(line):
                    del self.lines[: i + 1]
                    return line
            fd = self.proc.stderr.fileno()
            left = deadline - time.monotonic()
            ready, _, _ = select.select([fd], [], [], max(0.0, left)) if left > 0 else ([], [], [])
            chunk = os.read(fd, 4096) if ready else b""
            if not chunk:
                raise AssertionError(f"no line matching {pattern!r}; saw {self.seen[-30:]!r}")
            self.buf += chunk

    def stop(self, sig=signal.SIGTERM):
        """Sends `sig` (None: only waits for the end) and returns the status."""
        if sig is not None and self.proc.poll() is None:
            self.proc.send_signal(sig)
        _, err = self.proc.communicate(timeout=20)
        self.buf += err
        return self.proc.returncode


@unittest.skipUnless(os.access(APP, os.X_OK), f"{APP} not built")
class CheckLaunchTest(unittest.TestCase):
    def test_reports_ready_without_a_window(self):
        """As NBT Manager runs it after an install or an update: no display,
        ready on --ready-fd, heartbeats, then SIGTERM ends it with status 0."""
        with tempfile.TemporaryDirectory() as data:
            lease_fd = os.open(Path(data) / "lease", os.O_CREAT | os.O_RDWR, 0o600)
            rfd, wfd = os.pipe()
            env = {"NBT_LAUNCH": "check", "NBT_DATA_DIR": data}
            app = App(env, ["--ready-fd", str(wfd), "--lease-fd", str(lease_fd)], (wfd, lease_fd))
            os.close(wfd)
            try:
                app.wait(r"ready, 1280x480")
                for _ in range(2):  # ready, then a heartbeat
                    readable, _, _ = select.select([rfd], [], [], 5.0)
                    self.assertEqual(os.read(rfd, 1) if readable else b"", b"R")
                self.assertEqual(app.stop(), 0, app.seen[-20:])
            finally:
                if app.proc.poll() is None:
                    app.proc.kill()
                os.close(rfd)
                os.close(lease_fd)


@unittest.skipUnless(os.access(APP, os.X_OK), f"{APP} not built")
@unittest.skipUnless(shutil.which("Xvfb"), "Xvfb not installed")
class UserLaunchTest(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix="nbt-hello-"))
        self.x, self.display = xvfb()
        self.controller_app, self.controller = socket.socketpair(socket.AF_UNIX, socket.SOCK_DGRAM)
        self.pad_app, self.pad = socket.socketpair(socket.AF_UNIX, socket.SOCK_DGRAM)
        self.status_path = self.tmp / "status"
        self.status_fd = os.open(self.status_path, os.O_RDWR | os.O_CREAT, 0o600)
        self.settings_path = self.tmp / "settings"
        self.seq = self.pad_seq = 0

    def tearDown(self):
        for sock in (self.controller_app, self.controller, self.pad_app, self.pad):
            sock.close()
        os.close(self.status_fd)
        self.x.terminate()
        self.x.wait(timeout=5)
        shutil.rmtree(self.tmp, ignore_errors=True)

    def write_settings(self, **values):
        """The manager's snapshot, written in place: a private file."""
        text = "NBTVALUES1\n" + "".join(f"{k}={v}\n" for k, v in values.items())
        fd = os.open(self.settings_path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
        with open(fd, "w") as f:
            f.write(text)

    def start(self, **settings):
        self.write_settings(**settings)
        env = {
            "NBT_LAUNCH": "user", "DISPLAY": self.display, "LIBGL_ALWAYS_SOFTWARE": "1",
            "NBT_NO_AUDIO": "1", "NBT_DATA_DIR": str(self.tmp),
            "NBT_BRIDGE_CAPS": "show,controller,status,live-settings,gamepad",
            "NBT_CONTROLLER_FD": str(self.controller_app.fileno()),
            "NBT_GAMEPAD_FD": str(self.pad_app.fileno()),
            "NBT_STATUS_FD": str(self.status_fd),
            "NBT_SETTINGS_FILE": str(self.settings_path),
        }
        app = App(env, pass_fds=(self.controller_app.fileno(), self.pad_app.fileno(), self.status_fd))
        self.addCleanup(lambda: app.proc.poll() is None and app.proc.kill())
        app.wait(r"ready, 1280x480, caps show,controller,status,live-settings,gamepad")
        self.keys(0)  # keys count once all were seen up
        return app

    def keys(self, keys, rotation=0):
        self.seq += 1
        self.controller.send(controller_state(self.seq, keys, rotation))

    def pad_state(self, buttons=0):
        self.pad_seq += 1
        self.pad.send(gamepad_state(self.pad_seq, buttons))

    def pad_hold(self, buttons, seconds):
        """A pad sends its state all the time: one silent for half a second
        counts as gone, and its buttons count once seen released."""
        self.pad_state(0)
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            self.pad_state(buttons)
            time.sleep(0.05)
        self.pad_state(0)

    def status(self):
        """The status line once it has settled: "<ms> <text>", at 1 Hz."""
        time.sleep(1.2)
        return self.status_path.read_text().split(" ", 1)[1].strip()

    def position(self):
        size, x, y = re.fullmatch(r"Size (\d+) at (\d+),(\d+)", self.status()).groups()
        return int(size), int(x), int(y)

    def test_the_controller_moves_and_resizes_the_box(self):
        app = self.start()
        self.assertEqual(self.position(), (96, 640, 240))
        self.keys(TILT_RIGHT)
        time.sleep(0.5)
        self.keys(0)
        size, x, y = self.position()
        self.assertGreater(x, 700)  # 300 pixels per second
        self.assertEqual((size, y), (96, 240))
        self.keys(0, rotation=2)  # two detents: bigger for 200 ms
        size, _, _ = self.position()
        self.assertGreater(size, 110)
        self.keys(MENU, rotation=2)
        app.wait(r"help shown")
        self.keys(0, rotation=2)
        self.assertEqual(app.stop(), 0, app.seen[-20:])

    def test_a_gamepad_takes_over_and_settings_apply_live(self):
        app = self.start(speed="slow")
        self.pad_state(0)
        app.wait(r"input: Xbox controller")
        self.pad_hold(PAD_LEFT, 0.5)
        _, slow, _ = self.position()
        self.assertTrue(560 < slow < 620, slow)  # 120 pixels per second
        self.write_settings(speed="fast", colour="red")
        app.wait(r"settings changed", timeout=5)
        self.pad_hold(PAD_LEFT, 0.5)
        _, fast, _ = self.position()
        self.assertLess(fast, slow - 200)  # 600 pixels per second
        self.assertEqual(app.stop(), 0, app.seen[-20:])

    def test_a_long_back_hold_quits(self):
        app = self.start()
        self.keys(BACK)
        app.wait(r"help shown", timeout=3)  # a hold (0.6 s), on the way to the long one
        app.wait(r"BACK held: closing", timeout=3)
        self.assertEqual(app.stop(None), 0, app.seen[-20:])


def stand_in(form):
    """An ARM ELF header in place of the payload. With the runner (a module):
    ET_DYN carrying the SDK's NBT ABI note, which the packer wants with the
    `runner` capability and refuses without it. Native: a plain executable."""
    elf = bytearray(52)
    elf[:7] = b"\x7fELF\x01\x01\x01"
    if form == "native":
        elf[16:20] = b"\x02\x00\x28\x00"
        return bytes(elf)
    note = struct.pack("<III", 4, 12, 1) + b"NBT\0" + struct.pack("<III", 1, 1, 1)
    struct.pack_into("<HHI", elf, 16, 3, 40, 1)
    struct.pack_into("<I", elf, 28, 52)
    struct.pack_into("<HHH", elf, 40, 52, 32, 1)
    return bytes(elf) + struct.pack("<IIIIIIII", 4, 84, 0, 0, len(note), len(note), 4, 4) + note


class PackageTest(unittest.TestCase):
    def package(self, form):
        """The form's package, signed with a throw-away key: what nbt-verify
        reads from it, and its manifest."""
        verify = MGR_DIR / "build" / "nbt-verify"
        if not (MGR_DIR / "tools/package.py").exists() or not verify.exists():
            self.skipTest("the manager's tools/package.py or build/nbt-verify is missing")
        if not shutil.which("openssl"):
            self.skipTest("openssl is missing")
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            key, public = tmp / "app.key", tmp / "app.pub"
            subprocess.run(["openssl", "genrsa", "-out", key, "3072"], check=True, capture_output=True)
            subprocess.run(["openssl", "rsa", "-in", key, "-pubout", "-out", public], check=True,
                           capture_output=True)
            binary = tmp / ("stand-in" if form == "native" else "stand-in.so")
            binary.write_bytes(stand_in(form))
            subprocess.run(["make", "-s", "-C", str(ROOT), "package", f"FORM={form}", f"BUILD_DIR={tmp}",
                            f"PACKAGE_INPUT={binary}", f"APP_KEY={key}", f"NBT_MGR_DIR={MGR_DIR}",
                            f"NBT_SDK_DIR={SDK_DIR}", "APP_VERSION=0.1.0", "APP_SEQ=1000"],
                           check=True, capture_output=True)
            package = tmp / "hello-0.1.0.nbt"
            out = subprocess.run([str(verify), str(package), str(public), "hello", "nbt_6.5_armv7"],
                                 check=True, capture_output=True, text=True).stdout
            raw = package.read_bytes()
            return out, raw[24 : 24 + int.from_bytes(raw[8:12], "big")].decode("ascii")

    def test_with_the_runner_it_declares_bridge_1_7_and_its_settings(self):
        out, manifest = self.package("module")
        self.assertIn("app=hello\n", out)
        self.assertIn("bridge=1.7\n", out)
        self.assertIn("bridge-uses=show,controller,runner\n", out)
        self.assertIn("bridge-optional=status,live-settings,gamepad\n", out)
        # Every setting declared is one the application reads, with its
        # choices (src/scene.cpp).
        scene = (ROOT / "src/scene.cpp").read_text()
        settings = re.findall(r"^setting=(\w+)\|[^|]+\|(\w+)\|([\w,-]+)$", manifest, re.M)
        self.assertEqual([s[0] for s in settings], ["colour", "speed", "debug_log"])
        for key_name, default, choices in settings:
            self.assertIn(default, choices.split(","))
            for word in (key_name, *choices.split(",")):
                self.assertIn(f'"{word}"', scene)
        self.assertIn("property=Licence|MPL-2.0\n", manifest)
        self.assertIn("\ntext=\n", manifest)

    def test_without_the_runner_it_declares_bridge_1_5(self):
        out, manifest = self.package("native")
        self.assertIn("app=hello\n", out)
        self.assertIn("bridge=1.5\n", out)
        self.assertIn("bridge-uses=show,controller\n", out)
        self.assertIn("bridge-optional=status,live-settings,gamepad\n", out)
        self.assertEqual(len(re.findall(r"^setting=", manifest, re.M)), 3)


class NoticeTest(unittest.TestCase):
    def test_the_notices_are_current(self):
        if not (SDK_DIR / "tools/third_party_notices.py").exists():
            self.skipTest("no SDK checkout for its notices")
        check = subprocess.run([sys.executable, str(TOP / "tools/make_notice.py"), "--check",
                                "--out", str(ROOT / "legal/NOTICE.txt")],
                               env=dict(os.environ, NBT_SDK_DIR=str(SDK_DIR)),
                               capture_output=True, text=True)
        if check.returncode and "no picolibc notices" in check.stderr:
            self.skipTest("no NBT toolchain for its notices")
        self.assertEqual(check.returncode, 0, check.stderr)


if __name__ == "__main__":
    unittest.main()
