#pragma once

#include "Pch.h"

#include "Devices/Disk/DiskImage.h"
#include "Machines/Apple2/Common/Disk2NibbleEngine.h"
#include "Machines/Apple2/Common/DiskAccessLog.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2HeadMove
//
//  Where a drive's head was and is after the phases moved it, and whether it
//  hit a travel stop.
//
////////////////////////////////////////////////////////////////////////////////

struct Disk2HeadMove
{
    int   from     = 0;
    int   to       = 0;
    bool  isBumped = false;

    bool  IsMoved() const { return from != to || isBumped; }
};





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Drive
//
//  One Disk II drive: its head, the engine that streams its disk's bits, the
//  disk in it, and what the guest did with it since that disk went in. The
//  drive has an empty disk, which is the one in it when no other is
//  attached.
//
////////////////////////////////////////////////////////////////////////////////

class Disk2Drive
{
public:
    static constexpr int  kPerController   = 2;     // drives on one controller
    static constexpr int  kPhaseCount      = 4;
    static constexpr int  kMaxQuarterTrack = 139;

    // A phase's magnets are a half track (two quarter tracks) from the next
    // phase's.
    static constexpr int  kQuarterTracksPerHalfTrack = 2;

    // How far, in quarter tracks, the energized phases move a head at
    // quarterTrack.
    static int  GetPull (uint8_t phases, int quarterTrack);

    Disk2Drive();

    Disk2Drive (const Disk2Drive &)             = delete;
    Disk2Drive & operator= (const Disk2Drive &) = delete;

    Disk2HeadMove  MoveHead        (uint8_t phases);
    void           SetQuarterTrack (int quarterTrack);
    int            GetQuarterTrack () const { return m_quarterTrack; }

    HRESULT      Mount           (const string & path);
    void         Eject           ();
    void         SetExternalDisk (DiskImage * external);
    bool         HasExternalDisk () const { return m_active != &m_disk; }
    DiskImage  * GetDisk         ()       { return m_active; }
    bool         HasDisk         () const { return m_active->IsLoaded(); }
    bool         IsWriteProtected() const { return m_active->IsWriteProtected(); }

    void  NoteRead       (Byte nibble);
    void  LoadWriteLatch (Byte nibble, bool isMotorOn);
    void  EndRead        () { m_access.EndRead(); }
    void  Reset          ();

    //  What the guest did since the disk went in, for the disk inspector:
    //  arrivals of the head at each quarter track, where it read and wrote,
    //  and whether its last write was dropped because the disk cannot be
    //  written.
    uint32_t               GetVisitCount  (int quarterTrack) const;
    const DiskAccessLog  & GetAccessLog   () const { return m_access; }
    bool                   IsWriteBlocked () const { return m_isWriteBlocked; }

    Disk2NibbleEngine        & GetEngine()       { return m_engine; }
    const Disk2NibbleEngine  & GetEngine() const { return m_engine; }

private:
    void  CountLatchLoad();

    int                                             m_quarterTrack   = 0;
    DiskImage                                       m_disk;
    DiskImage                                     * m_active         = nullptr;
    Disk2NibbleEngine                               m_engine;
    std::array<uint32_t, kMaxQuarterTrack + 1>      m_visits         = {};
    DiskAccessLog                                   m_access;
    bool                                            m_isWriteBlocked = false;
};





//  A controller's drives, by index.
using Disk2Drives = std::array<Disk2Drive, Disk2Drive::kPerController>;
