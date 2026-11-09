#include "Pch.h"

#include "FileTypes/RandomText.h"
#include "FileTypes/TextCharset.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RandomText::Matches
//
//  A ProDOS text file with a record length, or a DOS 3.3 text file whose
//  entries start at a fixed spacing.
//
////////////////////////////////////////////////////////////////////////////////

bool RandomText::Matches (const FileFacts & facts, std::span<const Byte> bytes)
{
    if (facts.HasProDosType (kProDosText))
    {
        return facts.hasAuxType && facts.auxType > 1 && !facts.IsNameEnding (".S") && !facts.IsNameEnding (".VC");
    }

    return facts.IsDos33() && facts.type == kDosText && !facts.IsNameEnding (".VC") && FindRecordLength (bytes) > 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RandomText::FindRecordLength
//
//  The greatest common divisor of the offsets each run of text starts at
//  after a NUL; a sequential file doesn't have one, so the length is 0.
//
////////////////////////////////////////////////////////////////////////////////

size_t RandomText::FindRecordLength (std::span<const Byte> bytes)
{
    size_t  length = 0;
    bool    inGap  = false;



    for (size_t at = 0; at < bytes.size(); at++)
    {
        if (bytes[at] == 0)
        {
            inGap = true;
            continue;
        }

        if (inGap)
        {
            length = std::gcd (length, at);
            inGap  = false;
        }
    }

    return (length > 1) ? length : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RandomText::Decode
//
//  A row for each record with text, numbered from 0, as BASIC's R
//  parameter counts them.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RandomText::Decode (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    HRESULT  hr      = S_OK;
    size_t   length  = options.recordLength;
    size_t   fields  = 0;
    bool     usable  = false;



    if (length == 0)
    {
        length = facts.IsProDos() ? facts.auxType : FindRecordLength (bytes);
    }

    usable = length > 0;
    CBRFEx (usable, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = L"The file's record length isn't known.");

    outFile.kind = DecodedFile::Kind::Grid;

    for (size_t start = 0; start < bytes.size(); start += length)
    {
        std::span<const Byte>      record = bytes.subspan (start, std::min (length, bytes.size() - start));
        std::vector<std::wstring>  row    = { std::to_wstring (start / length) };
        size_t                     end    = 0;

        while (end < record.size() && record[end] != 0)
        {
            end++;
        }

        if (end == 0)
        {
            continue;
        }

        for (const std::wstring & field : TextCharset::DecodeLines (record.first (end), options.charset))
        {
            row.push_back (field);
        }

        fields = std::max (fields, row.size() - 1);
        outFile.grid.rows.push_back (std::move (row));
    }

    outFile.grid.columns.push_back (L"Record");

    for (size_t field = 1; field <= fields; field++)
    {
        outFile.grid.columns.push_back (std::format (L"Field {}", field));
    }

Error:
    return hr;
}
