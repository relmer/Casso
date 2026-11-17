#pragma once

#include "Pch.h"

#include "CassoLink/FullSend/FullSendContext.h"
#include "CassoLink/FullSend/FullSendDiskSend.h"
#include "CassoLink/FullSend/FullSendDriveCheck.h"
#include "CassoLink/FullSend/FullSendFolder.h"
#include "CassoLink/FullSend/FullSendStatus.h"

class IDiskFileIo;
class IFullSendLink;





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

    using ClockFn = FullSendDiskSend::ClockFn;

    FullSendSession (IFullSendLink & link, IDiskFileIo & fileIo, ClockFn clock, std::string folder, std::string extension, std::string prefix, FullSendStatus * status = nullptr);

    HRESULT                   Run           ();
    void                      SetProtection (const FullSendProtectionChoices & choices) { m_send.SetProtection (choices); }
    State                     GetState      () const                                    { return m_state; }
    EndReason                 GetEndReason  () const                                    { return m_endReason; }
    const FullSendManifest  & GetManifest   () const                                    { return m_folder.GetManifest(); }
    const FullSendScreen    & GetScreen     () const                                    { return m_context.screen; }
    const std::string       & GetPrefix     () const                                    { return m_prefix; }
    const std::string       & GetMessage    () const                                    { return m_message; }

private:
    static constexpr size_t  kPrefixLength = 16;

    HRESULT                   AwaitBegin    ();
    HRESULT                   EditPrefix    ();
    HRESULT                   RunRound      ();
    HRESULT                   Pause         ();
    HRESULT                   CheckDrives   (const std::string & retry, bool & outSent);
    HRESULT                   SendRepeat    (int drive, bool & outSent);
    HRESULT                   SendAgain     (int drive, const FullSendCheckedDisk & disk);
    HRESULT                   CheckDrive    (int drive, FullSendCheckedDisk & outDisk);
    HRESULT                   SendDisk      (int drive, const FullSendCheckedDisk & disk, FullSendSendMode mode);
    void                      End           (HRESULT hr);
    std::array<int, 2>        GetOrder      () const;
    std::vector<std::string>  GetPauseLines () const;
    std::string               GetPrefixLine () const;

    FullSendStatus            m_unread;                         // the status when the session was given none
    FullSendStatus          & m_status;
    FullSendFolder            m_folder;
    FullSendContext           m_context;
    FullSendDriveCheck        m_check;
    FullSendDiskSend          m_send;
    std::string               m_prefix;
    State                     m_state     = State::Starting;
    EndReason                 m_endReason = EndReason::None;
    std::string               m_message;
    int                       m_lastRead  = 0;                  // the drive whose disk was sent last
    bool                      m_lastSent  = false;              // and whether it was sent since the last pause
};
