// SPDX-License-Identifier: MPL-2.0
/* Hello: a complete small NBT Manager application on the NBT App SDK. It
 * opens the window, reads every input, follows its settings while it runs,
 * reports a status line and stops when asked. Start a new application from
 * it: keep the order of main(), replace the scene and the drawing. */
#include <stdio.h>
#include <unistd.h>

#include <GLES2/gl2.h>

#include "controls.hpp"
#include "nbt/app/clock.hpp"
#include "nbt/app/lifecycle.hpp"
#include "nbt/app/log.hpp"
#include "nbt/app/video.hpp"
#include "nbt/controller/input.hpp"
#include "nbt/frontend/hints.hpp"
#include "nbt/frontend/overlay.hpp"
#include "nbt/gamepad/input.hpp"
#include "scene.hpp"

namespace {

using nbt::frontend::PadMapper;

const uint32_t kFrameMs = 16;
/* The panel's size, for a health check: it has no window. */
const int kPanelWidth = 1280, kPanelHeight = 480;

struct Inputs {
  PadMapper* mapper;
  uint64_t now;
};

void on_controller(void* context, const nbt_controller_wire_state& state,
                   const nbt::controller::Event& event) {
  Inputs* in = static_cast<Inputs*>(context);
  in->mapper->controller(state, event, in->now);
}

void on_gamepad(void* context, const nbt_gamepad_wire_state& state) {
  Inputs* in = static_cast<Inputs*>(context);
  in->mapper->gamepad(state, in->now);
}

uint32_t held_actions(const PadMapper& mapper) {
  uint32_t held = 0;
  for (int action = 0; action < hello::kActionCount; ++action) {
    if (mapper.held(action)) held |= 1u << action;
  }
  return held;
}

/* The help names the input in use as printed on it (a pad plugged in takes
 * over; -1 is the iDrive controller). */
void draw_help(nbt::frontend::Overlay* overlay, int width, int height, int scale, int family) {
  char lines[4][64];
  if (family < 0) {
    snprintf(lines[0], sizeof(lines[0]), "Tilt: move");
    snprintf(lines[1], sizeof(lines[1]), "Turn the knob: resize");
    snprintf(lines[2], sizeof(lines[2]), "%s: centre",
             nbt::frontend::controller_key_name(NBT_CONTROLLER_PUSH));
    snprintf(lines[3], sizeof(lines[3]), "BACK, long hold: quit");
  } else {
    snprintf(lines[0], sizeof(lines[0]), "D-pad or left stick: move");
    snprintf(lines[1], sizeof(lines[1]), "%s / %s: resize",
             nbt::frontend::pad_button_name(family, NBT_GAMEPAD_LB),
             nbt::frontend::pad_button_name(family, NBT_GAMEPAD_RB));
    snprintf(lines[2], sizeof(lines[2]), "%s: centre", nbt::frontend::confirm_name(family));
    snprintf(lines[3], sizeof(lines[3]), "iDrive BACK, long hold: quit");
  }
  const int glyph = nbt::frontend::Overlay::kGlyph * scale;
  const int panel_w = 34 * glyph, panel_h = 8 * glyph;
  const int x = (width - panel_w) / 2, y = (height - panel_h) / 2;
  overlay->box(x, y, panel_w, panel_h, 0x000000d0u);
  overlay->text(x + glyph, y + glyph, scale, nbt::frontend::input_name(family), 0xffffffffu);
  for (int i = 0; i < 4; ++i) {
    overlay->text(x + glyph, y + (3 + i) * glyph, scale, lines[i], 0xc9d1d9ffu);
  }
}

void draw(nbt::frontend::Overlay* overlay, const hello::Scene& scene, const hello::Config& config,
          int width, int height, int family, bool help) {
  glViewport(0, 0, width, height);
  glClearColor(0.05f, 0.07f, 0.09f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  const int scale = nbt::frontend::Overlay::scale_for(height);
  const int glyph = nbt::frontend::Overlay::kGlyph * scale;
  overlay->begin(width, height);
  overlay->box(scene.left(), scene.top(), scene.size(), scene.size(),
               hello::colour_rgba(config.colour));
  overlay->text(glyph, glyph, scale * 2, "Hello", 0xffffffffu);
  char hint[64];
  if (family < 0) {
    snprintf(hint, sizeof(hint), "%s: help",
             nbt::frontend::controller_key_name(NBT_CONTROLLER_MENU));
  } else {
    snprintf(hint, sizeof(hint), "%s + %s: help",
             nbt::frontend::pad_button_name(family, NBT_GAMEPAD_VIEW),
             nbt::frontend::pad_button_name(family, NBT_GAMEPAD_MENU));
  }
  overlay->text(glyph, height - 2 * glyph, scale, hint, 0x8b949effu);
  if (help) draw_help(overlay, width, height, scale, family);
  overlay->end();
}

}  // namespace

int main(int argc, char** argv) {
  /* 1. The bridge: NBT Manager's descriptors, the launch mode, the settings. */
  nbt::app::log_init("hello");
  nbt::app::Lifecycle lifecycle;
  const int started = lifecycle.init(argc, argv);
  if (started != 0) return started;
  nbt::app::log_install_crash_handlers();
  hello::Config config = hello::read_config(lifecycle.settings());
  nbt::app::log_set_debug(config.debug_log);

  /* 2. The window, on a user launch only: a health check (after an install
   * or an update) has no screen and no input. */
  nbt::app::log_stage("window");
  nbt::app::VideoEgl video;
  nbt::frontend::Overlay overlay;
  const bool user = lifecycle.is_user_mode();
  if (user) {
    const nbt::app::VideoConfig window = {"Hello", kPanelWidth, kPanelHeight};
    if (!video.init(lifecycle.surface_id(), &window) || !overlay.init()) {
      nbt::app::log_info("no window: the car will not show it");
      return 1;
    }
  }
  const int width = user ? video.window_width() : kPanelWidth;
  const int height = user ? video.window_height() : kPanelHeight;

  /* 3. What can be prepared without the screen, so the health check covers
   * it; then ready, within 5 seconds of the start. */
  hello::Scene scene;
  scene.init(width, height);
  nbt::app::log_info("ready, %dx%d, caps %s", width, height, lifecycle.bridge_caps());
  if (!lifecycle.send_ready()) return 1;
  nbt::app::log_stage("running");

  /* 4. The loop, once per frame: input, the application, the heartbeat with
   * the status line and the settings, the frame. */
  PadMapper mapper(hello::controls());
  nbt::controller::Input controller;
  nbt::gamepad::Input gamepad;
  Inputs inputs = {&mapper, 0};
  bool help = false;
  uint64_t last = nbt::app::monotonic_ms();
  while (!lifecycle.stopping()) {
    const uint64_t now = nbt::app::monotonic_ms();
    inputs.now = now;
    if (lifecycle.controller_fd() >= 0) {
      controller.poll(lifecycle.controller_fd(), &on_controller, &inputs);
    }
    if (lifecycle.gamepad_fd() >= 0) {
      gamepad.poll(lifecycle.gamepad_fd(), now, &on_gamepad, &inputs);
    }
    nbt::app::HostEvent event;
    while (user && video.poll_event(&event)) { /* the desktop build's keyboard */
      if (event.type == nbt::app::HostEvent::kKeyDown ||
          event.type == nbt::app::HostEvent::kKeyUp) {
        mapper.host_key(event.key, event.type == nbt::app::HostEvent::kKeyDown);
      } else if (event.type == nbt::app::HostEvent::kClose) {
        lifecycle.request_stop();
      }
    }
    const nbt::frontend::Command command = mapper.poll(now);
    if (command == nbt::frontend::kCommandMenu) {
      help = !help;
      nbt::app::log_info("help %s", help ? "shown" : "hidden");
    } else if (command == nbt::frontend::kCommandQuit) {
      nbt::app::log_info("BACK held: closing");
      lifecycle.request_stop();
    }
    if (mapper.take_pad_change()) {
      nbt::app::log_info("input: %s", nbt::frontend::input_name(mapper.pad_family()));
    }

    scene.step(held_actions(mapper), static_cast<uint32_t>(now - last), config);
    last = now;

    char status[32];
    scene.status(status, sizeof(status));
    bool changed = false;
    lifecycle.tick(now, status, &changed);
    if (changed) {
      config = hello::read_config(lifecycle.settings());
      nbt::app::log_set_debug(config.debug_log);
      nbt::app::log_info("settings changed");
    }
    (void)lifecycle.consume_show_requested(); /* always on screen: nothing to bring forward */

    if (user) {
      draw(&overlay, scene, config, width, height, mapper.pad_family(), help);
      video.swap();
    }
    const uint64_t spent = nbt::app::monotonic_ms() - now;
    usleep(static_cast<useconds_t>((spent < kFrameMs ? kFrameMs - spent : 1) * 1000));
  }

  /* 5. A clean end: status 0 when asked to stop. */
  nbt::app::log_stage("closing");
  overlay.shutdown();
  video.shutdown();
  lifecycle.close_fds();
  return 0;
}
