#include "Pch.h"

#include "FileTypes/MagicWindowDocument.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MagicWindowDocument::Matches
//
////////////////////////////////////////////////////////////////////////////////

bool MagicWindowDocument::Matches (const FileFacts & facts, std::span<const Byte> bytes)
{
    return facts.IsDos33() && facts.type == kDosBinary && facts.IsNameEnding (".MW")
        && bytes.size() >= kHeaderBytes && bytes.size() <= kMaxBytes && bytes[0] == 0x8D && bytes[2] >= 0x20;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MagicWindowDocument::Decode
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MagicWindowDocument::Decode (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    std::wstring  line;



    UNREFERENCED_PARAMETER (facts);
    UNREFERENCED_PARAMETER (options);
    UNREFERENCED_PARAMETER (outError);

    for (Byte b : bytes.subspan (std::min (kHeaderBytes, bytes.size())))
    {
        Byte  low = (Byte) (b & 0x7F);

        if (low == kReturn)
        {
            outFile.lines.push_back (std::move (line));
            line.clear();
        }
        else if (low >= 0x20 && low != 0x7F)
        {
            line += (wchar_t) low;
        }
    }

    if (!line.empty())
    {
        outFile.lines.push_back (std::move (line));
    }

    return S_OK;
}
