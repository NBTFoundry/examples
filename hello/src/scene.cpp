// SPDX-License-Identifier: MPL-2.0
#include "scene.hpp"

#include <stdio.h>
#include <string.h>

namespace hello {

namespace {

const int kGrowPerSecond = 240; /* pixels of side per second */

bool held_bit(uint32_t held, Action action) {
  return (held >> action) & 1u;
}

}  // namespace

Config read_config(const nbt::app::Settings& settings) {
  Config config = {kColourBlue, 300, false};
  const char* colour = settings.get("colour", "blue");
  if (!strcmp(colour, "green")) config.colour = kColourGreen;
  if (!strcmp(colour, "red")) config.colour = kColourRed;
  const char* speed = settings.get("speed", "normal");
  if (!strcmp(speed, "slow")) config.speed = 120;
  if (!strcmp(speed, "fast")) config.speed = 600;
  config.debug_log = !strcmp(settings.get("debug_log", "off"), "on");
  return config;
}

uint32_t colour_rgba(Colour colour) {
  switch (colour) {
    case kColourGreen:
      return 0x3fb950ffu;
    case kColourRed:
      return 0xe5534bffu;
    default:
      return 0x3b82f6ffu;
  }
}

Scene::Scene() : width_(0), height_(0), x_(0), y_(0), size_(kDefaultSize << kShift) {}

void Scene::init(int width, int height) {
  width_ = width;
  height_ = height;
  x_ = (width / 2) << kShift;
  y_ = (height / 2) << kShift;
  size_ = kDefaultSize << kShift;
  clamp();
}

void Scene::step(uint32_t held, uint32_t elapsed_ms, const Config& config) {
  if (elapsed_ms > 100) elapsed_ms = 100; /* a stalled frame is not a jump */
  const int move =
      static_cast<int>((static_cast<int64_t>(config.speed) * elapsed_ms << kShift) / 1000);
  const int grow =
      static_cast<int>((static_cast<int64_t>(kGrowPerSecond) * elapsed_ms << kShift) / 1000);
  if (held_bit(held, kActionLeft)) x_ -= move;
  if (held_bit(held, kActionRight)) x_ += move;
  if (held_bit(held, kActionUp)) y_ -= move;
  if (held_bit(held, kActionDown)) y_ += move;
  if (held_bit(held, kActionBigger)) size_ += grow;
  if (held_bit(held, kActionSmaller)) size_ -= grow;
  if (held_bit(held, kActionCentre)) {
    x_ = (width_ / 2) << kShift;
    y_ = (height_ / 2) << kShift;
  }
  clamp();
}

/* The box stays whole inside the window, and no larger than it. */
void Scene::clamp() {
  const int largest = (width_ < height_ ? width_ : height_) << kShift;
  if (size_ > largest) size_ = largest;
  if (size_ < kMinSize << kShift) size_ = kMinSize << kShift;
  const int half = size_ / 2;
  if (x_ < half) x_ = half;
  if (y_ < half) y_ = half;
  if (x_ > (width_ << kShift) - half) x_ = (width_ << kShift) - half;
  if (y_ > (height_ << kShift) - half) y_ = (height_ << kShift) - half;
}

void Scene::status(char* out, size_t size) const {
  snprintf(out, size, "Size %d at %d,%d", this->size(), x_ >> kShift, y_ >> kShift);
}

}  // namespace hello
