#pragma once

#include "Pch.h"

#include "CassoLink/FullSend/FullSendManifest.h"
#include "CassoLink/FullSend/FullSendStatus.h"
#include "CassoLink/Session/CassoLinkSessionTypes.h"

class IDiskFileIo;





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

enum class FullSendKey
{
    Space,
    Quit,
    Drive1,
    Drive2,
    Verify,
    Prefix,
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

    //  The rows the Apple draws: the title with the settings, the counts, a
    //  line, and each drive's row and map. The prefix is quoted, so a space at
    //  its end shows.
    std::vector<std::string>  GetRows () const;

    //  A prompt line, from 1. Each fits after ON DECK-- on a 40-column row.
    static const char *  GetQuipText      (Byte quip);

    //  DUPE OF and an earlier disk's file, cut short to fit a drive's row.
    static std::string   GetDuplicateText (const std::string & file, const char * mark);
    static std::string & MakeTrackRuler   (std::string & ioMap);
};





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendReadReport
//
//  What a disk's read found: the sectors that weren't clean, and how many of its
//  tracks were verified.
//
////////////////////////////////////////////////////////////////////////////////

struct FullSendReadReport
{
    std::vector<SectorMark>  marks;
    int                      tracksVerified = 0;
    int                      emptyTracks    = 0;   // tracks where no sector was found
};





////////////////////////////////////////////////////////////////////////////////
//
//  IFullSendLink
//
//  The Apple, as a Full Send uses it. An image is DOS logical sectors in an
//  image-sized buffer. kQuit from any call is Q on the Apple;
//  ERROR_CANCELLED is Cancel on the PC.
//
////////////////////////////////////////////////////////////////////////////////

class IFullSendLink
{
public:
    //  HRESULT_FROM_WIN32 (ERROR_OPERATION_ABORTED).
    static constexpr HRESULT  kQuit = (HRESULT) 0x800703E3;

    virtual ~IFullSendLink () = default;

    virtual bool     HasDrive   (int drive) = 0;

    //  A track's sectors into the image; outHasDisk is clear for a drive with
    //  no readable disk, and outWhole for a track with an unreadable sector.
    virtual HRESULT  ReadTrack  (int drive, int track, std::vector<Byte> & ioImage, bool & outHasDisk, bool & outWhole) = 0;

    //  Every track but those already in the image; an unreadable sector is left
    //  as zeros and listed by track and DOS 3.3 sector.
    virtual HRESULT  ReadDisk   (int drive, std::vector<Byte> & ioImage, std::span<const int> haveTracks, std::vector<std::pair<int, int>> & outUnreadable, FullSendReadReport & outReport) = 0;
    virtual HRESULT  ShowScreen (const FullSendScreen & screen) = 0;

    //  No drive turns until a key arrives. The lines are drawn on the Apple until
    //  then.
    virtual HRESULT  WaitForKey (const std::vector<std::string> & lines, FullSendKey & outKey) = 0;

    //  As WaitForKey, with any key's code, as the Apple's keyboard sends it, for
    //  a line typed there.
    virtual HRESULT  WaitForAnyKey (const std::vector<std::string> & lines, Byte & outCode) = 0;

    //  Whether verified reads are on; V on the Apple switches them.
    virtual bool     IsVerifying    () = 0;
    virtual void     ToggleVerify   () = 0;

    //  Whether this PC bootstrapped the Apple, so no CassoLink disk was booted.
    virtual bool     IsBootstrapped () = 0;

    //  A drive's disk captured raw as a WOZ. outReads has every read of the
    //  capture, as CassoLinkCapture::SerializeReads writes them, when reads
    //  are saved, and is empty otherwise. With withEarlier, the WOZ is built
    //  from this capture's reads and those of the drive's capture before it,
    //  which was of the same disk.
    virtual HRESULT  CaptureRaw     (int drive, bool withEarlier, std::vector<Byte> & outWoz, std::vector<Byte> & outReads, std::string & outMarks) = 0;

