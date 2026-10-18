#include "Pch.h"

#include "Devices/Disk/Inspector/FieldLocator.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FieldMarks::MakeStandard
//
////////////////////////////////////////////////////////////////////////////////

FieldMarks FieldMarks::MakeStandard()
{
    FieldMarks  marks;



    marks.address16       = { DiskFieldFormat::GetAddressPrologue (DiskFieldKind::Sixteen) };
    marks.address13       = { DiskFieldFormat::GetAddressPrologue (DiskFieldKind::Thirteen) };
    marks.data            = { DiskFieldFormat::GetDataPrologue() };
    marks.addressEpilogue = { DiskFieldFormat::GetEpilogue() };
    marks.dataEpilogue    = { DiskFieldFormat::GetEpilogue() };

    return marks;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FieldLocator::Locate
//
//  One walk finds every prologue in passing order; pairing and data decoding
//  follow, because a data field's encoding comes from the address field it
//  pairs with. Data bodies hold no D5 or AA, so a prologue is never found
//  inside a body.
//
////////////////////////////////////////////////////////////////////////////////

void FieldLocator::Locate (const FramedTrack & track, const FieldMarks & marks, vector<LocatedField> & outFields)
{
    int            n              = static_cast<int> (track.nibbles.size());
    int            i              = 0;
    int            length         = 0;
    DiskFieldKind  kind           = DiskFieldKind::Sixteen;
    LocatedField   field;
    vector<int>    addressStarts;



    outFields.clear();

    for (i = 0; i < n; i++)
    {
        if (IsAddressAt (track, i, marks, kind, length))
        {
            field      = LocatedField();
            field.kind = kind;

            ReadAddressField (track, i, length, field);
            ReadEpilogue     (track, field.bodyNibble + DiskFieldFormat::kAddressNibbles, marks.addressEpilogue, field);
            SetCellSpan      (track, field);

            outFields.push_back (field);
            addressStarts.push_back (i);
        }
        else if (MatchesAny (track, i, marks.data, length))
        {
            field             = LocatedField();
            field.role        = FieldRole::Data;
            field.firstNibble = i;
            field.bodyNibble  = i + length;

            outFields.push_back (field);
        }
    }

    PairFields (track, addressStarts, outFields);

    for (LocatedField & f : outFields)
    {
        if (f.role != FieldRole::Data)
        {
            continue;
        }

        if (f.pairedField < 0)
        {
            ChooseOrphanKind (track, marks, f);
        }

        ReadDataField (track, f.firstNibble, f.bodyNibble - f.firstNibble, f.kind, f);
        ReadEpilogue  (track, f.bodyNibble + DiskFieldFormat::GetBodyLength (f.kind) + 1, marks.dataEpilogue, f);
        SetCellSpan   (track, f);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FieldLocator::GetNibble
//
//  The nibble at an index, wrapping at the end of the track.
//
////////////////////////////////////////////////////////////////////////////////

Byte FieldLocator::GetNibble (const FramedTrack & track, int index)
{
    int  n = static_cast<int> (track.nibbles.size());



    return track.nibbles[((index % n) + n) % n].value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FieldLocator::MatchesAny
//
//  True when any pattern matches the nibbles starting at index; outLength is
//  the matching pattern's length.
//
////////////////////////////////////////////////////////////////////////////////

bool FieldLocator::MatchesAny (const FramedTrack & track, int index, const vector<DiskMarkPattern> & patterns, int & outLength)
{
    std::array<Byte, DiskMarkPattern::kMaxNibbles>  window  = {};
    bool                                            isMatch = false;
    int                                             i       = 0;



    for (i = 0; i < DiskMarkPattern::kMaxNibbles; i++)
    {
        window[i] = GetNibble (track, index + i);
    }

    for (const DiskMarkPattern & pattern : patterns)
    {
        isMatch = pattern.Matches (window);

        if (isMatch)
        {
            outLength = pattern.GetLength();
            break;
        }
    }

    return isMatch;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FieldLocator::IsAddressAt
//
////////////////////////////////////////////////////////////////////////////////

bool FieldLocator::IsAddressAt (const FramedTrack & track, int index, const FieldMarks & marks, DiskFieldKind & outKind, int & outLength)
{
    bool  isSixteen  = MatchesAny (track, index, marks.address16, outLength);
    bool  isThirteen = !isSixteen && MatchesAny (track, index, marks.address13, outLength);



    outKind = isThirteen ? DiskFieldKind::Thirteen : DiskFieldKind::Sixteen;

    return isSixteen || isThirteen;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FieldLocator::ReadAddressField
//
//  Four 4-and-4 pairs after the prologue: volume, track, sector, checksum.
//
////////////////////////////////////////////////////////////////////////////////

void FieldLocator::ReadAddressField (const FramedTrack & track, int index, int prologueLength, LocatedField & outField)
{
    static constexpr int  kVolume   = 0;
    static constexpr int  kTrack    = 2;
    static constexpr int  kSector   = 4;
    static constexpr int  kChecksum = 6;



    int  body = index + prologueLength;
    int  i    = 0;



    outField.role        = FieldRole::Address;
    outField.firstNibble = index;
    outField.bodyNibble  = body;
    outField.nibbleCount = prologueLength + DiskFieldFormat::kAddressNibbles;

    for (i = 0; i < prologueLength; i++)
    {
        outField.prologueFound[i] = GetNibble (track, index + i);
    }

    outField.volume           = DiskFieldFormat::Decode44 (GetNibble (track, body + kVolume),   GetNibble (track, body + kVolume + 1));
    outField.track            = DiskFieldFormat::Decode44 (GetNibble (track, body + kTrack),    GetNibble (track, body + kTrack + 1));
    outField.sector           = DiskFieldFormat::Decode44 (GetNibble (track, body + kSector),   GetNibble (track, body + kSector + 1));
    outField.checksumStored   = DiskFieldFormat::Decode44 (GetNibble (track, body + kChecksum), GetNibble (track, body + kChecksum + 1));
    outField.checksumComputed = DiskFieldFormat::ComputeAddressChecksum (outField.volume, outField.track, outField.sector);

    outField.isAddressChecksumGood = outField.checksumStored == outField.checksumComputed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FieldLocator::ReadDataField
//
//  The body and checksum nibble after the prologue, decoded with the field's
//  encoding. Decoding goes on past a bad checksum or a nibble outside the
//  table, so the bytes can still be shown.
//
////////////////////////////////////////////////////////////////////////////////

void FieldLocator::ReadDataField (const FramedTrack & track, int index, int prologueLength, DiskFieldKind kind, LocatedField & outField)
{
    int           bodyLength = DiskFieldFormat::GetBodyLength (kind);
    vector<Byte>  body (bodyLength, 0);
    int           i          = 0;



    outField.role        = FieldRole::Data;
    outField.kind        = kind;
    outField.firstNibble = index;
    outField.bodyNibble  = index + prologueLength;
    outField.nibbleCount = prologueLength + bodyLength + 1;

    for (i = 0; i < prologueLength; i++)
    {
        outField.prologueFound[i] = GetNibble (track, index + i);
    }

    for (i = 0; i < bodyLength; i++)
    {
        body[i] = GetNibble (track, outField.bodyNibble + i);
    }

    DiskFieldFormat::DecodeData (kind, body, GetNibble (track, outField.bodyNibble + bodyLength), outField.data);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FieldLocator::ReadEpilogue
//
//  Keeps the three nibbles after the field whatever they are, and counts the
//  epilogue into the field only when a pattern matches. EB after it is
//  counted when present; no standard reader checks it.
//
////////////////////////////////////////////////////////////////////////////////

void FieldLocator::ReadEpilogue (const FramedTrack & track, int index, const vector<DiskMarkPattern> & patterns, LocatedField & inOutField)
{
    int  length = 0;
    int  i      = 0;



    for (i = 0; i < DiskMarkPattern::kMaxNibbles; i++)
    {
        inOutField.epilogueFound[i] = GetNibble (track, index + i);
    }

    inOutField.isEpilogueStandard = MatchesAny (track, index, patterns, length);
    inOutField.hasEpilogueTail    = inOutField.isEpilogueStandard
                                 && length < DiskMarkPattern::kMaxNibbles
                                 && inOutField.epilogueFound[length] == DiskFieldFormat::kEpilogueTail;

    if (inOutField.isEpilogueStandard)
    {
        inOutField.nibbleCount += length + (inOutField.hasEpilogueTail ? 1 : 0);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FieldLocator::ChooseOrphanKind
//
//  A data field with no address field gives no encoding, and the table fit
//  cannot settle it: every 5-and-3 nibble is also a 6-and-2 one. So each
//  encoding is tried, and the one whose checksum passes, then the one whose
//  epilogue sits where its body length puts it, is taken; otherwise 6-and-2,
//  the more common format.
//
////////////////////////////////////////////////////////////////////////////////

void FieldLocator::ChooseOrphanKind (const FramedTrack & track, const FieldMarks & marks, LocatedField & inOutField)
{
    LocatedField  as62     = inOutField;
    LocatedField  as53     = inOutField;
    int           prologue = inOutField.bodyNibble - inOutField.firstNibble;
    int           score62  = 0;
    int           score53  = 0;



    ReadDataField (track, inOutField.firstNibble, prologue, DiskFieldKind::Sixteen, as62);
    ReadEpilogue  (track, as62.bodyNibble + DiskFieldFormat::kBodyLength62 + 1, marks.dataEpilogue, as62);
    ReadDataField (track, inOutField.firstNibble, prologue, DiskFieldKind::Thirteen, as53);
    ReadEpilogue  (track, as53.bodyNibble + DiskFieldFormat::kBodyLength53 + 1, marks.dataEpilogue, as53);

    score62 = (as62.data.isChecksumGood ? 2 : 0) + (as62.isEpilogueStandard ? 1 : 0);
    score53 = (as53.data.isChecksumGood ? 2 : 0) + (as53.isEpilogueStandard ? 1 : 0);

    inOutField.kind = (score53 > score62) ? DiskFieldKind::Thirteen : DiskFieldKind::Sixteen;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FieldLocator::SetCellSpan
//
//  The cells from the prologue's first nibble to the end of the field's last
//  nibble, and whether any nibble in it is noise or any cell is random. For a
//  data field the span checked for noise is the prologue, body and checksum.
//
////////////////////////////////////////////////////////////////////////////////

void FieldLocator::SetCellSpan (const FramedTrack & track, LocatedField & inOutField)
{
    int       n         = static_cast<int> (track.nibbles.size());
    int       checked   = inOutField.nibbleCount;
    int       last      = 0;
    int       i         = 0;
    uint32_t  cells     = track.cellCount;
    uint32_t  cell      = 0;
    uint32_t  spanCells = 0;



    if (inOutField.role == FieldRole::Data)
    {
        checked = (inOutField.bodyNibble - inOutField.firstNibble) + DiskFieldFormat::GetBodyLength (inOutField.kind) + 1;
    }

    last                 = (inOutField.firstNibble + inOutField.nibbleCount - 1) % n;
    inOutField.startCell = track.nibbles[inOutField.firstNibble % n].startCell;
    inOutField.endCell   = (track.nibbles[last].startCell + LatchFramer::kNibbleCells) % std::max<uint32_t> (cells, 1);
    inOutField.hasNoise  = false;

    for (i = 0; i < checked && !inOutField.hasNoise; i++)
    {
        inOutField.hasNoise = track.nibbles[(inOutField.firstNibble + i) % n].isNoise;
    }

    last      = (inOutField.firstNibble + checked - 1) % n;
    spanCells = (track.nibbles[last].startCell + LatchFramer::kNibbleCells + cells - inOutField.startCell) % std::max<uint32_t> (cells, 1);

    for (cell = 0; cell < spanCells && !inOutField.hasNoise && cells > 0; cell++)
    {
        inOutField.hasNoise = track.isRandomCell[(inOutField.startCell + cell) % cells] != 0;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FieldLocator::PairFields
//
//  For each address field, the nearest data prologue after it, within the
//  search window counted from the end of the address field (its epilogue
//  included when found), and before the next address prologue. A data field
//  pairs once; a second address field that reaches it finds none.
//
////////////////////////////////////////////////////////////////////////////////

void FieldLocator::PairFields (const FramedTrack & track, const vector<int> & addressStarts, vector<LocatedField> & inOutFields)
{
    int     n         = static_cast<int> (track.nibbles.size());
    int     nextAddr  = 0;
    int     distance  = 0;
    int     best      = -1;
    int     bestDist  = 0;
    size_t  a         = 0;
    size_t  d         = 0;



    for (a = 0; a < inOutFields.size(); a++)
    {
        if (inOutFields[a].role != FieldRole::Address)
        {
            continue;
        }

        nextAddr = n;

        for (int start : addressStarts)
        {
            distance = (start - inOutFields[a].firstNibble + n) % n;

            if (distance > 0 && distance < nextAddr)
            {
                nextAddr = distance;
            }
        }

        best     = -1;
        bestDist = n;

        for (d = 0; d < inOutFields.size(); d++)
        {
            if (inOutFields[d].role != FieldRole::Data || inOutFields[d].pairedField >= 0)
            {
                continue;
            }

            distance = (inOutFields[d].firstNibble - inOutFields[a].firstNibble + n) % n;

            if (distance >= inOutFields[a].nibbleCount
                && distance - inOutFields[a].nibbleCount < kDataSearchWindowNibbles
                && distance < nextAddr
                && distance < bestDist)
            {
                best     = static_cast<int> (d);
                bestDist = distance;
            }
        }

        if (best >= 0)
        {
            inOutFields[a].pairedField    = best;
            inOutFields[best].pairedField = static_cast<int> (a);
            inOutFields[best].kind        = inOutFields[a].kind;
        }
    }
}
