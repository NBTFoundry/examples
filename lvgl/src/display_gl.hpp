// SPDX-License-Identifier: MPL-2.0
/* LVGL's display on the SDK's OpenGL ES 2 window: LVGL draws in software into
 * a full-panel 32-bit buffer (direct mode: only what changed is redrawn), the
 * changed areas go to a texture, and a frame is a textured quad. Without a
 * window (the health check) only the buffer is drawn. */
#ifndef NBT_HELLO_LVGL_DISPLAY_GL_HPP
#define NBT_HELLO_LVGL_DISPLAY_GL_HPP

#include <stddef.h>
#include <stdint.h>

namespace hello_lvgl {

class DisplayGl {
 public:
  DisplayGl();
  ~DisplayGl();
  /* Creates LVGL's display, `width` x `height`; with `gl`, also the texture
   * and shader (the window's context must be current). After lv_init(). */
  bool init(int width, int height, bool gl);
  /* True once per frame LVGL drew (the caller then draws and swaps). */
  bool take_frame();
  /* Draws the texture over the viewport `vw` x `vh`. */
  void draw(int vw, int vh);
  /* Before lv_deinit(). */
  void shutdown();

  /* For LVGL's flush callback. */
  void flushed(int x1, int y1, int x2, int y2, bool last);

 private:
  DisplayGl(const DisplayGl&);
  DisplayGl& operator=(const DisplayGl&);

  int width_, height_, stride_;
  uint8_t* buffer_;
  uint8_t* scratch_;
  bool gl_, frame_;
  unsigned texture_, program_, vbo_;
  int a_pos_, a_uv_, u_tex_;
  void* display_; /* lv_display_t */
};

}  // namespace hello_lvgl

#endif /* NBT_HELLO_LVGL_DISPLAY_GL_HPP */