    //  An image the session wrote, by its path, for the boot check. True
    //  when the image is booted, and AwaitBoot then returns once its result
    //  is in the session's status, or after a time limit.
    virtual bool     NoteImage      (const std::string & path) { UNREFERENCED_PARAMETER (path); return false; }
    virtual HRESULT  AwaitBoot      (const std::string & file) { UNREFERENCED_PARAMETER (file); return S_OK; }

    //  Whether the Apple sounds a tone for an image's boot result.
    virtual bool     HasBootTones   () { return false; }

    //  A line for the activity log.
    virtual void     Report     (const std::string & text) = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendSession
//
//  One Full Send: disks read from a linked Apple's drives into a folder, one
//  at a time, alternating between the drives, until Q, Cancel or a
//  failure ends it. Pure: the Apple, the clock and the files are injected.
//
//  It begins on Space. After each disk the other drive is checked. A disk is new
//  when its fingerprint is not in the manifest. With no new disk the session
//  pauses for Space, which checks both drives. A repeated disk isn't read: the
//  prompt lists its drive, and that drive's number sends it.
//
////////////////////////////////////////////////////////////////////////////////

class FullSendSession
{
public:
    enum class State
    {
        Starting,
        Checking,
        Sending,
        Paused,
        Ended,
    };

    enum class EndReason
    {
        None,
        Quit,
        Canceled,
        LinkFailed,
        ManifestDamaged,
        FileFailed,
    };

    using ClockFn = std::function<std::string()>;

    //  A chosen extension has its leading period; empty is .po for ProDOS and
    //  Pascal and .dsk for any other. The status is the Full Send window's
    //  model, written as the session goes; nullptr for none.
    FullSendSession (IFullSendLink & link, IDiskFileIo & fileIo, ClockFn clock, std::string folder, std::string extension, std::string prefix, FullSendStatus * status = nullptr);

    //  Returns when the session ends. Q returns kQuit and Cancel
    //  ERROR_CANCELLED; no session ends in S_OK.
    HRESULT  Run ();

    State                     GetState     () const { return m_state; }
    EndReason                 GetEndReason () const { return m_endReason; }
    const FullSendManifest  & GetManifest  () const { return m_manifest; }
    const FullSendScreen    & GetScreen    () const { return m_screen; }

    //  What begins each image's file name, as typed on the Apple last.
    const std::string       & GetPrefix    () const { return m_prefix; }

    //  The result for the user: the counts and why the session ended.
    const std::string       & GetMessage   () const { return m_message; }

    //  The lock every session in the process takes around a change to a
    //  folder's manifest and images, also taken by a rename in the folder.
    static std::mutex       & GetFolderLock (const std::string & folder);

private:
    enum class DiskCheck
    {
        None,
        New,
        Repeat,
        Blank,      // formatted, with no files; not sent unless its drive's number is pressed
    };

    //  A drive's disk as a check read it. The tracks read whole are not read a
    //  third time when the disk is sent, as each track read costs most of a second.
    struct CheckedDisk
    {
        DiskCheck          check = DiskCheck::None;
        std::string        fingerprint;
        std::vector<Byte>  image;
        std::vector<int>   wholeTracks;
        std::string        system;          // a blank disk's file system

        bool  IsReadWhole (int track) const { return std::find (wholeTracks.begin(), wholeTracks.end(), track) != wholeTracks.end(); }
    };

    static constexpr int     kFingerprintReads = 3;
    static constexpr size_t  kPrefixLength     = 16;

    static constexpr const char *  kRawExtension = ".woz";    // a raw capture's
    static constexpr const char *  kReadsSuffix  = ".reads";  // added to a raw capture's file for its reads

