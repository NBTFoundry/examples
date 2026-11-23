// SPDX-License-Identifier: MPL-2.0
#include "ui.hpp"

#include <stdio.h>
#include <string.h>

#include "lvgl.h"
#include "nbt/app/log.hpp"

namespace hello_lvgl {

namespace {

lv_obj_t* obj(void* p) {
  return static_cast<lv_obj_t*>(p);
}

void log_print(lv_log_level_t level, const char* text) {
  (void)level;
  const size_t length = strcspn(text, "\n");
  nbt::app::log_info("lvgl: %.*s", static_cast<int>(length), text);
}

/* LVGL asks for the encoder's state every refresh period. */
void read_encoder(lv_indev_t* indev, lv_indev_data_t* data) {
  Ui* ui = static_cast<Ui*>(lv_indev_get_user_data(indev));
  data->enc_diff = static_cast<int16_t>(ui->take_turn());
  data->state = ui->pressed() ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

void on_dial_changed(lv_event_t* event) {
  static_cast<Ui*>(lv_event_get_user_data(event))->show_value();
}

void on_reset_clicked(lv_event_t* event) {
  static_cast<Ui*>(lv_event_get_user_data(event))->reset();
}

}  // namespace

Config read_config(const nbt::app::Settings& settings) {
  Config config;
  config.dark = strcmp(settings.get("theme", "dark"), "light") != 0;
  config.debug_log = !strcmp(settings.get("debug_log", "off"), "on");
  return config;
}

void install_log() {
  lv_log_register_print_cb(&log_print);
}

Ui::Ui() : dial_(NULL), label_(NULL), turn_(0), pressed_(false) {}

bool Ui::init(int width, int height, const Config& config) {
  apply(config);
  lv_obj_t* screen = lv_screen_active();

  lv_obj_t* title = lv_label_create(screen);
  lv_label_set_text(title, "Hello LVGL");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, height / 20, height / 20);

  /* The dial: an arc with its value in the middle. */
  const int side = height * 6 / 10;
  lv_obj_t* dial = lv_arc_create(screen);
  lv_obj_set_size(dial, side, side);
  lv_arc_set_range(dial, 0, 100);
  lv_arc_set_value(dial, kStartValue);
  lv_obj_align(dial, LV_ALIGN_CENTER, -width / 8, height / 20);
  lv_obj_add_event_cb(dial, &on_dial_changed, LV_EVENT_VALUE_CHANGED, this);
  dial_ = dial;
  lv_obj_t* label = lv_label_create(dial);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_48, 0);
  lv_obj_center(label);
  label_ = label;
  show_value();

  lv_obj_t* reset = lv_button_create(screen);
  lv_obj_align(reset, LV_ALIGN_CENTER, width / 5, height / 20);
  lv_obj_add_event_cb(reset, &on_reset_clicked, LV_EVENT_CLICKED, this);
  lv_obj_t* reset_label = lv_label_create(reset);
  lv_label_set_text(reset_label, "Reset");

  lv_obj_t* hint = lv_label_create(screen);
  lv_label_set_text(hint, "Turn: choose    Push: change or press    BACK, long hold: quit");
  lv_obj_set_style_text_opa(hint, LV_OPA_60, 0);
  lv_obj_align(hint, LV_ALIGN_BOTTOM_LEFT, height / 20, -height / 20);

  /* The encoder moves the focus within its group; a push on the dial starts
   * and ends changing its value. */
  lv_group_t* group = lv_group_create();
  lv_group_add_obj(group, dial);
  lv_group_add_obj(group, reset);
  lv_indev_t* encoder = lv_indev_create();
  if (!group || !encoder) return false;
  lv_indev_set_type(encoder, LV_INDEV_TYPE_ENCODER);
  lv_indev_set_user_data(encoder, this);
  lv_indev_set_read_cb(encoder, &read_encoder);
  lv_indev_set_group(encoder, group);
  return true;
}

void Ui::apply(const Config& config) {
  lv_display_t* display = lv_display_get_default();
  lv_theme_t* theme =
      lv_theme_default_init(display, lv_palette_main(LV_PALETTE_BLUE),
                            lv_palette_main(LV_PALETTE_AMBER), config.dark, LV_FONT_DEFAULT);
  lv_display_set_theme(display, theme);
}

void Ui::input(int turned, bool pressed) {
  turn_ += turned;
  pressed_ = pressed;
}

int Ui::take_turn() {
  const int turn = turn_;
  turn_ = 0;
  return turn;
}

int Ui::value() const {
  return dial_ ? static_cast<int>(lv_arc_get_value(obj(dial_))) : kStartValue;
}

void Ui::show_value() {
  lv_label_set_text_fmt(obj(label_), "%d", value());
}

void Ui::reset() {
  lv_arc_set_value(obj(dial_), kStartValue);
  show_value();
  nbt::app::log_info("reset");
}

void Ui::status(char* out, size_t size) const {
  snprintf(out, size, "Value %d", value());
}

}  // namespace hello_lvgl
