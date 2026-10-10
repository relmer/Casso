#include "Pch.h"

#include "Devices/Disk/Inspector/InspectorSearch.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"





static constexpr int   s_kBitsPerNibble = 8;
static constexpr Byte  s_kLowDigit      = 0x0F;
static constexpr Byte  s_kHighDigit     = 0xF0;
static constexpr Byte  s_kAsciiMask     = 0x7F;
static constexpr int   s_kBlocksPerTrack = 8;





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorSearch::ParsePattern
//
//  Hex patterns are pairs of digits, either of which can be ?, spaces
//  ignored, and in a nibble pattern a + after a pair; text is any ASCII,
//  matched without regard to the high bit or to case.
//
////////////////////////////////////////////////////////////////////////////////

bool InspectorSearch::ParsePattern (const std::wstring & text, SearchKind kind, vector<SearchItem> & outItems)
{
    std::wstring  digits;
    size_t        i      = 0;
    int           value  = 0;
    bool          isOk   = !text.empty();
    SearchItem    item;



    outItems.clear();

    if (kind == SearchKind::SectorText)
    {
        for (i = 0; isOk && i < text.size(); i++)
        {
            isOk = text[i] < 0x80;
            outItems.push_back (SearchItem { static_cast<Byte> (text[i]), s_kAsciiMask, false });
        }
    }
    else
    {
        for (wchar_t ch : text)
        {
            if (ch != L' ')
            {
                digits.push_back (ch);
            }
        }

        for (i = 0; isOk && i < digits.size(); )
        {
            isOk = i + 1 < digits.size();
            item = SearchItem();

            for (int half = 0; isOk && half < 2; half++)
            {
                wchar_t  ch = digits[i + half];

                if (ch == L'?')
                {
                    item.mask &= (half == 0) ? s_kLowDigit : s_kHighDigit;
                }
                else
                {
                    isOk = InspectorFormat::TryParseHex (std::wstring (1, ch), value);
                    item.value |= static_cast<Byte> (value << ((half == 0) ? 4 : 0));
                }
            }

            i += 2;

            if (isOk && i < digits.size() && digits[i] == L'+')
            {
                isOk                 = kind == SearchKind::Nibbles;
                item.needsExtraZeros = true;
                i++;
            }

            outItems.push_back (item);
        }
    }

    if (!isOk)
    {
        outItems.clear();
    }

    return isOk && !outItems.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorSearch::Find
//
////////////////////////////////////////////////////////////////////////////////

vector<SearchHit> InspectorSearch::Find (const DiskAnalysis & analysis, int quarterTrack, const SearchQuery & query)
{
    vector<SearchHit>  hits;
    vector<SearchItem> items;
    vector<bool>       isSeen (analysis.tracks.size(), false);
    int                first  = query.isWholeDisk ? 0 : quarterTrack;
    int                last   = query.isWholeDisk ? DiskImage::kQuarterTrackCount - 1 : quarterTrack;
    int                qt     = 0;
    int                slot   = -1;



    if (ParsePattern (query.text, query.kind, items))
    {
        for (qt = std::max (first, 0); qt <= last && qt < DiskImage::kQuarterTrackCount; qt++)
        {
            slot = analysis.entries[qt].slot;

            if (slot >= 0 && slot < static_cast<int> (analysis.tracks.size()) && !isSeen[slot] && analysis.tracks[slot] != nullptr)
            {
                isSeen[slot] = true;
                FindInTrack (*analysis.tracks[slot], qt, items, query, hits);
            }
        }
    }

    return hits;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorSearch::FindInTrack
//
////////////////////////////////////////////////////////////////////////////////

void InspectorSearch::FindInTrack (const TrackAnalysis & track, int quarterTrack, const vector<SearchItem> & items, const SearchQuery & query,
                                   vector<SearchHit> & inOutHits)
{
    if (query.kind != SearchKind::Nibbles)
    {
        FindInSectors (track, quarterTrack, items, query.kind == SearchKind::SectorText, inOutHits);
    }
    else
    {
        FindAligned (track, quarterTrack, items, inOutHits);

        if (query.isAnyBitOffset)
        {
            FindAnyBit (track, quarterTrack, items, inOutHits);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorSearch::FindAligned
//
//  Runs of framed nibbles, across the index too.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorSearch::FindAligned (const TrackAnalysis & track, int quarterTrack, const vector<SearchItem> & items, vector<SearchHit> & inOutHits)
{
    const vector<FramedNibble> &  nibbles = track.framed.nibbles;
    size_t                        n       = nibbles.size();
    size_t                        i       = 0;
    size_t                        k       = 0;
    bool                          isMatch = false;



    for (i = 0; i < n && items.size() <= n; i++)
    {
        isMatch = true;

        for (k = 0; isMatch && k < items.size(); k++)
        {
            const FramedNibble &  nibble = nibbles[(i + k) % n];

            isMatch = IsMatch (nibble.value, items[k], false) && (!items[k].needsExtraZeros || nibble.extraZeroCells > 0);
        }

        if (isMatch)
        {
            SearchHit  hit;

            hit.quarterTrack = quarterTrack;
            hit.track        = &track;
            hit.firstNibble  = static_cast<int> (i);
            hit.nibbleCount  = static_cast<int> (items.size());
            hit.cell         = nibbles[i].startCell % std::max<uint32_t> (track.framed.cellCount, 1);
            inOutHits.push_back (hit);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorSearch::FindAnyBit
//
//  From each 1 cell that does not start a framed nibble, the nibbles the
//  latch would read if it began there: skip to a 1, take eight cells, and
//  so on. Hits that start on a framed nibble are the aligned ones, already
//  found.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorSearch::FindAnyBit (const TrackAnalysis & track, int quarterTrack, const vector<SearchItem> & items, vector<SearchHit> & inOutHits)
{
    const FramedTrack &  framed  = track.framed;
    uint32_t             count   = framed.cellCount;
    vector<bool>         isStart (count, false);
    uint32_t             c       = 0;
    uint32_t             pos     = 0;
    uint32_t             zeros   = 0;
    size_t               k       = 0;
    int                  b       = 0;
    Byte                 value   = 0;
    bool                 isMatch = false;



    for (const FramedNibble & nibble : framed.nibbles)
    {
        isStart[nibble.startCell % std::max<uint32_t> (count, 1)] = true;
    }

    for (c = 0; c < count && framed.cells.size() >= count; c++)
    {
        if (framed.cells[c] == 0 || isStart[c])
        {
            continue;
        }

        pos     = c;
        isMatch = true;

        for (k = 0; isMatch && k < items.size(); k++)
        {
            for (zeros = 0; framed.cells[pos % count] == 0 && zeros < count; zeros++)
            {
                pos++;
            }

            isMatch = k == 0 || !items[k - 1].needsExtraZeros || zeros > 0;
            value   = 0;

            for (b = 0; b < s_kBitsPerNibble; b++)
            {
                value = static_cast<Byte> ((value << 1) | (framed.cells[pos % count] != 0 ? 1 : 0));
                pos++;
            }

            isMatch = isMatch && IsMatch (value, items[k], false);
        }

        if (isMatch && items.back().needsExtraZeros)
        {
            isMatch = framed.cells[pos % count] == 0;
        }

        if (isMatch)
        {
            SearchHit  hit;

            hit.quarterTrack = quarterTrack;
            hit.track        = &track;
            hit.nibbleCount  = static_cast<int> (items.size());
            hit.cell         = c;
            hit.isAligned    = false;
            inOutHits.push_back (hit);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorSearch::FindInSectors
//
////////////////////////////////////////////////////////////////////////////////

void InspectorSearch::FindInSectors (const TrackAnalysis & track, int quarterTrack, const vector<SearchItem> & items, bool isText, vector<SearchHit> & inOutHits)
{
    size_t  s       = 0;
    size_t  i       = 0;
    size_t  k       = 0;
    bool    isMatch = false;



    for (s = 0; s < track.sectors.size(); s++)
    {
        if (track.sectors[s].dataField < 0)
        {
            continue;
        }

        const auto &  bytes = track.fields[track.sectors[s].dataField].data.bytes;

        for (i = 0; i + items.size() <= bytes.size(); i++)
        {
            isMatch = true;

            for (k = 0; isMatch && k < items.size(); k++)
            {
                isMatch = IsMatch (bytes[i + k], items[k], isText);
            }

            if (isMatch)
            {
                SearchHit  hit;

                hit.quarterTrack = quarterTrack;
                hit.track        = &track;
                hit.sectorIndex  = static_cast<int> (s);
                hit.offset       = static_cast<int> (i);
                hit.length       = static_cast<int> (items.size());
                inOutHits.push_back (hit);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorSearch::IsMatch
//
////////////////////////////////////////////////////////////////////////////////

bool InspectorSearch::IsMatch (Byte value, const SearchItem & item, bool isText)
{
    static constexpr Byte  kCaseBit = 0x20;



    Byte  a = static_cast<Byte> (value & s_kAsciiMask);
    Byte  b = item.value;



    if (isText)
    {
        a = (a >= 'a' && a <= 'z') ? static_cast<Byte> (a & ~kCaseBit) : a;
        b = (b >= 'a' && b <= 'z') ? static_cast<Byte> (b & ~kCaseBit) : b;
    }

    return isText ? a == b : (value & item.mask) == (item.value & item.mask);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorSearch::FormatHit
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorSearch::FormatHit (const SearchHit & hit)
{
    std::wstring  place = L"Track " + InspectorFormat::FormatQuarterTrack (hit.quarterTrack);



    if (hit.sectorIndex >= 0 && hit.track != nullptr)
    {
        place += L", sector " + InspectorFormat::FormatSector (hit.track->sectors[hit.sectorIndex].sector) + L" at " + InspectorFormat::FormatHexByte (hit.offset);
    }
    else if (hit.isAligned)
    {
        place += L", nibble " + InspectorFormat::FormatHexOffset (hit.firstNibble);
    }
    else
    {
        place += L", cell " + InspectorFormat::FormatCount (hit.cell) + L", off the framed nibbles";
    }

    return place;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorSearch::FormatHitCount
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorSearch::FormatHitCount (size_t count)
{
    return InspectorFormat::FormatCount (count) + ((count == 1) ? L" match" : L" matches");
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorSearch::IsOutOfDate
//
////////////////////////////////////////////////////////////////////////////////

bool InspectorSearch::IsOutOfDate (const SearchHit & hit, const DiskAnalysis & analysis)
{
    int  slot = (hit.quarterTrack >= 0 && hit.quarterTrack < DiskImage::kQuarterTrackCount) ? analysis.entries[hit.quarterTrack].slot : -1;



    return slot < 0 || slot >= static_cast<int> (analysis.tracks.size()) || analysis.tracks[slot].get() != hit.track;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorGoTo::Resolve
//
////////////////////////////////////////////////////////////////////////////////

bool InspectorGoTo::Resolve (const DiskAnalysis & analysis, int quarterTrack, GoToKind kind, const std::wstring & text, GoToTarget & outTarget,
                             std::wstring & outError)
{
    const TrackAnalysis  * track  = nullptr;
    std::wstring           plain;
    int                    value  = -1;
    int                    nibble = -1;
    uint32_t               best   = UINT32_MAX;
    uint32_t               cells  = 0;
    bool                   isRead = false;



    outTarget = GoToTarget();
    outError.clear();

    for (wchar_t ch : text)
    {
        if (ch != L',' && ch != L' ')
        {
            plain.push_back (ch);
        }
    }

    switch (kind)
    {
        case GoToKind::File:           isRead = true; value = 0;                                       break;
        case GoToKind::Track:          isRead = InspectorFormat::TryParseQuarterTrack (plain, value); break;
        case GoToKind::ProDosBlock:    isRead = InspectorFormat::TryParseDecimal (plain, value);      break;
        case GoToKind::Cell:           isRead = InspectorFormat::TryParseDecimal (plain, value);      break;
        default:                       isRead = InspectorFormat::TryParseHex (plain, value);          break;
    }

    if (!isRead || value < 0)
    {
        outError = (kind == GoToKind::Track) ? L"Enter a track from 0 to 39.75, such as 17.25." : L"Enter a number in the base the inspector shows it in.";
    }
    else if (kind == GoToKind::File)
    {
        outError = FindFile (analysis, text, outTarget) ? L"" : L"No file on this disk has the path " + text + L".";
    }
    else if (kind == GoToKind::Track)
    {
        outTarget.quarterTrack = value;
    }
    else
    {
        outTarget.quarterTrack = (kind == GoToKind::ProDosBlock) ? value / s_kBlocksPerTrack * DiskImage::kQuarterTracksPerWholeTrack : quarterTrack;
        track                  = GetTrack (analysis, outTarget.quarterTrack);

        if (track == nullptr || track->framed.nibbles.empty())
        {
            outError = L"Nothing is recorded on track " + InspectorFormat::FormatQuarterTrack (outTarget.quarterTrack) + L".";
        }
        else if (kind == GoToKind::NibbleOffset)
        {
            outTarget.firstNibble = (value < static_cast<int> (track->framed.nibbles.size())) ? value : -1;
            outError              = (outTarget.firstNibble < 0) ? std::format (L"This track has {} nibbles.", InspectorFormat::FormatCount (track->framed.nibbles.size())) : L"";
        }
        else if (kind == GoToKind::Cell)
        {
            cells = track->framed.cellCount;

            //  The nibble the cell is in is the one that starts closest
            //  before it, across the index if need be.
            for (size_t i = 0; cells > 0 && static_cast<uint32_t> (value) < cells && i < track->framed.nibbles.size(); i++)
            {
                uint32_t  back = (static_cast<uint32_t> (value) + cells - track->framed.nibbles[i].startCell % cells) % cells;

                if (back < best)
                {
                    best   = back;
                    nibble = static_cast<int> (i);
                }
            }

            outTarget.firstNibble = nibble;
            outError              = (nibble < 0) ? std::format (L"This track has {} cells.", InspectorFormat::FormatCount (cells)) : L"";
        }
        else
        {
            outTarget.sectorIndex = FindSector (*track, kind, value);
            outError              = (outTarget.sectorIndex >= 0) ? L""
                                  : (kind == GoToKind::ProDosBlock) ? std::format (L"Block {} is not on track {}.", value, InspectorFormat::FormatQuarterTrack (outTarget.quarterTrack))
                                  : std::format (L"No {} {} on this track.", (kind == GoToKind::Dos33Sector) ? L"DOS 3.3 sector" : L"sector", InspectorFormat::FormatSector (value));
        }
    }

    return outError.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorGoTo::GetLabel
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR InspectorGoTo::GetLabel (GoToKind kind)
{
    LPCWSTR  label = L"Track";



    switch (kind)
    {
        case GoToKind::PhysicalSector: label = L"Physical sector"; break;
        case GoToKind::Dos33Sector:    label = L"DOS 3.3 sector";  break;
        case GoToKind::ProDosBlock:    label = L"ProDOS block";    break;
        case GoToKind::NibbleOffset:   label = L"Nibble offset";   break;
        case GoToKind::Cell:           label = L"Cell";            break;
        case GoToKind::File:           label = L"File";            break;
        default:                                                   break;
    }

    return label;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorGoTo::GetTrack
//
////////////////////////////////////////////////////////////////////////////////

const TrackAnalysis * InspectorGoTo::GetTrack (const DiskAnalysis & analysis, int quarterTrack)
{
    int  slot = (quarterTrack >= 0 && quarterTrack < DiskImage::kQuarterTrackCount) ? analysis.entries[quarterTrack].slot : -1;



    return (slot >= 0 && slot < static_cast<int> (analysis.tracks.size())) ? analysis.tracks[slot].get() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorGoTo::FindSector
//
//  A ProDOS block's first half, since a block is two sectors.
//
////////////////////////////////////////////////////////////////////////////////

int InspectorGoTo::FindSector (const TrackAnalysis & track, GoToKind kind, int value)
{
    int  found = -1;



    for (size_t s = 0; found < 0 && s < track.sectors.size(); s++)
    {
        const AnalyzedSector &  sector = track.sectors[s];

        if ((kind == GoToKind::PhysicalSector && sector.sector == value) || (kind == GoToKind::Dos33Sector && sector.dos33Logical == value) ||
            (kind == GoToKind::ProDosBlock && sector.prodosBlock == value && sector.prodosHalf == 0))
        {
            found = static_cast<int> (s);
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorGoTo::FindFile
//
//  A file of any mapped volume whose path matches without regard to case,
//  and the physical sector that holds its first sector.
//
////////////////////////////////////////////////////////////////////////////////

bool InspectorGoTo::FindFile (const DiskAnalysis & analysis, const std::wstring & path, GoToTarget & outTarget)
{
    const TrackAnalysis *  track    = nullptr;
    bool                   isFound  = false;
    int                    physical = 0;



    auto  isSame = [] (const std::wstring & a, const std::wstring & b)
    {
        return a.size() == b.size() && std::equal (a.begin(), a.end(), b.begin(), [] (wchar_t x, wchar_t y)
        {
            return (x >= L'a' && x <= L'z' ? x - 32 : x) == (y >= L'a' && y <= L'z' ? y - 32 : y);
        });
    };

    for (const FileMap & map : analysis.fileMaps)
    {
        for (size_t f = 0; !isFound && f < map.files.size(); f++)
        {
            const MappedFile &  file = map.files[f];

            if (file.isDeleted || !isSame (file.path, path))
            {
                continue;
            }

            for (const FilePlace & place : file.sectors)
            {
                if (!isFound && place.cell >= 0)
                {
                    isFound                = true;
                    outTarget.quarterTrack = map.GetTrack (place.cell) * DiskImage::kQuarterTracksPerWholeTrack;
                    physical               = map.GetPhysical (place.cell, 0);
                    track                  = GetTrack (analysis, outTarget.quarterTrack);
                    outTarget.sectorIndex  = (track != nullptr) ? FindSector (*track, GoToKind::PhysicalSector, physical) : -1;
                }
            }
        }
    }

    return isFound;
}
