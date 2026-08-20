#pragma once

#include "Pch.h"

#include "DiskImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  NibblizationLayer
//
//  Static helpers that convert between flat sector images (.dsk / .do /
//  .po — 143360 bytes) and packed nibble bit streams suitable for the
//  Disk II nibble engine.
//
//  Encoding: standard Apple DOS 3.3 6+2 GCR per Sather UTAIIe Ch. 9.
//      Address field: $D5 $AA $96 ... $DE $AA $EB
//      Data    field: $D5 $AA $AD ... $DE $AA $EB
//      4-and-4 encoded volume / track / sector / checksum
//      6-and-2 encoded 256-byte sector → 342 nibble bytes + checksum
//
//  Format differences:
//      .dsk / .do — DOS 3.3 logical sector order (identical layout)
//      .po        — ProDOS order; sectors remapped via po_to_dos table
//
////////////////////////////////////////////////////////////////////////////////

//
//  What became of one sector.
//
//    Verified   both checksums matched. The bytes are the disk's.
//    Recovered  the data field decoded but did not verify -- a failed data
//               checksum or an illegal 6-and-2 nibble. The bytes are usable
//               but may contain errors, and how many is unknowable: the
//               decode is a running XOR chain, so a single bad nibble skews
//               every byte after it by one constant delta. Worth keeping
//               anyway. A sector that fails its checksum makes DOS report an
//               I/O error, which can cost a whole file; the same sector with
//               a recomputed checksum reads, and may even be perfect (if what
//               rotted was the check nibble itself, the 342 data nibbles are
//               untouched).
//    Lost       nothing usable. Either no data field was found, or the
//               ADDRESS field failed its checksum -- and that one cannot be
//               recovered on principle, because an untrustworthy sector
//               number means writing the data anywhere risks overwriting a
//               good sector.
//
enum class SectorOutcome
{
    Verified,
    Recovered,
    Lost
};





//
//  What one Denibblize pass managed to decode. A caller about to overwrite a
//  user's disk file needs to know the difference between "this track is blank"
//  and "this track would not read", because the buffer looks identical either
//  way.
//
struct DenibblizeReport
{
    //  One entry per track examined, in track order. Bit s is set when sector
    //  s decoded. Sized by the run, so it carries no copy of the geometry.
    vector<uint16_t>  decodedSectorMask;

    int  tracksPresent      = 0;   // tracks holding any bits at all
    int  tracksUnformatted  = 0;   // present, but not one sector verified
    int  tracksPartial      = 0;   // some sectors verified, some did not
    int  tracksComplete     = 0;   // every sector verified
    int  sectorsVerified    = 0;
    int  sectorsRecovered   = 0;   // decoded but unverified -- salvageable
    int  sectorsLost        = 0;   // nothing to keep
    int  sectorsMissing     = 0;   // not verified, over the partial tracks

    //  The corruption case: zeros sitting where real sectors should be.
    bool  HasPartialTrack () const { return tracksPartial > 0; }
};





class NibblizationLayer
{
public:
    static constexpr int    kSectorByteSize    = 256;
    static constexpr int    kSectorsPerTrack   = 16;
    static constexpr int    kTrackCount        = 35;
    static constexpr int    kImageByteSize     = kSectorByteSize * kSectorsPerTrack * kTrackCount;
    static constexpr Byte   kDefaultVolume     = 254;
    static constexpr size_t kTrackBitCapacity  = 6400 * 8;

    static HRESULT  Nibblize    (const vector<Byte> & raw, DiskFormat fmt, DiskImage & out);
    static HRESULT  NibblizeDsk (const vector<Byte> & raw, DiskImage & out);
    static HRESULT  NibblizeDo  (const vector<Byte> & raw, DiskImage & out);
    static HRESULT  NibblizePo  (const vector<Byte> & raw, DiskImage & out);

    // Decode a nibble image back to a plain sector image. Reports what it
    // managed to decode, because a buffer holding zeroed and misfiled sectors
    // is indistinguishable from a clean one -- and the caller writes it over
    // the user's file.
    //
    // Fails when any track decoded SOME of its sectors but not all: a
    // half-decoded track means the zeros in the gaps are lost data, not blank
    // media. A track that decodes nothing at all is unformatted, which for a
    // sector image legitimately IS zeros, so that succeeds.
    static HRESULT  Denibblize  (const DiskImage & img, DiskFormat fmt, vector<Byte> & out);
    static HRESULT  Denibblize  (const DiskImage  &  img,
                                 DiskFormat          fmt,
                                 vector<Byte>     &  out,
                                 DenibblizeReport &  report);

    // Salvage counterpart. Keeps every sector that decoded at all, verified or
    // not, and never fails on damage -- that IS the job. A recovered sector's
    // bytes are written as-is; re-nibblizing the result gives it a correct
    // checksum by construction, so a sector that would have made DOS report an
    // I/O error becomes readable instead. Lost sectors are zeroed, because
    // there is nothing to keep.
    //
    // Deliberately a separate entry point rather than a flag on Denibblize:
    // the strict path's refusal to write a partly-decoded image is a safety
    // property, and it should not be reachable by passing a bool.
    static HRESULT  SalvageSectors (const DiskImage  &  img,
                                    DiskFormat          fmt,
                                    vector<Byte>     &  out,
                                    DenibblizeReport &  report);

private:
    //  The walk shared by Denibblize and SalvageSectors. keepRecovered
    //  decides what becomes of a sector that decoded but did not verify.
    static HRESULT  DecodeTracks (const DiskImage  &  img,
                                  DiskFormat          fmt,
                                  bool                keepRecovered,
                                  vector<Byte>     &  out,
                                  DenibblizeReport &  report);
};
