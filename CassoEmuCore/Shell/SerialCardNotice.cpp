#include "Pch.h"

#include "Shell/SerialCardNotice.h"
#include "Core/MachineConfig.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MakeForMachine
//
//  Firmware problems come first: without firmware, PR#n and IN#n jump into an
//  empty socket. A file that is present but did not load (unreadable, or the
//  wrong size) reads as invalid. An attachment problem is reported only when
//  the firmware is in order, since the infobar has room for one report.
//
////////////////////////////////////////////////////////////////////////////////

SerialCardNotice SerialCardNotice::MakeForMachine (
    const MachineConfig & config,
    bool                  cardHasFirmware,
    const string        & attachmentProblem)
{
    HRESULT             hr      = S_OK;
    SerialCardNotice    notice;
    const SlotConfig  * card    = nullptr;
    bool                hasCard = false;
    wstring             path;



    for (const SlotConfig & slot : config.slots)
    {
        if (card == nullptr && slot.enabled && slot.device == kpszSuperSerialCardDevice)
        {
            card = &slot;
        }
    }

    hasCard = (card != nullptr);
    BAIL_OUT_IF (!hasCard, S_OK);

    path = TextEncoding::NarrowToWide (card->firmwarePath);

    if (card->firmwareStatus == SlotFirmwareStatus::Missing)
    {
        notice.kind    = SerialCardNoticeKind::FirmwareMissing;
        notice.text    = format (L"The Super Serial Card in slot {} has no firmware. Casso looked for {}.",
                                 card->slot, path);
        notice.actions = { kpszDownload, kpszDismiss };
    }
    else if (card->firmwareStatus != SlotFirmwareStatus::None && !cardHasFirmware)
    {
        notice.kind    = SerialCardNoticeKind::FirmwareInvalid;
        notice.text    = format (L"The Super Serial Card firmware at {} is not Apple's 2 KB ROM, so the card in "
                                 L"slot {} is running without firmware.",
                                 path, card->slot);
        notice.actions = { kpszDownload, kpszDismiss };
    }
    else if (!attachmentProblem.empty())
    {
        notice.kind    = SerialCardNoticeKind::AttachmentProblem;
        notice.text    = TextEncoding::NarrowToWide (attachmentProblem);
        notice.actions = { kpszDismiss };
    }

Error:
    return notice;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakeDownloading
//
////////////////////////////////////////////////////////////////////////////////

SerialCardNotice SerialCardNotice::MakeDownloading()
{
    SerialCardNotice  notice;



    notice.kind    = SerialCardNoticeKind::Downloading;
    notice.text    = L"Downloading the Super Serial Card firmware.";
    notice.actions = { kpszDismiss };

    return notice;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakeInstalled
//
//  The firmware is read when the machine is built, so it takes effect after a
//  restart, which rebuilds the machine.
//
////////////////////////////////////////////////////////////////////////////////

SerialCardNotice SerialCardNotice::MakeInstalled()
{
    SerialCardNotice  notice;



    notice.kind    = SerialCardNoticeKind::FirmwareInstalled;
    notice.text    = L"The Super Serial Card firmware is installed. Restart the machine to load it.";
    notice.actions = { kpszRestart, kpszDismiss };

    return notice;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakeDownloadFailed
//
////////////////////////////////////////////////////////////////////////////////

SerialCardNotice SerialCardNotice::MakeDownloadFailed (const string & error)
{
    SerialCardNotice  notice;



    notice.kind    = SerialCardNoticeKind::DownloadFailed;
    notice.text    = L"The Super Serial Card firmware could not be downloaded. " + TextEncoding::NarrowToWide (error);
    notice.actions = { kpszRetry, kpszDismiss };

    return notice;
}
