#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDisk
//
//  The values a Full Send computes from a disk's sectors, in DOS logical order
//  in an image-sized buffer with at least the tracks read so far: its
//  fingerprint, its whole-image hash and its image's file name.
//  Pure: no I/O, no Win32.
//
////////////////////////////////////////////////////////////////////////////////

class FullSendDisk
{
public:
    static constexpr int  kDosCatalogTrack   = 17;
    static constexpr int  kCpmDirectoryTrack = 3;

    //  The directory of ProDOS or Apple Pascal, blocks 2 to 5, is on track 0, so
    //  its fingerprint is track 0 alone. Every other disk uses track 0 and the DOS
    //  3.3 catalog track, and a disk with no file system recognized from those
    //  adds CP/M's directory track.
    static bool     IsCatalogOnTrackZero (const std::vector<Byte> & image);

    //  SHA-256 in lowercase hex.
    static HRESULT  ComputeFingerprint (const std::vector<Byte> & image, std::string & outHex);
    static HRESULT  ComputeHash        (std::span<const Byte> image, std::string & outHex);

    //  The volume's name on ProDOS or Apple Pascal, the first program's on
    //  DOS 3.3; empty when the catalog has neither.
    static std::string  GetCatalogTitle (const std::vector<Byte> & image);

    //  Whether a disk's tracks 0 and 17 have no file system Casso recognizes, so
    //  that its fingerprint adds CP/M's directory track. A CP/M data disk can
    //  have nothing on the other two. Judged from those tracks alone, so the
    //  result for a full image and for the tracks read first is the same.
    static bool     NeedsDirectoryTrack (const std::vector<Byte> & image);

    //  Whether a disk is formatted and has no files, and its file system's
    //  name. DOS 3.3, ProDOS, Pascal and CP/M are recognized. A DOS 3.3 or
    //  ProDOS disk must also have at least kBlankFreeTenths tenths of its
    //  sectors or blocks free: a game with a DOS boot and no catalog entries
    //  isn't blank. A DOS 3.3 disk whose one file is a BASIC program with no
    //  lines, as INIT writes after NEW, is blank too, when haveTracks has the
    //  tracks of the program's track/sector list and of its first sector. Only
    //  the image's directory tracks and those of haveTracks are read.
    static bool     IsBlank (const std::vector<Byte> & image, std::span<const int> haveTracks, std::string & outSystem);

    //  The track of the track/sector list of a DOS 3.3 disk's one file, when
    //  that file is a BASIC program of at most kGreetingSectors sectors, and
    //  -1 otherwise. IsBlank reads the program from that track.
    static int      FindLoneProgramTrack (const std::vector<Byte> & image);

    static constexpr uint32_t  kBlankFreeTenths = 8;
    static constexpr int       kGreetingSectors = 3;

    //  What a disk with unreadable sectors probably is, as a manifest status.
    //  Sector reads aren't conclusive: a recognized file system comes first,
    //  then whether whole tracks have no sectors at all.
    static const char *  AssessDamage (const std::vector<Byte> & image, int emptyTracks, int scatteredSectors);

    //  Tracks with an unreadable sector, on a disk with no file system Casso
    //  recognizes, before the rest of it is read once with no retries. Retries
    //  don't recover a copy-protected disk's nonstandard tracks, and each
    //  missing sector's retries take seconds.
    static constexpr int  kDamagedTracksToStop = 1;

    //  The prefix, as typed, then NNNN NAME, or NNNN alone, with the prefix and
    //  the name cleaned by the host file name rules.
    static std::string  MakeImageTitle (int sequence, const std::string & catalogTitle, const std::string & prefix);

    //  The file system's name, as DOS 3.3, or empty when it isn't recognized.
    static std::string  GetSystemName (const std::vector<Byte> & image);

    //  With a leading period: the one chosen, or .po for ProDOS and Apple Pascal,
    //  whose blocks are in the same order, and .dsk for any other.
    static std::string  GetImageExtension (const std::vector<Byte> & image, const std::string & chosen);

private:
    static bool         TryGetPascalVolume (const std::vector<Byte> & image, std::string & outName);
    static std::string  ToHex              (const std::vector<Byte> & digest);
    static int          FindDosEntries     (const std::vector<Byte> & image, Byte & outListTrack, Byte & outListSector, Byte & outType, int & outSectors);
    static size_t       GetDosSectorOffset (int track, int sector);
};
