#pragma once

#include "Pch.h"

#include "Shell/SerialCardNotice.h"

class BackgroundWorkQueue;
class EmulatorShell;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellSerialCard
//
//  The Super Serial Card's infobar: missing or invalid firmware, an
//  attachment that could not be opened, and the firmware download the
//  infobar and Settings start. With none of those, it reports the newest
//  problem any serial attachment posted to the machine's SerialLinkReports,
//  read again each frame its generation moves.
//
//  The machine may be rebuilt on a thread other than the UI's, so a build
//  only stores the notice for the infobar (NoteMachineBuilt), and the UI
//  thread applies it on its next frame (Tick). The download runs on a
//  background queue and is applied the same way.
//
//  The banner itself is a DxuiActionBanner. It shares the chrome's change
//  band with the disk-change banner, below it, so ShellChrome lays the two
//  out together.
//
////////////////////////////////////////////////////////////////////////////////

class ShellSerialCard
{
public:
    explicit ShellSerialCard (EmulatorShell & shell);
    ~ShellSerialCard();

    // Any thread: stores the notice for the machine just built.
    void  NoteMachineBuilt ();

    // UI thread, once per frame: applies a stored notice or a finished
    // download.
    void  Tick ();

    // UI thread: downloads Apple's firmware into the asset folder. With
    // `reportInInfobar`, progress and the result appear in the infobar;
    // Settings passes false and reports the result itself.
    void  StartFirmwareDownload (bool reportInInfobar = true);
    bool  IsDownloading () const { return m_isDownloading; }

    // Downloads finished so far, and the last one's outcome, for a caller
    // that started one and waits for the count to move.
    uint32_t        GetCompletedDownloadCount () const { return m_completedDownloads; }
    bool            DidLastDownloadSucceed    () const { return m_lastDownloadSucceeded; }
    const string &  GetLastDownloadError      () const { return m_lastDownloadError; }

    DxuiActionBanner &  GetBanner           () { return m_banner; }
    int                 GetBandThicknessPx  (int clientWidthPx) const;
    void                LayoutBanner        (const RECT & bounds);
    bool                OfferMouseToBanner  (DxuiMouseEventKind kind, int x, int y);

private:
    void  Show     (const SerialCardNotice & notice);
    void  ShowLinkReports ();
    void  OnAction (size_t index);

    EmulatorShell                        & m_shell;
    DxuiActionBanner                       m_banner;
    SerialCardNotice                       m_notice;

    mutex                                  m_pendingLock;
    SerialCardNotice                       m_pendingNotice;
    atomic<uint32_t>                       m_pendingGeneration     = 0;
    uint32_t                               m_appliedGeneration     = 0;
    uint64_t                               m_appliedLinkGeneration = 0;

    bool          m_isDownloading         = false;
    bool          m_reportInInfobar       = true;
    uint32_t      m_completedDownloads    = 0;
    bool          m_lastDownloadSucceeded = false;
    string        m_lastDownloadError;
    atomic<bool>  m_downloadFinished      = false;
    bool          m_downloadSucceeded     = false;
    string        m_downloadError;

    // Last, so it is destroyed first: its destructor finishes a download
    // job, which writes the members above.
    unique_ptr<BackgroundWorkQueue>        m_downloader;
};
