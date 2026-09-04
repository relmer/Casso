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
//  Every later read is the same trick used against the ROM rather than by it.
//  The loader points $26/$27 at the page to fill, $3D at the sector it wants,
//  $41 at the track the address fields will carry, writes a terminating count
//  of one more than that sector into $0800, and jumps back into the ROM's read
//  entry. The ROM reads that ONE sector and jumps to $0801 again, where the
//  loader picks up where it left off. There is no first-time branch anywhere
//  in it: the five variables it works from arrive as part of the sector,
//  already set to what a first read needs.
//
//  WHY ONE SECTOR PER CALL RATHER THAN A WHOLE TRACK. The ROM's own loop reads
//  ascending sector numbers into ascending pages, and the sixteen sectors lie
//  on the track in ascending order too, so it asks for each sector just after
//  that sector has passed under the head -- it spends a whole revolution
//  waiting for every one. Measured against this emulator, a payload cost about
//  200 ms per sector, which is one revolution each to within the error of the
//  measurement. Reading one sector per call is what buys the loader the right
//  to choose the ORDER, and the order is what closes that gap: see
//  BuildReadOrder.
//
//      0800  01                  .byte 1        ; the ROM's terminating count
//      0801  AD F0 08    enter   LDA secLeft
//      0804  F0 48               BEQ done
//      0806  AD F4 08            LDA index
//      0809  D0 08               BNE onTrack    ; index 0 means a fresh track
//      080B  20 56 08            JSR stepTrack
//      080E  AD F2 08            LDA track
//      0811  85 41               STA $41        ; the address field must agree
//      0813  AE F4 08    onTrack LDX index
//      0816  BD E0 08            LDA order,X    ; the sector to ask for
//      0819  85 3D               STA $3D
//      081B  18                  CLC
//      081C  69 01               ADC #$01
//      081E  8D 00 08            STA $0800      ; the ROM stops after that one
//      0821  A9 00               LDA #$00
//      0823  85 26               STA $26        ; whole pages only
//      0825  AD F1 08            LDA curPage
//      0828  85 27               STA $27
//      082A  EE F1 08            INC curPage
//      082D  CE F0 08            DEC secLeft
//      0830  EE F4 08            INC index
//      0833  AD F4 08            LDA index
//      0836  29 0F               AND #$0F       ; sixteen to a track
//      0838  8D F4 08            STA index
//      083B  A5 2B               LDA $2B        ; slot * 16, left by the ROM
//      083D  4A 4A 4A 4A         LSR / LSR / LSR / LSR
//      0841  09 C0               ORA #$C0
//      0843  85 3F               STA $3F
//      0845  A9 5C               LDA #$5C
//      0847  85 3E               STA $3E        ; -> $Cs5C, the ROM's reader
//      0849  A6 2B               LDX $2B
//      084B  6C 3E 00            JMP ($3E)
//      084E  A6 2B       done    LDX $2B
//      0850  BD 88 C0            LDA $C088,X    ; motor off; the payload owns
//      0853  4C 00 00            JMP entry      ;   the machine from here
//      0856  A0 02       stepTrack LDY #$02     ; two half-steps make a track
//      0858  AD F3 08    step    LDA phase
//      085B  0A                  ASL
//      085C  05 2B               ORA $2B
//      085E  AA                  TAX
//      085F  BD 80 C0            LDA $C080,X    ; the magnet now on, off
//      0862  EE F3 08            INC phase
//      0865  AD F3 08            LDA phase
//      0868  29 03               AND #$03
//      086A  8D F3 08            STA phase
//      086D  0A                  ASL
//      086E  05 2B               ORA $2B
//      0870  AA                  TAX
//      0871  BD 81 C0            LDA $C081,X    ; the next one on
//      0874  A9 56               LDA #$56       ; the settle the ROM itself
//      0876  20 A8 FC            JSR $FCA8      ;   uses while recalibrating
//      0879  88                  DEY
//      087A  D0 DC               BNE step
//      087C  EE F2 08            INC track
//      087F  60                  RTS
//
//  The head arrives at track 0 with magnet 0 energized, because that is how
//  the ROM's recalibrate loop ends, so `phase` and `track` both start at zero
//  and the first read steps to track 1 before fetching anything. That is why
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
    0xF0, 0x48,
    0xAD, 0xF4, 0x08,
    0xD0, 0x08,
    0x20, 0x56, 0x08,
    0xAD, 0xF2, 0x08,
    0x85, 0x41,
    0xAE, 0xF4, 0x08,
    0xBD, 0xE0, 0x08,
    0x85, 0x3D,
    0x18,
    0x69, 0x01,
    0x8D, 0x00, 0x08,
    0xA9, 0x00,
    0x85, 0x26,
    0xAD, 0xF1, 0x08,
    0x85, 0x27,
    0xEE, 0xF1, 0x08,
    0xCE, 0xF0, 0x08,
    0xEE, 0xF4, 0x08,
    0xAD, 0xF4, 0x08,
    0x29, 0x0F,
    0x8D, 0xF4, 0x08,
    0xA5, 0x2B,
    0x4A, 0x4A, 0x4A, 0x4A,
    0x09, 0xC0,
    0x85, 0x3F,
    0xA9, 0x5C,
    0x85, 0x3E,
    0xA6, 0x2B,
    0x6C, 0x3E, 0x00,
    0xA6, 0x2B,
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
              "a direct-boot payload must load between %s and %s. Page $08 contains the "
              "loader and $C000 is not memory. %s was requested",
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
    size_t  capacity   = GetCapacity (loadAddress);



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
//  DirectBootBuilder::GetCapacity
//
//  What the boot path can load, which is the distance from the payload's
//  first byte to the top of memory. The media never binds -- see the
//  assertion below, which is the compiler's job rather than a check nothing
//  could ever fail.
//
////////////////////////////////////////////////////////////////////////////////

