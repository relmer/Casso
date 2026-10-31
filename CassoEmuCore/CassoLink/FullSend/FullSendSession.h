#pragma once

#include "Pch.h"

#include "CassoLink/FullSend/FullSendManifest.h"

class IDiskFileIo;





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendScreen
//
//  The Apple's full send screen. Off in both drives clears it, and Escape on
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
};

enum class FullSendTone : Byte
{
    None,
    Done,
    Duplicate,
    BadSectors,
};

enum class FullSendKey
{
    Space,
    Escape,
};

struct FullSendScreen
{
    std::array<FullSendDriveState, 2>  drives     = { FullSendDriveState::Idle, FullSendDriveState::Idle };
    int                                sent       = 0;
    int                                duplicates = 0;
    int                                bad        = 0;
    FullSendTone                       tone       = FullSendTone::None;
    std::string                        name;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IFullSendLink
//
//  The Apple, as a full send uses it. An image is DOS logical sectors in an
//  image-sized buffer. kEscaped from any call is Escape on the Apple;
//  ERROR_CANCELLED is Cancel on the PC.
//
////////////////////////////////////////////////////////////////////////////////

class IFullSendLink
{
public:
    //  HRESULT_FROM_WIN32 (ERROR_OPERATION_ABORTED).
    static constexpr HRESULT  kEscaped = (HRESULT) 0x800703E3;

    virtual ~IFullSendLink () = default;

    virtual bool     HasDrive   (int drive) = 0;

    //  A track's sectors into the image; outHasDisk is clear for a drive with
    //  no readable disk, and outWhole for a track with an unreadable sector.
    virtual HRESULT  ReadTrack  (int drive, int track, std::vector<Byte> & ioImage, bool & outHasDisk, bool & outWhole) = 0;

    //  Every track but those already in the image; an unreadable sector is left
    //  as zeros and listed by track and DOS 3.3 sector.
    virtual HRESULT  ReadDisk   (int drive, std::vector<Byte> & ioImage, std::span<const int> haveTracks, std::vector<std::pair<int, int>> & outUnreadable) = 0;
    virtual HRESULT  ShowScreen (const FullSendScreen & screen) = 0;

    //  No drive turns until a key arrives.
    virtual HRESULT  WaitForKey (FullSendKey & outKey) = 0;

    //  A line for the activity log.
    virtual void     Report     (const std::string & text) = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendSession
//
//  One full send: disks read from a linked Apple's drives into a folder, one
//  at a time, alternating between the drives, until Escape, Cancel or a
//  failure ends it. Pure: the Apple, the clock and the files are injected.
//
//  After each disk the other drive is checked and then the drive just read.
//  A disk is new when its fingerprint reads the same twice and is not in the
//  manifest. With no new disk the session pauses for Space, and Space then
//  sends a repeated disk when there is no new one, the drive not read last
//  first.
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
        Escape,
        Canceled,
        LinkFailed,
        ManifestDamaged,
        FileFailed,
    };

    using ClockFn = std::function<std::string()>;

    //  A chosen extension has its leading period; empty is .po for ProDOS and
    //  .dsk for any other.
    FullSendSession (IFullSendLink & link, IDiskFileIo & fileIo, ClockFn clock, std::string folder, std::string extension);

    //  Returns when the session ends. Escape returns kEscaped and Cancel
    //  ERROR_CANCELLED; no session ends in S_OK.
    HRESULT  Run ();

    State                     GetState     () const { return m_state; }
    EndReason                 GetEndReason () const { return m_endReason; }
    const FullSendManifest  & GetManifest  () const { return m_manifest; }
    const FullSendScreen    & GetScreen    () const { return m_screen; }

    //  The result for the user: the counts and why the session ended.
    const std::string       & GetMessage   () const { return m_message; }

private:
    enum class DiskCheck
    {
        None,
        New,
        Repeat,
    };

    //  A drive's disk as a check read it. The tracks read whole are not read a
    //  third time when the disk is sent, as each track read costs most of a second.
    struct CheckedDisk
    {
        DiskCheck          check = DiskCheck::None;
        std::string        fingerprint;
        std::vector<Byte>  image;
        std::vector<int>   wholeTracks;
    };

    static constexpr int  kFingerprintReads = 3;

    HRESULT  LoadManifest    ();
    HRESULT  RunRound        ();
    HRESULT  Pause           ();
    HRESULT  CheckDrive      (int drive, CheckedDisk & outDisk);
    HRESULT  ReadFingerprint (int drive, CheckedDisk & outDisk, bool & outStable);
    HRESULT  ReadStableTrack (int drive, int track, CheckedDisk & ioDisk, bool & outHasDisk, bool & outStable);
    HRESULT  SendDisk        (int drive, const CheckedDisk & checked);
    HRESULT  WriteImage      (const std::string & title, const std::vector<Byte> & image, const std::vector<std::pair<int, int>> & unreadable, std::string & outFile);
    HRESULT  WriteManifest   ();
    void     End             (HRESULT hr);

    void         SetDriveState (int drive, FullSendDriveState state) { m_screen.drives[(size_t) drive - 1] = state; }
    std::string  GetPath       (const std::string & file) const;

    IFullSendLink          & m_link;
    IDiskFileIo            & m_fileIo;
    ClockFn                  m_clock;
    std::string              m_folder;
    std::string              m_extension;
    FullSendManifest         m_manifest;
    FullSendScreen           m_screen;
    State                    m_state      = State::Starting;
    EndReason                m_endReason  = EndReason::None;
    std::string              m_message;
    int                      m_lastRead   = 0;
    uint64_t                 m_commitTag  = 1;
    bool                     m_fileFailed = false;
    std::set<std::string>    m_changed;       // fingerprints of disks that changed during a read, sent only after Space
};
