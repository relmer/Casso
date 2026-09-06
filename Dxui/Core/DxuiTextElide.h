#pragma once

#include "Pch.h"


class IDxuiTextRenderer;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiElide
//
//  Which END of a string to sacrifice when it will not fit.
//
//  Tail      keep the beginning: "A very long lab..."
//            Right for a label, where the first words identify the thing.
//
//  PathHead  keep the END: "...\Pictures\Casso Screenshots"
//            Right for a path, where the leaf identifies the thing and the
//            head is what every path on the machine has in common. The cut
//            lands on a separator, because a component sliced in half looks
//            like a real folder and is not.
//
////////////////////////////////////////////////////////////////////////////////

enum class DxuiElide
{
    None,
    Tail,
    PathHead,
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextElide
//
//  Fits a string to a WIDTH, not to a character count.
//
//  Characters are the wrong unit and only look like the right one: in a
//  proportional face the same count of them can differ in width by a factor
//  of three, so a character budget either overflows its box on wide text or
//  wastes half of it on narrow text. The renderer is the only thing that
//  knows how wide a string actually is, so the fitting happens where it is --
//  at paint time, in the widget, against the box the text has to occupy.
//
//  Measurement is the expensive part, so the search is binary: log(n) measures
//  rather than one per character.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiTextElide
{
public:
    static std::wstring  ToWidth (IDxuiTextRenderer  & text,
                                  const std::wstring & value,
                                  float                fontDip,
                                  const wchar_t      * fontFamily,
                                  float                maxWidthDip,
                                  DxuiElide            mode);

private:
    static std::wstring  ElideTail     (IDxuiTextRenderer  & text,
                                        const std::wstring & value,
                                        float                fontDip,
                                        const wchar_t      * fontFamily,
                                        float                maxWidthDip);

    static std::wstring  ElidePathHead (IDxuiTextRenderer  & text,
                                        const std::wstring & value,
                                        float                fontDip,
                                        const wchar_t      * fontFamily,
                                        float                maxWidthDip);

    static bool  Fits (IDxuiTextRenderer  & text,
                       const std::wstring & candidate,
                       float                fontDip,
                       const wchar_t      * fontFamily,
                       float                maxWidthDip);
};
