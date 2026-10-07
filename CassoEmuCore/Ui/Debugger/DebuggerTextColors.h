#pragma once

#include "Ui/Debugger/SourceSyntax.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerTextColors
//
//  The colors the debugger draws text in, for a theme's background: Visual
//  Studio's code colors for the background's darkness, the theme's own text
//  and result colors, and a changed value's red. Every one is moved toward
//  black on a light background, or toward white on a dark one, until it
//  reaches the WCAG AA ratio for text, so no theme leaves text too faint to
//  read.
//
////////////////////////////////////////////////////////////////////////////////

class DebuggerTextColors
{
public:
    static constexpr float  s_kMinTextContrast     = 4.5f;

    //  On a light page: how far the darkest row tint is toward black, and how
    //  far plain text is moved toward the page so it is not stark black.
    static constexpr float  s_kLightRowShade       = 0.15f;
    static constexpr float  s_kLightTextSoftening  = 0.25f;

    //  How far each row fill is mixed from the page toward its hue: the PC's
    //  dark yellow, a branch destination's accent, a navigated row's green.
    static constexpr float  s_kDarkPcRowMix        = 0.22f;
    static constexpr float  s_kLightPcRowMix       = 0.55f;
    static constexpr float  s_kDarkTargetRowMix    = 0.16f;
    static constexpr float  s_kLightTargetRowMix   = 0.20f;

    //  How far the heat map's untouched address is mixed from the page toward
    //  white on a dark page, or toward black on a light one.
    static constexpr float  s_kDarkHeatColdMix     = 0.14f;
    static constexpr float  s_kLightHeatColdMix    = 0.10f;

    struct Set
    {
        SourceSyntax::Colors  syntax;
        uint32_t              annotation     = 0;
        uint32_t              changed        = 0;

        //  A memory window's ROM bytes and I/O bytes.
        uint32_t              rom            = 0;
        uint32_t              io             = 0;
        uint32_t              result         = 0;
        uint32_t              muted          = 0;

        //  An operand's address, set apart from an immediate's number.
        uint32_t              operandAddress = 0;

        //  The opaque row fills every color above reads on: the PC's row, a
        //  branch's destination, and a row another pane brought into view.
        uint32_t              pcRow          = 0;
        uint32_t              targetRow      = 0;
        uint32_t              navigatedRow   = 0;

        //  The memory map's sources, which a memory window's language card
        //  and aux RAM boxes share.
        uint32_t              mapMain        = 0;
        uint32_t              mapAux         = 0;
        uint32_t              mapLcBank1     = 0;
        uint32_t              mapLcBank2     = 0;
        uint32_t              mapRom         = 0;
        uint32_t              mapSlotRom     = 0;
        uint32_t              mapIo          = 0;

        //  The heat map's untouched address: a dark gray on a dark page and a
        //  light gray on a light one, so every cell stands apart from the
        //  page between them.
        uint32_t              heatCold       = 0;

        //  The heat map's reads before written, a magenta apart from every
        //  other kind's color, and its writes that changed their byte, an
        //  amber apart from the writes' red.
        uint32_t              heatUnwritten  = 0;
        uint32_t              heatChanged    = 0;
    };

    //  Make with every one of a theme's own colors, its memory map's among
    //  them, each falling back where the theme gives none.
    static Set       MakeFor     (const DxuiTheme & theme);

    //  `resultText`, `changedText`, `romText` or `ioText` of zero means the
    //  theme gives no color of its own for it.
    static Set       Make        (uint32_t background,
                                  uint32_t foreground,
                                  uint32_t muted,
                                  uint32_t resultText,
                                  uint32_t accent      = 0xFF3C8CE6,
                                  uint32_t changedText = 0,
                                  uint32_t romText     = 0,
                                  uint32_t ioText      = 0);

    //  The changed color on a row filled with `fill`, moved until it reads
    //  there; a zero `fill` is the page, where it is `set.changed` itself.
    static uint32_t  GetChangedOn (const Set & set, uint32_t fill);

    static bool      IsDark      (uint32_t background);

    //  The darkest a row is tinted on a light page, which every
    //  color must read against.
    static uint32_t  GetRowShade (uint32_t background);

    //  `argb`, or the nearest color along the way to black or white that
    //  reaches s_kMinTextContrast against `background`.
    static uint32_t  GetReadable (uint32_t argb, uint32_t background);

    //  A disassembled instruction's colored runs, as a list cell's color
    //  ranges: first, end and color. A number after # is an immediate in the
    //  number color; any other number is an address in the operand color.
    using Ranges = std::vector<std::tuple<int, int, uint32_t>>;

    static Ranges    GetInstructionRanges (const std::wstring & instruction, const Set & set);

    //  The same against several backgrounds at once, to `minRatio`.
    static uint32_t  GetReadable (uint32_t argb, const std::vector<uint32_t> & backgrounds, float minRatio = s_kMinTextContrast);

private:
    static float     GetLowestRatio (uint32_t argb, const std::vector<uint32_t> & backgrounds);
};
