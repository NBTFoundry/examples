// SPDX-License-Identifier: MPL-2.0
/* Our logic without a window: the settings, the scene, and every input
 * mapped to the actions (README, Controls). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "controls.hpp"
#include "nbt/app/video.hpp"
#include "scene.hpp"

using nbt::frontend::PadMapper;

namespace {

#define CHECK(cond)                                                   \
  do {                                                                \
    if (!(cond)) {                                                    \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      exit(1);                                                        \
    }                                                                 \
  } while (0)

void test_config() {
  nbt::app::Settings settings;
  hello::Config config = hello::read_config(settings);
  CHECK(config.colour == hello::kColourBlue && config.speed == 300 && !config.debug_log);
  CHECK(settings.set("colour", "red") && settings.set("speed", "fast") &&
        settings.set("debug_log", "on"));
  config = hello::read_config(settings);
  CHECK(config.colour == hello::kColourRed && config.speed == 600 && config.debug_log);
  /* A value the package does not list is the default. */
  CHECK(settings.set("colour", "pink") && settings.set("speed", "warp"));
  config = hello::read_config(settings);
  CHECK(config.colour == hello::kColourBlue && config.speed == 300);
  CHECK(hello::colour_rgba(hello::kColourGreen) != hello::colour_rgba(hello::kColourRed));
}

void test_scene() {
  nbt::app::Settings settings;
  const hello::Config config = hello::read_config(settings);
  hello::Scene scene;
  scene.init(1280, 480);
  char status[32];
  scene.status(status, sizeof(status));
  CHECK(!strcmp(status, "Size 96 at 640,240"));
  CHECK(scene.left() == 592 && scene.top() == 192 && scene.size() == 96);

  /* 300 pixels per second: 30 in 100 ms. */
  scene.step(1u << hello::kActionRight, 100, config);
  CHECK(scene.left() == 622);
  scene.step(1u << hello::kActionUp, 100, config);
  CHECK(scene.top() == 162);
  /* Small steps add up. */
  for (int i = 0; i < 10; ++i) scene.step(1u << hello::kActionLeft, 1, config);
  CHECK(scene.left() == 619);
  scene.step(1u << hello::kActionCentre, 16, config);
  CHECK(scene.left() == 592 && scene.top() == 192);

  /* The box stays inside the window, between its smallest size and the
   * window's height. */
  for (int i = 0; i < 100; ++i)
    scene.step(1u << hello::kActionLeft | 1u << hello::kActionUp, 100, config);
  CHECK(scene.left() == 0 && scene.top() == 0);
  for (int i = 0; i < 100; ++i) scene.step(1u << hello::kActionBigger, 100, config);
  CHECK(scene.size() == 480 && scene.left() == 0 && scene.top() == 0);
  for (int i = 0; i < 100; ++i) scene.step(1u << hello::kActionSmaller, 100, config);
  CHECK(scene.size() == hello::Scene::kMinSize);
  /* The longest status line fits NBT Manager's 24 characters. */
  for (int i = 0; i < 100; ++i)
    scene.step(1u << hello::kActionRight | 1u << hello::kActionDown, 100, config);
  for (int i = 0; i < 100; ++i) scene.step(1u << hello::kActionBigger, 100, config);
  scene.status(status, sizeof(status));
  CHECK(strlen(status) <= 24);
}

/* Only `action` of the application's is held (-1: none). */
bool only(const PadMapper& m, int action) {
  for (int id = 0; id < hello::kActionCount; ++id) {
    if (m.held(id) != (id == action)) return false;
  }
  return true;
}

nbt_gamepad_wire_state pad(uint32_t buttons, unsigned family) {
  nbt_gamepad_wire_state s;
  memset(&s, 0, sizeof(s));
  s.connected = 1;
  s.family = static_cast<uint8_t>(family);
  s.buttons = buttons;
  return s;
}

struct Expect {
  uint32_t control;
  int action;
};