    HRESULT  LoadManifest     ();
    HRESULT  ReloadManifest   ();
    HRESULT  AwaitBegin       ();
    HRESULT  EditPrefix       ();
    HRESULT  RunRound         ();
    HRESULT  Pause            ();
    HRESULT  CheckDrive       (int drive, CheckedDisk & outDisk);
    HRESULT  CheckBlank       (int drive, CheckedDisk & ioDisk);
    HRESULT  ReadFingerprint  (int drive, CheckedDisk & outDisk, bool & outStable);
    HRESULT  ReadStableTrack  (int drive, int track, CheckedDisk & ioDisk, bool & outHasDisk, bool & outStable);
    HRESULT  SendDisk         (int drive, const CheckedDisk & checked, bool anyway);
    HRESULT  CaptureProtected (int drive, const std::vector<Byte> & image, const std::vector<std::pair<int, int>> & unreadable, const FullSendReadReport & report, std::vector<Byte> & outRaw);
    HRESULT  WriteImage       (const std::string & title, const std::vector<Byte> & image, const std::vector<Byte> & raw, const std::vector<std::pair<int, int>> & unreadable, const std::vector<SectorMark> & marks, std::string & outFile);
    void     ApplyBoots       ();
    HRESULT  SendAgain        (int drive, const CheckedDisk & disk);
    HRESULT  WriteManifest    ();
    void     Report           (const std::string & text);
    void     MarkTracks       (int drive, const std::vector<std::pair<int, int>> & unreadable, const FullSendReadReport & report);
    HRESULT  FindMovedImages  (const std::function<bool (const FullSendRow & row)> & matches);
    HRESULT  HashUnlisted     ();
    bool     HasImage         (const FullSendRow & row);
    void     End              (HRESULT hr);

    void                             ReportRepeat  (int drive, const CheckedDisk & disk);
    const FullSendRow              * FindWithImage (const std::function<bool (const FullSendRow & row)> & matches);
    Byte                             PickQuip      ();
    std::vector<std::string>         GetPauseLines (const std::array<bool, 2> & repeats) const;
    std::string                      GetPrefixLine () const;
    static std::string               GetScreenName (int sequence, const std::string & catalogTitle);
    void                             SetDriveState (int drive, FullSendDriveState state) { m_screen.drives[(size_t) drive - 1] = state; }
    std::string                      GetPath       (const std::string & file) const;

    IFullSendLink             & m_link;
    IDiskFileIo               & m_fileIo;
    ClockFn                     m_clock;
    std::string                 m_folder;
    std::string                 m_extension;
    std::string                 m_prefix;
    FullSendStatus              m_unread;        // the status when the session was given none
    FullSendStatus            & m_status;
    FullSendManifest            m_manifest;
    FullSendScreen              m_screen;
    State                       m_state      = State::Starting;
    EndReason                   m_endReason  = EndReason::None;
    std::string                 m_message;
    int                         m_lastRead   = 0;
    bool                        m_lastSent   = false;
    std::array<bool, 2>         m_deckShown  = { false, false };
    std::array<bool, 2>         m_repeats    = { false, false };   // drives whose last check matched the manifest
    std::array<bool, 2>         m_swapped    = { false, false };   // and of those, the disks that changed during their read
    std::array<std::string, 2>  m_counted;                         // the repeat each drive last added to the duplicates
    std::array<bool, 2>         m_blank      = { false, false };   // drives whose last check found a blank disk
    std::vector<std::string>    m_unbooted;                        // a line for each image that did not boot, for the next pause
    std::string                 m_retryPrint;                      // the fingerprint of the disk the pause offers to read again
    std::set<std::string>       m_retried;                         // the fingerprint of each disk read again
    bool                        m_deeper     = false;              // the disk being sent is one read again
    std::string                 m_latestBoot;                      // the boot result ApplyBoots took last
    std::array<std::string, 2>  m_blankDisk;                       // the blank each drive last added to the blanks
    uint64_t                    m_commitTag  = 1;
    std::vector<Byte>           m_rawReads;                        // the last raw capture's reads, until its image is written
    std::string                 m_rawMarks;                        // and what it marked, for its row
    std::mt19937                m_random     { std::random_device{}() };
    std::vector<Byte>           m_quips;         // the prompt lines left before any repeats
    Byte                        m_lastQuip   = 0;
    bool                        m_fileFailed = false;
    std::set<std::string>       m_changed;       // fingerprints of disks that changed during a read, sent only after Space

    //  The folder's images that the manifest doesn't list, by file name and
    //  hash, read once the first time a row's image is missing.
    std::vector<std::pair<std::string, std::string>>  m_unlisted;
    bool                                              m_unlistedRead = false;
};
