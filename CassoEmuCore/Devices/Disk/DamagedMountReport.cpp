#include "Pch.h"

#include "DamagedMountReport.h"
#include "Core/UnicodeSymbols.h"





static constexpr const wchar_t *  s_kpszFractions[DiskImage::kQuarterTracksPerWholeTrack] =
{
    L"", L".25", L".5", L".75",
};





////////////////////////////////////////////////////////////////////////////////
//
//  DamagedMountReport::FormatTrackNumber
//
//  The head steps in quarter tracks, and a damaged track can sit on a half or
//  quarter track, so the number includes the fraction when there is one.
//
////////////////////////////////////////////////////////////////////////////////

wstring DamagedMountReport::FormatTrackNumber (int quarterTrack)
{
    int  whole    = quarterTrack / DiskImage::kQuarterTracksPerWholeTrack;
    int  fraction = quarterTrack % DiskImage::kQuarterTracksPerWholeTrack;



    return to_wstring (whole) + s_kpszFractions[fraction];
}





////////////////////////////////////////////////////////////////////////////////
//
//  DamagedMountReport::FormatTrackList
//
//  Tracks one whole track apart join into a run, so a disk whose flux tracks
//  sit on half tracks reads "tracks 1.5-19.5" rather than a list of nineteen.
//
////////////////////////////////////////////////////////////////////////////////

wstring DamagedMountReport::FormatTrackList (const vector<int> & quarterTracks)
{
    wstring  text;
    size_t   count = quarterTracks.size();
    size_t   first = 0;
    size_t   last  = 0;



    if (count == 0)
    {
        return text;
    }

    text = (count == 1) ? L"track " : L"tracks ";

    while (first < count)
    {
        last = first;

        while (last + 1 < count &&
               quarterTracks[last + 1] - quarterTracks[last] == DiskImage::kQuarterTracksPerWholeTrack)
        {
            last++;
        }

        if (first > 0)
        {
            text += L", ";
        }

        text += FormatTrackNumber (quarterTracks[first]);

        if (last > first)
        {
            text += L"-" + FormatTrackNumber (quarterTracks[last]);
        }

        first = last + 1;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DamagedMountReport::GetDamagedQuarterTracks
//
//  A track usually covers several quarter tracks -- the one it was written on
//  and the ones either side the head still reads it from -- so the one in the
//  middle of that run is the track's number.
//
////////////////////////////////////////////////////////////////////////////////

vector<int> DamagedMountReport::GetDamagedQuarterTracks (const DiskImage & image)
{
    vector<int>  result;
    vector<int>  mapped;
    size_t       d  = 0;
    int          qt = 0;



    for (d = 0; d < image.GetDamagedTracks().size(); d++)
    {
        mapped.clear();

        for (qt = 0; qt < DiskImage::kQuarterTrackCount; qt++)
        {
            if (image.GetMappedSlot (qt) == image.GetDamagedTracks()[d].trkIndex)
            {
                mapped.push_back (qt);
            }
        }

        if (!mapped.empty())
        {
            result.push_back (mapped[(mapped.size() - 1) / 2]);
        }
    }

    for (const DamagedQuarterTrack & entry : image.GetDamagedQuarterTrackEntries())
    {
        result.push_back (entry.quarterTrack);
    }

    sort (result.begin(), result.end());
    result.erase (unique (result.begin(), result.end()), result.end());

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DamagedMountReport::FormatBody
//
////////////////////////////////////////////////////////////////////////////////

wstring DamagedMountReport::FormatBody (const DiskImage & image, const wstring & path)
{
    return FormatBody (image.GetWriteProtectInfo(), fs::path (path).filename().wstring());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DamagedMountReport::FormatBody
//
//  One bullet per problem found, then what Casso did about it. The dialog on
//  insert and the drive's tooltip both show this text.
//
////////////////////////////////////////////////////////////////////////////////

wstring DamagedMountReport::FormatBody (const WriteProtectInfo & wp, const wstring & fileName)
{
    wstring  text;
    wstring  bullet = wstring (1, s_kchBullet) + L" ";



    if (!wp.IsDamaged())
    {
        return text;
    }

    text  = L"Casso found problems in ";
    text += fileName.empty() ? wstring (L"this disk image") : fileName;
    text += L":\n\n";

    if (wp.checksumMismatch)
    {
        text += bullet + L"The stored checksum does not match the contents.\n";
    }

    if (wp.damagedTracks)
    {
        text += bullet + L"Unable to read ";
        text += wp.damagedQuarterTracks.empty() ? wstring (L"some tracks")
                                                : FormatTrackList (wp.damagedQuarterTracks);
        text += L".\n";
    }

    text += L"\nCasso has loaded the disk so you can read the undamaged portions and has "
            L"write-protected it for this session, because rewriting the file would hide "
            L"the damage.";

    if (wp.damagedTracks)
    {
        text += L" Unreadable tracks read as blank.";
    }

    return text;
}