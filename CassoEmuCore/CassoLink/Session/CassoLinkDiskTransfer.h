#pragma once

#include "Pch.h"

#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/Session/CassoLinkCapture.h"
#include "CassoLink/Session/CassoLinkSessionTypes.h"

class CassoLinkSession;
struct CassoLinkStatus;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDiskTransfer
//
//  Whole disks over the link: a disk read into an image track by track,
//  the unreadable sectors' list beside it, an image written back, and the
//  drive timed before a format or for a WOZ built to the disk's layout.
//
////////////////////////////////////////////////////////////////////////////////

class CassoLinkDiskTransfer
{
public:
    explicit CassoLinkDiskTransfer (CassoLinkSession & session);

    //  The list of the sectors a read wrote as zeros, saved beside its image
    //  as NAME.unreadable.txt: lines starting with # are comments, and each
    //  other line is a sector's track and DOS 3.3 sector in decimal, as "17 3".
    static std::string  GetUnreadableListPath (const std::string & imagePath);
    static std::string  FormatUnreadableList  (const std::vector<std::pair<int, int>> & unreadable, const std::vector<SectorMark> & marks = {});

    //  A drive's speed for the activity log, from its measured revolution:
    //  D1 turns at 293.1 rpm, 2.3% slow, with a note when it is more than
    //  1.5% from 300 rpm, the usual setting.
    static std::string  DescribeDriveSpeed (int drive, double revolutionMs);

    //  Revolutions timed for a drive's speed, once a connection for each
    //  drive: about two seconds.
    static constexpr Byte  kTimedRevolutions = 10;
    static constexpr int   kWriteRetries     = 2;

    //  Requests after a read with sectors not found, each for only the sectors
    //  not yet read, so the Apple reads them in whatever order they pass under
    //  the head. The retries end after a request in which no sector is read.
    static constexpr int   kRetryPasses      = 3;

    //  The sectors with their entries false in ioRead, read again in up to
    //  kRetryPasses requests; ioStatus is the last request's.
    static HRESULT  ReadMissing (CassoLinkHost & host, Byte drive, Byte track, std::span<const Byte> sectors, std::vector<Byte> & ioData, std::vector<bool> & ioRead, CassoLinkStatus & ioStatus);

    //  A Status error for sectors not read, rather than for the drive.
    static bool     IsDamaged   (Byte error);

    HRESULT  ReadWholeDisk      (LinkOperation & operation);
    HRESULT  CaptureWholeDisk   (LinkOperation & operation);
    HRESULT  SaveUnreadableList (const LinkOperation & operation);
    HRESULT  WriteWholeDisk     (LinkOperation & operation);
    HRESULT  CompareWrittenDisk (LinkOperation & operation, const std::vector<Byte> & sectors);
    HRESULT  ReadBootSector     (LinkOperation & operation);

    //  A 13-sector disk to a .d13 image, and a 13-sector image to the disk,
    //  through the 13-sector module (CassoLinkThirteenSector).
    HRESULT  ReadThirteenSectorDisk  (LinkOperation & operation);
    HRESULT  WriteThirteenSectorDisk (LinkOperation & operation, const std::vector<Byte> & sectors);
    HRESULT  ReadTrack          (int drive, int track, std::vector<Byte> & sectors, std::vector<std::pair<int, int>> & unreadable, bool autoVerify = false, std::vector<SectorMark> * marks = nullptr);
    HRESULT  ReadVerifiedTrack  (int drive, int track, std::vector<Byte> & sectors, std::vector<std::pair<int, int>> & unreadable, std::vector<SectorMark> & marks);
    HRESULT  TimeDriveForFormat (int drive, Byte & outGap);

    //  A turn's bits on the drive, for a WOZ built to its disk's layout, and
    //  a track's measure appended to ioMeasures: zeros for a track with no
    //  sector 0.
    HRESULT  TimeDriveForLayout (int drive, int & outBitsPerTurn);
    HRESULT  MeasureTrack       (int drive, int track, std::vector<CassoLinkHost::TrackMeasure> & ioMeasures);

    static std::string  FormatCaptureTimes (const CassoLinkCapture::Result & result);
    static std::string  FormatPosition     (int position);
    static std::string  FormatDuration     (double seconds);

    //  A capture's progress on the status bar.
    void  ReportCapture (LinkOperation & operation, const CassoLinkCapture::Progress & where);

private:
    friend class CassoLinkSession;

    bool  IsTimed () const;

    CassoLinkSession  & m_session;

    std::array<double, CassoLinkSessionTypes::kDriveCount>  m_revolutionMs = {};   // each drive's, once timed this connection
    uint64_t                                                m_commitTag    = 1;
};
