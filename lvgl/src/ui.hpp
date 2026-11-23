// SPDX-License-Identifier: MPL-2.0
/* The screen, in LVGL widgets: a dial and a button, driven by LVGL's encoder
 * input, which is what the iDrive controller is: turn to choose, push to
 * change or press. Replace it with the application's own screens. */
#ifndef NBT_HELLO_LVGL_UI_HPP
#define NBT_HELLO_LVGL_UI_HPP

#include <stddef.h>

#include "nbt/app/settings.hpp"

namespace hello_lvgl {

/* The settings of release/package.args as the application uses them. */
struct Config {
  bool dark;
  bool debug_log;
};

/* A value the package does not list counts as the setting's default. */
Config read_config(const nbt::app::Settings& settings);

/* LVGL's warnings and errors in the application's log; before lv_init(). */
void install_log();

class Ui {
 public:
  enum { kStartValue = 50 };

  Ui();
  /* Builds the screen on LVGL's display (display_gl.hpp), `width` x `height`. */
  bool init(int width, int height, const Config& config);
  void apply(const Config& config);
  /* This frame's input: detents turned (clockwise positive) and the push. */
  void input(int turned, bool pressed);

  /* The dial's value, 0 to 100. */
  int value() const;
  /* The status line NBT Manager and the phone show: at most 24 characters. */
  void status(char* out, size_t size) const;

  /* For LVGL's callbacks. */
  int take_turn();
  bool pressed() const { return pressed_; }
  void show_value();
  void reset();

 private:
  Ui(const Ui&);
  Ui& operator=(const Ui&);

  void* dial_;  /* lv_obj_t */
  void* label_; /* lv_obj_t */
  int turn_;
  bool pressed_;
};

}  // namespace hello_lvgl

#endif /* NBT_HELLO_LVGL_UI_HPP */
