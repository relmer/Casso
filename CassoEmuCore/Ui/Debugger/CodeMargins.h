#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  CodeMargins
//
//  The margins ahead of a code view's text, measured from the view's left
//  edge and the same in a disassembly view and a source document: the glyph
//  margin, with a breakpoint's dot centered kGlyphCenterDip in, as Visual
//  Studio centers one, and the PC's arrow beside the dot; then, while heat is
//  on, the heat strip.
//
////////////////////////////////////////////////////////////////////////////////

struct CodeMargins
{
    static constexpr float  kGlyphCenterDip = 8.4f;
    static constexpr int    kGlyphDip       = 34;
    static constexpr int    kIconDip        = 16;
    static constexpr int    kHeatDip        = 36;
};
