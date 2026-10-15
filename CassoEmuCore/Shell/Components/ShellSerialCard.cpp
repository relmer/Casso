#include "Pch.h"

#include "Shell/Components/ShellSerialCard.h"
#include "Shell/Components/ShellChrome.h"
#include "Shell/BackgroundWorkQueue.h"
#include "Shell/EmulatorShell.h"
#include "Machines/Apple2/Common/SuperSerialCard.h"
#include "Core/MachineConfig.h"
#include "AssetBootstrap.h"
#include "resource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ShellSerialCard
//
////////////////////////////////////////////////////////////////////////////////

ShellSerialCard::ShellSerialCard (EmulatorShell & shell)
    : m_shell      (shell),
      m_downloader (make_unique<BackgroundWorkQueue>())
{
    m_banner.SetSeverity (DxuiInfoBanner::Severity::Warning);
    m_banner.SetVisible  (false);
    m_banner.SetOnAction ([this] (size_t index) { OnAction (index); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~ShellSerialCard
//
//  The download queue finishes a download already under way before it is
//  destroyed.
//
////////////////////////////////////////////////////////////////////////////////

ShellSerialCard::~ShellSerialCard() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  NoteMachineBuilt
//
////////////////////////////////////////////////////////////////////////////////

void ShellSerialCard::NoteMachineBuilt()
{
    SuperSerialCard  * card        = m_shell.m_machine.GetRefs().superSerialCard;
    bool               hasFirmware = (card != nullptr) && card->HasFirmware();
    string             problem     = (card != nullptr) ? card->GetAttachmentProblem() : string();
    SerialCardNotice   notice      = SerialCardNotice::MakeForMachine (m_shell.m_machine.GetConfig(),
                                                                       hasFirmware,
                                                                       problem);
    lock_guard<mutex>  guard (m_pendingLock);



    m_pendingNotice = move (notice);
    ++m_pendingGeneration;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Tick
//
////////////////////////////////////////////////////////////////////////////////

void ShellSerialCard::Tick()
{
    uint32_t          generation = m_pendingGeneration.load();
    SerialCardNotice  notice;



    if (generation != m_appliedGeneration)
    {
        {
            lock_guard<mutex>  guard (m_pendingLock);

            notice = m_pendingNotice;
        }

        m_appliedGeneration = generation;

        // While a download is in progress, its own report is not replaced.
        if (!m_isDownloading)
        {
            Show (notice);
        }
    }

    if (m_downloadFinished.exchange (false))
    {
        m_isDownloading = false;
        Show (m_downloadSucceeded ? SerialCardNotice::MakeInstalled()
                                  : SerialCardNotice::MakeDownloadFailed (m_downloadError));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  StartFirmwareDownload
//
//  The result fields are written by the download job before it sets the
//  finished flag, and read by Tick only after it clears it.
//
////////////////////////////////////////////////////////////////////////////////

void ShellSerialCard::StartFirmwareDownload()
{
    HRESULT  hr = S_OK;



    BAIL_OUT_IF (m_isDownloading, S_OK);

    m_isDownloading = true;
    Show (SerialCardNotice::MakeDownloading());

    m_downloader->Post ([this] ()
    {
        fs::path  path;
        string    error;
        HRESULT   hrDownload = AssetBootstrap::DownloadOptionalRom ("SSC.rom", path, error);

        m_downloadSucceeded = SUCCEEDED (hrDownload);
        m_downloadError     = error;
        m_downloadFinished.store (true);
    });

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetBandThicknessPx
//
//  Zero while hidden, so the change band is only as tall as the visible
//  banners.
//
////////////////////////////////////////////////////////////////////////////////

int ShellSerialCard::GetBandThicknessPx (int clientWidthPx) const
{
    int  height = 0;



    if (m_banner.IsVisible() && clientWidthPx > 0)
    {
        height = static_cast<int> (m_banner.GetPreferredHeightPx (static_cast<float> (clientWidthPx), m_shell.m_scaler));
    }

    return height;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LayoutBanner
//
////////////////////////////////////////////////////////////////////////////////

void ShellSerialCard::LayoutBanner (const RECT & bounds)
{
    if (m_banner.IsVisible() && bounds.right > bounds.left)
    {
        m_banner.Layout (bounds, m_shell.m_scaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OfferMouseToBanner
//
//  Only while the banner is up, and only inside it.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellSerialCard::OfferMouseToBanner (DxuiMouseEventKind kind, int x, int y)
{
    RECT            bounds  = m_banner.GetBounds();
    DxuiMouseEvent  ev      = {};
    bool            inside  = m_banner.IsVisible()
                              && x >= bounds.left && x < bounds.right
                              && y >= bounds.top  && y < bounds.bottom;
    bool            handled = false;



    if (inside)
    {
        ev.kind        = kind;
        ev.button      = DxuiMouseButton::Left;
        ev.positionDip = POINT { x, y };

        handled = m_banner.OnMouse (ev);
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Show
//
//  Sets the banner's text, buttons and severity from a notice, and hides it
//  for None.
//
////////////////////////////////////////////////////////////////////////////////

void ShellSerialCard::Show (const SerialCardNotice & notice)
{
    bool  isGood = notice.kind == SerialCardNoticeKind::FirmwareInstalled ||
                   notice.kind == SerialCardNoticeKind::Downloading;



    m_notice = notice;

    m_banner.SetSeverity (isGood ? DxuiInfoBanner::Severity::Info : DxuiInfoBanner::Severity::Warning);
    m_banner.SetText     (notice.text);
    m_banner.SetActions  (notice.actions);
    m_banner.SetVisible  (notice.kind != SerialCardNoticeKind::None);

    if (m_shell.m_chrome != nullptr)
    {
        m_shell.m_chrome->ReflowChromeForChangeBand();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnAction
//
//  Restart rebuilds the current machine, which reads the firmware file
//  again; a Ctrl-Reset would not.
//
////////////////////////////////////////////////////////////////////////////////

void ShellSerialCard::OnAction (size_t index)
{
    wstring  label = (index < m_notice.actions.size()) ? m_notice.actions[index] : wstring();



    if (label == SerialCardNotice::kpszDownload || label == SerialCardNotice::kpszRetry)
    {
        StartFirmwareDownload();
    }
    else if (label == SerialCardNotice::kpszRestart)
    {
        Show (SerialCardNotice());
        m_shell.PostCommand (IDM_FILE_OPEN, m_shell.GetCurrentMachineNameNarrow());
    }
    else
    {
        Show (SerialCardNotice());
    }
}
