#include "canvas.h"

#include <cstring>

namespace adr {

namespace {
// 4 x 4 Bayer thresholds, 0..15.
const uint8_t kBayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
}  // namespace

bool Canvas::dither(int x, int y, int level) {
  if (level >= kInk) return true;
  if (level <= 0) return false;
  return kBayer[y & 3][x & 3] < level;
}

void Canvas::attach(uint8_t* fb, int panelW, int panelH, Rotation r) {
  _fb = fb;
  _pw = panelW;
  _ph = panelH;
  _stride = (panelW + 7) / 8;
  setRotation(r);
}

void Canvas::setRotation(Rotation r) {
  _rot = r;
  const bool quarter = r == Rotation::R90 || r == Rotation::R270;
  _w = quarter ? _ph : _pw;
  _h = quarter ? _pw : _ph;
}

// Page (x, y) -> panel (px, py):
//   R0   px = x            py = y
//   R90  px = y            py = ph - 1 - x
//   R180 px = pw - 1 - x   py = ph - 1 - y
//   R270 px = pw - 1 - y   py = x
Point Canvas::toPage(int px, int py) const {
  switch (_rot) {
    case Rotation::R0: return {px, py};
    case Rotation::R90: return {_ph - 1 - py, px};
    case Rotation::R180: return {_pw - 1 - px, _ph - 1 - py};
    case Rotation::R270: return {py, _pw - 1 - px};
  }
  return {px, py};
}

void Canvas::pixel(int x, int y, bool black) {
  if (!_fb || x < 0 || y < 0 || x >= _w || y >= _h) return;
  int px, py;
  switch (_rot) {
    case Rotation::R0: px = x; py = y; break;
    case Rotation::R90: px = y; py = _ph - 1 - x; break;
    case Rotation::R180: px = _pw - 1 - x; py = _ph - 1 - y; break;
    default: px = _pw - 1 - y; py = x; break;
  }
  uint8_t& cell = _fb[py * _stride + (px >> 3)];
  const uint8_t mask = static_cast<uint8_t>(0x80 >> (px & 7));
  if (black) cell &= static_cast<uint8_t>(~mask);
  else cell |= mask;
}

bool Canvas::get(int x, int y) const {
  if (!_fb || x < 0 || y < 0 || x >= _w || y >= _h) return false;
  int px, py;
  switch (_rot) {
    case Rotation::R0: px = x; py = y; break;
    case Rotation::R90: px = y; py = _ph - 1 - x; break;
    case Rotation::R180: px = _pw - 1 - x; py = _ph - 1 - y; break;
    default: px = _pw - 1 - y; py = x; break;
  }
  return !(_fb[py * _stride + (px >> 3)] & (0x80 >> (px & 7)));
}

void Canvas::clear(bool black) {
  if (_fb) memset(_fb, black ? 0x00 : 0xFF, static_cast<size_t>(_stride) * _ph);
}

void Canvas::fill(Rect r, bool black) {
  const int x0 = r.x < 0 ? 0 : r.x, y0 = r.y < 0 ? 0 : r.y;
  const int x1 = r.right() > _w ? _w : r.right(), y1 = r.bottom() > _h ? _h : r.bottom();
  for (int y = y0; y < y1; ++y)
    for (int x = x0; x < x1; ++x) pixel(x, y, black);
}

void Canvas::shade(Rect r, int level) {
  const int x0 = r.x < 0 ? 0 : r.x, y0 = r.y < 0 ? 0 : r.y;
  const int x1 = r.right() > _w ? _w : r.right(), y1 = r.bottom() > _h ? _h : r.bottom();
  for (int y = y0; y < y1; ++y)
    for (int x = x0; x < x1; ++x)
      if (dither(x, y, level)) pixel(x, y, true);
}

void Canvas::hline(int x, int y, int w, int thick, bool black) { fill({x, y, w, thick}, black); }
void Canvas::vline(int x, int y, int h, int thick, bool black) { fill({x, y, thick, h}, black); }

void Canvas::frame(Rect r, int t) {
  hline(r.x, r.y, r.w, t);
  hline(r.x, r.bottom() - t, r.w, t);
  vline(r.x, r.y, r.h, t);
  vline(r.right() - t, r.y, r.h, t);
}

void Canvas::dottedFrame(Rect r) {
  for (int x = r.x; x < r.right(); x += 2) {
    pixel(x, r.y);
    pixel(x, r.bottom() - 1);
  }
  for (int y = r.y; y < r.bottom(); y += 2) {
    pixel(r.x, y);
    pixel(r.right() - 1, y);
  }
}

void Canvas::dotted(int x, int y, int w, int step) {
  for (int i = 0; i < w; i += step) pixel(x + i, y);
}

void Canvas::invert(Rect r) {
  for (int y = r.y; y < r.bottom(); ++y)
    for (int x = r.x; x < r.right(); ++x) pixel(x, y, !get(x, y));
}

void Canvas::glyph(const Glyph& g, int x, int baseline, int level, bool white) {
  const int stride = (g.w + 7) / 8;
  const int gx = x + g.xoff, gy = baseline - g.top;
  for (int row = 0; row < g.h; ++row) {
    const uint8_t* bits = g.rows + row * stride;
    for (int col = 0; col < g.w; ++col) {
      if (!(bits[col >> 3] & (0x80 >> (col & 7)))) continue;
      int px = gx + col, py = gy + row;
      if (_turn) {  // along the run and down from the baseline, turned onto the page
        const int along = px, down = py;
        px = _turn > 0 ? _tx - down : _tx + down;
        py = _turn > 0 ? _ty + along : _ty - along;
      }
      if (level < kInk && !dither(px, py, level)) continue;
      pixel(px, py, !white);
    }
  }
}

int Canvas::text(FontId f, int x, int baseline, const char* s, int n, int level, bool white) {
  if (!s) return x;
  const Font font(f);
  const char* end = s + n;
  Glyph g;
  while (uint32_t cp = nextCodepoint(s, end)) {
    if (font.find(cp, g)) {
      glyph(g, x, baseline, level, white);
      x += g.advance;
    } else if ((cp == 0x2019 || cp == 0x2018) && font.find('\'', g)) {
      glyph(g, x, baseline, level, white);
      x += g.advance;
    } else {
      x += font.charWidth(cp);
    }
  }
  return x;
}

void Canvas::textTurned(FontId f, int x, int y, const char* s, bool clockwise) {
  _turn = clockwise ? 1 : -1;
  _tx = x;
  _ty = y;
  text(f, 0, 0, s);
  _turn = 0;
}

int Canvas::text(FontId f, int x, int baseline, const char* s, int level, bool white) {
  return s ? text(f, x, baseline, s, static_cast<int>(strlen(s)), level, white) : x;
}

int Canvas::textRight(FontId f, int right, int baseline, const char* s, int level, bool white) {
  return text(f, right - Font(f).width(s), baseline, s, level, white);
}

int Canvas::textCentered(FontId f, int cx, int baseline, const char* s, int level, bool white) {
  return text(f, cx - Font(f).width(s) / 2, baseline, s, level, white);
}

int Canvas::textFit(FontId f, int x, int baseline, const char* s, int maxWidth, int level, bool white) {
  if (!s) return x;
  const Font font(f);
  const int n = static_cast<int>(strlen(s));
  if (font.width(s) <= maxWidth) return text(f, x, baseline, s, n, level, white);
  const int ell = font.charWidth(0x2026);
  int w = 0, cut = 0;
  const char* p = s;
  const char* end = s + n;
  while (p < end) {
    const char* q = p;
    const uint32_t cp = nextCodepoint(q, end);
    const int cw = font.charWidth(cp);
    if (w + cw + ell > maxWidth) break;
    w += cw;
    p = q;
    cut = static_cast<int>(p - s);
  }
  const int after = text(f, x, baseline, s, cut, level, white);
  return text(f, after, baseline, "\xE2\x80\xA6", 3, level, white);
}

int wrapLine(FontId f, const char* s, int n, int maxWidth, int& next) {
  const Font font(f);
  int w = 0, lastSpace = -1;
  const char* p = s;
  const char* end = s + n;
  while (p < end) {
    const char* q = p;
    const uint32_t cp = nextCodepoint(q, end);
    if (cp == '\n') {
      next = static_cast<int>(q - s);
      return static_cast<int>(p - s);
    }
    const int cw = font.charWidth(cp);
    if (cp == ' ') lastSpace = static_cast<int>(p - s);
    if (w + cw > maxWidth && p > s) {
      if (lastSpace > 0) {
        next = lastSpace + 1;
        return lastSpace;
      }
      next = static_cast<int>(p - s);
      return next;
    }
    w += cw;
    p = q;
  }
  next = n;
  return n;
}

}  // namespace adr
