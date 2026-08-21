#include "Pch.h"

#include "DirectBootBuilder.h"

#include "Dos33Skeleton.h"





////////////////////////////////////////////////////////////////////////////////
//
//  The boot sector, assembled.
//
//  The Disk II boot ROM reads track 0's first sector into $0800, then keeps
//  reading successive sectors into successive pages for as long as the sector
//  it is about to fetch is below the byte at $0800 -- and finally jumps to
//  $0801. Byte 0 below is therefore 1: exactly this sector, and then us.
//
//  Every later pass is the same trick used against the ROM rather than by it.
//  The loader points $26/$27 at the next page to fill, $3D at the first sector
//  on the track, $41 at the track the address fields will carry, writes the
//  count it wants into $0800, and jumps back into the ROM's read entry. The
//  ROM reads that many sectors and jumps to $0801 again, where the loader
//  picks up where it left off. There is no first-time branch anywhere in it:
//  the four variables it works from arrive as part of the sector, already set
//  to what a first pass needs.
//
//      0800  01                  .byte 1        ; the ROM's terminating count
//      0801  AD F0 08    enter   LDA sectorsLeft
//      0804  F0 43               BEQ done
//      0806  20 4F 08            JSR stepTrack
//      0809  AD F2 08            LDA track
//      080C  85 41               STA $41        ; the address field must agree
//      080E  A9 00               LDA #$00
//      0810  85 3D               STA $3D        ; first sector on this track
//      0812  85 26               STA $26        ; whole pages only
//      0814  AD F1 08            LDA curPage
//      0817  85 27               STA $27
//      0819  AD F0 08            LDA sectorsLeft
//      081C  C9 10               CMP #16
//      081E  90 02               BCC part
//      0820  A9 10               LDA #16
//      0822  8D 00 08    part    STA $0800      ; where the ROM's loop stops
//      0825  18                  CLC
//      0826  6D F1 08            ADC curPage
//      0829  8D F1 08            STA curPage
//      082C  AD F0 08            LDA sectorsLeft
//      082F  38                  SEC
//      0830  ED 00 08            SBC $0800
//      0833  8D F0 08            STA sectorsLeft
//      0836  A5 2B               LDA $2B        ; slot * 16, left by the ROM
//      0838  4A 4A 4A 4A         LSR / LSR / LSR / LSR
//      083C  09 C0               ORA #$C0
//      083E  85 3F               STA $3F
//      0840  A9 5C               LDA #$5C
//      0842  85 3E               STA $3E        ; -> $Cs5C, the ROM's reader
//      0844  A6 2B               LDX $2B
//      0846  6C 3E 00            JMP ($3E)
//      0849  BD 88 C0    done    LDA $C088,X    ; motor off; the payload owns
//      084C  4C 00 00            JMP entry      ;   the machine from here
//      084F  A0 02       stepTrack LDY #$02     ; two half-steps make a track
//      0851  AD F3 08    step    LDA phase
//      0854  0A                  ASL
//      0855  05 2B               ORA $2B
//      0857  AA                  TAX
//      0858  BD 80 C0            LDA $C080,X    ; the magnet now on, off
//      085B  EE F3 08            INC phase
//      085E  AD F3 08            LDA phase
//      0861  29 03               AND #$03
//      0863  8D F3 08            STA phase
//      0866  0A                  ASL
//      0867  05 2B               ORA $2B
//      0869  AA                  TAX
//      086A  BD 81 C0            LDA $C081,X    ; the next one on
//      086D  A9 56               LDA #$56       ; the settle the ROM itself
//      086F  20 A8 FC            JSR $FCA8      ;   uses while recalibrating
//      0872  88                  DEY
//      0873  D0 DC               BNE step
//      0875  EE F2 08            INC track
//      0878  60                  RTS
//
//  The head arrives at track 0 with magnet 0 energized, because that is how
//  the ROM's recalibrate loop ends, so `phase` and `track` both start at zero
//  and the first pass steps to track 1 before reading anything. That is why
//  the payload begins on track 1 rather than in track 0's fifteen spare
//  sectors: a pass that sometimes reads a partial track and sometimes a whole
//  one costs more code than fifteen sectors are worth on a disk where memory,
//  not media, is the limit.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr Byte  s_kBootLoader[] =
{
    0x01,
    0xAD, 0xF0, 0x08,
    0xF0, 0x43,
    0x20, 0x4F, 0x08,
    0xAD, 0xF2, 0x08,
    0x85, 0x41,
    0xA9, 0x00,
    0x85, 0x3D,
    0x85, 0x26,
    0xAD, 0xF1, 0x08,
    0x85, 0x27,
    0xAD, 0xF0, 0x08,
    0xC9, 0x10,
    0x90, 0x02,
    0xA9, 0x10,
    0x8D, 0x00, 0x08,
    0x18,
    0x6D, 0xF1, 0x08,
    0x8D, 0xF1, 0x08,
    0xAD, 0xF0, 0x08,
    0x38,
    0xED, 0x00, 0x08,
    0x8D, 0xF0, 0x08,
    0xA5, 0x2B,
    0x4A, 0x4A, 0x4A, 0x4A,
    0x09, 0xC0,
    0x85, 0x3F,
    0xA9, 0x5C,
    0x85, 0x3E,
    0xA6, 0x2B,
    0x6C, 0x3E, 0x00,
    0xBD, 0x88, 0xC0,
    0x4C, 0x00, 0x00,
    0xA0, 0x02,
    0xAD, 0xF3, 0x08,
    0x0A,
    0x05, 0x2B,
    0xAA,
    0xBD, 0x80, 0xC0,
    0xEE, 0xF3, 0x08,
    0xAD, 0xF3, 0x08,
    0x29, 0x03,
    0x8D, 0xF3, 0x08,
    0x0A,
    0x05, 0x2B,
    0xAA,
    0xBD, 0x81, 0xC0,
    0xA9, 0x56,
    0x20, 0xA8, 0xFC,
    0x88,
    0xD0, 0xDC,
    0xEE, 0xF2, 0x08,
    0x60,
};





