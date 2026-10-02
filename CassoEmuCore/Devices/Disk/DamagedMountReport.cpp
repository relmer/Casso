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

    sort (result.begin(), result.end());
    result.erase (unique (result.begin(), result.end()), result.end());

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DamagedMountReport::FormatBody
//
//  One bullet per problem found, then the file, then what Casso did about
//  it. A flux disk adds what salvage cannot keep, because salvage writes
//  standard sectors and a flux disk's timing is usually the reason it is a
//  flux disk.
//
////////////////////////////////////////////////////////////////////////////////

wstring DamagedMountReport::FormatBody (const DiskImage & image, const wstring & path)
{
    wstring      text;
    wstring      bullet = wstring (1, s_kchBullet) + L" ";
    vector<int>  tracks = GetDamagedQuarterTracks (image);



    if (!image.IsDamaged())
    {
        return text;
    }

    text = L"Casso found these problems in this disk image:\n";

    if (image.HasSourceCrcMismatch())
    {
        text += bullet + L"The stored checksum does not match the contents.\n";
    }

    if (image.HasDamagedTracks())
    {
        text += bullet + L"Unable to read ";
        text += tracks.empty() ? wstring (L"some tracks") : FormatTrackList (tracks);
        text += L".\n";
    }

    text += L"\n" + path + L"\n\n";
    text += L"Casso has loaded the disk so you can read it, and has write-protected it "
            L"for this session, because rewriting the file would hide the damage.";

    if (image.HasDamagedTracks())
    {
        text += L" Unreadable tracks read as blank.";
    }

    if (image.HasFluxTracks())
    {
        text += L" A salvaged copy is a standard disk image of the readable sectors, "
                L"without this disk's flux timing or its copy protection.";
    }

    return text;
}
