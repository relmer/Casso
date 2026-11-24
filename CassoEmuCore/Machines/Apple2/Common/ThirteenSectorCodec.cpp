#include "Pch.h"

#include "Machines/Apple2/Common/ThirteenSectorCodec.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Devices/Disk/DiskFieldFormat.h"
#include "Devices/Disk/Inspector/LatchFramer.h"
#include "Devices/Disk/Inspector/TrackCopy.h"
#include "Devices/Disk/Stamp/FormatStamp.h"
#include "Devices/Disk/Stamp/StampedTrackMarks.h"
#include "Machines/Apple2/Common/TrackMarks.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ThirteenSectorCodec::MakeMarks
//
//  The standard marks, plus the address prologue of RDOS 3.2, plus the
//  prologues of the 1981 Ultima's program disk. Each of its tracks has the
//  standard D5 AA B5 and D5 AA AD with extra bits set in the first and
//  third nibbles: D5, D7, DD or DF, then B5, B7, F5 or F7 for an address
//  and AD, BD, ED or FD for data.
//
////////////////////////////////////////////////////////////////////////////////

FieldMarks ThirteenSectorCodec::MakeMarks()
{
    static constexpr Byte  kFirsts[]        = { 0xD5, 0xD7, 0xDD, 0xDF };
    static constexpr Byte  kAddressThirds[] = { 0xB5, 0xB7, 0xF5, 0xF7 };
    static constexpr Byte  kDataThirds[]    = { 0xAD, 0xBD, 0xED, 0xFD };
    FieldMarks             marks            = FieldMarks::MakeStandard();



    marks.address13.push_back (DiskMarkPattern::MakeExact (0xD4, 0xAA, 0xB7));

    for (Byte first : kFirsts)
    {
        for (size_t i = 0; i < std::size (kAddressThirds); i++)
        {
            AddMark (marks.address13, DiskMarkPattern::MakeExact (first, 0xAA, kAddressThirds[i]));
            AddMark (marks.data,      DiskMarkPattern::MakeExact (first, 0xAA, kDataThirds[i]));
        }
    }

    return marks;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThirteenSectorCodec::AddMark
//
//  The pattern at the end of the list, unless the list has it already.
//
////////////////////////////////////////////////////////////////////////////////

void ThirteenSectorCodec::AddMark (vector<DiskMarkPattern> & ioMarks, const DiskMarkPattern & mark)
{
    if (std::ranges::find (ioMarks, mark) == ioMarks.end())
    {
        ioMarks.push_back (mark);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThirteenSectorCodec::LocateTrack
//
//  The fields of a whole track. The list is empty for a track with no record.
//
////////////////////////////////////////////////////////////////////////////////

void ThirteenSectorCodec::LocateTrack (const DiskImage & image, int track, const FieldMarks & marks, vector<LocatedField> & outFields)
{
    int                               slot = image.ResolveWholeTrack (track);
    std::shared_ptr<const TrackCopy>  copy;
    FramedTrack                       framed;



    outFields.clear();

    if (slot < 0)
    {
        return;
    }

    copy = TrackCopy::MakeFromImage (image, slot);

    LatchFramer::Frame (*copy, framed);
    FieldLocator::Locate (framed, marks, outFields);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThirteenSectorCodec::IsGoodSector
//
//  A 13-sector address field with a good checksum, a sector number on the
//  track, and a data field whose checksum is good.
//
////////////////////////////////////////////////////////////////////////////////

bool ThirteenSectorCodec::IsGoodSector (const vector<LocatedField> & fields, const LocatedField & address)
{
    return address.role == FieldRole::Address
        && address.kind == DiskFieldKind::Thirteen
        && address.isAddressChecksumGood
        && address.sector < kSectorsPerTrack
        && address.pairedField >= 0
        && fields[(size_t) address.pairedField].data.isChecksumGood;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThirteenSectorCodec::IsUnwrittenSector
//
//  A 13-sector address field with a good checksum and no data field after it.
//  DOS 3.2's INIT writes the address fields of a track and no data fields; a
//  sector gets its data field the first time it is written. Such a sector
//  reads as zeros and is not damage.
//
////////////////////////////////////////////////////////////////////////////////

bool ThirteenSectorCodec::IsUnwrittenSector (const LocatedField & address)
{
    return address.role == FieldRole::Address
        && address.kind == DiskFieldKind::Thirteen
        && address.isAddressChecksumGood
        && address.sector < kSectorsPerTrack
        && address.pairedField < 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThirteenSectorCodec::IsThirteenSector
//
//  The catalog track first, since a disk of either format has one there.
//
////////////////////////////////////////////////////////////////////////////////

bool ThirteenSectorCodec::IsThirteenSector (const DiskImage & image)
{
    static constexpr int  kProbeTracks[] = { 17, 1, 2, 3, 0 };



    vector<LocatedField>  fields;
    FieldMarks            marks    = MakeMarks();
    FormatStamp           stamp;
    int                   thirteen = 0;
    int                   sixteen  = 0;



    for (int track : kProbeTracks)
    {
        if (thirteen + sixteen > 0)
        {
            break;
        }

        LocateTrack (image, track, marks, fields);

        for (const LocatedField & f : fields)
        {
            if (f.role == FieldRole::Address && f.isAddressChecksumGood)
            {
                thirteen += (f.kind == DiskFieldKind::Thirteen) ? 1 : 0;
                sixteen  += (f.kind == DiskFieldKind::Sixteen)  ? 1 : 0;
            }
        }
    }

    //  With no address field the standard marks match, the format stamp's
    //  encodings settle it.
    if (thirteen + sixteen == 0 && StampedTrackMarks::TryLoadStamp (image.GetWozMetadata(), stamp))
    {
        for (const StampTrack & entry : stamp.tracks)
        {
            thirteen += (entry.encoding == StampEncoding::Thirteen) ? 1 : 0;
            sixteen  += (entry.encoding == StampEncoding::Sixteen)  ? 1 : 0;
        }
    }

    return thirteen > sixteen;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThirteenSectorCodec::Denibblize
//
//  A track with no 13-sector address field is Unformatted. One with all
//  thirteen sectors, each once and each either good or never written, is
//  Complete. Anything else is Partial.
//
////////////////////////////////////////////////////////////////////////////////

void ThirteenSectorCodec::Denibblize (const DiskImage & image, vector<Byte> & outSectors, SectorDecodeReport & outReport)
{
    vector<LocatedField>  fields;
    FormatStamp           stamp;
    bool                  hasStamp = StampedTrackMarks::TryLoadStamp (image.GetWozMetadata(), stamp);
    FieldMarks            standard = MakeMarks();



    outSectors.assign (kImageByteSize, 0);
    outReport.Reset (kTrackCount);

    for (int track = 0; track < kTrackCount; track++)
    {
        TrackDecodeOutcome  outcome    = TrackDecodeOutcome::Unformatted;
        Word                coverage   = 0;
        bool                duplicated = false;
        int                 addresses  = 0;
        TrackMarks          marks;

        LocateTrack (image, track, standard, fields);
        TakeSectors (fields, track, 0, coverage, duplicated, addresses, outSectors);

        //  The stamped marks only add sectors missing from the standard
        //  marks' read, so a stale or wrong stamp cannot change a sector it has.
        if (coverage != kAllSectors && hasStamp && StampedTrackMarks::TryGetTrackMarks (stamp, track, marks))
        {
            LocateTrack (image, track, MakeStampedMarks (marks), fields);
            TakeSectors (fields, track, coverage, coverage, duplicated, addresses, outSectors);
        }

        if (addresses > 0)
        {
            coverage = (Word) (coverage | kUnusedCoverage);
            outcome  = (coverage == SectorDecodeReport::kFullCoverage && !duplicated) ? TrackDecodeOutcome::Complete
                                                                                      : TrackDecodeOutcome::Partial;
        }

        outReport.SetOutcome (track, outcome, coverage, duplicated);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThirteenSectorCodec::TakeSectors
//
//  Each good or unwritten 13-sector address field's sector into the sector
//  image, skipping the sectors in `skip`. A sector found twice is a
//  duplicate, and its first copy is used.
//
////////////////////////////////////////////////////////////////////////////////

void ThirteenSectorCodec::TakeSectors (
    const vector<LocatedField>  &  fields,
    int                            track,
    Word                           skip,
    Word                        &  ioCoverage,
    bool                        &  ioDuplicated,
    int                         &  ioAddresses,
    vector<Byte>                &  outSectors)
{
    Word  taken = 0;



    for (const LocatedField & f : fields)
    {
        Word    bit       = 0;
        size_t  base      = 0;
        bool    unwritten = false;

        if (f.role != FieldRole::Address || f.kind != DiskFieldKind::Thirteen)
        {
            continue;
        }

        ioAddresses++;

        unwritten = IsUnwrittenSector (f);

        if (!unwritten && !IsGoodSector (fields, f))
        {
            continue;
        }

        bit = (Word) (1u << f.sector);

        if ((skip & bit) != 0)
        {
            continue;
        }

        if ((taken & bit) != 0)
        {
            ioDuplicated = true;
            continue;
        }

        taken = (Word) (taken | bit);
        base  = ((size_t) track * kSectorsPerTrack + f.sector) * kSectorByteSize;

        if (unwritten)
        {
            continue;
        }

        std::copy (fields[(size_t) f.pairedField].data.bytes.begin(),
                   fields[(size_t) f.pairedField].data.bytes.end(),
                   outSectors.begin() + (ptrdiff_t) base);
    }

    ioCoverage = (Word) (ioCoverage | taken);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThirteenSectorCodec::MakeStampedMarks
//
//  A format stamp's marks for one track as the field locator matches them:
//  its 13-sector address prologs, its data prologs, and the standard
//  epilogs.
//
////////////////////////////////////////////////////////////////////////////////

FieldMarks ThirteenSectorCodec::MakeStampedMarks (const TrackMarks & marks)
{
    FieldMarks  stamped  = FieldMarks::MakeStandard();



    stamped.address16.clear();
    stamped.address13.clear();
    stamped.data.clear();

    for (const TrackMarks::Prolog & prolog : marks.address13)
    {
        stamped.address13.push_back (DiskMarkPattern::MakeExact (prolog[0], prolog[1], prolog[2]));
    }

    for (const TrackMarks::Prolog & prolog : marks.data)
    {
        stamped.data.push_back (DiskMarkPattern::MakeExact (prolog[0], prolog[1], prolog[2]));
    }

    return stamped;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThirteenSectorCodec::AppendNibble
//
//  Eight cells, most significant bit first. The track's bytes are zero before
//  the first nibble, so only the one bits are written.
//
////////////////////////////////////////////////////////////////////////////////

void ThirteenSectorCodec::AppendNibble (vector<Byte> & bits, size_t & bitOffset, Byte nibble)
{
    int  bit = 0;



    for (bit = 7; bit >= 0; bit--)
    {
        Byte  cell = (Byte) ((nibble >> bit) & 1);

        bits[bitOffset >> 3] = (Byte) (bits[bitOffset >> 3] | (cell << (7 - (bitOffset & 7))));
        bitOffset++;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThirteenSectorCodec::AppendSync
//
//  A run of 13-sector sync nibbles: $FF and one zero cell, nine cells each.
//
////////////////////////////////////////////////////////////////////////////////

void ThirteenSectorCodec::AppendSync (vector<Byte> & bits, size_t & bitOffset, int count)
{
    int  i = 0;



    for (i = 0; i < count; i++)
    {
        AppendNibble (bits, bitOffset, 0xFF);
        bitOffset++;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThirteenSectorCodec::AppendMark
//
////////////////////////////////////////////////////////////////////////////////

void ThirteenSectorCodec::AppendMark (vector<Byte> & bits, size_t & bitOffset, const DiskMarkPattern & mark)
{
    int  i = 0;



    for (i = 0; i < mark.GetLength(); i++)
    {
        AppendNibble (bits, bitOffset, mark.GetValue (i));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThirteenSectorCodec::AppendSector
//
//  One sector: a sync run, the address field, a short sync run and the data
//  field.
//
////////////////////////////////////////////////////////////////////////////////

void ThirteenSectorCodec::AppendSector (
    vector<Byte>           & bits,
    size_t                 & bitOffset,
    Byte                     volume,
    Byte                     track,
    Byte                     sector,
    std::span<const Byte>    bytes)
{
    static constexpr int  kSyncBeforeAddress = 24;
    static constexpr int  kSyncBeforeData    = 12;



    const Byte         address[] = { volume, track, sector, DiskFieldFormat::ComputeAddressChecksum (volume, track, sector) };
    std::vector<Byte>  body;
    Byte               checksum  = 0;
    Byte               odd       = 0;
    Byte               even      = 0;



    AppendSync (bits, bitOffset, kSyncBeforeAddress);
    AppendMark (bits, bitOffset, DiskFieldFormat::GetAddressPrologue (DiskFieldKind::Thirteen));

    for (Byte value : address)
    {
        DiskFieldFormat::Encode44 (value, odd, even);
        AppendNibble (bits, bitOffset, odd);
        AppendNibble (bits, bitOffset, even);
    }

    AppendMark (bits, bitOffset, DiskFieldFormat::GetEpilogue());
    AppendSync (bits, bitOffset, kSyncBeforeData);
    AppendMark (bits, bitOffset, DiskFieldFormat::GetDataPrologue());

    DiskFieldFormat::EncodeData (DiskFieldKind::Thirteen, bytes, body, checksum);

    for (Byte nibble : body)
    {
        AppendNibble (bits, bitOffset, nibble);
    }

    AppendNibble (bits, bitOffset, checksum);
    AppendMark   (bits, bitOffset, DiskFieldFormat::GetEpilogue());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThirteenSectorCodec::Nibblize
//
//  Sectors are laid down in the order DOS 3.2's formatter writes them.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ThirteenSectorCodec::Nibblize (const vector<Byte> & sectors, Byte volume, DiskImage & out)
{
    static constexpr int  kSectorOrder[kSectorsPerTrack] = { 0, 10, 7, 4, 1, 11, 8, 5, 2, 12, 9, 6, 3 };



    HRESULT         hr    = S_OK;
    bool            sized = sectors.size() == kImageByteSize;
    int             track = 0;
    vector<Byte>  * bits  = nullptr;



    CBREx (sized, HRESULT_FROM_WIN32 (ERROR_BAD_LENGTH));

    out.EnsureTrackSlots (kTrackCount);

    for (track = 0; track < kTrackCount; track++)
    {
        size_t  bitOffset = 0;

        out.ResizeTrack (track, NibbleFormat::kTrackBitCapacity);

        bits = &out.GetTrackBitsForWrite (track);

        std::fill (bits->begin(), bits->end(), (Byte) 0);

        for (int sector : kSectorOrder)
        {
            size_t  base = ((size_t) track * kSectorsPerTrack + (size_t) sector) * kSectorByteSize;

            AppendSector (*bits, bitOffset, volume, (Byte) track, (Byte) sector,
                          std::span<const Byte> (sectors.data() + base, kSectorByteSize));
        }

        out.SetTrackBitCount (track, bitOffset);
    }

    out.ClearDirty();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThirteenSectorCodec::CollectChangedTracks
//
////////////////////////////////////////////////////////////////////////////////

void ThirteenSectorCodec::CollectChangedTracks (
    const vector<Byte>  & priorSectors,
    const vector<Byte>  & editedSectors,
    vector<int>         & outTracks)
{
    static constexpr size_t  kTrackBytes = kSectorsPerTrack * kSectorByteSize;



    int  track = 0;



    outTracks.clear();

    if (priorSectors.size() != kImageByteSize || editedSectors.size() != kImageByteSize)
    {
        return;
    }

    for (track = 0; track < kTrackCount; track++)
    {
        ptrdiff_t  base = (ptrdiff_t) ((size_t) track * kTrackBytes);

        if (!std::equal (priorSectors.begin() + base, priorSectors.begin() + base + (ptrdiff_t) kTrackBytes, editedSectors.begin() + base))
        {
            outTracks.push_back (track);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThirteenSectorCodec::CollectSectorWrites
//
//  One write per changed sector. The buffer is in physical sector order, so
//  the sector of the buffer is the sector of the address field.
//
////////////////////////////////////////////////////////////////////////////////

void ThirteenSectorCodec::CollectSectorWrites (
    const vector<Byte>   & priorSectors,
    const vector<Byte>   & editedSectors,
    vector<SectorWrite>  & outWrites)
{
    int          track  = 0;
    int          sector = 0;
    SectorWrite  write;



    outWrites.clear();

    if (priorSectors.size() != kImageByteSize || editedSectors.size() != kImageByteSize)
    {
        return;
    }

    for (track = 0; track < kTrackCount; track++)
    {
        for (sector = 0; sector < kSectorsPerTrack; sector++)
        {
            ptrdiff_t  base = (ptrdiff_t) (((size_t) track * kSectorsPerTrack + (size_t) sector) * kSectorByteSize);

            if (std::equal (priorSectors.begin() + base, priorSectors.begin() + base + (ptrdiff_t) kSectorByteSize, editedSectors.begin() + base))
            {
                continue;
            }

            write        = SectorWrite();
            write.track  = track;
            write.sector = (Byte) sector;
            std::copy (editedSectors.begin() + base, editedSectors.begin() + base + (ptrdiff_t) kSectorByteSize, write.bytes.begin());

            outWrites.push_back (write);
        }
    }
}
