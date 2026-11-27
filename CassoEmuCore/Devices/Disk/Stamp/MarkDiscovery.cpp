#include "Pch.h"

#include "Devices/Disk/Stamp/MarkDiscovery.h"

#include "Devices/Disk/Inspector/FieldLocator.h"
#include "Devices/Disk/Inspector/LatchFramer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MarkDiscovery::Discover
//
//  Adds the prologs found at least kMinMatches times to outMarks, the
//  address prologs to address16 or address13 by their data fields'
//  encoding. A prolog already in a list is not added again.
//
//  After each address field, every data body in the search window whose
//  checksum matches is a candidate. A body of one encoding can pass the
//  other's checksum by chance at a wrong offset, so the candidate whose
//  prolog and encoding are found after the most address fields is taken.
//
////////////////////////////////////////////////////////////////////////////////

void MarkDiscovery::Discover (const FramedTrack & track, int physicalTrack, FieldMarks & outMarks)
{
    constexpr size_t              kProlog       = 3;
    const vector<FramedNibble>  & nibbles       = track.nibbles;
    std::map<DataHit, int>        dataCounts;
    std::map<Prolog, int>         addressCounts;
    vector<Field>                 fields;



    for (size_t i = 0; i < nibbles.size(); i++)
    {
        Field  field;

        if (!IsAddressBodyAt (nibbles, i + kProlog, physicalTrack))
        {
            continue;
        }

        for (size_t k = 0; k < kProlog; k++)
        {
            field.address[k] = GetNibble (nibbles, i + k);
        }

        FindData (nibbles, i + kProlog + DiskFieldFormat::kAddressNibbles, field.hits);

        for (const DataHit & hit : field.hits)
        {
            dataCounts[hit]++;
        }

        fields.push_back (std::move (field));
    }

    for (const Field & field : fields)
    {
        const DataHit  *  best      = nullptr;
        int               bestCount = 0;

        for (const DataHit & hit : field.hits)
        {
            if (dataCounts[hit] > bestCount)
            {
                best      = &hit;
                bestCount = dataCounts[hit];
            }
        }

        if (best == nullptr || bestCount < kMinMatches)
        {
            continue;
        }

        addressCounts[field.address]++;

        if (addressCounts[field.address] == kMinMatches)
        {
            AddMark (best->kind == DiskFieldKind::Thirteen ? outMarks.address13 : outMarks.address16, field.address);
            AddMark (outMarks.data, best->prolog);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MarkDiscovery::GetNibble
//
//  The nibble at an index, wrapping at the end of the track.
//
////////////////////////////////////////////////////////////////////////////////

Byte MarkDiscovery::GetNibble (const vector<FramedNibble> & nibbles, size_t index)
{
    return nibbles.empty() ? (Byte) 0 : nibbles[index % nibbles.size()].value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MarkDiscovery::IsAddressBodyAt
//
//  Whether the eight nibbles at index are a 4-and-4 volume, track, sector
//  and checksum, each nibble with its odd bits set, whose checksum matches
//  and whose track is the physical track.
//
////////////////////////////////////////////////////////////////////////////////

bool MarkDiscovery::IsAddressBodyAt (const vector<FramedNibble> & nibbles, size_t index, int physicalTrack)
{
    constexpr Byte  kOddBits        = 0xAA;
    constexpr int   kFields         = 4;
    Byte            values[kFields] = {};
    bool            isBody          = nibbles.size() > (size_t) DiskFieldFormat::kAddressNibbles;



    for (int field = 0; isBody && field < kFields; field++)
    {
        Byte  odd  = GetNibble (nibbles, index + field * 2);
        Byte  even = GetNibble (nibbles, index + field * 2 + 1);

        isBody        = (odd & kOddBits) == kOddBits && (even & kOddBits) == kOddBits;
        values[field] = DiskFieldFormat::Decode44 (odd, even);
    }

    return isBody
        && values[1] == physicalTrack
        && DiskFieldFormat::ComputeAddressChecksum (values[0], values[1], values[2]) == values[3];
}





////////////////////////////////////////////////////////////////////////////////
//
//  MarkDiscovery::FindData
//
//  Every prolog and encoding in the search window after an address field
//  whose data body decodes with a matching checksum.
//
////////////////////////////////////////////////////////////////////////////////

void MarkDiscovery::FindData (const vector<FramedNibble> & nibbles, size_t from, vector<DataHit> & outHits)
{
    constexpr size_t         kProlog  = 3;
    constexpr DiskFieldKind  kKinds[] = { DiskFieldKind::Sixteen, DiskFieldKind::Thirteen };



    outHits.clear();

    for (size_t j = from; j < from + FieldLocator::kDataSearchWindowNibbles; j++)
    {
        for (DiskFieldKind kind : kKinds)
        {
            DataHit  hit;

            if (!IsDataBodyAt (nibbles, j + kProlog, kind))
            {
                continue;
            }

            for (size_t k = 0; k < kProlog; k++)
            {
                hit.prolog[k] = GetNibble (nibbles, j + k);
            }

            hit.kind = kind;
            outHits.push_back (hit);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MarkDiscovery::IsDataBodyAt
//
//  Whether the nibbles at index are a data body of the encoding, followed by
//  its checksum nibble, that decode with a matching checksum and with every
//  nibble in the encoding's table.
//
////////////////////////////////////////////////////////////////////////////////

bool MarkDiscovery::IsDataBodyAt (const vector<FramedNibble> & nibbles, size_t index, DiskFieldKind kind)
{
    int              length = DiskFieldFormat::GetBodyLength (kind);
    vector<Byte>     body ((size_t) length);
    DataFieldDecode  decode;
    bool             isLong = nibbles.size() > (size_t) length;



    for (int i = 0; isLong && i < length; i++)
    {
        body[(size_t) i] = GetNibble (nibbles, index + (size_t) i);
    }

    if (isLong)
    {
        DiskFieldFormat::DecodeData (kind, body, GetNibble (nibbles, index + (size_t) length), decode);
    }

    return isLong && decode.isChecksumGood && decode.invalidNibbles.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MarkDiscovery::AddMark
//
////////////////////////////////////////////////////////////////////////////////

void MarkDiscovery::AddMark (vector<DiskMarkPattern> & ioMarks, const Prolog & nibbles)
{
    DiskMarkPattern  mark = DiskMarkPattern::MakeExact (nibbles[0], nibbles[1], nibbles[2]);



    if (std::ranges::find (ioMarks, mark) == ioMarks.end())
    {
        ioMarks.push_back (mark);
    }
}
