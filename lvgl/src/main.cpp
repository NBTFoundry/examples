// SPDX-License-Identifier: MPL-2.0
/* Hello LVGL: an NBT Manager application whose screen is made of LVGL
 * widgets (third_party/lvgl), on the NBT App SDK. LVGL draws in software;
 * src/display_gl.cpp shows its buffer in the SDK's window. The iDrive
 * controller is LVGL's encoder: turn to choose, push to change or press. A
 * USB gamepad and the desktop keyboard do the same. */
#include <stdio.h>
#include <unistd.h>

#include "display_gl.hpp"
#include "lvgl.h"
#include "nbt/app/clock.hpp"
#include "nbt/app/lifecycle.hpp"
#include "nbt/app/log.hpp"
#include "nbt/app/video.hpp"
#include "nbt/controller/input.hpp"
#include "nbt/frontend/hints.hpp"
#include "nbt/frontend/pad_profiles.hpp"
#include "nbt/gamepad/input.hpp"
#include "nbt/touch/input.hpp"
#include "ui.hpp"

namespace {

using nbt::frontend::Binding;
using nbt::frontend::PadMapper;

const uint32_t kFrameMs = 16;
/* The panel's size, for a health check: it has no window. */
const int kPanelWidth = 1280, kPanelHeight = 480;

/* --- Controls: every input ends as the encoder's turn and push. --- */
void read_pointer(lv_indev_t* indev, lv_indev_data_t* data) {
  const nbt::touch::Event& event =
      *static_cast<const nbt::touch::Event*>(lv_indev_get_user_data(indev));
  data->point.x = event.x;
  data->point.y = event.y;
  data->state = event.phase == nbt::touch::Up || event.phase == nbt::touch::Cancel
                    ? LV_INDEV_STATE_RELEASED
                    : LV_INDEV_STATE_PRESSED;
}

void on_pointer(void* context, const nbt::touch::Event& event) {
  lv_indev_t* indev = static_cast<lv_indev_t*>(context);
  if (event.phase == nbt::touch::Cancel) lv_indev_wait_release(indev);
  nbt::touch::Event sample = event;
  lv_indev_set_user_data(indev, &sample);
  lv_indev_read(indev);
  lv_indev_set_user_data(indev, NULL);
}

enum Action { kActionBack = 0, kActionForward, kActionPush };

/* A pad: the D-pad, the left stick or the shoulder buttons turn, the bottom
 * face button (Xbox A, DualShock and DualSense Cross) pushes. */
const Binding kPadButtons[] = {
    {NBT_GAMEPAD_LEFT, kActionBack},    {NBT_GAMEPAD_UP, kActionBack},
    {NBT_GAMEPAD_LB, kActionBack},      {NBT_GAMEPAD_RIGHT, kActionForward},
    {NBT_GAMEPAD_DOWN, kActionForward}, {NBT_GAMEPAD_RB, kActionForward},
    {NBT_GAMEPAD_SOUTH, kActionPush}};
const nbt::frontend::PadProfile kPad = {kPadButtons, sizeof(kPadButtons) / sizeof(kPadButtons[0]),
                                        kActionBack, kActionForward,
                                        kActionBack, kActionForward,
                                        kActionBack, kActionForward,
                                        0,           0};

/* The iDrive controller: the knob's detents go to LVGL one by one (taken
 * from the event, below), so the profile binds no pulse; tilts turn too. A
 * long BACK hold quits. */
const Binding kControllerKeys[] = {{NBT_CONTROLLER_PUSH, kActionPush},
                                   {NBT_CONTROLLER_TILT_LEFT, kActionBack},
                                   {NBT_CONTROLLER_TILT_UP, kActionBack},
                                   {NBT_CONTROLLER_TILT_RIGHT, kActionForward},
                                   {NBT_CONTROLLER_TILT_DOWN, kActionForward}};
const nbt::frontend::ControllerProfile kController = {
    kControllerKeys,
    sizeof(kControllerKeys) / sizeof(kControllerKeys[0]),
    nbt::frontend::kNoAction,
    nbt::frontend::kNoAction,
    0,
    nbt::frontend::kNoAction,
    0};

/* The desktop: the arrows turn, Return pushes. */
const Binding kKeys[] = {{nbt::app::kHostKeyLeft, kActionBack},
                         {nbt::app::kHostKeyUp, kActionBack},
                         {nbt::app::kHostKeyRight, kActionForward},
                         {nbt::app::kHostKeyDown, kActionForward},
                         {nbt::app::kHostKeyReturn, kActionPush}};
const nbt::frontend::KeyboardProfile kKeyboard = {kKeys, sizeof(kKeys) / sizeof(kKeys[0]), 0};

const nbt::frontend::Profiles kProfiles = {{&kPad, &kPad, &kPad, &kPad}, &kController, &kKeyboard};

/* A key or button going down is one detent. */
class Turns : public nbt::app::InputSink {
 public:
  Turns() : turned_(0) {}
  void on_action(const nbt::app::InputAction& action) {
    if (action.type != nbt::app::InputAction::kKeyDown) return;
    if (action.value == kActionBack) --turned_;
    if (action.value == kActionForward) ++turned_;
  }
  void add(int detents) { turned_ += detents; }
  int take() {
    const int turned = turned_;
    turned_ = 0;
    return turned;
  }

