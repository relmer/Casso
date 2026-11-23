#pragma once

#include "Pch.h"

#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/FluxBitView.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"





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
    static HRESULT  Nibblize    (const vector<Byte> & raw, DiskFormat fmt, DiskImage & out);
    static HRESULT  NibblizeDsk (const vector<Byte> & raw, DiskImage & out);
    static HRESULT  NibblizeDo  (const vector<Byte> & raw, DiskImage & out);
    static HRESULT  NibblizePo  (const vector<Byte> & raw, DiskImage & out);

    //  The volume a sector image's address fields are nibblized with: a DOS
    //  3.3 VTOC's, or 254 for any other disk. The sectors are in DOS 3.3 order.
    static Byte     GetImageVolume (const vector<Byte> & sectors);

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

    //  Re-encodes ONLY the named tracks; every other track's packed bits are
    //  left byte-identical, so a write cannot disturb what it did not touch.
    static HRESULT  RenibblizeTracks (const vector<Byte>    & sectors,
                                      DiskFormat              fmt,
                                      std::span<const int>    tracks,
                                      DiskImage             & inOutImage);

private:
    //  A sector write on a flux track: each sector whose bytes changed has
    //  its data field spliced into the flux at the controller's cell. A blank
    //  flux track gets a whole standard track the same way.
    static HRESULT  WriteFluxTrackSectors (const vector<Byte>  &  sectors,
                                           const int           *  interleave,
                                           int                    track,
                                           DiskImage           &  inOutImage);

    static void     WriteBlankFluxTrack   (const vector<Byte>  &  sectors,
                                           const int           *  interleave,
                                           int                    track,
                                           DiskImage           &  inOutImage);

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
};
