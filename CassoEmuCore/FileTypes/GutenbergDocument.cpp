#include "Pch.h"

#include "FileTypes/GutenbergDocument.h"
#include "Machines/Apple2/Common/GutenbergVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergDocument::Matches
//
////////////////////////////////////////////////////////////////////////////////

bool GutenbergDocument::Matches (const FileFacts & facts, std::span<const Byte> bytes)
{
    return facts.volume == VolumeKind::Gutenberg && GutenbergVolume::IsDocumentType (facts.type) && !bytes.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergDocument::Decode
//
////////////////////////////////////////////////////////////////////////////////

HRESULT GutenbergDocument::Decode (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    std::wstring  line;
    bool          hasAlternate = false;



    UNREFERENCED_PARAMETER (facts);
    UNREFERENCED_PARAMETER (options);
    UNREFERENCED_PARAMETER (outError);

    for (Byte b : bytes)
    {
        if (b == kEnd)
        {
            break;
        }

        if (b == kReturn)
        {
            outFile.lines.push_back (std::move (line));
            line.clear();
        }
        else if (b < kHighBit)
        {
            hasAlternate = true;
        }
        else if (b >= kFirstPrinted && b != kRubout)
        {
            line += (wchar_t) (b & 0x7F);
        }
    }

    if (!line.empty())
    {
        outFile.lines.push_back (std::move (line));
    }

    if (hasAlternate)
    {
        outFile.notes.push_back (L"Characters of the alternate set are dropped.");
    }

    return S_OK;
}
