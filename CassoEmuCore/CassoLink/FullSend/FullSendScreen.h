#pragma once

#include "Pch.h"

struct SectorMark;





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendScreen
//
//  The Apple's Full Send screen. Off in both drives clears it, and Q on
//  the Apple ends CassoLink again rather than the session.
//
////////////////////////////////////////////////////////////////////////////////

enum class FullSendDriveState : Byte
{
    Off,
    Idle,
    Checking,
    Sending,
    Empty,
    Repeat,
    Sent,
    OnDeck,
    Protected,
    Changed,
    Capturing,
    Blank,
    Validating,     // the image is being booted on the PC
    ProtectedSent,  // a likely copy-protected disk saved as sectors, not captured raw
};

enum class FullSendTone : Byte
{
    None,
    Done,
    Duplicate,
    BadSectors,
    Protected,
    Booted,         // the image booted on the PC
    DidNotBoot,
};

struct FullSendScreen
{
    std::array<FullSendDriveState, 2>  drives     = { FullSendDriveState::Idle, FullSendDriveState::Idle };
    int                                sent       = 0;
    int                                duplicates = 0;
    int                                bad        = 0;
    FullSendTone                       tone       = FullSendTone::None;
    Byte                               quip       = 0;      // the prompt line: 0 none, else a line from 1
    std::array<std::string, 2>         names;               // each drive's latest disk, as 13 GAMES DISK
    std::array<std::string, 2>         maps;                // each drive's tracks with a problem, or empty
    bool                               verify     = false;  // verified reads are on
    int                                recovered  = 0;      // disks with a sector recovered
    int                                prot       = 0;      // disks that are likely copy-protected
    std::string                        prefix;              // what begins each image's file name

    static constexpr int  kQuipCount = 16;

    std::vector<std::string>  GetRows          () const;
    void                      SetDriveState    (int drive, FullSendDriveState state) { drives[(size_t) drive - 1] = state; }
    std::string             & GetName          (int drive)                           { return names[(size_t) drive - 1]; }
    std::string             & GetMap           (int drive)                           { return maps[(size_t) drive - 1]; }

    static const char       * GetQuipText      (Byte quip);
    static std::string        GetDuplicateText (const std::string & file, const char * mark);
    static std::string      & MakeTrackRuler   (std::string & ioMap);
    static std::string      & MakeTrackMap     (std::string & ioMap, const std::vector<SectorMark> & marks, const std::vector<std::pair<int, int>> & unreadable);
};
