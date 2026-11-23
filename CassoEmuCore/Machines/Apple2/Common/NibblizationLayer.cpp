#include "Pch.h"

#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Devices/Disk/DiskFieldFormat.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/NibbleFieldWriter.h"
#include "Machines/Apple2/Common/SectorInterleave.h"
#include "Machines/Apple2/Common/TrackBitReader.h"





static constexpr Byte  kAddrProlog0           = 0xD5;
static constexpr Byte  kAltAddrProlog0        = 0xD4;   // Ultima I's address fields
static constexpr Byte  kAddrProlog1           = 0xAA;
static constexpr Byte  kAddrProlog2           = 0x96;
static constexpr Byte  kDataProlog2           = 0xAD;





////////////////////////////////////////////////////////////////////////////////
//
//  GetAddressVolume
//
//  The volume a disk's address fields have: a DOS 3.3 VTOC's, as INIT writes
//  it, or 254 for any other disk. The VTOC is DOS sector 0 of track 17, which
//  is physical sector 0 in either order.
//
////////////////////////////////////////////////////////////////////////////////

static Byte GetAddressVolume (const vector<Byte> & raw, const int * interleave)
{
    size_t  vtoc   = static_cast<size_t> (Dos33Skeleton::kVtocTrack * NibbleFormat::kSectorsPerTrack + interleave[0]) * NibbleFormat::kSectorByteSize;
    Byte    volume = raw[vtoc + Dos33Skeleton::kVtocOffVolumeNumber];
    bool    isVtoc = raw[vtoc + Dos33Skeleton::kVtocOffCatalogTrack] == Dos33Skeleton::kVtocTrack
                  && raw[vtoc + Dos33Skeleton::kVtocOffMaxTsPairs]   == Dos33Skeleton::kMaxTsPairs
                  && raw[vtoc + Dos33Skeleton::kVtocOffTrackCount]   == NibbleFormat::kTrackCount
                  && raw[vtoc + Dos33Skeleton::kVtocOffSectorCount]  == NibbleFormat::kSectorsPerTrack;



    return (isVtoc && volume != 0 && volume != 0xFF) ? volume : NibbleFormat::kDefaultVolume;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblizeWithMap
//
//  Common nibblization driver. The address fields go down in sequential
//  order, so the loop variable is the physical sector; `interleave` names the
//  file offset whose 256 bytes belong under that address mark.
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT NibblizeWithMap (
    const vector<Byte>  &  raw,
    const int           *  interleave,
    DiskImage           &  out)
{
    HRESULT   hr        = S_OK;
    int       track     = 0;
    int       physical  = 0;
    size_t    offset    = 0;
    size_t    rawSize   = 0;
    Byte      volume    = 0;



    rawSize = raw.size();

    // A WRONG-SIZED BUFFER IS USER INPUT HERE, not a caller bug. This guard
    // asserted for as long as the only callers were trusted; then the live
    // mount path started handing it whatever file was dropped on a drive, and
    // a 4 KB .dsk raised an assertion dialog in a Debug build before anything
    // could report it. E_INVALIDARG marks a coding error in this tree and
    // always asserts, so it is the wrong verdict for a file somebody
    // downloaded.
    //
    // ERROR_BAD_LENGTH rather than a bare failure: the caller has to be able
    // to tell a wrong size from every other way a load can go wrong, or the
    // message it writes cannot say which one happened.
    CBREx (rawSize == NibbleFormat::kImageByteSize, HRESULT_FROM_WIN32 (ERROR_BAD_LENGTH));

    volume = GetAddressVolume (raw, interleave);

    for (track = 0; track < NibbleFormat::kTrackCount; track++)
    {
        size_t   bitOffset = 0;

        out.ResizeTrack (track, NibbleFormat::kTrackBitCapacity);

        for (physical = 0; physical < NibbleFormat::kSectorsPerTrack; physical++)
        {
            offset = static_cast<size_t> (track * NibbleFormat::kSectorsPerTrack + interleave[physical])
                   * NibbleFormat::kSectorByteSize;

            NibbleFieldWriter::AppendAddressField (out.GetTrackBitsForWrite (track), bitOffset,
                                                   volume,
                                                   static_cast<Byte> (track),
                                                   static_cast<Byte> (physical));
            NibbleFieldWriter::AppendDataField    (out.GetTrackBitsForWrite (track), bitOffset, &raw[offset]);
        }

        // Trim the track to what we actually wrote so the engine wraps
        // back to the address-field sync gap (real Disk II behavior),
        // not into a run of zero bits left over from the resize.
        out.SetTrackBitCount (track, bitOffset);
    }

    out.ClearDirty();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblizationLayer::RenibblizeTracks
//
//  Re-encodes only the named tracks, leaving every other track's packed bits
//  exactly as they are.
//
//  This is what makes a write to a bit-stream image safe. Re-encoding the whole
//  image would replace all 35 tracks with freshly synthesized standard nibbles,
//  discarding timing, sync patterns, and weak bits everywhere -- including on
//  tracks the operation never touched. A 512-byte file has no business
//  rewriting a disk.
//
//  Tracks outside the image are skipped rather than treated as an error: the
//  caller names the tracks its edit landed on, and an image shorter than the
//  nominal geometry simply has nothing at those positions.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NibblizationLayer::RenibblizeTracks (
    const vector<Byte>    &  sectors,
    DiskFormat               fmt,
    std::span<const int>     tracks,
    DiskImage             &  inOutImage)
{
    HRESULT       hr         = S_OK;
    const int *   interleave = nullptr;
    size_t        rawSize    = sectors.size();
    size_t        i          = 0;
    int           track      = 0;
    int           physical   = 0;
    int           trackCount = inOutImage.GetTrackCount();
    Byte          volume     = 0;



    CBRAEx (rawSize == (size_t) NibbleFormat::kImageByteSize, E_INVALIDARG);

    switch (fmt)
    {
        case DiskFormat::Dsk: interleave = SectorInterleave::kDsk_PhysicalToFile;  break;
        case DiskFormat::Do:  interleave = SectorInterleave::kDsk_PhysicalToFile;  break;
        case DiskFormat::Po:  interleave = SectorInterleave::kPo_PhysicalToFile;   break;
        case DiskFormat::Woz: interleave = SectorInterleave::kDsk_PhysicalToFile;  break;
        default:              hr         = E_INVALIDARG;         break;
    }

    CHR (hr);

    volume = GetAddressVolume (sectors, interleave);

    for (i = 0; i < tracks.size(); i++)
    {
        size_t  bitOffset = 0;

        track = tracks[i];

        if (track < 0 || track >= NibbleFormat::kTrackCount || track >= trackCount)
        {
            continue;
        }

        //  A flux track is never regenerated: only the data fields that
        //  changed are spliced in, and everything else keeps its timing.
        if (inOutImage.GetTrackKind (track) == TrackKind::Flux)
        {
            hr = WriteFluxTrackSectors (sectors, interleave, track, inOutImage);
            CHR (hr);
            continue;
        }

        inOutImage.ResizeTrack (track, NibbleFormat::kTrackBitCapacity);

        for (physical = 0; physical < NibbleFormat::kSectorsPerTrack; physical++)
        {
            size_t  offset = static_cast<size_t> (track * NibbleFormat::kSectorsPerTrack + interleave[physical])
                           * NibbleFormat::kSectorByteSize;

            NibbleFieldWriter::AppendAddressField (inOutImage.GetTrackBitsForWrite (track), bitOffset,
                                                   volume,
                                                   static_cast<Byte> (track),
                                                   static_cast<Byte> (physical));
            NibbleFieldWriter::AppendDataField    (inOutImage.GetTrackBitsForWrite (track), bitOffset, &sectors[offset]);
        }

        inOutImage.SetTrackBitCount (track, bitOffset);

        //  This track's bits were just replaced. Saying so is what lets a
        //  writer that copies untouched tracks tell them from rewritten ones.
        inOutImage.MarkTrackDirty (track);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblizationLayer::GetImageVolume
//
////////////////////////////////////////////////////////////////////////////////

Byte NibblizationLayer::GetImageVolume (const vector<Byte> & sectors)
{
    bool  isWhole = sectors.size() >= (size_t) NibbleFormat::kImageByteSize;



    return isWhole ? GetAddressVolume (sectors, SectorInterleave::kDsk_PhysicalToFile) : NibbleFormat::kDefaultVolume;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblizationLayer::NibblizeDsk / NibblizeDo / NibblizePo
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NibblizationLayer::NibblizeDsk (const vector<Byte> & raw, DiskImage & out)
{
    HRESULT   hr = NibblizeWithMap (raw, SectorInterleave::kDsk_PhysicalToFile, out);



    if (SUCCEEDED (hr))
    {
        out.SetSourceFormat (DiskFormat::Dsk);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblizeDo
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NibblizationLayer::NibblizeDo (const vector<Byte> & raw, DiskImage & out)
{
    HRESULT   hr = NibblizeWithMap (raw, SectorInterleave::kDsk_PhysicalToFile, out);



    if (SUCCEEDED (hr))
    {
        out.SetSourceFormat (DiskFormat::Do);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblizePo
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NibblizationLayer::NibblizePo (const vector<Byte> & raw, DiskImage & out)
{
    HRESULT   hr = NibblizeWithMap (raw, SectorInterleave::kPo_PhysicalToFile, out);



    if (SUCCEEDED (hr))
    {
        out.SetSourceFormat (DiskFormat::Po);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Nibblize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NibblizationLayer::Nibblize (const vector<Byte> & raw, DiskFormat fmt, DiskImage & out)
{
    HRESULT  hr = S_OK;



    switch (fmt)
    {
        case DiskFormat::Dsk: hr = NibblizeDsk (raw, out); break;
        case DiskFormat::Do:  hr = NibblizeDo  (raw, out); break;
        case DiskFormat::Po:  hr = NibblizePo  (raw, out); break;
        default:              hr = E_INVALIDARG;           break;
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PopCount
//
//  Set bits in a 16-sector mask.
//
////////////////////////////////////////////////////////////////////////////////

static int PopCount (uint16_t mask)
{
    int   count = 0;
    int   bit   = 0;



    for (bit = 0; bit < 16; bit++)
    {
        if ((mask & (1 << bit)) != 0)
        {
            count++;
        }
    }

    return count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HasDataField
//
//  Whether one revolution of the track has a data field's prolog, D5 AA AD.
//
////////////////////////////////////////////////////////////////////////////////

static bool HasDataField (const TrackBitReader::TrackBits & src)
{
    size_t  bitPos = 0;
    Byte    n0     = 0;
    Byte    n1     = 0;
    Byte    n2     = 0;
    bool    found  = false;



    while (!found && src.bitCount != 0 && bitPos < src.bitCount)
    {
        n0    = n1;
        n1    = n2;
        n2    = TrackBitReader::ReadNibbleAt (src, bitPos);
        found = n0 == kAddrProlog0 && n1 == kAddrProlog1 && n2 == kDataProlog2;
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DecodeOneSector
//
//  Walks the bit stream until it locates the next address-field prologue,
//  then decodes the V/T/S header followed by the data field. On success
//  fills outSectorData[256] and returns the address-field track/sector via
//  out params.
//
//  Success means VERIFIED, not merely parsed. Four things are checked, three
//  of which the format was already carrying and this code used to discard:
//
//    * the address field's own checksum, which says the sector number can be
//      trusted (it was read into locals and thrown away);
//    * that a data field turns up before the next address field, so a sector
//      whose data is missing cannot silently take its neighbor's;
//    * that every data nibble is a legal 6-and-2 code;
//    * the data field's 343rd checksum nibble, the boot ROM's success gate
//      (it was never read at all).
//
//  Returns E_FAIL if any of those fail, or if no sector can be found before
//  the cursor wraps. The caller distinguishes the two by whether the cursor
//  moved.
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT DecodeOneSector (
    const TrackBitReader::TrackBits  &  src,
    size_t                              &  bitPos,
    Byte                                &  outSector,
    Byte                                *  outData,
    SectorOutcome                       &  outcome,
    size_t                              &  outFieldStart,
    size_t                              &  outDataFieldStart)
{
    HRESULT          hr                                    = S_OK;
    Byte             n0                                    = 0;
    Byte             n1                                    = 0;
    Byte             n2                                    = 0;
    Byte             vOdd                                  = 0;
    Byte             vEven                                 = 0;
    Byte             tOdd                                  = 0;
    Byte             tEven                                 = 0;
    Byte             sOdd                                  = 0;
    Byte             sEven                                 = 0;
    Byte             cOdd                                  = 0;
    Byte             cEven                                 = 0;
    Byte             foundProlog                           = 0;
    Byte             field[NibbleFormat::kDataNibbleCount] = {};
    Byte             addrVolume                            = 0;
    Byte             addrTrack                             = 0;
    Byte             addrChecksum                          = 0;
    DataFieldDecode  decode;
    size_t           posBeforeProlog                       = 0;
    bool             addrChecksumOk                        = false;
    bool             dataFieldMissing                      = false;
    bool             dataTrusted                           = false;
    size_t           i                                     = 0;
    size_t           trackBits                             = src.bitCount;
    size_t           startBitPos                           = bitPos;
    size_t           bitsConsumed                          = 0;
    size_t           addrFieldStart                        = 0;



    CBR (trackBits != 0);

    foundProlog = 0;

    outFieldStart     = SIZE_MAX;
    outDataFieldStart = SIZE_MAX;

    while (foundProlog == 0)
    {
        addrFieldStart = bitPos;
        n0             = TrackBitReader::ReadNibbleAt (src, bitPos);

        if (n0 != kAddrProlog0 && n0 != kAltAddrProlog0)
        {
            bitsConsumed = (bitPos - startBitPos);
            CBR (bitsConsumed <= trackBits + 8);
            continue;
        }

        n1 = TrackBitReader::ReadNibbleAt (src, bitPos);
        n2 = TrackBitReader::ReadNibbleAt (src, bitPos);

        if (n1 == kAddrProlog1 && n2 == kAddrProlog2)
        {
            foundProlog = 1;
        }
    }

    //  WHERE THE HEADER WAS, not where the cursor ended up. The caller bounds
    //  the scan at one revolution and has to test the header's position to do
    //  it: the cursor can still sit inside the gap trailing the last sector, so
    //  bounding on the cursor lets one more attempt wrap onto a header already
    //  read and report it as a duplicate the disk does not have.
    outFieldStart = addrFieldStart;

    vOdd  = TrackBitReader::ReadNibbleAt (src, bitPos);
    vEven = TrackBitReader::ReadNibbleAt (src, bitPos);
    tOdd  = TrackBitReader::ReadNibbleAt (src, bitPos);
    tEven = TrackBitReader::ReadNibbleAt (src, bitPos);
    sOdd  = TrackBitReader::ReadNibbleAt (src, bitPos);
    sEven = TrackBitReader::ReadNibbleAt (src, bitPos);
    cOdd  = TrackBitReader::ReadNibbleAt (src, bitPos);
    cEven = TrackBitReader::ReadNibbleAt (src, bitPos);

    outSector    = DiskFieldFormat::Decode44 (sOdd, sEven);
    addrVolume   = DiskFieldFormat::Decode44 (vOdd, vEven);
    addrTrack    = DiskFieldFormat::Decode44 (tOdd, tEven);
    addrChecksum = DiskFieldFormat::Decode44 (cOdd, cEven);

    // The address field carries its own checksum, and it was being read and
    // thrown away. It is the only thing that says the sector NUMBER can be
    // trusted -- without it a corrupted address field files a good sector's
    // data under whatever number the damage happened to spell.
    //
    // Only the field's internal consistency is checked, not whether its track
    // matches the one being read: half-track and quarter-track images
    // legitimately carry a different number there.
    // Lost until something proves otherwise, so every early exit below
    // classifies correctly without having to remember to say so.
    outcome = SectorOutcome::Lost;

    addrChecksumOk = (addrChecksum == (addrVolume ^ addrTrack ^ outSector));
    BAIL_OUT_IF (!addrChecksumOk, S_OK);

    foundProlog = 0;

    while (foundProlog == 0)
    {
        posBeforeProlog = bitPos;
        n0              = TrackBitReader::ReadNibbleAt (src, bitPos);

        if (n0 != kAddrProlog0 && n0 != kAltAddrProlog0)
        {
            bitsConsumed = (bitPos - startBitPos);
            CBR (bitsConsumed <= trackBits + 8);
            continue;
        }

        n1 = TrackBitReader::ReadNibbleAt (src, bitPos);
        n2 = TrackBitReader::ReadNibbleAt (src, bitPos);

        if (n0 == kAddrProlog0 && n1 == kAddrProlog1 && n2 == kDataProlog2)
        {
            foundProlog = 1;
        }
        else if (n1 == kAddrProlog1 && n2 == kAddrProlog2)
        {
            // The NEXT sector's address field, reached without ever finding
            // this one's data field. Running on to take that sector's data
            // is how one point of damage used to corrupt two sectors: the
            // bytes decode cleanly and pass their own checksum, they are
            // simply filed under the wrong number.
            //
            // Rewind so the caller's next attempt starts on this address
            // field rather than past it, or the good sector after a damaged
            // one is lost too.
            bitPos           = posBeforeProlog;
            dataFieldMissing = true;
            break;
        }
    }

    BAIL_OUT_IF (dataFieldMissing, S_OK);

    // Where this sector's data field begins, which a write that replaces only
    // the data field needs.
    outDataFieldStart = posBeforeProlog;

    for (i = 0; i < NibbleFormat::kDataNibbleCount; i++)
    {
        field[i] = TrackBitReader::ReadNibbleAt (src, bitPos);
    }

    // An illegal nibble makes the sector untrustworthy, but decoding goes on:
    // the bytes before it are the disk's, and a salvage writes them. The
    // 343rd nibble is the data field's checksum, the boot ROM's success gate.
    DiskFieldFormat::DecodeData (DiskFieldKind::Sixteen,
                                 std::span<const Byte> (field, DiskFieldFormat::kBodyLength62),
                                 field[DiskFieldFormat::kBodyLength62],
                                 decode);

    dataTrusted = decode.isChecksumGood && decode.invalidNibbles.empty();
    outcome     = dataTrusted ? SectorOutcome::Verified : SectorOutcome::Recovered;

    memcpy (outData, decode.bytes.data(), NibbleFormat::kSectorByteSize);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindSectorBase
//
//  The number a track's address fields count their sectors from: 0, or the
//  lowest number when every sector of the track is numbered 16 or more and
//  the track has sixteen of them in a row. Ultima V numbers its sectors
//  $11 to $20 from track 17 on.
//
////////////////////////////////////////////////////////////////////////////////

static Byte FindSectorBase (const TrackBitReader::TrackBits & src)
{
    size_t         bitPos                                = 0;
    size_t         field                                 = SIZE_MAX;
    size_t         data                                  = SIZE_MAX;
    Byte           sector                                = 0;
    Byte           lowest                                = 0xFF;
    Byte           highest                               = 0;
    Byte           base                                  = 0;
    Byte           buffer[NibbleFormat::kSectorByteSize] = {};
    SectorOutcome  outcome                               = SectorOutcome::Lost;
    bool           low                                   = false;
    bool           seen[256]                             = {};
    int            count                                 = 0;
    bool           more                                  = src.bitCount != 0;



    while (more && bitPos < src.bitCount)
    {
        HRESULT  hr = DecodeOneSector (src, bitPos, sector, buffer, outcome, field, data);

        more = SUCCEEDED (hr) && field < src.bitCount;

        if (more && outcome != SectorOutcome::Lost)
        {
            count        += seen[sector] ? 0 : 1;
            seen[sector]  = true;
            low           = low || sector < NibbleFormat::kSectorsPerTrack;
            lowest        = std::min (lowest, sector);
            highest       = std::max (highest, sector);
        }
    }

    if (!low && count == NibbleFormat::kSectorsPerTrack && highest - lowest == NibbleFormat::kSectorsPerTrack - 1)
    {
        base = lowest;
    }

    return base;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Denibblize
//
//  Walk every track's bit stream and recover its 16 sectors into the
//  flat output buffer. The output layout is the inverse of the matching
//  Nibblize call: DSK/DO use the DOS interleave; PO uses the ProDOS map.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NibblizationLayer::Denibblize (const DiskImage & img, DiskFormat fmt, vector<Byte> & out)
{
    HRESULT             hr          = S_OK;
    SectorDecodeReport  coverage;
    bool                lostSectors = false;



    hr = Denibblize (img, fmt, out, coverage);
    CHR (hr);

    //  THE REPORTLESS SIGNATURE IS NOT AN ESCAPE HATCH. It exists for callers
    //  that do not want the detail, not for callers that want to ignore it, and
    //  DiskImage::Serialize -- the flush path, where silent truncation cost data
    //  in the first place -- is the one production caller.
    //
    //  IT ASKS COVERAGE RATHER THAN THE SECTOR COUNTS, and the difference is a
    //  track whose sixteen slots are all filled because one sector arrived
    //  twice. The counts read that as complete; coverage reads it as a slot
    //  claimed by a sector that cannot be the one belonging to it.
    lostSectors = coverage.HasDataLoss();
    CBR (!lostSectors);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Denibblize
//
//  Decode every track's bit stream back to a plain 143,360-byte sector image.
//
//  What used to lose data was the SILENCE, not the early stop. This returned
//  S_OK over a track it had only partly decoded, and the flush path wrote that
//  buffer over the user's file -- so a damaged track was saved as a mixture of
//  real sectors, zeroed sectors and wrong sectors, reported as a clean save.
//
//  Measured, on an image with one sector's data field destroyed: that sector
//  comes back as zeros, AND a second sector comes back holding the wrong data,
//  because the scan for the missing data field runs on and finds the NEXT
//  sector's, storing it under the sector number the address field gave. Two
//  sectors wrong from one point of damage, and nothing said so.
//
//  So the fix that matters is the report, and a partial track failing the whole
//  operation: the zeros and the misfiled sectors are lost data rather than
//  blank media, and refusing to save is the only answer that destroys nothing.
//  The caller keeps the image dirty and the existing file intact.
//
//  A track that decodes nothing at all is a different case and still succeeds:
//  an unformatted track in a sector image legitimately IS zeros.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NibblizationLayer::SalvageSectors (
    const DiskImage   &  img,
    DiskFormat           fmt,
    vector<Byte>      &  out,
    DenibblizeReport  &  report)
{
    HRESULT   hr = S_OK;



    hr = DecodeTracks (img, fmt, true, out, report, nullptr);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Denibblize
//
//  The same decode, reported as COVERAGE rather than as sector counts.
//
//  IT SUCCEEDS OVER DAMAGE ON PURPOSE. The strict overload refuses, because its
//  caller is about to write the buffer back over the user's file. This one is
//  for callers that can present a partial result -- listing a damaged disk,
//  extracting the files that survived -- and refusing would leave them with
//  nothing to show for sectors that read perfectly well.
//
//  Both reports come off ONE pass, so they cannot disagree about the same disk.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NibblizationLayer::Denibblize (
    const DiskImage     &  img,
    DiskFormat             fmt,
    vector<Byte>        &  out,
    SectorDecodeReport  &  outReport)
{
    HRESULT           hr      = S_OK;
    DenibblizeReport  sectors;



    hr = DecodeTracks (img, fmt, false, out, sectors, &outReport);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Denibblize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NibblizationLayer::Denibblize (
    const DiskImage   &  img,
    DiskFormat           fmt,
    vector<Byte>      &  out,
    DenibblizeReport  &  report)
{
    HRESULT   hr          = S_OK;
    bool      lostSectors = false;



    hr = DecodeTracks (img, fmt, false, out, report, nullptr);
    CHR (hr);

    // A half-decoded track means the output holds zeros where the user's data
    // should be. Saying so is the whole point: the caller writes this buffer
    // over their file, and once it lands nothing distinguishes it from a
    // clean save. Salvage takes the other entry point and does not come here.
    lostSectors = report.HasPartialTrack();
    CBR (!lostSectors);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DecodeTracks
//
//  The shared walk. keepRecovered decides what happens to a sector that
//  decoded but did not verify: the strict path drops it (so the track reads
//  as partial and the save is refused), salvage keeps its bytes.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NibblizationLayer::DecodeTracks (
    const DiskImage     &  img,
    DiskFormat             fmt,
    bool                   keepRecovered,
    vector<Byte>        &  out,
    DenibblizeReport    &  report,
    SectorDecodeReport  *  outCoverage)
{
    // Enough attempts to still find all sixteen good sectors when several
    // reads fail or repeat, without letting a hostile track spin forever.
    constexpr int  kMaxAttemptsPerTrack = NibbleFormat::kSectorsPerTrack * 2;
    constexpr int  kAllSectorsMask      = (1 << NibbleFormat::kSectorsPerTrack) - 1;



    HRESULT                      hr                                  = S_OK;
    const int                  * interleave                          = nullptr;
    int                          track                               = 0;
    int                          attempt                             = 0;
    int                          decoded                             = 0;
    uint16_t                     mask                                = 0;
    uint16_t                     recovered                           = 0;
    uint16_t                     lost                                = 0;
    uint16_t                     duplicated                          = 0;
    uint16_t                     slotMask                            = 0;
    uint16_t                     seenAtRevStart                      = 0;
    size_t                       revolutionEnd                       = 0;
    Byte                         outSector                           = 0;
    Byte                         sectorBase                          = 0;
    Byte                         data[NibbleFormat::kSectorByteSize] = {};
    size_t                       bitPos                              = 0;
    size_t                       offset                              = 0;
    size_t                       trackBits                           = 0;
    int                          trackLimit                          = img.GetTrackCount();
    FluxBitView                  fluxView;
    TrackBitReader::TrackBits    src;



    out.assign (NibbleFormat::kImageByteSize, 0);

    report = DenibblizeReport();

    switch (fmt)
    {
        case DiskFormat::Dsk: interleave = SectorInterleave::kDsk_PhysicalToFile;  break;
        case DiskFormat::Do:  interleave = SectorInterleave::kDsk_PhysicalToFile;  break;
        case DiskFormat::Po:  interleave = SectorInterleave::kPo_PhysicalToFile;   break;
        default:              hr         = E_INVALIDARG;         break;
    }

    CHR (hr);

    if (trackLimit > NibbleFormat::kTrackCount)
    {
        trackLimit = NibbleFormat::kTrackCount;
    }

    report.decodedSectorMask.assign (static_cast<size_t> (trackLimit), 0);

    if (outCoverage != nullptr)
    {
        outCoverage->Reset (trackLimit);
    }

    for (track = 0; track < trackLimit; track++)
    {
        bitPos     = 0;
        mask       = 0;
        recovered  = 0;
        lost       = 0;
        duplicated = 0;
        slotMask   = 0;
        src        = TrackBitReader::GetTrackBits (img, img.ResolveWholeTrack (track), fluxView);
        trackBits  = src.bitCount;
        sectorBase = FindSectorBase (src);

        //  A revolution that turns up nothing new means there is nothing left
        //  to find, and the scan can stop. Without this an EMPTY track is the
        //  worst case in the whole decoder: the mask never fills, so the loop
        //  below runs its full count, and each attempt scans a whole
        //  revolution looking for a prologue that is not there -- thirty-two
        //  passes over the same bits to conclude what the first one did.
        //
        //  It costs the damaged-media recovery nothing. That recovery works by
        //  arriving at a skipped sector from a different offset on the next
        //  time round, so it pays off within a revolution or two; a revolution
        //  that adds no sector, recovers none and loses none has met only
        //  headers it has already read.
        revolutionEnd  = trackBits;
        seenAtRevStart = 0;

        for (attempt = 0; attempt < kMaxAttemptsPerTrack && mask != kAllSectorsMask; attempt++)
        {
            //  AT THE TOP, because every failure path below continues, and a
            //  track with nothing on it takes all of them -- which is the one
            //  case this exists to cut short.
            if (bitPos >= revolutionEnd)
            {
                uint16_t  seen = static_cast<uint16_t> (mask | recovered | lost);

                if (seen == seenAtRevStart)
                {
                    break;
                }

                seenAtRevStart = seen;
                revolutionEnd  = bitPos + trackBits;
            }

            //  DUPLICATES ARE ONLY MEANINGFUL WITHIN ONE REVOLUTION. This loop
            //  deliberately keeps scanning after the cursor has been all the way
            //  round, because that is how it retries the sectors a damaged header
            //  cost it -- and a second pass necessarily meets headers it has
            //  already read. Counting those would invent damage the scan caused
            //  rather than report damage the disk carries.
            size_t         fieldStart = SIZE_MAX;
            size_t         dataStart  = SIZE_MAX;
            SectorOutcome  outcome    = SectorOutcome::Lost;
            HRESULT        hrSector   = DecodeOneSector (src, bitPos, outSector,
                                                         data, outcome, fieldStart, dataStart);
            bool           firstPass  = fieldStart < trackBits;

            // A hard failure means no address field could be found before the
            // cursor wrapped -- there is nothing further on this track. Keep
            // scanning anyway: the loop is bounded, and abandoning it here is
            // what would cost the sectors after a damaged one the moment the
            // decoder grows a per-sector failure mode. It has one now.
            if (FAILED (hrSector))
            {
                continue;
            }

            outSector = (outSector >= sectorBase) ? static_cast<Byte> (outSector - sectorBase) : outSector;

            if (outSector >= NibbleFormat::kSectorsPerTrack)
            {
                continue;
            }

            if (outcome == SectorOutcome::Lost)
            {
                lost = static_cast<uint16_t> (lost | (1 << outSector));
                continue;
            }

            if (outcome == SectorOutcome::Recovered)
            {
                recovered = static_cast<uint16_t> (recovered | (1 << outSector));

                if (!keepRecovered)
                {
                    continue;
                }
            }

            offset = static_cast<size_t> (track * NibbleFormat::kSectorsPerTrack + interleave[outSector])
                   * NibbleFormat::kSectorByteSize;

            memcpy (&out[offset], data, NibbleFormat::kSectorByteSize);

            if (outcome == SectorOutcome::Verified)
            {
                //  A SECOND COPY OF A SECTOR IS DAMAGE, AND SETTING THE BIT
                //  AGAIN HIDES IT. The buffer keeps whichever copy landed last,
                //  the mask is full either way, and every count below reads the
                //  track as complete -- so coverage has to be asked whether each
                //  slot was filled EXACTLY once, not merely whether it is full.
                if (firstPass && (mask & (1 << outSector)) != 0)
                {
                    duplicated = static_cast<uint16_t> (duplicated | (1 << outSector));
                }

                mask = static_cast<uint16_t> (mask | (1 << outSector));

                //  COVERAGE IS INDEXED BY OUTPUT SLOT, and the mask above is
                //  indexed by the number the address field carried. The two
                //  differ by the interleave, so a consumer holding the buffer
                //  and reading the mask directly would test the wrong sector
                //  and find zeros in a slot the report calls present.
                slotMask = static_cast<uint16_t> (slotMask | (1 << interleave[outSector]));
            }
        }

        report.decodedSectorMask[static_cast<size_t> (track)] = mask;

        //  UNFORMATTED IS DECIDED BY THE BITS, NOT BY THE MASK. A track that
        //  holds no bits at all was never written; a track that holds bits but
        //  yielded no sector is damaged, and calling that one blank is what
        //  licenses writing zeros over it.
        if (outCoverage != nullptr)
        {
            TrackDecodeOutcome  outcome = TrackDecodeOutcome::Partial;

            //  UNFORMATTED MEANS NO ADDRESS FIELD ANYWHERE, not "no bits". A
            //  wiped track still carries bits, and a track whose headers are all
            //  destroyed yields nothing at all -- while a track that yielded
            //  something and then failed is damaged, which is a different thing
            //  and must not license writing zeros over it.
            //  A track with data fields and no address field the decoder
            //  accepts was written: its sectors are lost, not blank.
            if (trackBits == 0 || (slotMask == 0 && recovered == 0 && lost == 0 && !HasDataField (src)))
            {
                outcome = TrackDecodeOutcome::Unformatted;
            }
            else if (slotMask == kAllSectorsMask && duplicated == 0)
            {
                outcome = TrackDecodeOutcome::Complete;
            }

            outCoverage->SetOutcome (track, outcome, slotMask, duplicated != 0);
        }

        if (trackBits == 0)
        {
            continue;
        }

        decoded = PopCount (mask);

        report.tracksPresent++;
        report.sectorsVerified  += decoded;
        report.sectorsRecovered += PopCount (recovered);
        report.sectorsLost      += PopCount (lost);

        if (decoded == 0)
        {
            report.tracksUnformatted++;
        }
        else if (decoded < NibbleFormat::kSectorsPerTrack)
        {
            report.tracksPartial++;
            report.sectorsMissing += NibbleFormat::kSectorsPerTrack - decoded;
        }
        else
        {
            report.tracksComplete++;
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblizationLayer::WriteFluxTrackSectors
//
//  A bit track takes a sector write by being regenerated, which costs it any
//  timing or weak bits it had. A flux track does not: each sector's data
//  field is found in the track decoded at the controller's cell, and only the
//  fields whose bytes changed are spliced back into the flux, at the cell a
//  drive writes. Address fields, gaps and every other sector keep the timing
//  they were recorded with.
//
//  The splice starts at the data prologue rather than the sync bytes before
//  it, so the sync already on the disk -- written at whatever speed the disk
//  was -- stays.
//
//  A changed sector whose data field cannot be found fails the write, rather
//  than regenerating the track to make room for it. A track with no sectors
//  at all is blank, so there is nothing to keep and a standard track is laid
//  down in its place.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NibblizationLayer::WriteFluxTrackSectors (
    const vector<Byte>  &  sectors,
    const int           *  interleave,
    int                    track,
    DiskImage           &  inOutImage)
{
    constexpr int     kMaxAttempts    = NibbleFormat::kSectorsPerTrack * 2;
    constexpr size_t  kSyncNibbleBits = 8 + NibbleFieldWriter::kSyncTailZeros;
    constexpr size_t  kMaxLeadZeros   = 16;



    HRESULT                    hr                                  = S_OK;
    FluxBitView                view;
    TrackBitReader::TrackBits  src                                 = TrackBitReader::GetTrackBits (inOutImage, track, view);
    size_t                     bitPos                              = 0;
    size_t                     start                               = 0;
    size_t                     fieldBits                           = 0;
    size_t                     offset                              = 0;
    size_t                     lead                                = 0;
    int                        attempt                             = 0;
    int                        physical                            = 0;
    uint16_t                   found                               = 0;
    uint16_t                   writeMask                           = 0;
    Byte                       outSector                           = 0;
    Byte                       data[NibbleFormat::kSectorByteSize] = {};
    bool                       changed                             = false;
    bool                       located                             = false;
    vector<size_t>   dataStart (NibbleFormat::kSectorsPerTrack, SIZE_MAX);
    vector<Byte>     priorData (static_cast<size_t> (NibbleFormat::kSectorsPerTrack) * NibbleFormat::kSectorByteSize, 0);
    vector<Byte>     field;
    vector<uint8_t>  bits;



    for (attempt = 0; attempt < kMaxAttempts && found != 0xFFFF && src.bitCount > 0; attempt++)
    {
        size_t         fieldStart = SIZE_MAX;
        size_t         dataAt     = SIZE_MAX;
        SectorOutcome  outcome    = SectorOutcome::Lost;
        HRESULT        hrSector   = DecodeOneSector (src, bitPos, outSector, data, outcome, fieldStart, dataAt);

        if (FAILED (hrSector) || outcome == SectorOutcome::Lost || dataAt == SIZE_MAX)
        {
            continue;
        }

        if (outSector >= NibbleFormat::kSectorsPerTrack || (found & (1 << outSector)) != 0)
        {
            continue;
        }

        found = static_cast<uint16_t> (found | (1 << outSector));
        dataStart[outSector] = dataAt;
        memcpy (&priorData[static_cast<size_t> (outSector) * NibbleFormat::kSectorByteSize], data, NibbleFormat::kSectorByteSize);
    }

    if (found == 0)
    {
        WriteBlankFluxTrack (sectors, interleave, track, inOutImage);
    }

    // Every changed sector is found before any is written, so a write that
    // cannot be completed leaves the track exactly as it was.
    for (physical = 0; found != 0 && physical < NibbleFormat::kSectorsPerTrack; physical++)
    {
        offset  = static_cast<size_t> (track * NibbleFormat::kSectorsPerTrack + interleave[physical]) * NibbleFormat::kSectorByteSize;
        changed = memcmp (&sectors[offset], &priorData[static_cast<size_t> (physical) * NibbleFormat::kSectorByteSize], NibbleFormat::kSectorByteSize) != 0;
        located = (dataStart[physical] != SIZE_MAX);

        CBREx (!changed || located, HRESULT_FROM_WIN32 (ERROR_SECTOR_NOT_FOUND));

        writeMask = static_cast<uint16_t> (writeMask | (changed ? (1 << physical) : 0));
    }

    for (physical = 0; physical < NibbleFormat::kSectorsPerTrack; physical++)
    {
        if ((writeMask & (1 << physical)) == 0)
        {
            continue;
        }

        offset = static_cast<size_t> (track * NibbleFormat::kSectorsPerTrack + interleave[physical]) * NibbleFormat::kSectorByteSize;

        field.assign (NibbleFormat::kTrackBitCapacity / 8, 0);
        fieldBits = 0;
        NibbleFieldWriter::AppendDataField (field, fieldBits, &sectors[offset]);
        TrackBitReader::UnpackBits (field, NibbleFieldWriter::kDataPrologueGap * kSyncNibbleBits, fieldBits, bits);

        // The decoder's position can sit in the zero tail of the sync byte
        // before the prologue; the field itself starts at its first 1 bit.
        start = dataStart[physical];

        for (lead = 0; lead < kMaxLeadZeros && TrackBitReader::GetBit (src, start) == 0; lead++)
        {
            start++;
        }

        inOutImage.SpliceFluxBulk (track, view.GetTickForBit (start), bits);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblizationLayer::WriteBlankFluxTrack
//
//  A flux track with no sectors on it has no timing worth keeping, so it gets
//  a whole standard track written at the controller's cell.
//
////////////////////////////////////////////////////////////////////////////////

void NibblizationLayer::WriteBlankFluxTrack (
    const vector<Byte>  &  sectors,
    const int           *  interleave,
    int                    track,
    DiskImage           &  inOutImage)
{
    vector<Byte>     field (NibbleFormat::kTrackBitCapacity / 8, 0);
    vector<uint8_t>  bits;
    size_t           fieldBits = 0;
    size_t           offset    = 0;
    int              physical  = 0;



    for (physical = 0; physical < NibbleFormat::kSectorsPerTrack; physical++)
    {
        offset = static_cast<size_t> (track * NibbleFormat::kSectorsPerTrack + interleave[physical]) * NibbleFormat::kSectorByteSize;

        NibbleFieldWriter::AppendAddressField (field, fieldBits, NibbleFormat::kDefaultVolume, static_cast<Byte> (track), static_cast<Byte> (physical));
        NibbleFieldWriter::AppendDataField    (field, fieldBits, &sectors[offset]);
    }

    TrackBitReader::UnpackBits (field, 0, fieldBits, bits);
    inOutImage.SpliceFluxBulk (track, 0, bits);
}
