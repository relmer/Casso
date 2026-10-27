#pragma once

#include "Pch.h"

#include "CassoLink/Session/CassoLinkSessionTypes.h"

class IByteStream;
class IByteStreamFactory;
class IDiskFileIo;
class CassoLinkSession;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDiskTransfer
//
//  Whole disks over the link: a disk read into an image track by track,
//  the unreadable sectors' list beside it, an image written back, and the
//  drive timed before a format.
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
    static std::string  FormatUnreadableList  (const std::vector<std::pair<int, int>> & unreadable);

    //  A drive's speed for the activity log, from its measured revolution:
    //  D1 turns at 293.1 rpm, 2.3% slow, with a note when it is more than
    //  1.5% from 300 rpm, the usual setting.
    static std::string  DescribeDriveSpeed (int drive, double revolutionMs);

    //  Revolutions timed for a drive's speed, once a connection for each
    //  drive: about two seconds.
    static constexpr Byte  kTimedRevolutions = 10;
    HRESULT  ReadWholeDisk (LinkOperation & operation);
    HRESULT  SaveUnreadableList (const LinkOperation & operation);
    HRESULT  WriteWholeDisk (LinkOperation & operation);
    HRESULT  ReadBootSector (LinkOperation & operation);
    HRESULT  ReadTrack     (int drive, int track, std::vector<Byte> & sectors, std::vector<std::pair<int, int>> & unreadable);
    HRESULT  TimeDriveForFormat (int drive, Byte & outGap);

private:
    friend class CassoLinkSession;

    CassoLinkSession  & m_session;

    std::array<double, CassoLinkSessionTypes::kDriveCount>  m_revolutionMs = {};   // each drive's, once timed this connection
    uint64_t                                                m_commitTag    = 1;
};
