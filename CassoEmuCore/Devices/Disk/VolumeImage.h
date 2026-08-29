#pragma once

#include "Pch.h"

#include "DiskImage.h"
#include "MountDiagnosis.h"
#include "SectorDecodeReport.h"





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeKind
//
//  Which filesystem a sector buffer carries. Determined INDEPENDENTLY of the
//  sector order, because the two are unrelated: a ProDOS volume stored in DOS
//  sector order is the common case in the wild, not an exotic one, and neither
//  property can be inferred from the other.
//
////////////////////////////////////////////////////////////////////////////////

enum class VolumeKind
{
    Unknown,
    Dos33,
    ProDos,
};





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeImage
//
//  Turns an image file into the one currency the volume layer speaks: a flat
//  143,360-byte buffer in DOS LOGICAL sector order.
//
//  Every skeleton and both volume readers address that layout, including the
//  ProDOS ones -- ProDOS blocks map onto DOS-ordered sectors through the
//  standard pairing. So the ordering question is settled once, here, rather
//  than by each caller remembering which format it holds.
//
//  Detection corroborates across several fields rather than trusting one, and
//  that is not defensive tidiness. Probing a real DOS 3.3 disk at the offset
//  where a ProDOS volume header would sit finds a storage-type nibble of $F --
//  exactly what a genuine header looks like -- because those bytes are 6502
//  boot code. A single-field check calls that disk ProDOS. The name that
//  follows decodes to garbage, which is what gives it away, so the name is
//  validated too and the other filesystem's own structures are cross-checked.
//
////////////////////////////////////////////////////////////////////////////////

class VolumeImage
{
public:
    //  Any mountable image file to a DOS-logical sector buffer. The report
    //  describes what the track layer could recover, and is empty of meaning
    //  for a sector-order file, which has no tracks to decode.
    static HRESULT  Load (const vector<Byte>   & fileBytes,
                          const std::string    & path,
                          vector<Byte>         & outSectors,
                          SectorDecodeReport   & outReport);

    //  The same load, saying why it refused. The console's `disk` commands
    //  take this form so a user gets the same specific reason the emulator's
    //  mount does -- the two arrive at the refusal through the same loaders,
    //  and a reason only one of them could give would be a reason the other
    //  went looking for and did not find.
    static HRESULT  Load (const vector<Byte>   & fileBytes,
                          const std::string    & path,
                          vector<Byte>         & outSectors,
                          SectorDecodeReport   & outReport,
                          MountDiagnosis       & outDiagnosis);

    //  An edited sector buffer rendered back into the container it came from.
    //  A bit-stream image keeps every track the edit did not touch VERBATIM,
    //  so timing, sync patterns and weak bits survive a write elsewhere.
    //
    //  Refusal is WHOLE-OPERATION. If any track the edit lands on cannot be
    //  rewritten as standard sectors, nothing is produced at all -- skipping
    //  that track and committing the rest is precisely the half-written image
    //  the all-or-nothing rule exists to forbid, and it would be reported as
    //  success.
    static HRESULT  Save (const vector<Byte>  & originalFileBytes,
                          const std::string   & path,
                          const vector<Byte>  & editedSectors,
                          vector<Byte>        & outFileBytes,
                          std::string         & outRefusalReason);

    //  Tracks whose sectors differ between two buffers of the same volume.
    //  Ascending, and empty when the two agree.
    static void     ChangedTracks (const vector<Byte> & priorSectors,
                                   const vector<Byte> & editedSectors,
                                   vector<int>        & outTracks);

    //  Reorders a ProDOS-ordered file image into DOS logical order. Public
    //  because the inverse is what writing a .po image needs.
    static void     ProDosFileToDosLogical (const vector<Byte> & fileBytes, vector<Byte> & outSectors);
    static void     DosLogicalToProDosFile (const vector<Byte> & sectors,   vector<Byte> & outFileBytes);

    //  Which filesystem the DOS-logical buffer carries.
    static VolumeKind  DetectFilesystem (const vector<Byte> & sectors);

private:
    //  The bit-stream half of Save: decode, judge, re-encode only what changed.
    static HRESULT  SaveBitStream (const vector<Byte>  & originalFileBytes,
                                   const vector<Byte>  & editedSectors,
                                   vector<Byte>        & outFileBytes,
                                   std::string         & outRefusalReason);

    static std::string  DescribeUnwritableTrack (int track);

    static bool  LooksLikeDos33  (const vector<Byte> & sectors);
    static bool  LooksLikeProDos (const vector<Byte> & sectors);

    //  A ProDOS volume name: 1-15 characters, letter first, then letters,
    //  digits or periods. Boot code masquerading as a header fails this.
    static bool  IsPlausibleVolumeName (const vector<Byte> & sectors, size_t nameLength);
};
