// fonts.h — the bitmap strikes (src/generated/fonts_data.cpp, from tools/gen_fonts.py).
//
// Times, the Adobe X11 100 dpi strikes: 25 px (18 pt at 100 dpi, CrossPoint's "Times 12") for the
// body and everything else, 34 px for titles, and a 20 px italic for a longer caption only. UW
// ttyp0, a monospace, at 22 and 30 px for the map and the ascent.
#pragma once

#include <cstddef>
#include <cstdint>

namespace adr {

enum FontId : uint8_t {
  F_R25, F_R34,
  F_I20, F_I25,
  F_B25, F_B34,
  F_M22, F_MB22, F_MB30,
  F_COUNT
};

struct GlyphRec {
  uint32_t offset;  // into the strike's bitmap: h rows of (w + 7) / 8 bytes, MSB first, set = ink
  uint8_t w, h;
  int8_t xoff, top;  // from the pen: right, and up from the baseline
  uint8_t advance;
};

struct FontData {
  int8_t ascent, descent;
  uint8_t pixelSize, space;
  uint16_t count;
  const uint16_t* cps;  // sorted
  const GlyphRec* glyphs;
  const uint8_t* bitmap;
};
extern const FontData kFonts[F_COUNT];

struct Glyph {
  const uint8_t* rows = nullptr;
  uint8_t w = 0, h = 0;
  int8_t xoff = 0, top = 0;
  uint8_t advance = 0;
};

class Font {
 public:
  explicit Font(FontId id) : _d(&kFonts[id]) {}
  bool find(uint32_t cp, Glyph& g) const;
  int ascent() const { return _d->ascent; }
  int descent() const { return _d->descent; }
  int lineHeight() const { return _d->ascent + _d->descent; }
  int pixelSize() const { return _d->pixelSize; }
  // The advance of a run of UTF-8 text.
  int width(const char* s, size_t n = SIZE_MAX) const;
  int charWidth(uint32_t cp) const;

 private:
  const FontData* _d;
};

// UTF-8: the next codepoint at s (advancing it), 0 at the end.
uint32_t nextCodepoint(const char*& s, const char* end);

}  // namespace adr
