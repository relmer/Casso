#pragma once

#include "Pch.h"

#include "NibblizationLayer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DirectBootSpec
//
//  Where the payload wants to be and where it wants to start. The two are
//  separate because a binary whose first byte is data rather than an
//  instruction is ordinary -- a header, a jump table, a length word -- and
//  making the entry follow the load address would force such a payload to be
//  rewritten before it could boot.
//
////////////////////////////////////////////////////////////////////////////////

struct DirectBootSpec
{
    Word  loadAddress  = 0x0900;
    Word  entryAddress = 0x0900;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DirectBootBuilder
//
//  A disk that boots a supplied binary with no operating system on it at all.
//
//  Track 0's first sector carries a loader; the payload follows from track 1,
//  one sector per memory page, and the loader jumps to it. Nothing else is on
//  the disk -- no VTOC, no catalog, no filesystem of any kind -- so between
//  powering on and the developer's own code there is only the drive's boot
//  ROM and a hundred and twenty bytes.
//
//  WHY THE LOADER REUSES THE BOOT ROM'S READ ROUTINE. The Disk II ROM already
//  contains a working sector reader, and its read loop returns to $0801 after
//  every pass with the sector number, the buffer page and the terminating
//  count all in memory the loader owns. Re-entering it is what every
//  operating system on this machine does, it costs nothing, and it leaves
//  room in one sector for the part the ROM does not do: stepping the head.
//
//  WHAT THAT COSTS, AND WHY THE PAYLOAD CANNOT LOAD BELOW $0900. The ROM's
//  loop terminates by comparing against the byte at $0800 -- an absolute
//  address baked into the ROM -- and it jumps to $0801, so page $08 has to
//  stay the loader's for as long as anything is being read. $0300 through
//  $03FF is the ROM's own decode table and secondary buffer, and $C000 up is
//  not memory. That leaves $0900 to $BFFF, which is where a payload goes and
//  what GetCapacity measures.
//
////////////////////////////////////////////////////////////////////////////////

class DirectBootBuilder
{
public:
    //  The window a payload may occupy. Below the first is the loader and the
    //  boot ROM's workspace; at the second, memory stops being memory.
    static constexpr Word  kLowestLoadAddress = 0x0900;
    static constexpr Word  kMemoryCeiling     = 0xC000;

    //  Track 0 sector 0 is the loader; the payload starts on the next track.
    static constexpr int   kLoaderTrack       = 0;
    static constexpr int   kLoaderSector      = 0;
    static constexpr int   kFirstPayloadTrack = 1;

    //  The most any payload can be, whatever it asks for: the capacity at the
    //  lowest address one may load at.
    static constexpr size_t  kLargestCapacity = (size_t) (kMemoryCeiling - kLowestLoadAddress);

    static constexpr size_t  kMostSectors     = kLargestCapacity
                                              / (size_t) NibblizationLayer::kSectorByteSize;

    //  How many bytes a payload loading here may occupy, and how many sectors
    //  that many bytes take on the disk. Zero for an address the boot path
    //  cannot load at, which is what makes an address outside the window a
    //  refusal of its own rather than a capacity of some odd size.
    static size_t   GetCapacity      (Word loadAddress);
    static size_t   GetSectorsNeeded (Word loadAddress, size_t payloadBytes);

    //  The complete 143,360-byte DOS-ordered sector buffer for the image, or
    //  a refusal naming one reason. All-or-nothing: outSectors is assigned
    //  only once every check has passed.
    static HRESULT  Build            (const vector<Byte>    & payload,
                                      const DirectBootSpec  & spec,
                                      vector<Byte>          & outSectors,
                                      std::string           & outRefusal);

private:
    //  Byte offsets into the loader sector that a build fills in. Everything
    //  else in the sector is the loader verbatim.
    static constexpr size_t  kSectorCountOffset = 0xF0;
    static constexpr size_t  kLoadPageOffset    = 0xF1;
    static constexpr size_t  kTrackOffset       = 0xF2;
    static constexpr size_t  kPhaseOffset       = 0xF3;
    static constexpr size_t  kEntryLowOffset    = 0x4D;
    static constexpr size_t  kEntryHighOffset   = 0x4E;

    static HRESULT      Validate         (const vector<Byte>    & payload,
                                          const DirectBootSpec  & spec,
                                          std::string           & outRefusal);

    static void         WriteLoader      (const DirectBootSpec  & spec,
                                          size_t                  sectorCount,
                                          vector<Byte>          & inOutSectors);

    static void         PlacePayload     (const vector<Byte>    & onDisk,
                                          vector<Byte>          & inOutSectors);

    static std::string  FormatAddress    (Word value);
    static std::string  DescribeWindow   (Word loadAddress);
    static std::string  DescribeTooLarge (Word loadAddress, size_t payloadBytes);
    static std::string  DescribeEntry    (const DirectBootSpec & spec, size_t payloadBytes);
};
