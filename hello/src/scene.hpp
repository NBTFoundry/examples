// SPDX-License-Identifier: MPL-2.0
/* What the application shows: a box the user moves and resizes. No window
 * and no input code here, so tests/hello_test.cpp checks it on its own.
 * Replace it with the application's own state. */
#ifndef NBT_HELLO_SCENE_HPP
#define NBT_HELLO_SCENE_HPP

#include <stddef.h>
#include <stdint.h>

#include "nbt/app/settings.hpp"

namespace hello {

/* The application's actions: the numbers the controls map every input to
 * (src/controls.cpp). */
enum Action {
  kActionUp = 0,
  kActionDown,
  kActionLeft,
  kActionRight,
  kActionBigger,
  kActionSmaller,
  kActionCentre,
  kActionCount
};

enum Colour { kColourBlue = 0, kColourGreen, kColourRed, kColourCount };

/* The settings of release/package.args as the application uses them. */
struct Config {
  Colour colour;
  int speed; /* pixels per second */
  bool debug_log;
};

/* A value the package does not list counts as the setting's default. */
Config read_config(const nbt::app::Settings& settings);

/* 0xRRGGBBAA, as nbt::frontend::Overlay takes it. */
uint32_t colour_rgba(Colour colour);

class Scene {
 public:
  enum { kMinSize = 32, kDefaultSize = 96 };

  Scene();
  /* The window's size; the box starts in its centre. */
  void init(int width, int height);
  /* One frame: `held` has bit (1 << Action) set for each action held. */
  void step(uint32_t held, uint32_t elapsed_ms, const Config& config);

  /* The box: its top left corner and its side. */
  int left() const { return (x_ >> kShift) - size() / 2; }
  int top() const { return (y_ >> kShift) - size() / 2; }
  int size() const { return size_ >> kShift; }
  /* The status line NBT Manager and the phone show: at most 24 characters. */
  void status(char* out, size_t size) const;

 private:
  enum { kShift = 8 }; /* positions in 1/256 pixel: a slow move still adds up */

  void clamp();

  int width_, height_;
  int x_, y_, size_;
};

}  // namespace hello

#endif /* NBT_HELLO_SCENE_HPP */
