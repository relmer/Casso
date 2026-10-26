#pragma once

#include "Pch.h"

struct MachineConfig;





////////////////////////////////////////////////////////////////////////////////
//
//  SerialCardNoticeKind
//
////////////////////////////////////////////////////////////////////////////////

enum class SerialCardNoticeKind
{
    None,
    FirmwareMissing,
    FirmwareInvalid,
    AttachmentProblem,
    Downloading,
    FirmwareInstalled,
    DownloadFailed,
};





////////////////////////////////////////////////////////////////////////////////
//
//  SerialCardNotice
//
//  What the Super Serial Card's infobar reports, and the buttons it offers:
//  firmware that is missing or invalid, a serial attachment that could not
//  be opened, and the progress of a firmware download. None means the
//  infobar is hidden.
//
//  The text is built here rather than in the shell so a test can check it,
//  and so the card's state is read once, when the machine is built, rather
//  than from the UI thread while another thread rebuilds the machine.
//
////////////////////////////////////////////////////////////////////////////////

class SerialCardNotice
{
public:
    static constexpr const wchar_t *  kpszDownload = L"Download";
    static constexpr const wchar_t *  kpszRestart  = L"Restart";
    static constexpr const wchar_t *  kpszRetry    = L"Retry";
    static constexpr const wchar_t *  kpszDismiss  = L"Dismiss";

    // The first enabled Super Serial Card in `config`, with whether its
    // firmware loaded and any attachment problem the build stored.
    static SerialCardNotice  MakeForMachine     (const MachineConfig & config,
                                                 bool                  cardHasFirmware,
                                                 const string        & attachmentProblem);
    static SerialCardNotice  MakeDownloading    ();
    static SerialCardNotice  MakeInstalled      ();
    static SerialCardNotice  MakeDownloadFailed (const string & error);

    SerialCardNoticeKind  kind = SerialCardNoticeKind::None;
    wstring               text;
    vector<wstring>       actions;
};
