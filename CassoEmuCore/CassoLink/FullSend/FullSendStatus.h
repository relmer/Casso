#pragma once

#include "Pch.h"

#include "CassoLink/FullSend/FullSendManifest.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendActivity
//
//  What a Full Send is doing, or what it waits for.
//
////////////////////////////////////////////////////////////////////////////////

enum class FullSendActivity : Byte
{
    None,
    Starting,
    Checking,
    Sending,
    Capturing,
    WaitingForDisk,
    WaitingForApple,
    EditingPrefix,
    Ended,
};

enum class FullSendTrackMark : Byte
{
    Unread,
    Read,
    Verified,
    Unreliable,
    Unreadable,
};

enum class FullSendPositionMark : Byte
{
    Unread,
    Read,
    Skipped,
    Weak,
    Unstable,
};

enum class FullSendDiskState : Byte
{
    None,
    Checking,
    Sending,
    Capturing,
    Sent,
    Duplicate,
    Empty,
    Blank,
};





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDriveStatus
//
//  One drive's disk in the Full Send window: its state, title, a mark per
//  track, and in a raw capture a mark per quarter track with the pass and
//  the reads done. Times are the status's clock, in seconds; a negative
//  secondsLeft is no estimate.
//
////////////////////////////////////////////////////////////////////////////////

struct FullSendDriveStatus
{
    static constexpr int  kTracks    = 35;
    static constexpr int  kPositions = (kTracks - 1) * 4 + 1;

    FullSendDiskState                  state       = FullSendDiskState::None;
    std::string                        title;
    std::string                        fileSystem;
    bool                               verified    = false;
    std::vector<FullSendTrackMark>     tracks      = std::vector<FullSendTrackMark> (kTracks, FullSendTrackMark::Unread);
    std::vector<FullSendPositionMark>  positions;                  // empty but in a raw capture
    int                                pass        = 0;
    int                                position    = 0;
    int                                read        = 0;
    int                                reads       = 0;
    int                                percent     = 0;
    double                             started     = 0.0;
    double                             elapsed     = 0.0;
    double                             secondsLeft = -1.0;
    double                             diskSeconds = 0.0;
    double                             lineSeconds = 0.0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDoneDisk, FullSendLine, FullSendTotals
//
//  A disk the session finished, as its manifest row and the seconds it took;
//  a line of the activity list; and the session's counts.
//
////////////////////////////////////////////////////////////////////////////////

struct FullSendDoneDisk
{
    FullSendRow  row;
    std::string  fileSystem;
    double       seconds = 0.0;
};

struct FullSendLine
{
    std::string  time;
    std::string  text;
};

struct FullSendTotals
{
    int       sent        = 0;
    int       duplicates  = 0;
    int       rawCaptures = 0;
    int       blanks      = 0;      // formatted disks with no files, which weren't sent
    int       badSectors  = 0;      // disks with an unreadable or unreliable sector
    double    elapsed     = 0.0;
    double    average     = 0.0;    // seconds a disk, over the disks sent; 0 with none
    uint64_t  bytes       = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendSnapshot
//
//  Everything the Full Send window draws, copied under the status's lock.
//
////////////////////////////////////////////////////////////////////////////////

struct FullSendSnapshot
{
    FullSendActivity                    activity = FullSendActivity::None;
    int                                 drive    = 0;       // the activity's drive, or 0
    double                              waited   = 0.0;     // seconds waiting for the Apple
    std::string                         reason;             // why the session ended
    std::array<FullSendDriveStatus, 2>  drives;
    std::array<bool, 2>                 present  = { true, true };  // clear for a drive the Apple lacks
    FullSendTotals                      totals;
    std::vector<FullSendDoneDisk>       disks;              // newest first
    std::vector<FullSendLine>           lines;
    uint64_t                            revision = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendStatus
//
//  One Full Send session for the Full Send window: written by the session's
//  thread as the session goes, and copied by the window's. The clocks are
//  injected: seconds for times left, and a text time for each activity line.
//  Pure: no I/O, no Win32.
//
//  The time left for a sector image is from the rate of this disk's tracks
//  once kRateTracks are read, and before that from the session's average for
//  a disk read the same way, verified or not. A raw capture's is the
//  capture's. With neither it is negative.
//
////////////////////////////////////////////////////////////////////////////////

class FullSendStatus
{
public:
    using SecondsFn = std::function<double()>;
    using TimeFn    = std::function<std::string()>;

