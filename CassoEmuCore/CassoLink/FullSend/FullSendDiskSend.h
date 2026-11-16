#pragma once

#include "Pch.h"

#include "CassoLink/FullSend/FullSendFolder.h"
#include "CassoLink/FullSend/FullSendManifest.h"
#include "CassoLink/FullSend/FullSendProtection.h"

class FullSendContext;
class FullSendDriveCheck;
struct FullSendCheckedDisk;





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDiskSend
//
//  A disk sent from a drive into the folder: read, captured raw when it is
//  likely copy-protected, written and listed in the manifest, then booted
//  on the PC.
//
////////////////////////////////////////////////////////////////////////////////

enum class FullSendSendMode
{
    New,            // a disk the check found new
    Anyway,         // a repeat or a blank, sent on its drive's number
    Again,          // a disk whose image did not boot, read again with verified reads
    RawAgain,       // and one whose first image is the sectors of a disk that may be copy-protected
};

class FullSendDiskSend
{
public:
    using ClockFn = std::function<std::string()>;

    FullSendDiskSend (FullSendContext & context, FullSendDriveCheck & check, ClockFn clock);

    HRESULT  Send          (int drive, const FullSendCheckedDisk & checked, FullSendSendMode mode, const std::string & prefix, bool & outSent);
    void     SetProtection (const FullSendProtectionChoices & choices) { m_protection = choices; }
    bool     HasFileFailed () const                                    { return m_fileFailed; }

private:
    //  One disk as it is sent.
    struct Sending
    {
        int                          drive       = 0;
        FullSendSendMode             mode        = FullSendSendMode::New;
        const FullSendCheckedDisk  * checked     = nullptr;
        std::string                  prefix;
        std::string                  title;
        FullSendImage                image;
        bool                         same        = true;     // the disk didn't change during its read
        bool                         isRawRetry  = false;    // a raw capture of a disk sent as sectors just before
        FullSendRow                  first;                  // and that image's row
        bool                         isLikely    = false;    // likely copy-protected
        bool                         isProtected = false;    // and saved as its sectors
        bool                         hasEarlier  = false;    // a duplicate of an earlier image
        std::string                  earlier;                // and its file
        std::string                  match;                  // an image with the same sectors, sent anyway or not
        FullSendRow                  row;
    };

    HRESULT             Begin            (Sending & disk);
    HRESULT             Read             (Sending & disk);
    HRESULT             ReadWholeDisk    (int drive, const FullSendCheckedDisk & checked, FullSendImage & ioImage, bool & outSame);
    HRESULT             DropChanged      (Sending & disk);
    HRESULT             CaptureProtected (Sending & disk);
    HRESULT             CheckVolume      (Sending & disk);
    HRESULT             Record           (Sending & disk);
    HRESULT             Identify         (Sending & disk);
    void                BuildRow         (Sending & disk, int sequence, const std::string & hash);
    void                Tally            (Sending & disk);
    void                ReportSent       (const Sending & disk);
    HRESULT             AwaitBootCheck   (Sending & disk);
    void                MarkTracks       (int drive, const FullSendImage & image);
    static std::string  GetScreenName    (int sequence, const std::string & catalogTitle);
    static bool         IsAgain          (FullSendSendMode mode) { return mode == FullSendSendMode::Again || mode == FullSendSendMode::RawAgain; }

    FullSendContext            & m_context;
    FullSendDriveCheck         & m_check;
    ClockFn                      m_clock;
    FullSendProtectionChoices    m_protection;
    std::array<bool, 2>          m_deckShown  = { false, false };
    bool                         m_fileFailed = false;
};
