#include "Pch.h"

#include "DamagedMountReport.h"





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
////////////////////////////////////////////////////////////////////////////////

wstring DamagedMountReport::FormatTrackList (const vector<int> & quarterTracks)
{
    wstring  text;
    size_t   count  = quarterTracks.size();
    size_t   listed = min (count, kMaxListedTracks);
    size_t   i      = 0;



    if (count == 0)
    {
        return text;
    }

    text = (count == 1) ? L"track " : L"tracks ";

    for (i = 0; i < listed; i++)
    {
        if (i > 0 && count == 2)
        {
            text += L" and ";
        }
        else if (i > 0 && i == listed - 1 && listed == count)
        {
            text += L", and ";
        }
        else if (i > 0)
        {
            text += L", ";
        }

        text += FormatTrackNumber (quarterTracks[i]);
    }

    if (count > listed)
    {
        text += L", and " + to_wstring (count - listed) + L" more";
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
//  What is wrong, then the file, then what Casso did about it. A flux disk
//  adds what salvage cannot keep, because salvage writes standard sectors and
//  a flux disk's timing is usually the reason it is a flux disk.
//
////////////////////////////////////////////////////////////////////////////////

wstring DamagedMountReport::FormatBody (const DiskImage & image, const wstring & path)
{
    wstring      text;
    vector<int>  tracks = GetDamagedQuarterTracks (image);



    if (!image.IsDamaged())
    {
        return text;
    }

    if (image.HasSourceCrcMismatch())
    {
        text += L"This disk image's stored checksum does not match its contents. "
                L"The file is damaged or was written by a tool that miscomputed it.";
    }

    if (image.HasDamagedTracks())
    {
        text += text.empty() ? L"Some" : L" Also, some";
        text += L" of this disk image's tracks could not be read from the file";
        text += tracks.empty() ? L"." : (L": " + FormatTrackList (tracks) + L".");
        text += L" Their data is missing or cut short, so the drive reads them as blank.";
    }

    text += L"\n\n" + path + L"\n\n";
    text += L"Casso has loaded it so you can read it, and has write-protected it "
            L"for this session. ";
    text += image.HasDamagedTracks()
            ? L"Rewriting the file would replace the damaged tracks with blank ones, "
              L"silently hiding the damage."
            : L"Rewriting the file would give it a newly computed checksum, "
              L"silently hiding the damaged sectors.";

    if (image.HasFluxTracks())
    {
        text += L"\n\nSalvage copies the readable sectors to a standard disk image. "
                L"It does not keep this disk's flux timing or its copy protection.";
    }

    return text;
}