////////////////////////////////////////////////////////////////////////////////
//
//  DirectBootBuilder::FormatAddress
//
////////////////////////////////////////////////////////////////////////////////

std::string DirectBootBuilder::FormatAddress (Word value)
{
    char  text[8] = {};



    snprintf (text, sizeof (text), "$%04X", (unsigned) value);

    return std::string (text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DirectBootBuilder::DescribeWindow
//
////////////////////////////////////////////////////////////////////////////////

std::string DirectBootBuilder::DescribeWindow (Word loadAddress)
{
    char  text[256] = {};



    snprintf (text, sizeof (text),
              "a direct-boot payload must load between %s and %s -- page $08 carries the "
              "loader and $C000 is not memory -- and %s was asked for",
              FormatAddress (kLowestLoadAddress).c_str(),
              FormatAddress ((Word) (kMemoryCeiling - 1)).c_str(),
              FormatAddress (loadAddress).c_str());

    return std::string (text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DirectBootBuilder::DescribeTooLarge
//
//  THE CAPACITY IS STATED BY THE THING THAT KNOWS IT. A caller re-deriving
//  the number would be a second implementation of the window arithmetic, and
//  a second implementation of an arithmetic is how this feature's sector
//  reorder came to be wrong in two places that agreed with each other.
//
////////////////////////////////////////////////////////////////////////////////

std::string DirectBootBuilder::DescribeTooLarge (Word loadAddress, size_t payloadBytes)
{
    size_t  capacity   = CapacityFor (loadAddress);



    char    text[256]  = {};



    snprintf (text, sizeof (text),
              "the payload is %zu bytes and a direct-boot image loading at %s can carry "
              "%zu (%zu sectors)",
              payloadBytes,
              FormatAddress (loadAddress).c_str(),
              capacity,
              capacity / (size_t) NibblizationLayer::kSectorByteSize);

    return std::string (text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DirectBootBuilder::DescribeEntry
//
////////////////////////////////////////////////////////////////////////////////

std::string DirectBootBuilder::DescribeEntry (const DirectBootSpec & spec, size_t payloadBytes)
{
    char  text[256] = {};



    snprintf (text, sizeof (text),
              "the entry address %s is outside the payload, which occupies %s through %s",
              FormatAddress (spec.entryAddress).c_str(),
              FormatAddress (spec.loadAddress).c_str(),
              FormatAddress ((Word) (spec.loadAddress + payloadBytes - 1)).c_str());

    return std::string (text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DirectBootBuilder::CapacityFor
//
//  What the boot path can load, which is the distance from the payload's
//  first byte to the top of memory. The media never binds -- see the
//  assertion below, which is the compiler's job rather than a check nothing
//  could ever fail.
//
////////////////////////////////////////////////////////////////////////////////

size_t DirectBootBuilder::CapacityFor (Word loadAddress)
{
    static_assert (kMostSectors <=
                   (size_t) ((NibblizationLayer::kTrackCount - kFirstPayloadTrack)
                             * NibblizationLayer::kSectorsPerTrack),
                   "memory has to run out before the disk does, or the loader would "
                   "step past the last track looking for sectors that are not there");

    bool  inWindow = loadAddress >= kLowestLoadAddress && loadAddress < kMemoryCeiling;



    if (!inWindow)
    {
        return 0;
    }

    return (size_t) (kMemoryCeiling - loadAddress);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DirectBootBuilder::SectorsNeededFor
//
//  The ROM reads whole pages into page-aligned buffers, so a payload whose
//  load address is not page-aligned is carried with its own lead-in: the
//  bytes from the start of its page up to it. They land in memory the payload
//  asked for the far side of and nothing else on the disk claims.
//
////////////////////////////////////////////////////////////////////////////////

size_t DirectBootBuilder::SectorsNeededFor (Word loadAddress, size_t payloadBytes)
{
    constexpr size_t  kSectorBytes = (size_t) NibblizationLayer::kSectorByteSize;
    size_t            leadIn       = (size_t) (loadAddress % kSectorBytes);



    return (leadIn + payloadBytes + kSectorBytes - 1) / kSectorBytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DirectBootBuilder::Validate
//
//  One reason, in that order, because the later questions are meaningless
//  once an earlier one has failed -- an address outside the window has a
//  capacity of zero, so asking about size first would blame the payload's
//  length for an address problem.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DirectBootBuilder::Validate (
    const vector<Byte>    & payload,
    const DirectBootSpec  & spec,
    std::string           & outRefusal)
{
    HRESULT  hr       = S_OK;
    size_t   capacity = 0;
    size_t   length   = payload.size();
    bool     hasBytes = length > 0;
    bool     inWindow = false;
    bool     fits     = false;
    bool     entryOk  = false;



    CBRFEx (hasBytes,
            HRESULT_FROM_WIN32 (ERROR_INVALID_DATA),
            outRefusal = "there is nothing to boot into: the payload is empty");

    capacity = CapacityFor (spec.loadAddress);
    inWindow = capacity > 0;

    CBRFEx (inWindow,
            HRESULT_FROM_WIN32 (ERROR_INVALID_ADDRESS),
            outRefusal = DescribeWindow (spec.loadAddress));

    fits = length <= capacity;

    CBRFEx (fits,
            HRESULT_FROM_WIN32 (ERROR_FILE_TOO_LARGE),
            outRefusal = DescribeTooLarge (spec.loadAddress, length));

    entryOk = spec.entryAddress >= spec.loadAddress
           && (size_t) (spec.entryAddress - spec.loadAddress) < length;

    //  The same code as the window refusal on purpose: both say the boot path
    //  was handed an address it cannot use, and the sentence says which one.
    CBRFEx (entryOk,
            HRESULT_FROM_WIN32 (ERROR_INVALID_ADDRESS),
            outRefusal = DescribeEntry (spec, length));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DirectBootBuilder::WriteLoader
//
//  The loader verbatim, then the four bytes that make it this image's loader
//  rather than any other's. The track and phase bytes are written rather than
//  left to the zero fill, so the sector says what it starts from instead of
//  inheriting it from whatever the buffer happened to be.
//
////////////////////////////////////////////////////////////////////////////////

void DirectBootBuilder::WriteLoader (
    const DirectBootSpec  & spec,
    size_t                  sectorCount,
    vector<Byte>          & inOutSectors)
{
    size_t  at = Dos33Skeleton::SectorOffset (
                     kLoaderTrack,
                     NibblizationLayer::DosFileIndexForPhysicalSector (kLoaderSector));



    std::copy (std::begin (s_kBootLoader), std::end (s_kBootLoader),
               inOutSectors.begin() + at);

    inOutSectors[at + kSectorCountOffset] = (Byte) sectorCount;
    inOutSectors[at + kLoadPageOffset]    = (Byte) (spec.loadAddress >> 8);
    inOutSectors[at + kTrackOffset]       = (Byte) kLoaderTrack;
    inOutSectors[at + kPhaseOffset]       = 0;
    inOutSectors[at + kEntryLowOffset]    = (Byte) (spec.entryAddress & 0xFF);
    inOutSectors[at + kEntryHighOffset]   = (Byte) (spec.entryAddress >> 8);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DirectBootBuilder::PlacePayload
//
//  Page N of the payload goes where the drive will present it Nth.
//
//  THE MAPPING IS THE WHOLE OF THIS FUNCTION AND IT IS NOT THE IDENTITY. A
//  sector buffer is in DOS logical order; the loader asks the ROM for the
//  sector whose address field carries a number, and the sixteen address
//  fields on a track are laid down in physical order. Writing page N at
//  logical sector N produces an image that reads back perfectly through our
//  own reader and hands the guest its pages shuffled. The physical-to-logical
//  answer is taken from the layer that owns the interleave rather than
//  restated here, for that reason.
//
////////////////////////////////////////////////////////////////////////////////

void DirectBootBuilder::PlacePayload (const vector<Byte> & onDisk, vector<Byte> & inOutSectors)
{
    constexpr size_t  kSectorBytes = (size_t) NibblizationLayer::kSectorByteSize;
    size_t            sectorCount  = (onDisk.size() + kSectorBytes - 1) / kSectorBytes;
    size_t            index        = 0;



    for (index = 0; index < sectorCount; index++)
    {
        int     track    = kFirstPayloadTrack
                         + (int) (index / (size_t) NibblizationLayer::kSectorsPerTrack);
        int     physical = (int) (index % (size_t) NibblizationLayer::kSectorsPerTrack);
        size_t  at       = Dos33Skeleton::SectorOffset (
                               track,
                               NibblizationLayer::DosFileIndexForPhysicalSector (physical));
        size_t  from     = index * kSectorBytes;
        size_t  span     = (std::min) (kSectorBytes, onDisk.size() - from);

        std::copy (onDisk.begin() + from, onDisk.begin() + from + span,
                   inOutSectors.begin() + at);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DirectBootBuilder::Build
//
//  Every check first, then a buffer built whole. Nothing reaches the caller
//  until the last copy has been made, so a refused build cannot leave a
//  half-formed image behind to be committed by somebody who did not look at
//  the return.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DirectBootBuilder::Build (
    const vector<Byte>    & payload,
    const DirectBootSpec  & spec,
    vector<Byte>          & outSectors,
    std::string           & outRefusal)
{
    HRESULT       hr          = S_OK;
    size_t        leadIn      = 0;
    size_t        sectorCount = 0;
    vector<Byte>  onDisk;
    vector<Byte>  built;



    outRefusal.clear();

    hr = Validate (payload, spec, outRefusal);
    CHR (hr);

    leadIn = (size_t) (spec.loadAddress % (size_t) NibblizationLayer::kSectorByteSize);

    onDisk.assign (leadIn, (Byte) 0);
    onDisk.insert (onDisk.end(), payload.begin(), payload.end());

    sectorCount = SectorsNeededFor (spec.loadAddress, payload.size());

    built.assign ((size_t) NibblizationLayer::kImageByteSize, (Byte) 0);

    WriteLoader  (spec, sectorCount, built);
    PlacePayload (onDisk, built);

    outSectors = std::move (built);

Error:
    return hr;
}
