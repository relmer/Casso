#pragma once

#include "Pch.h"
#include "Machines/Apple2/Common/CharacterRomData.h"


class IFileSystem;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleTextFont
//
//  The Apple II character set an Apple text preview is drawn in, from the
//  video ROM Casso installs.
//
//  THE ENHANCED //E ROM COMES FIRST, for its lowercase; then any other
//  machine's, a ][ or ][+ ROM drawing lowercase as uppercase as those
//  machines do; then the built-in table, so a preview never fails for lack
//  of a ROM.
//
//  A character is drawn from the primary set's normal half, the high bit
//  set, and anything outside printable ASCII is drawn as a question mark.
//
////////////////////////////////////////////////////////////////////////////////

class AppleTextFont
{
public:
    //  A glyph's dots, bit 0 of a row the leftmost.
    static constexpr int  kDotsWide = 7;
    static constexpr int  kDotsHigh = 8;

    //  The ROMs looked for under Casso's asset directory, in order.
    static constexpr const wchar_t *  kRomPaths[] =
    {
        L"Machines\\Apple2eEnhanced\\Apple2eEnhanced_Video.rom",
        L"Machines\\Apple2e\\Apple2e_Video.rom",
        L"Machines\\Apple2c\\Apple2c_Video.rom",
        L"Machines\\Apple2Plus\\Apple2_Video.rom",
        L"Machines\\Apple2\\Apple2_Video.rom",
    };

    //  The first ROM found under `assetDir`, or the built-in table when none is.
    void  Load (IFileSystem & fs, const std::wstring & assetDir);

    bool                  HasLowercase () const { return m_lowercase; }
    const std::wstring  & GetSource    () const { return m_source; }

    //  The primary-set code a character is drawn with.
    Byte  GetCode (wchar_t ch) const;

    //  One row of the dots a character is drawn with, row 0 the top.
    Byte  GetGlyphRow (wchar_t ch, int row) const { return m_rom.GetGlyphRow (GetCode (ch), row); }

private:
    static constexpr size_t  s_kLowercaseRomBytes = 4096;

    CharacterRomData  m_rom;
    bool              m_lowercase = false;
    std::wstring      m_source;
};
