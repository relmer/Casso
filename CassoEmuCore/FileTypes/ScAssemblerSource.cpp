#include "Pch.h"

#include "FileTypes/ScAssemblerSource.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ScAssemblerSource::Matches
//
////////////////////////////////////////////////////////////////////////////////

bool ScAssemblerSource::Matches (const FileFacts & facts, std::span<const Byte> bytes)
{
    return facts.IsDos33() && facts.type == kDosInteger && Check (bytes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScAssemblerSource::Check
//
//  Every line's length lands on the next and its bytes are valid, which an
//  Integer BASIC program's tokens below $20 never are.
//
////////////////////////////////////////////////////////////////////////////////

bool ScAssemblerSource::Check (std::span<const Byte> bytes)
{
    size_t  at = 0;



    if (bytes.size() < 4)
    {
        return false;
    }

    while (at < bytes.size())
    {
        size_t  length = bytes[at];

        if (length < 4 || at + length > bytes.size() || bytes[at + length - 1] != 0)
        {
            return false;
        }

        for (size_t b = at + 3; b < at + length - 1; b++)
        {
            if (bytes[b] < 0x20 || (bytes[b] > kRepeat))
            {
                return false;
            }

            if (bytes[b] == kRepeat)
            {
                b += 2;
            }
        }

        at += length;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScAssemblerSource::Decode
//
//  The line number, four digits, then the text.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ScAssemblerSource::Decode (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    HRESULT  hr = S_OK;
    bool     ok = Check (bytes);
    size_t   at = 0;



    UNREFERENCED_PARAMETER (facts);
    UNREFERENCED_PARAMETER (options);

    CBRFEx (ok, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = L"The file isn't S-C Assembler source.");

    outFile.kind = DecodedFile::Kind::Lines;

    while (at < bytes.size())
    {
        size_t        length = bytes[at];
        Word          number = (Word) (bytes[at + 1] | (bytes[at + 2] << 8));
        std::wstring  line   = std::format (L"{:04} ", number);

        for (size_t b = at + 3; b < at + length - 1; b++)
        {
            Byte  value = bytes[b];

            if (value == kRepeat && b + 2 < at + length)
            {
                line.append (bytes[b + 1], (wchar_t) (bytes[b + 2] & 0x7F));
                b += 2;
            }
            else if (value >= 0x80)
            {
                line.append ((size_t) (value - 0x80), L' ');
            }
            else
            {
                line += (wchar_t) value;
            }
        }

        outFile.lines.push_back (line);
        at += length;
    }

Error:
    return hr;
}