size_t DirectBootBuilder::GetCapacity (Word loadAddress)
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
//  DirectBootBuilder::GetSectorsNeeded
//
//  The ROM reads whole pages into page-aligned buffers, so a payload whose
//  load address is not page-aligned is carried with its own lead-in: the
//  bytes from the start of its page up to it. They land in memory the payload
//  asked for the far side of and nothing else on the disk claims.
//
////////////////////////////////////////////////////////////////////////////////

size_t DirectBootBuilder::GetSectorsNeeded (Word loadAddress, size_t payloadBytes)
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

    capacity = GetCapacity (spec.loadAddress);
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
//  DirectBootBuilder::BuildReadOrder
//
//  The order in which the loader asks a track for its sixteen sectors, and so
//  also the order the builder lays the payload's pages down in.
//
//  ASKING FOR THEM IN ASCENDING ORDER COSTS A REVOLUTION EACH. The sectors are
//  written around the track in ascending order, and by the time the ROM has
//  denibblized one and the loader has come back for the next, the next has
//  already gone by; the drive then has to bring it round again. That is a
//  measured 200 ms per sector, and 200 ms is a revolution.
//
//  So the loader asks for every kInterleave-th sector instead, stepping past
//  one already taken, which is the same construction a formatter uses for a
//  soft interleave. A track then takes kInterleave revolutions rather than
//  sixteen. The factor has to be at least as large as the gap the reader needs
//  -- too small and a sector is missed and costs a whole revolution again,
//  which is the very thing this avoids -- so it is measured rather than
//  guessed. Loading 32 sectors, from the first factor to the sixth:
//
//      1      9.6 s        the ascending order this replaced
//      2      3.8 s
//      3      4.4 s
//      4      4.8 s
//      5      4.8 s
//      6      5.4 s
//
//  About 3.2 s of each is the emulator starting and the ROM booting, and the
//  rest tracks kInterleave * 200 ms per track exactly, which is kInterleave
//  revolutions. Two is therefore not merely the fastest measured, it is the
//  theoretical floor for this reader: it lands on the model rather than under
//  a cliff, so a sector is not being missed and there is margin below it.
//
//  NOT DOS 3.3'S SKEW, THOUGH THAT TABLE IS RIGHT THERE. Reading a track in
//  DOS's logical order walks physical 0, 13, 11, 9, ... -- thirteen or fourteen
//  positions apart in the direction the disk turns, so this reader waits most
//  of a revolution for each anyway. On the same 32 sectors it costs 8.4 s,
//  against 9.4 s for ascending order and 4.0 s for the order built here. The
//  table is not wrong; it is cut for RWTS and the file manager above it, which
//  have far more to do between sectors than the boot ROM does. Which skew is
//  right is a property of the reader, not of the medium.
//
////////////////////////////////////////////////////////////////////////////////

