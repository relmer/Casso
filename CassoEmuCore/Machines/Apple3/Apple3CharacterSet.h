#pragma once

#include "Pch.h"

#include "Core/IMachineState.h"
#include "IGlyphSource.h"

class Prng;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3CharacterSet
//
//  The Apple ///'s character generator RAM: 128 characters of 8 rows, 1,024
//  bytes, with nothing in it but what software loads. Software loads it by
//  putting eight characters in the text pages' unused bytes and turning the
//  download on with $C0DB. During each vertical blanking period while the
//  download is on, the video circuitry reads, for each cell c (0-7) and row
//  r (0-7), a character code from the system bank at page
//  $08 + (c >> 2) * 2 + (r >= 4), offset $78, $7C, $F8 or $FC (by r & 3)
//  plus c & 3, and the row's dots from the same address with the page XOR
//  $0C, and writes the dots to that character's row. $C0DA turns the
//  download off.
//
//  The RAM is static, so a reset doesn't change it; at power on it has
//  random contents.
//
////////////////////////////////////////////////////////////////////////////////

class Apple3CharacterSet : public IGlyphSource, public IMachineState
{
public:
    static constexpr Word      kDownloadOff   = 0xC0DA;
    static constexpr Word      kDownloadOn    = 0xC0DB;
    static constexpr int       kCellCount     = 8;
    static constexpr size_t    kSize          = (size_t) kCharacterCount * kRowCount;

    static constexpr uint32_t  kStateTag     = IMachineState::MakeTag ('A', '3', 'C', 'S');
    static constexpr uint16_t  kStateVersion = 1;

    using SystemReader = std::function<Byte (Word)>;

    Byte     GetGlyphRow        (Byte code, int row) const override;

    void     PowerOn            (Prng & prng);
    void     SetDownloadEnabled (bool enabled) { m_isDownloading = enabled; }
    bool     IsDownloadEnabled  () const       { return m_isDownloading; }

    //  Runs one vertical blanking period's download, if it is on.
    void     OnVerticalBlank    (const SystemReader & readSystem);

    static Word  GetCodeAddress (int cell, int row);
    static Word  GetDotsAddress (int cell, int row);

    HRESULT  SaveState          (StateWriter & writer) const override;
    HRESULT  LoadState          (StateReader & reader) override;

private:
    std::array<Byte, kSize>  m_glyphs        = {};
    bool                     m_isDownloading = false;
};
