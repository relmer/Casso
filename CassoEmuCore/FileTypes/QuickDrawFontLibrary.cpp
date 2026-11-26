#include "Pch.h"

#include "FileTypes/QuickDrawFontLibrary.h"
#include "Machines/Apple2/Common/ProDosVolume.h"





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawFontLibrary::Add
//
////////////////////////////////////////////////////////////////////////////////

bool QuickDrawFontLibrary::Add (std::span<const Byte> bytes)
{
    QuickDrawFont  font;



    if (!QuickDrawFont::TryParse (bytes, font))
    {
        return false;
    }

    m_fonts.push_back (std::move (font));
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawFontLibrary::AddVolume
//
//  `blocks` is a ProDOS volume in block order, as a .po file has it. A
//  file in the folder that isn't a font, or doesn't parse, is passed over.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT QuickDrawFontLibrary::AddVolume (const vector<Byte> & blocks)
{
    static constexpr const char *  s_kpszFontsFolder = "System/Fonts";



    HRESULT        hr = S_OK;
    ProDosVolume   volume (blocks);
    VolumeListing  listing;



    hr = volume.EnumerateDirectory (FilePath::Parse (s_kpszFontsFolder), listing);
    CHR (hr);

    for (const FileEntry & entry : listing.entries)
    {
        FilePayload  payload;



        if (entry.isDirectory || entry.type != kFontType)
        {
            continue;
        }

        hr = volume.Read (FilePath::Parse (std::string (s_kpszFontsFolder) + "/" + entry.name), payload);
        CHR (hr);

        Add (payload.bytes);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawFontLibrary::Find
//
////////////////////////////////////////////////////////////////////////////////

const QuickDrawFont * QuickDrawFontLibrary::Find (Word family, int size) const
{
    const QuickDrawFont  * best     = nullptr;
    int                    bestGap  = 0;



    for (const QuickDrawFont & font : m_fonts)
    {
        int   gap    = 0;
        bool  closer = false;



        if (font.GetFamily() != family)
        {
            continue;
        }

        gap    = std::abs (font.GetSize() - size);
        closer = (best == nullptr)
              || (gap < bestGap)
              || (gap == bestGap && font.GetSize() < best->GetSize());

        if (closer)
        {
            best    = &font;
            bestGap = gap;
        }
    }

    return best;
}