    static constexpr int  kRateTracks = 2;

    FullSendStatus (SecondsFn seconds, TimeFn time);

    //  A session's start clears the one before it.
    void  Begin          ();
    void  End            (const std::string & reason);

    void  SetActivity    (FullSendActivity activity, int drive = 0);

    //  Whether the Apple has the drive. The window omits a drive it lacks.
    void  SetPresent     (int drive, bool present);

    //  The Apple's reply is awaited, or has come after a wait.
    void  SetWaiting     (bool waiting);

    //  haveTracks are the tracks a check read, marked without counting
    //  toward the disk's rate.
    void  BeginDisk      (int drive, bool verified, std::span<const int> haveTracks);
    void  SetTitle       (int drive, const std::string & title, const std::string & fileSystem);
    void  MarkTrack      (int drive, int track, FullSendTrackMark mark);

    void  BeginCapture   (int drive);
    void  SetCapture     (int drive, int pass, int position, int read, int reads, int percent, double secondsLeft);
    void  MarkPosition   (int drive, int position, FullSendPositionMark mark);
    void  SetCaptureTime (int drive, double diskSeconds, double lineSeconds);

    //  A disk done: sent or a duplicate, with its row and its image's size.
    void  EndDisk        (int drive, const FullSendRow & row, uint64_t bytes);

    //  A drive with no disk, or a disk left unsent.
    void  ClearDisk      (int drive, FullSendDiskState state);

    //  A blank disk, counted in the totals.
    void  AddBlank       ();

    void  AddLine        (const std::string & text);

    //  An image's boot result, as BootProbe::GetVerdictName has it, from the
    //  thread that booted it: set in the image's row of the disks done.
    //  TakeBoots returns each result since the last call, by file, for the
    //  session's manifest. renamedTo is the file's name now, when its boot
    //  renamed it.
    void  SetBoot        (const std::string & file, const std::string & boot, const std::string & renamedTo = std::string());

    std::vector<std::pair<std::string, std::string>>  TakeBoots ();

    FullSendSnapshot  GetSnapshot () const;

private:
    struct Sample
    {
        bool    verified = false;
        double  seconds  = 0.0;
    };

    FullSendDriveStatus & GetDrive      (int drive) { return m_snapshot.drives[GetIndex (drive)]; }
    static size_t         GetIndex      (int drive) { return drive == 2 ? 1 : 0; }
    double                EstimateLeft  (size_t index, double now) const;
    static int            CountRead     (const FullSendDriveStatus & drive);
    void                  AddTimedLine  (const std::string & text);

    mutable std::mutex                                m_lock;
    SecondsFn                                         m_seconds;
    TimeFn                                            m_time;
    FullSendSnapshot                                  m_snapshot;
    FullSendActivity                                  m_resume      = FullSendActivity::None;   // the activity a wait for the Apple interrupted
    int                                               m_resumeDrive = 0;
    double                                            m_began       = 0.0;
    double                                            m_waitBegan   = 0.0;
    double                                            m_ended       = -1.0;
    double                                            m_sentSeconds = 0.0;   // over every disk sent
    std::vector<Sample>                               m_samples;   // the sector images sent
    std::vector<std::pair<std::string, std::string>>  m_boots;   // each boot result the session hasn't taken
    std::array<int, 2>                                m_baseCount   = { 0, 0 };   // the tracks each drive's disk began with
    std::array<double, 2>                             m_lastMark    = { 0.0, 0.0 };   // when each drive's latest track or read was marked
};
