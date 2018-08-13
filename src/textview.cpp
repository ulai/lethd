//
//  Copyright (c) 2016-2018 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Lukas Zeller <luz@plan44.ch>
//
//  This file is part of pixelboardd.
//
//  pixelboardd is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  pixelboardd is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with pixelboardd. If not, see <http://www.gnu.org/licenses/>.
//

#include "textview.hpp"

using namespace p44;



// MARK ====== Simple 7-pixel height dot matrix font
// Note: the font is derived from a monospaced 7*5 pixel font, but has been adjusted a bit
//       to get rendered proportionally (variable character width, e.g. "!" has width 1, whereas "m" has 7)
//       In the fontGlyphs table below, every char has a number of pixel colums it consists of, and then the
//       actual column values encoded as a string.

typedef struct {
  uint8_t width;
  const char *cols;
} glyph_t;

const int numGlyphs = 102; // 96 ASCII 0x20..0x7F plus 6 ÄÖÜäöü
const int rowsPerGlyph = 7;

static const glyph_t fontGlyphs[numGlyphs] = {
  { 5, "\x00\x00\x00\x00\x00" },  //   0x20 (0)
  { 1, "\x5f" },                  // ! 0x21 (1)
  { 3, "\x03\x00\x03" },          // " 0x22 (2)
  { 5, "\x28\x7c\x28\x7c\x28" },  // # 0x23 (3)
  { 5, "\x24\x2a\x7f\x2a\x12" },  // $ 0x24 (4)
  { 5, "\x4c\x2c\x10\x68\x64" },  // % 0x25 (5)
  { 5, "\x30\x4e\x55\x22\x40" },  // & 0x26 (6)
  { 1, "\x01" },                  // ' 0x27 (7)
  { 3, "\x1c\x22\x41" },          // ( 0x28 (8)
  { 3, "\x41\x22\x1c" },          // ) 0x29 (9)
  { 5, "\x2A\x14\x7F\x14\x2A" },  // * 0x2A (10)
  { 5, "\x08\x08\x3e\x08\x08" },  // + 0x2B (11)
  { 2, "\x50\x30" },              // , 0x2C (12)
  { 5, "\x08\x08\x08\x08\x08" },  // - 0x2D (13)
  { 2, "\x60\x60" },              // . 0x2E (14)
  { 5, "\x40\x20\x10\x08\x04" },  // / 0x2F (15)

  { 5, "\x3e\x51\x49\x45\x3e" },  // 0 0x30 (0)
  { 3, "\x42\x7f\x40" },          // 1 0x31 (1)
  { 5, "\x62\x51\x49\x49\x46" },  // 2 0x32 (2)
  { 5, "\x22\x41\x49\x49\x36" },  // 3 0x33 (3)
  { 5, "\x0c\x0a\x09\x7f\x08" },  // 4 0x34 (4)
  { 5, "\x4f\x49\x49\x49\x31" },  // 5 0x35 (5)
  { 5, "\x3e\x49\x49\x49\x32" },  // 6 0x36 (6)
  { 5, "\x03\x01\x71\x09\x07" },  // 7 0x37 (7)
  { 5, "\x36\x49\x49\x49\x36" },  // 8 0x38 (8)
  { 5, "\x26\x49\x49\x49\x3e" },  // 9 0x39 (9)
  { 2, "\x66\x66" },              // : 0x3A (10)
  { 2, "\x56\x36" },              // ; 0x3B (11)
  { 4, "\x08\x14\x22\x41" },      // < 0x3C (12)
  { 4, "\x24\x24\x24\x24" },      // = 0x3D (13)
  { 4, "\x41\x22\x14\x08" },      // > 0x3E (14)
  { 5, "\x02\x01\x59\x09\x06" },  // ? 0x3F (15)

  { 5, "\x3e\x41\x5d\x55\x5e" },  // @ 0x40 (0)
  { 5, "\x7c\x0a\x09\x0a\x7c" },  // A 0x41 (1)
  { 5, "\x7f\x49\x49\x49\x36" },  // B 0x42 (2)
  { 5, "\x3e\x41\x41\x41\x22" },  // C 0x43 (3)
  { 5, "\x7f\x41\x41\x22\x1c" },  // D 0x44 (4)
  { 5, "\x7f\x49\x49\x41\x41" },  // E 0x45 (5)
  { 5, "\x7f\x09\x09\x01\x01" },  // F 0x46 (6)
  { 5, "\x3e\x41\x49\x49\x7a" },  // G 0x47 (7)
  { 5, "\x7f\x08\x08\x08\x7f" },  // H 0x48 (8)
  { 3, "\x41\x7f\x41" },          // I 0x49 (9)
  { 5, "\x30\x40\x40\x40\x3f" },  // J 0x4A (10)
  { 5, "\x7f\x08\x0c\x12\x61" },  // K 0x4B (11)
  { 5, "\x7f\x40\x40\x40\x40" },  // L 0x4C (12)
  { 7, "\x7f\x02\x04\x0c\x04\x02\x7f" },  // M 0x4D (13)
  { 5, "\x7f\x02\x04\x08\x7f" },  // N 0x4E (14)
  { 5, "\x3e\x41\x41\x41\x3e" },  // O 0x4F (15)

  { 5, "\x7f\x09\x09\x09\x06" },  // P 0x50 (0)
  { 5, "\x3e\x41\x51\x61\x7e" },  // Q 0x51 (1)
  { 5, "\x7f\x09\x09\x09\x76" },  // R 0x52 (2)
  { 5, "\x26\x49\x49\x49\x32" },  // S 0x53 (3)
  { 5, "\x01\x01\x7f\x01\x01" },  // T 0x54 (4)
  { 5, "\x3f\x40\x40\x40\x3f" },  // U 0x55 (5)
  { 5, "\x1f\x20\x40\x20\x1f" },  // V 0x56 (6)
  { 9, "\x0f\x30\x40\x30\x0C\x30\x40\x30\x0f" },  // W 0x57 (7)
  { 5, "\x63\x14\x08\x14\x63" },  // X 0x58 (8)
  { 5, "\x03\x04\x78\x04\x03" },  // Y 0x59 (9)
  { 5, "\x61\x51\x49\x45\x43" },  // Z 0x5A (10)
  { 3, "\x7f\x41\x41" },          // [ 0x5B (11)
  { 5, "\x04\x08\x10\x20\x40" },  // \ 0x5C (12)
  { 3, "\x41\x41\x7f" },          // ] 0x5D (13)
  { 4, "\x04\x02\x01\x02" },      // ^ 0x5E (14)
  { 5, "\x40\x40\x40\x40\x40" },  // _ 0x5F (15)

  { 2, "\x01\x02" },              // ` 0x60 (0)
  { 5, "\x20\x54\x54\x54\x78" },  // a 0x61 (1)
  { 5, "\x7f\x44\x44\x44\x38" },  // b 0x62 (2)
  { 5, "\x38\x44\x44\x44\x08" },  // c 0x63 (3)
  { 5, "\x38\x44\x44\x44\x7f" },  // d 0x64 (4)
  { 5, "\x38\x54\x54\x54\x18" },  // e 0x65 (5)
  { 5, "\x08\x7e\x09\x09\x02" },  // f 0x66 (6)
  { 5, "\x48\x54\x54\x54\x38" },  // g 0x67 (7)
  { 5, "\x7f\x08\x08\x08\x70" },  // h 0x68 (8)
  { 3, "\x48\x7a\x40" },          // i 0x69 (9)
  { 5, "\x20\x40\x40\x48\x3a" },  // j 0x6A (10)
  { 4, "\x7f\x10\x28\x44" },      // k 0x6B (11)
  { 3, "\x3f\x40\x40" },          // l 0x6C (12)
  { 7, "\x7c\x04\x04\x38\x04\x04\x78" },  // m 0x6D (13)
  { 5, "\x7c\x04\x04\x04\x78" },  // n 0x6E (14)
  { 5, "\x38\x44\x44\x44\x38" },  // o 0x6F (15)

  { 5, "\x7c\x14\x14\x14\x08" },  // p 0x70 (0)
  { 5, "\x08\x14\x14\x7c\x40" },  // q 0x71 (1)
  { 5, "\x7c\x04\x04\x04\x08" },  // r 0x72 (2)
  { 5, "\x48\x54\x54\x54\x24" },  // s 0x73 (3)
  { 5, "\x04\x04\x7f\x44\x44" },  // t 0x74 (4)
  { 5, "\x3c\x40\x40\x40\x7c" },  // u 0x75 (5)
  { 5, "\x1c\x20\x40\x20\x1c" },  // v 0x76 (6)
  { 7, "\x7c\x40\x40\x38\x40\x40\x7c" },  // w 0x77 (7)
  { 5, "\x44\x28\x10\x28\x44" },  // x 0x78 (8)
  { 5, "\x0c\x50\x50\x50\x3c" },  // y 0x79 (9)
  { 5, "\x44\x64\x54\x4c\x44" },  // z 0x7A (10)
  { 3, "\x08\x36\x41" },          // { 0x7B (11)
  { 1, "\x7f" },                  // | 0x7C (12)
  { 3, "\x41\x36\x08" },          // } 0x7D (13)
  { 4, "\x04\x02\x04\x08" },      // ~ 0x7E (14)
  { 5, "\x7F\x41\x41\x41\x7F" },  //   0x7F (15)

  { 5, "\x7D\x0a\x09\x0a\x7D" },  // Ä 0x80 (0)
  { 5, "\x3D\x42\x42\x42\x3D" },  // Ö 0x81 (1)
  { 5, "\x3D\x40\x40\x40\x3D" },  // Ü 0x82 (2)
  { 5, "\x20\x55\x54\x55\x78" },  // ä 0x83 (3)
  { 5, "\x38\x45\x44\x45\x38" },  // ö 0x84 (4)
  { 5, "\x3c\x41\x40\x41\x7c" },  // ü 0x85 (5)
};


