#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/TrackBitReader.h"





class DiskImage;
class SectorDecodeReport;
enum class DiskFormat;
struct TrackMarks;





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





////////////////////////////////////////////////////////////////////////////////
//
//  Denibblizer
//
//  Decodes a 16-sector nibble image back to a .dsk / .do / .po sector image.
//
////////////////////////////////////////////////////////////////////////////////

class Denibblizer
{
public:
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

    //  The same walk, reported per track rather than per sector: what each
    //  track YIELDED, including whether its coverage was complete, incomplete
    //  or duplicated. Succeeds whenever it could decode at all, so a caller
    //  that can present partial results decides for itself what to do.
    //
    //  TWO REPORTS BECAUSE THERE ARE TWO QUESTIONS, and neither answers the
    //  other. DenibblizeReport counts sectors by how far they got -- verified,
    //  recovered, lost -- which is what salvage acts on. SectorDecodeReport
    //  says whether a track's sixteen slots were each filled exactly once,
    //  which is what a lossless rewrite depends on and what a sector counter
    //  cannot see: a duplicated sector number leaves the count full.
    static HRESULT  Denibblize  (const DiskImage     & img,
                                 DiskFormat            fmt,
                                 vector<Byte>        & out,
                                 SectorDecodeReport  & outReport);

private:
    static constexpr uint16_t  kAllSectorsMask = 0xFFFF;

    struct TrackScan;

    //  The walk shared by every entry point above. keepRecovered decides what
    //  becomes of a sector that decoded but did not verify; both reports are
    //  filled from the one pass, so they can never disagree about the same
    //  disk.
    static HRESULT  DecodeTracks (const DiskImage     &  img,
                                  DiskFormat             fmt,
                                  bool                   keepRecovered,
                                  vector<Byte>        &  out,
                                  DenibblizeReport    &  report,
                                  SectorDecodeReport  *  outCoverage);

    static void     ScanTrack    (const TrackBitReader::TrackBits & src, const TrackMarks * marks, TrackScan & outScan);
    static void     MergeScans   (const TrackScan & stamped, TrackScan & ioScan);
    static void     WriteTrack   (const TrackScan & scan, int track, const int * interleave, bool keepRecovered, vector<Byte> & out);
    static void     ReportTrack  (const TrackScan                  &  scan,
                                  int                                 track,
                                  const TrackBitReader::TrackBits  &  src,
                                  const TrackMarks                 *  marks,
                                  const int                        *  interleave,
                                  DenibblizeReport                 &  report,
                                  SectorDecodeReport               *  outCoverage);
};