 private:
  int turned_;
};

struct Inputs {
  PadMapper* mapper;
  Turns* turns;
  uint64_t now;
};

void on_controller(void* context, const nbt_controller_wire_state& state,
                   const nbt::controller::Event& event) {
  Inputs* in = static_cast<Inputs*>(context);
  in->turns->add(event.rotation);
  in->mapper->controller(state, event, in->now);
}

void on_gamepad(void* context, const nbt_gamepad_wire_state& state) {
  Inputs* in = static_cast<Inputs*>(context);
  in->mapper->gamepad(state, in->now);
}

uint32_t tick_ms() {
  return static_cast<uint32_t>(nbt::app::monotonic_ms());
}

}  // namespace

int main(int argc, char** argv) {
  /* 1. The bridge: NBT Manager's descriptors, the launch mode, the settings. */
  nbt::app::log_init("hello-lvgl");
  nbt::app::Lifecycle lifecycle;
  const int started = lifecycle.init(argc, argv);
  if (started != 0) return started;
  nbt::app::log_install_crash_handlers();
  hello_lvgl::Config config = hello_lvgl::read_config(lifecycle.settings());
  nbt::app::log_set_debug(config.debug_log);

  /* 2. The window, on a user launch only: a health check (after an install
   * or an update) has no screen and no input. */
  nbt::app::log_stage("window");
  nbt::app::VideoEgl video;
  const bool user = lifecycle.is_user_mode();
  if (user) {
    const nbt::app::VideoConfig window = {"Hello LVGL", kPanelWidth, kPanelHeight};
    if (!video.init(lifecycle.surface_id(), &window)) {
      nbt::app::log_info("no window: the car will not show it");
      return 1;
    }
  }
  const int width = user ? video.window_width() : kPanelWidth;
  const int height = user ? video.window_height() : kPanelHeight;

  /* 3. LVGL, the screen and one frame: also in a health check, off screen,
   * so the check covers them; then ready, within 5 seconds of the start. */
  nbt::app::log_stage("lvgl");
  hello_lvgl::install_log();
  lv_init();
  lv_tick_set_cb(&tick_ms);
  hello_lvgl::DisplayGl display;
  hello_lvgl::Ui ui;
  if (!display.init(width, height, user) || !ui.init(width, height, config)) {
    nbt::app::log_info("the screen could not be built (%dx%d)", width, height);
    return 1;
  }
  lv_refr_now(NULL);
  if (display.take_frame() && user) {
    display.draw(width, height);
    video.swap();
  }
  nbt::app::log_info("ready, %dx%d, caps %s", width, height, lifecycle.bridge_caps());
  if (!lifecycle.send_ready()) return 1;
  nbt::app::log_stage("running");

  /* 4. The loop, once per frame: input, the heartbeat with the status line
   * and the settings, LVGL's timers, the frame when LVGL drew one. */
  PadMapper mapper(kProfiles);
  Turns turns;
  mapper.set_sink(&turns);
  nbt::touch::Pointer pointer;
  lv_indev_t* touch = NULL;
  if (lifecycle.touch_fd() >= 0) {
    touch = lv_indev_create();
    if (!touch) return 1;
    lv_indev_set_type(touch, LV_INDEV_TYPE_POINTER);
    lv_indev_set_mode(touch, LV_INDEV_MODE_EVENT);
    lv_indev_set_read_cb(touch, &read_pointer);
  }
  nbt::controller::Input controller;
  nbt::gamepad::Input gamepad;
  Inputs inputs = {&mapper, &turns, 0};
  while (!lifecycle.stopping()) {
    const uint64_t now = nbt::app::monotonic_ms();
    inputs.now = now;
    if (touch) pointer.poll(lifecycle.touch_fd(), width, height, &on_pointer, touch);
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
    if (mapper.poll(now) == nbt::frontend::kCommandQuit) {
      nbt::app::log_info("BACK held: closing");
      lifecycle.request_stop();
    }
    if (mapper.take_pad_change()) {
      nbt::app::log_info("input: %s", nbt::frontend::input_name(mapper.pad_family()));
    }
    ui.input(turns.take(), mapper.held(kActionPush));

    char status[32];
    ui.status(status, sizeof(status));
    bool changed = false;
    lifecycle.tick(now, status, &changed);
    if (changed) {
      config = hello_lvgl::read_config(lifecycle.settings());
      nbt::app::log_set_debug(config.debug_log);
      ui.apply(config);
      nbt::app::log_info("settings changed: theme %s", config.dark ? "dark" : "light");
    }
    (void)lifecycle.consume_show_requested(); /* always on screen: nothing to bring forward */

    lv_timer_handler();
    if (display.take_frame() && user) {
      display.draw(width, height);
      video.swap();
    }
    const uint64_t spent = nbt::app::monotonic_ms() - now;
    usleep(static_cast<useconds_t>((spent < kFrameMs ? kFrameMs - spent : 1) * 1000));
  }

  /* 5. A clean end: status 0 when asked to stop. */
  nbt::app::log_stage("closing");
  display.shutdown();
  lv_deinit();
  video.shutdown();
  lifecycle.close_fds();
  return 0;
}
