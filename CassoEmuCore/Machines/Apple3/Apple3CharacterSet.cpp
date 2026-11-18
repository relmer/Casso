#include "Pch.h"

#include "Apple3CharacterSet.h"
#include "Core/Prng.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"





static constexpr Byte  s_kCodeMask       = 0x7F;
static constexpr int   s_kFirstCodePage  = 0x08;
static constexpr int   s_kCellsPerGroup  = 4;
static constexpr int   s_kRowsPerHalf    = 4;
static constexpr int   s_kPagesPerGroup  = 2;
static constexpr Byte  s_kDotsPageXor    = 0x0C;
static constexpr int   s_kPageShift      = 8;

static constexpr std::array<Byte, s_kRowsPerHalf>  s_kRowOffsets = { 0x78, 0x7C, 0xF8, 0xFC };





////////////////////////////////////////////////////////////////////////////////
//
//  GetGlyphRow
//
////////////////////////////////////////////////////////////////////////////////

Byte Apple3CharacterSet::GetGlyphRow (Byte code, int row) const
{
    size_t  index = (size_t) (code & s_kCodeMask) * kRowCount + (size_t) (row & (kRowCount - 1));



    return m_glyphs[index];
}





////////////////////////////////////////////////////////////////////////////////
//
//  PowerOn
//
////////////////////////////////////////////////////////////////////////////////

void Apple3CharacterSet::PowerOn (Prng & prng)
{
    prng.Fill (m_glyphs.data(), m_glyphs.size());

    m_isDownloading = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCodeAddress
//
//  The system bank address of a download cell's character code for a row.
//
////////////////////////////////////////////////////////////////////////////////

Word Apple3CharacterSet::GetCodeAddress (int cell, int row)
{
    int  page   = s_kFirstCodePage + (cell / s_kCellsPerGroup) * s_kPagesPerGroup + ((row >= s_kRowsPerHalf) ? 1 : 0);
    int  offset = s_kRowOffsets[(size_t) (row % s_kRowsPerHalf)] + (cell % s_kCellsPerGroup);



    return (Word) ((page << s_kPageShift) | offset);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDotsAddress
//
////////////////////////////////////////////////////////////////////////////////

Word Apple3CharacterSet::GetDotsAddress (int cell, int row)
{
    return (Word) (GetCodeAddress (cell, row) ^ (s_kDotsPageXor << s_kPageShift));
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnVerticalBlank
//
////////////////////////////////////////////////////////////////////////////////

void Apple3CharacterSet::OnVerticalBlank (const SystemReader & readSystem)
{
    if (!m_isDownloading)
    {
        return;
    }

    for (int cell = 0; cell < kCellCount; cell++)
    {
        for (int row = 0; row < kRowCount; row++)
        {
            Byte  code = (Byte) (readSystem (GetCodeAddress (cell, row)) & s_kCodeMask);



            m_glyphs[(size_t) code * kRowCount + (size_t) row] = readSystem (GetDotsAddress (cell, row));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SaveState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Apple3CharacterSet::SaveState (StateWriter & writer) const
{
    writer.BeginSection (kStateTag, kStateVersion);

    writer.WriteBytes (m_glyphs.data(), m_glyphs.size());
    writer.WriteBool  (m_isDownloading);

    return writer.EndSection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Apple3CharacterSet::LoadState (StateReader & reader)
{
    HRESULT   hr      = S_OK;
    uint16_t  version = 0;



    hr = reader.BeginSection (kStateTag, kStateVersion, version);
    CHR (hr);

    reader.ReadBytes (m_glyphs.data(), m_glyphs.size());
    reader.ReadBool  (m_isDownloading);

    hr = reader.EndSection();
    CHR (hr);

Error:
    return hr;
}