void test_pads() {
  const Expect buttons[] = {
      {NBT_GAMEPAD_UP, hello::kActionUp},        {NBT_GAMEPAD_DOWN, hello::kActionDown},
      {NBT_GAMEPAD_LEFT, hello::kActionLeft},    {NBT_GAMEPAD_RIGHT, hello::kActionRight},
      {NBT_GAMEPAD_RB, hello::kActionBigger},    {NBT_GAMEPAD_LB, hello::kActionSmaller},
      {NBT_GAMEPAD_SOUTH, hello::kActionCentre}, {NBT_GAMEPAD_EAST, -1}};
  for (unsigned family = 0; family <= NBT_GAMEPAD_FAMILY_MAX; ++family) {
    PadMapper* m = new PadMapper(hello::controls());
    uint64_t now = 0;
    for (size_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i) {
      m->gamepad(pad(buttons[i].control, family), ++now);
      CHECK(only(*m, buttons[i].action));
      m->gamepad(pad(0, family), ++now);
      CHECK(only(*m, -1));
    }
    /* The left stick moves; the triggers resize. */
    nbt_gamepad_wire_state s = pad(0, family);
    s.lx = 20000;
    m->gamepad(s, ++now);
    CHECK(only(*m, hello::kActionRight));
    s.lx = 0;
    s.rt = 40000;
    m->gamepad(s, ++now);
    CHECK(only(*m, hello::kActionBigger));
    /* The help: View and Menu together, or the Xbox / PS button. */
    m->gamepad(pad(0, family), ++now);
    m->gamepad(pad(NBT_GAMEPAD_VIEW | NBT_GAMEPAD_MENU, family), ++now);
    CHECK(m->poll(now) == nbt::frontend::kCommandMenu);
    m->gamepad(pad(0, family), ++now);
    m->gamepad(pad(NBT_GAMEPAD_GUIDE, family), ++now);
    CHECK(m->poll(now) == nbt::frontend::kCommandMenu);
    CHECK(m->pad_family() == static_cast<int>(family));
    delete m;
  }
}

void test_controller() {
  PadMapper* m = new PadMapper(hello::controls());
  nbt_controller_wire_state c;
  memset(&c, 0, sizeof(c));
  c.connected = 1;
  const Expect keys[] = {{NBT_CONTROLLER_TILT_UP, hello::kActionUp},
                         {NBT_CONTROLLER_TILT_DOWN, hello::kActionDown},
                         {NBT_CONTROLLER_TILT_LEFT, hello::kActionLeft},
                         {NBT_CONTROLLER_TILT_RIGHT, hello::kActionRight},
                         {NBT_CONTROLLER_PUSH, hello::kActionCentre},
                         {NBT_CONTROLLER_OPTION, -1}};
  uint64_t now = 0;
  for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
    c.keys = keys[i].control;
    const nbt::controller::Event down = {keys[i].control, 0, 0};
    m->controller(c, down, ++now);
    CHECK(only(*m, keys[i].action));
    c.keys = 0;
    const nbt::controller::Event up = {0, keys[i].control, 0};
    m->controller(c, up, ++now);
    CHECK(only(*m, -1));
  }
  /* The knob: a detent holds Bigger or Smaller for 100 ms. */
  const nbt::controller::Event right = {0, 0, 1};
  m->controller(c, right, 100);
  CHECK(only(*m, hello::kActionBigger));
  m->poll(210);
  CHECK(only(*m, -1));
  const nbt::controller::Event left = {0, 0, -1};
  m->controller(c, left, 300);
  CHECK(only(*m, hello::kActionSmaller));
  m->poll(410);
  /* MENU or a BACK hold shows the help; a long BACK hold quits; a tap does
   * nothing. */
  const nbt::controller::Event menu = {NBT_CONTROLLER_MENU, 0, 0};
  c.keys = NBT_CONTROLLER_MENU;
  m->controller(c, menu, 1000);
  CHECK(m->poll(1000) == nbt::frontend::kCommandMenu);
  const nbt::controller::Event none = {0, 0, 0};
  c.keys = NBT_CONTROLLER_BACK;
  m->controller(c, none, 2000);
  c.keys = 0;
  m->controller(c, none, 2100);
  CHECK(m->poll(2100) == nbt::frontend::kCommandNone && only(*m, -1));
  c.keys = NBT_CONTROLLER_BACK;
  m->controller(c, none, 3000);
  CHECK(m->poll(3700) == nbt::frontend::kCommandMenu);
  CHECK(m->poll(4500) == nbt::frontend::kCommandQuit);
  delete m;
}

void test_keyboard() {
  PadMapper* m = new PadMapper(hello::controls());
  const Expect keys[] = {{nbt::app::kHostKeyUp, hello::kActionUp},
                         {nbt::app::kHostKeyDown, hello::kActionDown},
                         {nbt::app::kHostKeyLeft, hello::kActionLeft},
                         {nbt::app::kHostKeyRight, hello::kActionRight},
                         {'w', hello::kActionBigger},
                         {'q', hello::kActionSmaller},
                         {nbt::app::kHostKeyReturn, hello::kActionCentre}};
  for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
    m->host_key(static_cast<int>(keys[i].control), true);
    CHECK(only(*m, keys[i].action));
    m->host_key(static_cast<int>(keys[i].control), false);
    CHECK(only(*m, -1));
  }
  m->host_key(nbt::app::kHostKeyEscape, true);
  CHECK(m->poll(0) == nbt::frontend::kCommandMenu);
  delete m;
}

}  // namespace

int main() {
  test_config();
  test_scene();
  test_pads();
  test_controller();
  test_keyboard();
  printf("HELLO: ALL TESTS PASSED\n");
  return 0;
}
