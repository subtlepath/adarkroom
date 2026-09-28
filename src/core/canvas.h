// canvas.h — drawing onto the panel's 1-bit framebuffer, in the page's own coordinates.
//
// The framebuffer is the panel's own (800 x 480 on the X4s, 792 x 528 on the X3), one bit per
// pixel, MSB first, a set bit white (the SDK's convention). The page may be turned: R90 is the
// SDK's portrait, upright with the device held tall. toPage() maps a touch back the same way, so
// what is drawn and what is touched can never disagree.
//
// Grey is a 4 x 4 ordered dither: e-paper shows it as grey at arm's length, and the game's
// fading log and greyed-out buttons read as they do in the browser.
#pragma once

#include <cstdint>

#include "fonts.h"

namespace adr {

enum class Rotation : uint8_t { R0 = 0, R90 = 1, R180 = 2, R270 = 3 };

struct Point {
  int x = 0, y = 0;
};

struct Rect {
  int x = 0, y = 0, w = 0, h = 0;
  bool contains(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; }
  int right() const { return x + w; }
  int bottom() const { return y + h; }
  Rect inset(int d) const { return {x + d, y + d, w - 2 * d, h - 2 * d}; }
};

// Ink levels, 0 (white) to 16 (black).
constexpr int kInk = 16;

class Canvas {
 public:
  void attach(uint8_t* fb, int panelW, int panelH, Rotation r);
  void setRotation(Rotation r);
  void retarget(uint8_t* fb) { _fb = fb; }
  uint8_t* buffer() const { return _fb; }
  int bytes() const { return _stride * _ph; }

  int width() const { return _w; }
  int height() const { return _h; }
  Rotation rotation() const { return _rot; }
  Point toPage(int px, int py) const;

  void clear(bool black = false);
  void pixel(int x, int y, bool black = true);
  bool get(int x, int y) const;  // true = ink
  void fill(Rect r, bool black = true);
  // Grey: every pixel of r whose dither threshold is under `level` inked.
  void shade(Rect r, int level);
  void hline(int x, int y, int w, int thick = 1, bool black = true);
  void vline(int x, int y, int h, int thick = 1, bool black = true);
  void frame(Rect r, int thick = 1);
  void dottedFrame(Rect r);
  void dotted(int x, int y, int w, int step = 3);
  void invert(Rect r);

  // Text on a baseline. `level` below kInk draws it grey (the dither applied to the glyphs); `white`
  // draws it white (on a black ground). Returns the pen position after the run.
  int text(FontId f, int x, int baseline, const char* s, int level = kInk, bool white = false);
  int text(FontId f, int x, int baseline, const char* s, int n, int level, bool white);
  int textRight(FontId f, int right, int baseline, const char* s, int level = kInk, bool white = false);
  int textCentered(FontId f, int cx, int baseline, const char* s, int level = kInk, bool white = false);
  // One line cut to maxWidth, with an ellipsis when it does not fit.
  int textFit(FontId f, int x, int baseline, const char* s, int maxWidth, int level = kInk, bool white = false);
  // Text turned a quarter, to run along a side of the page: clockwise it runs down from (x, y), the
  // tops of its letters to the right; anticlockwise it runs up, their tops to the left. (x, y) is
  // the start of its baseline.
  void textTurned(FontId f, int x, int y, const char* s, bool clockwise);

  static bool dither(int x, int y, int level);

 private:
  void glyph(const Glyph& g, int x, int baseline, int level, bool white);
  int8_t _turn = 0;  // while textTurned draws: 1 clockwise, -1 anticlockwise, about (_tx, _ty)
  int _tx = 0, _ty = 0;
  uint8_t* _fb = nullptr;
  int _pw = 0, _ph = 0, _stride = 0;
  int _w = 0, _h = 0;
  Rotation _rot = Rotation::R0;
};

// Word wrap: the byte length of the longest run of `s` (up to `n` bytes) that fits `maxWidth`,
// broken at a space when it can be. `next` is where the following line starts.
int wrapLine(FontId f, const char* s, int n, int maxWidth, int& next);

}  // namespace adr