void DirectBootBuilder::BuildReadOrder (Byte * outOrder)
{
    constexpr int  kCount        = (int) NibblizationLayer::kSectorsPerTrack;
    bool           taken[kCount] = {};
    int            at            = 0;
    int            index         = 0;



    for (index = 0; index < kCount; index++)
    {
        while (taken[at])
        {
            at = (at + 1) % kCount;
        }

        outOrder[index] = (Byte) at;
        taken[at]       = true;
        at              = (at + kInterleave) % kCount;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DirectBootBuilder::WriteLoader
//
//  The loader verbatim, then the bytes that make it this image's loader rather
//  than any other's. The track, phase and index bytes are written rather than
//  left to the zero fill, so the sector says what it starts from instead of
//  inheriting it from whatever the buffer happened to be.
//
////////////////////////////////////////////////////////////////////////////////

void DirectBootBuilder::WriteLoader (
    const DirectBootSpec  & spec,
    size_t                  sectorCount,
    vector<Byte>          & inOutSectors)
{
    constexpr size_t  kPerTrack        = (size_t) NibblizationLayer::kSectorsPerTrack;
    size_t            at               = 0;
    Byte              order[kPerTrack] = {};



    at = Dos33Skeleton::GetSectorOffset (
             kLoaderTrack,
             NibblizationLayer::GetDosFileIndexForPhysicalSector (kLoaderSector));

    std::copy (std::begin (s_kBootLoader), std::end (s_kBootLoader),
               inOutSectors.begin() + at);

    BuildReadOrder (order);

    std::copy (std::begin (order), std::end (order),
               inOutSectors.begin() + at + kOrderOffset);

    inOutSectors[at + kSectorCountOffset] = (Byte) sectorCount;
    inOutSectors[at + kLoadPageOffset]    = (Byte) (spec.loadAddress >> 8);
    inOutSectors[at + kTrackOffset]       = (Byte) kLoaderTrack;
    inOutSectors[at + kPhaseOffset]       = 0;
    inOutSectors[at + kIndexOffset]       = 0;
    inOutSectors[at + kEntryLowOffset]    = (Byte) (spec.entryAddress & 0xFF);
    inOutSectors[at + kEntryHighOffset]   = (Byte) (spec.entryAddress >> 8);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DirectBootBuilder::PlacePayload
//
//  Page N of the payload goes where the drive will present it Nth.
//
//  THE MAPPING IS THE WHOLE OF THIS FUNCTION AND IT IS NOT THE IDENTITY, for
//  two separate reasons that both have to be applied.
//
//  The first is the interleave: the loader does not walk a track's sectors in
//  ascending order, it walks BuildReadOrder's, so the page it reads Nth is the
//  one written at physical sector order[N]. The two functions are the two
//  halves of one agreement, and only this call keeps them the same shape.
//
//  The second is the buffer's own order. A sector buffer is in DOS logical
//  order; the loader asks the ROM for the sector whose address field carries a
//  number, and the sixteen address fields on a track are laid down in physical
//  order. Writing a page at the logical sector of the same number produces an
//  image that reads back perfectly through our own reader and hands the guest
//  its pages shuffled. The physical-to-logical answer is taken from the layer
//  that owns that mapping rather than restated here, for that reason.
//
////////////////////////////////////////////////////////////////////////////////

void DirectBootBuilder::PlacePayload (const vector<Byte> & onDisk, vector<Byte> & inOutSectors)
{
    constexpr size_t  kSectorBytes     = (size_t) NibblizationLayer::kSectorByteSize;
    constexpr size_t  kPerTrack        = (size_t) NibblizationLayer::kSectorsPerTrack;
    size_t            sectorCount      = (onDisk.size() + kSectorBytes - 1) / kSectorBytes;
    size_t            index            = 0;
    Byte              order[kPerTrack] = {};



    BuildReadOrder (order);

    for (index = 0; index < sectorCount; index++)
    {
        int     track    = kFirstPayloadTrack + (int) (index / kPerTrack);
        int     physical = (int) order[index % kPerTrack];
        size_t  at       = Dos33Skeleton::GetSectorOffset (
                               track,
                               NibblizationLayer::GetDosFileIndexForPhysicalSector (physical));
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

    sectorCount = GetSectorsNeeded (spec.loadAddress, payload.size());

    built.assign ((size_t) NibblizationLayer::kImageByteSize, (Byte) 0);

    WriteLoader  (spec, sectorCount, built);
    PlacePayload (onDisk, built);

    outSectors = std::move (built);

Error:
    return hr;
}