// MARK: ===== TextView


TextView::TextView()
{
  textSpacing = 2;
  textColor.r = 255;
  textColor.g = 255;
  textColor.b = 255;
  textColor.a = 255;
}


void TextView::clear()
{
  setText("");
}


TextView::~TextView()
{
}


void TextView::setText(const string aText)
{
  text = aText;
  renderText();
}


void TextView::renderText()
{
  // convert to glyph indices
  string glyphs;
  size_t i = 0;
  while (i<text.size()) {
    uint8_t textbyte = text[i++];
    unsigned char c = 0x7F; // placeholder for unknown
    // Ä = C3 84
    // Ö = C3 96
    // Ü = C3 9C
    // ä = C3 A4
    // ö = C3 B6
    // ü = C3 BC
    if (textbyte==0xC3) {
      if (i>=text.size()) break; // end of text
      switch ((uint8_t)text[i++]) {
        case 0x84: c = 0x80; break; // Ä
        case 0x96: c = 0x81; break; // Ö
        case 0x9C: c = 0x82; break; // Ü
        case 0xA4: c = 0x83; break; // ä
        case 0xB6: c = 0x84; break; // ö
        case 0xBC: c = 0x85; break; // ü
      }
    }
    else {
      c = textbyte;
    }
    // convert to glyph number
    if (c<0x20 || c>=0x20+numGlyphs) {
      c = 0x7F; // placeholder for unknown
    }
    c -= 0x20;
    glyphs += c;
  }
  // now render glyphs
  textPixelCols.clear();
  for (size_t i = 0; i<glyphs.size(); ++i) {
    const glyph_t &g = fontGlyphs[glyphs[i]];
    for (int j = 0; j<g.width; ++j) {
      textPixelCols.append(1, g.cols[j]);
    }
    for (int j = 0; j<textSpacing; ++j) {
      textPixelCols.append(1, 0);
    }
  }
  // set content size
  setContentSize((int)textPixelCols.size(), rowsPerGlyph);
  makeDirty();
}


PixelColor TextView::contentColorAt(int aX, int aY)
{
  if (isInContentSize(aX, aY)) {
    uint8_t col = textPixelCols[aX];
    if (col & (1<<(rowsPerGlyph-1-aY))) {
      return textColor;
    }
    else {
      return inherited::contentColorAt(aX, aY);;
    }
  }
  else {
    return inherited::contentColorAt(aX, aY);
  }
}

