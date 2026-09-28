#include "fonts.h"

namespace adr {

uint32_t nextCodepoint(const char*& s, const char* end) {
  if (s >= end || !*s) return 0;
  const uint8_t c = static_cast<uint8_t>(*s++);
  if (c < 0x80) return c;
  int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
  uint32_t cp = c & (0x3F >> extra);
  while (extra-- > 0 && s < end && (static_cast<uint8_t>(*s) & 0xC0) == 0x80) cp = (cp << 6) | (static_cast<uint8_t>(*s++) & 0x3F);
  return cp;
}

bool Font::find(uint32_t cp, Glyph& g) const {
  int lo = 0, hi = _d->count - 1;
  while (lo <= hi) {
    const int mid = (lo + hi) / 2;
    const uint32_t c = _d->cps[mid];
    if (c == cp) {
      const GlyphRec& r = _d->glyphs[mid];
      g.rows = _d->bitmap + r.offset;
      g.w = r.w;
      g.h = r.h;
      g.xoff = r.xoff;
      g.top = r.top;
      g.advance = r.advance;
      return true;
    }
    if (c < cp) lo = mid + 1;
    else hi = mid - 1;
  }
  return false;
}

int Font::charWidth(uint32_t cp) const {
  Glyph g;
  if (find(cp, g)) return g.advance;
  if (cp == 0x2019 || cp == 0x2018) return charWidth('\'');
  return _d->space;
}

int Font::width(const char* s, size_t n) const {
  const char* end = n == SIZE_MAX ? s + __builtin_strlen(s) : s + n;
  int w = 0;
  while (uint32_t cp = nextCodepoint(s, end)) w += charWidth(cp);
  return w;
}

}  // namespace adr
