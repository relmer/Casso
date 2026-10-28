#include "Pch.h"

#include "Shell/Components/ShellPrinter.h"
#include "Shell/EmulatorShell.h"
#include "Shell/WindowCommandManager.h"
#include "Shell/Components/ShellWindow.h"
#include "Shell/Components/ShellRenderer.h"
#include "Shell/Components/ShellChrome.h"
#include "Shell/Components/ShellSettings.h"
#include "Shell/MachineBuilder.h"
#include "Shell/Components/ShellAudio.h"
#include "Shell/EmulatorShellInternal.h"
#include "AssetBootstrap.h"
#include "Config/MonitorCatalog.h"
#include "Config/MachineInputPrefs.h"
#include "Config/CrtPresets.h"
#include "Config/CrtResolver.h"
#include "Print/PrintJobStore.h"
#include "Machines/Apple2/Common/PrinterCard.h"
#include "Ui/PrinterPanel.h"
#include "Core/PathResolver.h"
#include "Version.h"
#include "BuildInfo.h"
#include "resource.h"
#include "Devices/RamDevice.h"
#include "Devices/RomDevice.h"
#include "Machines/Apple2/Common/AppleKeyboard.h"
#include "Machines/Apple2/Apple2e/Apple2eKeyboard.h"
#include "Machines/Apple2/Common/AppleSoftSwitchBank.h"
#include "Machines/Apple2/Common/AppleGamePort.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "Machines/Apple2/Common/AppleSpeaker.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Machines/Apple2/Common/MockingboardCard.h"
#include "Machines/Apple2/Common/LanguageCard.h"
#include "Machines/Apple2/Apple2e/Apple2eMmu.h"
#include "Machines/Apple2/Apple2c/Apple2cRomBank.h"
#include "Machines/MachineDefinitions.h"
#include "Shell/FramePacing.h"
#include "Shell/Input/AppleKeyMapping.h"
#include "Shell/Layout/DriveRowLayout.h"
#include "Machines/Apple2/Common/AppleMouse.h"
#include "Core/Prng.h"
#include "Config/DiskSettings.h"
#include "Core/UnicodeSymbols.h"
#include "Core/MachineConfig.h"
#include "Core/JsonParser.h"
#include "Machines/Apple2/Common/AppleTextMode.h"
#include "Machines/Apple2/Common/Apple80ColTextMode.h"
#include "Machines/Apple2/Common/AppleLoResMode.h"
#include "Machines/Apple2/Common/AppleHiResMode.h"
#include "Machines/Apple2/Common/AppleDoubleHiResMode.h"
#include "Video/PixelFormat.h"
#include "Video/MonochromeTint.h"
#include "Ui/Chrome/ChromeMetrics.h"
#include "Ui/DriveWidgetController.h"
#include "Shell/DiskMru.h"
#include "Ui/Dialogs/DialogBodyContent.h"
#include "Ui/Dialogs/MessageDialog.h"
#include "Ui/Dialogs/SalvageDialogContent.h"
#include "Ui/Settings/SettingsPanelState.h"
#include "Ui/Settings/SettingsSheet.h"   // TEMP (T162 3a dev trigger)
#include "Seams/Win32IntentChannel.h"
#include "Devices/Disk/PreservedCopy.h"
#include "Core/ThreadName.h"
#include "Shell/ClipboardManager.h"
#include "Ui/Dialogs/DialogDefinition.h"
#include "Devices/Printer/PaperRenderer.h"
#include "Devices/Printer/PngCodec.h"
#include "Devices/Printer/PrintDelivery.h"
#include "Devices/Printer/PrintFileNaming.h"
#include "Devices/Printer/PrintPagination.h"
#include "Devices/Printer/PrintRaster.h"
#include "Devices/Printer/RgbaImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ShellPrinter
//
////////////////////////////////////////////////////////////////////////////////

ShellPrinter::ShellPrinter (EmulatorShell & shell)
    : m_shell (shell)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~ShellPrinter
//
//  Out of line so PrinterPanel is complete where its unique_ptr destroys it.
//
////////////////////////////////////////////////////////////////////////////////

ShellPrinter::~ShellPrinter() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellPrinter::BindBuildServices
//
////////////////////////////////////////////////////////////////////////////////

void ShellPrinter::BindBuildServices (MachineBuildServices & services)
{
    services.printerWorker           = &m_printerWorker;
    services.printerAutoOpenActivity = &m_printerAutoOpenActivity;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellPrinter::ShowPrinterPanel
//
//  Lazily creates the printer panel / print preview window, wires its toolbar
//  callbacks to the existing delivery commands, pushes a fresh strip snapshot,
//  and brings it to the foreground.
//
////////////////////////////////////////////////////////////////////////////////

void ShellPrinter::ShowPrinterPanel (bool activate)
{
    HRESULT     hr        = S_OK;
    HINSTANCE   hInstance = nullptr;



    DXUI_ASSERT_UI_THREAD();   // creates / shows a Dxui window


    if (m_printerPanel == nullptr || m_printerPanel->GetHwnd() == nullptr)
    {
        hInstance      = reinterpret_cast<HINSTANCE> (GetWindowLongPtr (m_shell.m_hwnd, GWLP_HINSTANCE));
        m_printerPanel = std::make_unique<PrinterPanel> ();

        // No owner window: the preview is a peer of the main window, not an
        // owned popup. An owned window is permanently z-locked above its owner
        // (always-on-top of Casso); a peer can be sent behind Casso normally.
        hr = m_printerPanel->Create (hInstance,
                                     nullptr,
                                     m_shell.m_hwnd,     // placement anchor only -- not an owner
                                     m_shell.m_renderer->GetD3D().GetDevice(),
                                     m_shell.m_renderer->GetD3D().GetContext(),
                                     &m_shell.m_chrome->GetTheme());
        CHRF (hr, m_printerPanel.reset());

        m_shell.m_window->ApplyAppIconToWindow (m_printerPanel->GetHwnd());

        // Toolbar actions route through the existing command path (which
        // quiesces the worker, delivers/clears, and resumes), then re-snapshot.
        // Print / Save / Copy are non-destructive: they deliver the strip and
        // leave the paper in the printer, so one printout can be printed AND
        // saved AND copied. Discard is the one tear-off.
        m_printerPanel->SetOnPrint ([this] ()
        {
            m_shell.m_windowCommandManager->HandleCommand (IDM_PRINTER_PRINT);
            SnapshotStripToPanel();
        });
        m_printerPanel->SetOnSaveAs ([this] ()
        {
            m_shell.m_windowCommandManager->HandleCommand (IDM_PRINTER_SAVEAS);
            SnapshotStripToPanel();
        });
        m_printerPanel->SetOnCopy ([this] ()
        {
            m_shell.m_windowCommandManager->HandleCommand (IDM_PRINTER_COPY);
            SnapshotStripToPanel();
        });
        m_printerPanel->SetOnDiscard ([this] ()
        {
            // The tear-off sound fires from the confirmed branch of the discard
            // handler (WindowCommandManager), NOT here -- so canceling the
            // confirmation dialog does not rip a page we are keeping.
            m_shell.m_windowCommandManager->HandleCommand (IDM_PRINTER_DISCARD);
            SnapshotStripToPanel();
        });
        m_printerPanel->SetOnFormFeed ([this] ()
        {
            int    rowsOnPage = 0;
            float  unused     = 0.0f;

            // The real ImageWriter's FORM FEED button, honored only while
            // the printer is idle -- pressing it mid-print would interleave
            // a page break into the guest's own stream, so it is ignored
            // (same idle signal that re-arms the auto-open logic).
            static constexpr int64_t   s_kFormFeedIdleMs = 1200;

            int64_t   nowMs = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                                  std::chrono::steady_clock::now().time_since_epoch()).count();

            if (nowMs - m_printerActiveLastMs < s_kFormFeedIdleMs)
            {
                return;   // an actual print is streaming: ignore the button
            }

            // Scale the form-feed sound by how much of the current page will
            // feed to the tear bar (less unused -> shorter feed -> shorter
            // grain). A page that just wrapped feeds a full sheet (unused ~1).
            rowsOnPage = m_printerWorker.GetRowsUsed() % PrinterGrid::kPageRows;
            unused = 1.0f - (float) rowsOnPage / (float) PrinterGrid::kPageRows;
            m_shell.m_audio->GetPrinterAudio().PlayFormFeed (unused);

            m_printerWorker.FormFeed();
        });

        // Dragging the preview's caption or edge enters the OS modal move/size
        // loop, which owns the UI thread and would otherwise freeze the print
        // mid-page (the carriage stops, the reveal stalls) until the drag ends.
        // Pump a full host frame per loop tick so the emulator keeps running
        // and the carriage keeps animating while the user repositions the
        // window -- the same keep-alive the main window uses for its caption.
        m_printerPanel->SetOnModalLoopTick ([this] ()
        {
            m_shell.m_renderer->TryPresentUiFrame();
        });
    }

    SnapshotStripToPanel();

    // activate=false (auto-open path) shows the preview without pulling focus
    // off the guest, so a print popping up the window never eats keystrokes.
    m_printerPanel->Show (activate);

    // Auto-open sits the preview JUST BELOW the main window in the z-order, not
    // on top: a print arriving while the user is working must never pop a window
    // over what they are looking at (or steal focus from under them). The user
    // clicks / Alt-Tabs it forward whenever they want to watch it.
    if (!activate && m_printerPanel->GetHwnd() != nullptr)
    {
        SetWindowPos (m_printerPanel->GetHwnd(), m_shell.m_hwnd, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellPrinter::UpdatePrinterStatus
//
//  Samples the worker's thread-safe status signals, recomputes the LED state
//  through the pure PrinterStatusModel, feeds the toolbar's printer button,
//  and marks a redraw only on a change so a static screen still repaints the
//  LED on a transition.
//
////////////////////////////////////////////////////////////////////////////////

void ShellPrinter::UpdatePrinterStatus()
{
    int64_t        nowMs  = 0;
    PrinterStatus  status = PrinterStatus::Idle;



    if (m_shell.m_machine.GetRefs().printerCard == nullptr)
    {
        m_printerLed.SetPresent (false);
        return;   // no card: the toolbar's printer button disables
    }

    m_printerLed.SetPresent (true);

    nowMs = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                std::chrono::steady_clock::now().time_since_epoch()).count();

    // A latched delivery error clears itself when the guest prints something
    // new -- red means "this page needs attention", and a fresh print means
    // the user has moved on (Error outranks Receiving in the model, so a
    // stale latch would otherwise mask the live print).
    if (m_printerDeliveryError &&
        m_printerWorker.GetActivityCount() != m_printerErrorActivity)
    {
        m_printerDeliveryError = false;
    }

    m_printerStatus.Update (m_printerWorker.GetActivityCount(),
                            (double) nowMs,
                            m_printerWorker.HasContent(),
                            m_printerDeliveryError);

    status = m_printerStatus.GetStatus();

    // The toolbar's printer button carries the status light (DCR-2); repaint
    // only when the LED state actually changes.
    if (status != m_printerStatusShown)
    {
        m_printerStatusShown = status;
        m_printerLed.SetStatus (status);
        m_shell.m_renderer->GetD3D().MarkRedrawNeeded();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellPrinter::UpdatePrinterPreview
//
//  Per-frame: auto-open the preview the moment the guest starts printing, then
//  refresh the strip live as bytes flow. The read is non-destructive (see
//  SnapshotStripToPanel), so watching a print in progress never perturbs it.
//
////////////////////////////////////////////////////////////////////////////////

void ShellPrinter::UpdatePrinterPreview()
{
    static constexpr int64_t   s_kAutoOpenIdleMs = 1200;   // activity gap that re-arms auto-open



    HRESULT    hr        = S_OK;
    uint64_t   activity  = 0;
    int64_t    nowMs     = 0;
    bool       previewUp = false;

    BAIL_OUT_IF (m_shell.m_machine.GetRefs().printerCard == nullptr, S_OK);   // machine has no printer card

    activity = m_printerWorker.GetActivityCount();
    nowMs    = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                   std::chrono::steady_clock::now().time_since_epoch()).count();

    // Auto-open on a NEW print session: activity advancing after an idle gap.
    // Arm while idle, fire once (without stealing focus) when bytes resume, then
    // stay disarmed until idle again. This opens for a fresh print even when a
    // prior pending strip is still loaded (no HasContent edge to ride), yet
    // closing the window mid-print never fights a re-open -- activity keeps
    // advancing through the print, so the edge does not re-arm until it finishes.
    if (activity != m_printerAutoOpenActivity)
    {
        if (m_printerAutoOpenArmed)
        {
            ShowPrinterPanel (false /* activate */);
            m_printerAutoOpenArmed = false;
        }

        m_printerAutoOpenActivity = activity;
        m_printerActiveLastMs     = nowMs;
    }
    else if (!m_printerAutoOpenArmed && nowMs - m_printerActiveLastMs > s_kAutoOpenIdleMs)
    {
        m_printerAutoOpenArmed = true;   // print settled: re-arm for the next one
    }

    // Live refresh while the preview is genuinely visible. The panel's viewport
    // does its own change detection and renders at most the visible ~1-page span
    // (FR-033), so this per-frame call is flat-cost regardless of strip length.
    // A hidden panel bails: rendering off-screen buys nothing.
    previewUp = m_printerPanel != nullptr
                && m_printerPanel->IsOpen()
                && IsWindowVisible (m_printerPanel->GetHwnd());

    BAIL_OUT_IF (!previewUp, S_OK);

    m_printerPanel->RefreshLive (m_printerWorker, nowMs);

    // Feed the paced carriage position to the printer audio so the mechanical
    // sound tracks the on-screen head (Option A), sharing the exact reveal the
    // panel just advanced. The audio thread gates its carriage loop + fires the
    // line-feed clacks off this; a closed / hidden preview stops publishing, so
    // the loop naturally goes quiet.
    {
        int64_t  progressDots   = 0;
        int      colDots        = 0;
        bool     inkActive      = false;
        int      sweepWidthDots = PrinterGrid::kDotsPerRow;
        m_printerPanel->GetPacedReveal (progressDots, colDots, inkActive, sweepWidthDots);
        m_shell.m_audio->GetPrinterAudio().PublishReveal (progressDots, colDots, inkActive, sweepWidthDots);
    }

    // Printer-sound volume + mute (Settings > Printing audio, FR-034). Read from
    // prefs each frame so an OK / Cancel in Settings binds on the next update
    // without any live-apply plumbing; the shared "Drive audio" master still
    // gates the whole bus above this.
    m_shell.m_audio->GetPrinterAudio().SetVolume (m_shell.m_settings->GetPrefs().printerAudioVolume);
    m_shell.m_audio->GetPrinterAudio().SetMuted  (!m_shell.m_settings->GetPrefs().printerAudioEnabled);

    // Position the printer sound in the stereo field. Manual override (Settings >
    // Printing) pins a fixed pan; otherwise it auto-follows where the preview
    // window sits relative to the main Casso window -- center-to-center X offset,
    // normalized so the two windows just touching side by side is a hard pan and
    // a fully overlapping (co-centered) window is dead center.
    {
        float  pan  = 0.0f;
        float  panL = 0.0f;
        float  panR = 0.0f;

        if (m_shell.m_settings->GetPrefs().printerAudioPanOverride)
        {
            pan = std::clamp (m_shell.m_settings->GetPrefs().printerAudioPan, -1.0f, 1.0f);
        }
        else
        {
            RECT  mainR    = {};
            RECT  printerR = {};

            if (GetWindowRect (m_shell.m_hwnd, &mainR) &&
                GetWindowRect (m_printerPanel->GetHwnd(), &printerR))
            {
                float  mainCenter    = (float) (mainR.left    + mainR.right)    * 0.5f;
                float  printerCenter = (float) (printerR.left + printerR.right) * 0.5f;
                float  mainHalf      = (float) (mainR.right    - mainR.left)    * 0.5f;
                float  printerHalf   = (float) (printerR.right - printerR.left) * 0.5f;
                float  reference     = mainHalf + printerHalf;   // touching side by side

                if (reference > 1.0f)
                {
                    pan = std::clamp ((printerCenter - mainCenter) / reference, -1.0f, 1.0f);
                }
            }
        }

        DriveAudioMixer::PanToStereo (pan, panL, panR);
        m_shell.m_audio->GetPrinterAudio().SetPan (panL, panR);
    }

    // Hold a smooth present cadence while the carriage is sweeping or a pan/zoom
    // is easing. Without this the loop drops to Sleep(1) whenever the emulator
    // framebuffer is static (a guest that prints without touching the screen),
    // and that coarse, jittery tick makes the head step across the platen.
    if (m_printerPanel->NeedsAnimationFrame())
    {
        m_shell.m_renderer->GetD3D().MarkRedrawNeeded();
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetPrinterBannerMessage
//
//  One-line printer summary for the Settings > Printing info banner. The
//  machine-can-print fact comes from the config's enabled slots (core, tested);
//  the wording is host UI copy. //c is slotless, so it reads as no printer.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ShellPrinter::GetPrinterBannerMessage() const
{
    std::wstring  message;



    if (m_shell.m_machine.GetConfig().HasEnabledSlotDevice ("parallel-printer"))
    {
        message = L"Emulating an Apple ImageWriter II connected via parallel interface.";
    }
    else
    {
        message = L"No printer is connected to this " + fs::path (m_shell.m_machine.GetConfig().name).wstring() + L".";
    }

    return message;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellPrinter::ClosePrinterPanel
//
////////////////////////////////////////////////////////////////////////////////

void ShellPrinter::ClosePrinterPanel()
{
    m_printerPanel.reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellPrinter::RenderPanelFrame
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ShellPrinter::RenderPanelFrame()
{
    HRESULT  hr = S_OK;



    if (m_printerPanel != nullptr)
    {
        hr = m_printerPanel->RenderFrame();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellPrinter::StopAndSavePendingStrip
//
//  Joins the printer drain thread before the card it reads is freed -- its job
//  holds a reference into the card's ring -- then persists the pending strip
//  for the outgoing machine (FR-026). An empty strip clears any stale sidecar.
//  The machine host still holds the outgoing machine's name here.
//
////////////////////////////////////////////////////////////////////////////////

void ShellPrinter::StopAndSavePendingStrip()
{
    m_printerWorker.Stop();

    if (!m_shell.m_machine.GetCurrentMachineName().empty())
    {
        PrinterJob *   printJob = m_printerWorker.GetJob();

        if (printJob != nullptr && printJob->HasContent())
        {
            HRESULT   hrSave = PrintJobStore::Save (m_shell.GetPendingPrintDir(), printJob->GetRaster());
            IGNORE_RETURN_VALUE (hrSave, S_OK);
        }
        else
        {
            PrintJobStore::Clear (m_shell.GetPendingPrintDir());
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellPrinter::NotePrinterDeliveryResult
//
////////////////////////////////////////////////////////////////////////////////

void ShellPrinter::NotePrinterDeliveryResult (bool failed)
{
    m_printerDeliveryError = failed;
    m_printerErrorActivity = m_printerWorker.GetActivityCount();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellPrinter::SnapshotStripToPanel
//
//  Force-refreshes the panel from the drain worker WITHOUT stopping it: the
//  panel snapshots only its visible viewport span under the worker's raster
//  lock while the same interpreter keeps running. Fully non-destructive --
//  previewing (or refreshing) mid-print can never reset the guest's in-flight
//  state, so it cannot distort the output. (The original path stopped and
//  re-Start()ed the worker, which rebuilt the interpreter and reset its line
//  feed from Print Shop's ESC T back to the default, stretching everything
//  printed after a mid-print preview.)
//
////////////////////////////////////////////////////////////////////////////////

void ShellPrinter::SnapshotStripToPanel()
{
    int64_t   nowMs      = 0;
    bool      panelIsUp  = m_printerPanel != nullptr && m_printerPanel->IsOpen();
    bool      hasCard    = m_shell.m_machine.GetRefs().printerCard != nullptr;



    if (panelIsUp && !hasCard)
    {
        PrintRaster   empty;

        m_printerPanel->SetStrip (empty);   // blank sheet
    }
    else if (panelIsUp)
    {
        nowMs = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                    std::chrono::steady_clock::now().time_since_epoch()).count();

        // Forced refresh through the panel's viewport: snapshots and renders
        // only the visible ~1-page span (never the whole strip), same as the
        // live path.
        m_printerPanel->RefreshLive (m_printerWorker, nowMs, true /* force */);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellPrinter::GetPrinterDialogOwner
//
//  Owner HWND for the printer's confirmation / notice boxes. When the preview
//  panel is open the user is acting inside it (its Finish / Copy / Discard
//  buttons, or a menu command while watching it), so own the box by the panel
//  -- the modal box then centers on the panel and disables it while up. With
//  the panel closed the command came from the main menu, so own it by the main
//  window.
//
////////////////////////////////////////////////////////////////////////////////

HWND ShellPrinter::GetPrinterDialogOwner() const
{
    HWND  owner       = m_shell.m_hwnd;
    bool  panelIsUp   = m_printerPanel != nullptr
                        && m_printerPanel->IsOpen()
                        && m_printerPanel->GetHwnd() != nullptr
                        && IsWindowVisible (m_printerPanel->GetHwnd());



    if (panelIsUp)
    {
        owner = m_printerPanel->GetHwnd();
    }

    return owner;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintDpiFromPrefs
//
//  Settings > Printing knobs, read live from GlobalUserPrefs at each eject.
//
////////////////////////////////////////////////////////////////////////////////

static int PrintDpiFromPrefs (const GlobalUserPrefs & p)
{
    return (p.printOutputDpi == 288) ? 288 : 576;   // only 288 / 576 valid (FR-028)
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintDotStyleFromPrefs
//
////////////////////////////////////////////////////////////////////////////////

static DotStyle PrintDotStyleFromPrefs (const GlobalUserPrefs & p)
{
    return (p.printDotStyle == "plain") ? DotStyle::Plain : DotStyle::Ink;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WholeStripDpi
//
//  Cap the render dpi for a WHOLE-strip render (PNG file, clipboard) so a long
//  fanfold banner's single RGBA image stays within a memory budget instead of
//  ballooning to gigabytes (each row at 576 dpi is 4608 px * 4 B; a 60-page
//  banner is ~95k rows). The native grid is only 160x144 dpi, so dropping a huge
//  banner from 576 toward ~150 dpi is still well above source resolution -- no
//  meaningful quality loss, and it never OOMs. Short jobs keep the full dpi.
//
////////////////////////////////////////////////////////////////////////////////

static int WholeStripDpi (const GlobalUserPrefs & prefs, int rows)
{
    const double   kBudgetPx = 128.0 * 1024.0 * 1024.0;   // ~512 MB of RGBA
    int            dpi       = PrintDpiFromPrefs (prefs);



    if (rows > 0)
    {
        // outPx = (kDotsPerRow/160 * dpi) * (rows/144 * dpi)
        //       = kDotsPerRow * rows * dpi^2 / (160 * 144)
        double   maxDpi = std::sqrt (kBudgetPx * 160.0 * 144.0
                                     / ((double) PrinterGrid::kDotsPerRow * (double) rows));

        if ((int) maxDpi < dpi)
        {
            dpi = (std::max) (120, (int) maxDpi);
        }
    }

    return dpi;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HrFromSpoolResult
//
//  StartPage / EndPage / EndDoc report failure as a return value <= 0 using
//  the SP_* family, and frequently do NOT set GetLastError -- which is how a
//  print failure used to degrade to "Unspecified error" (or worse: CWRF's
//  HRESULT_FROM_WIN32(0) == S_OK, a silent false success). Map the result to
//  an honest HRESULT: a user abort (the Print-to-PDF Save-As cancel can
//  surface mid-job on modern Windows, not just at StartDoc) becomes
//  HRESULT_FROM_WIN32 (ERROR_CANCELLED), which names itself at the call
//  site; disk-full / out-of-memory get their real codes; anything else uses
//  GetLastError when present and only degrades to E_FAIL when the driver
//  reported nothing at all.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ShellPrinter::HrFromSpoolResult (int ret, const wchar_t * call, int pageIx)
{
    HRESULT   hr  = S_OK;
    DWORD     gle = ::GetLastError();   // capture before logging can clobber it



    if (ret <= 0)
    {
        if      (ret == SP_USERABORT)                                    { hr = HRESULT_FROM_WIN32 (ERROR_CANCELLED); }
        else if (ret == SP_APPABORT)                                     { hr = E_ABORT;                              }
        else if (ret == SP_OUTOFDISK)                                    { hr = HRESULT_FROM_WIN32 (ERROR_DISK_FULL); }
        else if (ret == SP_OUTOFMEMORY)                                  { hr = E_OUTOFMEMORY;                        }
        else if (gle == ERROR_CANCELLED || gle == ERROR_PRINT_CANCELLED) { hr = HRESULT_FROM_WIN32 (ERROR_CANCELLED); }
        else if (gle != 0)                                               { hr = HRESULT_FROM_WIN32 (gle);             }
        else                                                             { hr = E_FAIL;                               }
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrimeDefaultPrinterDriver
//
//  Load ("prime") the default printer's driver on a short-lived no-COM (MTA)
//  thread. v4 / XPS print drivers -- notably Microsoft Print to PDF -- create
//  their device context through cross-process COM that aborts with 995
//  (ERROR_OPERATION_ABORTED) when first touched from our STA UI thread at
//  Medium integrity, yet succeeds from an MTA thread. Doing one MTA CreateDC
//  loads the driver so a subsequent PrintDlg on the UI thread creates its DC
//  normally. PD_RETURNDEFAULT gives us the default printer with no UI and no
//  DC of its own. Best-effort; any failure is ignored (the real path still
//  has its own fallback).
//
////////////////////////////////////////////////////////////////////////////////

void  ShellPrinter::PrimeDefaultPrinterDriver()
{
    PRINTDLGW   def = {};



    def.lStructSize = sizeof (def);
    def.Flags       = PD_RETURNDEFAULT | PD_NOPAGENUMS | PD_NOSELECTION;

    if (PrintDlgW (&def) && def.hDevNames != nullptr)
    {
        std::wstring       driver;
        std::wstring       device;
        const DEVNAMES *   dnp = (const DEVNAMES *) GlobalLock (def.hDevNames);

        if (dnp != nullptr)
        {
            const wchar_t *  b = (const wchar_t *) dnp;
            driver = b + dnp->wDriverOffset;
            device = b + dnp->wDeviceOffset;
            GlobalUnlock (def.hDevNames);
        }

        std::thread ([driver, device] ()
        {
            ThreadName::Set (L"Casso printer device");

            HDC  h = CreateDCW (driver.c_str(), device.c_str(), nullptr, nullptr);
            if (h != nullptr) { DeleteDC (h); }
        }).join();
    }

    if (def.hDevMode  != nullptr) { GlobalFree (def.hDevMode); }
    if (def.hDevNames != nullptr) { GlobalFree (def.hDevNames); }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateDcFromDevNames
//
//  Build a printer DC from the DEVNAMES + DEVMODE the print dialog returned,
//  when PrintDlg's own PD_RETURNDC came back null (a v4 driver flaking on the
//  STA UI thread). Created on a plain (no-COM / MTA) thread for the same
//  reason priming works -- an STA CreateDC aborts (995) where an MTA one
//  succeeds. NULL port: the Print-to-PDF file prompt belongs at StartDoc, not
//  DC creation. Returns null only if the names are missing or CreateDC fails.
//
////////////////////////////////////////////////////////////////////////////////

HDC  ShellPrinter::CreateDcFromDevNames (const PRINTDLGW & pd)
{
    std::wstring                 driver;
    std::wstring                 device;
    std::vector<unsigned char>   devmode;
    HDC                          hdc = nullptr;



    // The driver / device names are mandatory: without them there is nothing
    // to hand CreateDC. Note the GlobalUnlock is paired inside the same
    // branch that locked -- a bare return between the two would leak the lock.
    if (pd.hDevNames != nullptr)
    {
        const DEVNAMES *  dn = (const DEVNAMES *) GlobalLock (pd.hDevNames);

        if (dn != nullptr)
        {
            const wchar_t *  base = (const wchar_t *) dn;

            driver = base + dn->wDriverOffset;
            device = base + dn->wDeviceOffset;
            GlobalUnlock (pd.hDevNames);
        }
    }

    // Copy the (variable-length) DEVMODE so the worker thread owns stable
    // memory independent of the caller's global handle lock.
    if (!device.empty() && pd.hDevMode != nullptr)
    {
        const DEVMODEW *  dm = (const DEVMODEW *) GlobalLock (pd.hDevMode);

        if (dm != nullptr)
        {
            const unsigned char *  p  = (const unsigned char *) dm;
            size_t                 sz = (size_t) dm->dmSize + dm->dmDriverExtra;

            devmode.assign (p, p + sz);
            GlobalUnlock (pd.hDevMode);
        }
    }

    // A null DEVMODE is fine (driver defaults); an empty device name is not.
    if (!device.empty())
    {
        std::thread ([&] ()
        {
            const DEVMODEW *  dmp = devmode.empty() ? nullptr : (const DEVMODEW *) devmode.data();



            ThreadName::Set (L"Casso printer device");

            hdc = CreateDCW (driver.c_str(), device.c_str(), nullptr, dmp);
        }).join();
    }

    return hdc;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlitRgbaToDc
//
//  Blit an R,G,B,A image onto a printer HDC. The strip is scaled to fit the
//  page WIDTH (uniform scale, aspect preserved) and top-aligned so the
//  fanfold continues downward across page breaks. Fitting to width -- not
//  min(width,height) -- is deliberate: the strip width is identical on every
//  page, so a width fit gives every page the same horizontal scale and left
//  edge. A min() fit would height-limit full pages but width-limit the short
//  last page, scaling their columns differently and misaligning page-to-page.
//  GDI DIBs are BGRA, so the channels are swapped into a scratch buffer.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ShellPrinter::BlitRgbaToDc (HDC hdc, const RgbaImage & img, int pageW, int pageH, int outputDpi)
{
    HRESULT                    hr    = S_OK;
    vector<Byte>               bgra;
    BITMAPINFO                 bmi   = {};
    size_t                     count = 0;
    size_t                     i     = 0;
    PrintPagination::PageFit   fit;
    int                        destW = 0;
    int                        destH = 0;
    int                        blit  = 0;



    CBR (img.width > 0 && img.height > 0);
    CBR (pageW > 0 && pageH > 0);

    count = (size_t) img.width * img.height;
    bgra.resize (count * 4);
    for (i = 0; i < count; i++)
    {
        bgra[i * 4 + 0] = img.rgba[i * 4 + 2];   // B
        bgra[i * 4 + 1] = img.rgba[i * 4 + 1];   // G
        bgra[i * 4 + 2] = img.rgba[i * 4 + 0];   // R
        bgra[i * 4 + 3] = img.rgba[i * 4 + 3];   // A
    }

    // Fit a FULL page to the printable height, capped by width, at one uniform
    // scale (device pixels, so the output dpi is the box's vertical unit). The
    // shared fanfold fit, so classic print, modern print and preview all agree.
    fit   = PrintPagination::FitFullPageToBox ((double) img.width, (double) img.height,
                                               (double) pageW, (double) pageH, (double) outputDpi);
    destW = (std::max) (1, (int) (img.width  * fit.scale));
    destH = (std::max) (1, (int) (img.height * fit.scale));

    bmi.bmiHeader.biSize        = sizeof (BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = img.width;
    bmi.bmiHeader.biHeight      = -img.height;   // negative == top-down
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    SetStretchBltMode (hdc, HALFTONE);
    SetBrushOrgEx     (hdc, 0, 0, nullptr);

    // Failure is GDI_ERROR (-1 as an int) *or* zero scan lines copied (the
    // destination always spans >= 1 line, so a genuine success copies at
    // least one) -- blit <= 0 covers both. GDI often reports these with
    // GetLastError()==0; CWRF would turn that into HRESULT_FROM_WIN32(0)
    // == S_OK -- a silent false success -- so fall back to E_FAIL
    // explicitly when there is no error code to keep.
    blit = StretchDIBits (hdc,
                          (pageW - destW) / 2, 0, destW, destH,
                          0, 0, img.width, img.height,
                          bgra.data(), &bmi, DIB_RGB_COLORS, SRCCOPY);
    if (blit <= 0)
    {
        DWORD   gle = ::GetLastError();

        hr = (gle != 0) ? HRESULT_FROM_WIN32 (gle) : E_FAIL;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SavePrintoutAs
//
//  The user picks the destination through IFileSaveDialog (seeded with the
//  default folder <Pictures>\Casso Prints and a timestamped name), and the
//  strip renders to that exact path at the configured dpi + dot style.
//  Cancellation is reported through outOutcome, not the result code.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ShellPrinter::SavePrintoutAs (const PrintRaster & raster, fs::path & outFile, PrintOutcome & outOutcome)
{
    HRESULT                   hr          = S_OK;
    PWSTR                     picturesRaw = nullptr;
    FileDialogSpec            spec;
    fs::path                  folder;
    fs::path                  suggested;
    vector<Byte>              png;
    SYSTEMTIME                now         = {};
    bool                      isOpen      = false;
    bool                      wroteWell   = false;
    bool                      picked      = false;
    HRESULT                   hrPictures  = S_OK;
    std::error_code           ec;
    const GlobalUserPrefs &   prefs       = m_shell.m_settings->GetPrefs();



    outOutcome = PrintOutcome::Delivered;

    // Seed the default folder <Pictures>\Casso Prints + a timestamped name.
    hrPictures = SHGetKnownFolderPath (FOLDERID_Pictures, 0, nullptr, &picturesRaw);
    if (SUCCEEDED (hrPictures))
    {
        folder = fs::path (picturesRaw) / L"Casso Prints";
        fs::create_directories (folder, ec);
    }

    GetLocalTime (&now);
    suggested = PrintFileNaming::ComposeTimestampedPath (folder, L"Casso Print", L".png", now,
                    [] (const fs::path & p) { std::error_code e; return fs::exists (p, e); });

    spec.filters          = { { L"PNG image", L"*.png" } };
    spec.defaultExtension = L"png";
    spec.defaultFileName  = suggested.filename().wstring();
    spec.initialFolder    = folder;

    hr = m_shell.GetHostDialogs().PickFileToSave (GetPrinterDialogOwner(), spec, outFile, picked);
    CHR (hr);

    // A user cancel is not a delivery failure.
    if (!picked)
    {
        outOutcome = PrintOutcome::Canceled;
    }

    BAIL_OUT_IF (!picked, S_OK);

    hr = PrintDelivery::RenderToPng (raster, 0, raster.GetRowsUsed() - 1,
                                     WholeStripDpi (prefs, raster.GetRowsUsed()),
                                     PrintDotStyleFromPrefs (prefs), png);
    CHR (hr);

    {
        std::ofstream   out (outFile, std::ios::binary | std::ios::trunc);

        isOpen = out.is_open();
        CBR (isOpen);

        out.write ((const char *) png.data(), (std::streamsize) png.size());
        wroteWell = out.good();
        CBR (wroteWell);
    }

Error:
    if (picturesRaw != nullptr)
    {
        CoTaskMemFree (picturesRaw);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintToWindowsPrinter
//
//  Delivers the strip to a Windows printer through the standard print dialog.
//  The strip is paginated (PrintPagination -- core, unit-tested) and each
//  page's row span is rendered (PaperRenderer -- core) and StretchDIBits'd onto
//  the printer DC. Only the dialog + GDI job are here (the untestable Win32
//  edge). Cancellation -- of the dialog, the Save-As
//  prompt inside StartDoc, or (modern Print-to-PDF) an abort surfacing at a
//  later spool call. On failure `failedStage` names the call that failed so
//  the error dialog's Details line pinpoints it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ShellPrinter::PrintToWindowsPrinter (const PrintRaster & raster, std::wstring & failedStage, PrintOutcome & outOutcome)
{
    HRESULT                               hr       = S_OK;
    const GlobalUserPrefs               & prefs    = m_shell.m_settings->GetPrefs();
    vector<PrintPagination::PageRange>    pages    = PrintPagination::Paginate (raster);
    PRINTDLGW                             pd       = {};
    DOCINFOW                              di       = {};
    bool                                  started  = false;
    BOOL                                  dlgOk    = FALSE;
    int                                   pageW    = 0;
    int                                   pageH    = 0;
    int                                   pageIx   = 0;
    bool                                  hasPages = false;



    outOutcome = PrintOutcome::Delivered;

    hasPages = !pages.empty();
    CBRF (hasPages,
          failedStage = L"pagination (the page has no printable content)");

    // Microsoft Print to PDF (and other v4 / XPS print drivers) create their
    // device context through cross-process COM that aborts with 995
    // (ERROR_OPERATION_ABORTED) the first time it is touched from our STA UI
    // thread (OleInitialize) at Medium integrity, but succeeds from a plain MTA
    // thread. Loading the driver once on a short-lived no-COM thread lets the
    // PrintDlg below then create its DC on the UI thread normally. Diagnosed
    // live: the STA CreateDC fails 995, an MTA CreateDC succeeds, and the real
    // PrintDlg then returns a valid DC and the job completes. Best-effort.
    PrimeDefaultPrinterDriver();

    pd.lStructSize = sizeof (pd);
    pd.hwndOwner   = m_shell.m_hwnd;
    pd.Flags       = PD_RETURNDC | PD_NOPAGENUMS | PD_NOSELECTION | PD_USEDEVMODECOPIESANDCOLLATE;
    pd.nCopies     = 1;

    // A false return is overwhelmingly the user closing the dialog
    // (CommDlgExtendedError() == 0); the Error label turns ERROR_CANCELLED
    // into outOutcome = Canceled with a success code.
    dlgOk = PrintDlgW (&pd);
    BAIL_OUT_IF (!dlgOk, HRESULT_FROM_WIN32 (ERROR_CANCELLED));

    // PD_RETURNDC should have created the DC. When it flakes (TRUE + null hDC +
    // no error -- seen intermittently with Print-to-PDF in a long-lived
    // process), build the DC ourselves from the dialog's DEVNAMES/DEVMODE rather
    // than surfacing a spurious failure. We own this DC exactly as we would the
    // PrintDlg-created one (the Error cleanup DeleteDC's pd.hDC either way).
    if (pd.hDC == nullptr)
    {
        pd.hDC = CreateDcFromDevNames (pd);
    }

    CBRF (pd.hDC != nullptr,
            failedStage = L"PrintDlg (the chosen printer returned no device context)");

    di.cbSize      = sizeof (di);
    di.lpszDocName = L"Casso printout";

    // "Microsoft Print to PDF" (and some drivers) pop a Save-As prompt inside
    // StartDoc; canceling it is a user cancel, not a delivery failure -- so it
    // reports exactly like a canceled print dialog (outOutcome = Canceled: keep the page, no
    // scary "could not deliver" popup). HrFromSpoolResult owns that mapping,
    // including the SP_* codes and the GetLastError()==0 case.
    hr = HrFromSpoolResult (StartDocW (pd.hDC, &di), L"StartDoc", 0);
    CHRF (hr, failedStage = L"StartDoc (starting the print job)");

    started = true;

    pageW = GetDeviceCaps (pd.hDC, HORZRES);
    pageH = GetDeviceCaps (pd.hDC, VERTRES);

    for (const PrintPagination::PageRange & pr : pages)
    {
        PaperRenderer            renderer;
        PaperRenderer::Options   opt;
        RgbaImage                img;

        opt.outputDpi = PrintDpiFromPrefs (prefs);
        opt.style     = PrintDotStyleFromPrefs (prefs);

        hr = renderer.Render (raster, pr.firstRow, pr.lastRow, opt, img);
        CHRF (hr, failedStage = std::format (L"rendering page {}", pageIx + 1));

        // Every spool call routes through HrFromSpoolResult: SP_* return codes
        // and GetLastError map to honest HRESULTs, and a mid-job user abort
        // (Print-to-PDF Save-As cancel on modern Windows) surfaces as ERROR_CANCELLED --
        // exit quietly, keep the page, no failure dialog.
        hr = HrFromSpoolResult (StartPage (pd.hDC), L"StartPage", pageIx);
        CHRF (hr, failedStage = std::format (L"StartPage (page {})", pageIx + 1));

        hr = BlitRgbaToDc (pd.hDC, img, pageW, pageH, opt.outputDpi);
        CHRF (hr, failedStage = std::format (L"drawing page {} onto the printer", pageIx + 1));

        hr = HrFromSpoolResult (EndPage (pd.hDC), L"EndPage", pageIx);
        CHRF (hr, failedStage = std::format (L"EndPage (page {})", pageIx + 1));

        pageIx++;
    }

    hr = HrFromSpoolResult (EndDoc (pd.hDC), L"EndDoc", pageIx);
    CHRF (hr, failedStage = L"EndDoc (finishing the print job)");

    started = false;

Error:
    // A user cancel anywhere in the job -- the print dialog up front, or the
    // Print-to-PDF Save-As prompt that surfaces mid-job -- is not a delivery
    // failure. Every stage reports it the same way, so the mapping lives here
    // once rather than being repeated at each spool call. failedStage is
    // cleared because there is no failing stage to name.
    if (hr == HRESULT_FROM_WIN32 (ERROR_CANCELLED))
    {
        outOutcome = PrintOutcome::Canceled;
        failedStage.clear();
        hr         = S_OK;
    }

    if (started && pd.hDC != nullptr)
    {
        AbortDoc (pd.hDC);
    }

    if (pd.hDC != nullptr)
    {
        DeleteDC (pd.hDC);
    }

    if (pd.hDevMode != nullptr)
    {
        GlobalFree (pd.hDevMode);
    }

    if (pd.hDevNames != nullptr)
    {
        GlobalFree (pd.hDevNames);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CopyPrintoutToClipboard
//
//  Copies the finished strip to the clipboard as one continuous image (the
//  fanfold metaphor -- no pagination). Offers CF_DIB (bottom-up BGRA) for
//  classic paste targets and a registered "PNG" blob for apps that prefer
//  lossless. Render/encode are core (unit-tested); only the DIB packing and
//  Win32 clipboard calls live here. Does not consume the strip.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ShellPrinter::CopyPrintoutToClipboard (const PrintRaster & raster)
{
    HRESULT                  hr       = S_OK;
    const GlobalUserPrefs &  prefs    = m_shell.m_settings->GetPrefs();
    PaperRenderer            renderer;
    PaperRenderer::Options   opt;
    RgbaImage                img;
    vector<Byte>             png;
    HGLOBAL                  hDib     = nullptr;
    HGLOBAL                  hPng     = nullptr;
    bool                     opened   = false;
    bool                     didOpen  = false;
    bool                     didEmpty = false;
    HRESULT                  hrEncode = S_OK;
    size_t                   px       = 0;
    size_t                   dibBytes = 0;
    // A 32bpp DIB of the whole strip must stay bounded so a huge multi-page
    // banner cannot try to place gigabytes on the clipboard. Above the cap we
    // skip the bitmap and rely on the (compressed) PNG blob instead.
    const size_t             kMaxDibBytes = (size_t) 256 * 1024 * 1024;



    // Render the whole strip exactly once, capping dpi for very tall banners so
    // neither the DIB below nor the PNG blob materializes gigabytes. The source
    // is only 160x144 dpi, so the cap is effectively lossless.
    opt.outputDpi = WholeStripDpi (prefs, raster.GetRowsUsed());
    opt.style     = PrintDotStyleFromPrefs (prefs);

    hr = renderer.Render (raster, 0, raster.GetRowsUsed() - 1, opt, img);
    CHR (hr);
    CBR (img.width > 0 && img.height > 0);

    px       = (size_t) img.width * img.height;
    dibBytes = sizeof (BITMAPINFOHEADER) + px * 4;

    if (dibBytes <= kMaxDibBytes)
    {
        Byte *             dest = nullptr;
        BITMAPINFOHEADER   bih  = {};

        hDib = GlobalAlloc (GMEM_MOVEABLE, dibBytes);
        CPR (hDib);

        dest = (Byte *) GlobalLock (hDib);
        CPR (dest);

        bih.biSize        = sizeof (bih);
        bih.biWidth       = img.width;
        bih.biHeight      = img.height;   // positive == bottom-up rows
        bih.biPlanes      = 1;
        bih.biBitCount    = 32;
        bih.biCompression = BI_RGB;
        bih.biSizeImage   = (DWORD) (px * 4);

        memcpy (dest, &bih, sizeof (bih));
        dest += sizeof (bih);

        // Bottom-up DIB: emit rows last-to-first, swapping RGBA -> BGRA.
        for (int y = img.height - 1; y >= 0; y--)
        {
            const Byte * src = &img.rgba[(size_t) y * img.width * 4];

            for (int x = 0; x < img.width; x++)
            {
                dest[x * 4 + 0] = src[x * 4 + 2];   // B
                dest[x * 4 + 1] = src[x * 4 + 1];   // G
                dest[x * 4 + 2] = src[x * 4 + 0];   // R
                dest[x * 4 + 3] = src[x * 4 + 3];   // A
            }

            dest += (size_t) img.width * 4;
        }

        GlobalUnlock (hDib);
    }

    // Encode the PNG from the image we already rendered rather than rendering
    // the strip a second time (the old path doubled peak memory on big banners).
    hrEncode = PngCodec::EncodeRgba (img, opt.outputDpi, png);

    if (SUCCEEDED (hrEncode) && !png.empty())
    {
        Byte *   dest = nullptr;

        hPng = GlobalAlloc (GMEM_MOVEABLE, png.size());

        if (hPng != nullptr)
        {
            dest = (Byte *) GlobalLock (hPng);

            if (dest != nullptr)
            {
                memcpy (dest, png.data(), png.size());
                GlobalUnlock (hPng);
            }
            else
            {
                GlobalFree (hPng);
                hPng = nullptr;
            }
        }
    }

    CBR (hDib != nullptr || hPng != nullptr);

    didOpen = OpenClipboard (m_shell.m_hwnd);
    CBR (didOpen);

    opened = true;

    didEmpty = EmptyClipboard();
    CBR (didEmpty);

    // On success the clipboard takes ownership, so null the handle to keep the
    // cleanup path from freeing it out from under the clipboard.
    if (hDib != nullptr && SetClipboardData (CF_DIB, hDib) != nullptr)
    {
        hDib = nullptr;
    }

    if (hPng != nullptr)
    {
        UINT   fmt = RegisterClipboardFormatW (L"PNG");

        if (fmt != 0 && SetClipboardData (fmt, hPng) != nullptr)
        {
            hPng = nullptr;
        }
    }

Error:
    if (opened)          { CloseClipboard(); }
    if (hDib != nullptr) { GlobalFree (hDib); }
    if (hPng != nullptr) { GlobalFree (hPng); }
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnPrinterCommand
//
//  Handle the strip-level printer commands. Print / Save / Copy are all
//  NON-DESTRUCTIVE: they deliver the current strip and leave the paper in
//  the printer, so one printout can be printed AND saved AND copied. Discard
//  is the one explicit tear-off (confirmed, clears the persisted pending
//  copy, FR-029). All drain the ring first so they act on the complete
//  strip, reading the raster from a quiesced worker. Print always targets a
//  Windows printer; Save always writes a PNG through the file dialog -- the
//  destination is chosen by which command runs, not a stored preference.
//
////////////////////////////////////////////////////////////////////////////////

void ShellPrinter::OnPrinterCommand (int id)
{
    PrinterJob *   job   = nullptr;
    bool           known = (id == IDM_PRINTER_DISCARD || id == IDM_PRINTER_COPY ||
                            id == IDM_PRINTER_PRINT   || id == IDM_PRINTER_SAVEAS)
                           && m_shell.m_machine.GetRefs().printerCard != nullptr;



    if (known)
    {
        // Take ownership of the strip: stop the worker, then flush any tail
        // bytes. Every strip-level command acts on the whole page, so we drain
        // first and read the job's raster from a quiesced worker (no
        // concurrent mutation). Every arm below is responsible for restarting
        // the worker -- that is why they take the job rather than re-reading it.
        GetWorker().Stop();

        {
            vector<PrinterEvent>   events;
            GetWorker().FlushNow (events);
        }

        job = GetWorker().GetJob();

        // "No page" also covers a strip whose drained bytes left nothing on the
        // paper (no ink AND no feed -- e.g. a bare escape preamble): HasContent
        // is true but GetRowsUsed is 0, which would otherwise reach delivery,
        // paginate to zero pages, and surface as a scary "something went wrong".
        if (job == nullptr || !job->HasContent() || job->GetRaster().GetRowsUsed() <= 0)
        {
            OnPrinterNoPage (id, job);
        }
        else if (id == IDM_PRINTER_COPY)
        {
            OnPrinterCopy (job);
        }
        else if (id == IDM_PRINTER_DISCARD)
        {
            OnPrinterDiscard (job);
        }
        else
        {
            OnPrinterDeliver (job, id == IDM_PRINTER_PRINT);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnPrinterNoPage
//
//  Nothing on the paper: say so in the command's own words and resume. A
//  null job means the worker never had one, so it restarts empty.
//
////////////////////////////////////////////////////////////////////////////////

void ShellPrinter::OnPrinterNoPage (int id, PrinterJob * job)
{
    const wchar_t * emptyMsg =
        (id == IDM_PRINTER_COPY)    ? L"The printer has no page to copy yet."
      : (id == IDM_PRINTER_DISCARD) ? L"The printer has no page to discard."
      : (id == IDM_PRINTER_PRINT)   ? L"The printer has no page to print yet."
                                    : L"The printer has no page to save yet.";



    DxuiMessageBox (GetPrinterDialogOwner(), &m_shell.m_chrome->GetTheme(), emptyMsg, L"Casso printer", MB_OK | MB_ICONINFORMATION);

    if (job != nullptr)
    {
        GetWorker().Start (m_shell.m_machine.GetRefs().printerCard->GetByteRing(), job->GetRaster());
    }
    else
    {
        GetWorker().Start (m_shell.m_machine.GetRefs().printerCard->GetByteRing());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnPrinterCopy
//
//  Copy never consumes the strip: the worker resumes on the same page
//  whether or not the clipboard accepted it.
//
////////////////////////////////////////////////////////////////////////////////

void ShellPrinter::OnPrinterCopy (PrinterJob * job)
{
    HRESULT  hr = CopyPrintoutToClipboard (job->GetRaster());



    GetWorker().Start (m_shell.m_machine.GetRefs().printerCard->GetByteRing(), job->GetRaster());
    NotePrinterDeliveryResult (FAILED (hr));

    if (FAILED (hr))
    {
        DxuiMessageBox (GetPrinterDialogOwner(), &m_shell.m_chrome->GetTheme(), L"Could not copy the printout to the clipboard.",
                     L"Casso printer", MB_OK | MB_ICONWARNING);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnPrinterDiscard
//
//  Tear off and throw away the current page (FR-029). Confirm first --
//  there is no undo -- and default the dialog to "No" so a stray Enter
//  never destroys a page.
//
////////////////////////////////////////////////////////////////////////////////

void ShellPrinter::OnPrinterDiscard (PrinterJob * job)
{
    int   choice = DxuiMessageBox (
        GetPrinterDialogOwner(),
        &m_shell.m_chrome->GetTheme(),
        L"Tear off and discard the current printout?\n\n"
        L"The page in the printer will be thrown away without saving. "
        L"This cannot be undone.",
        L"Discard printout", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);



    if (choice != IDYES)
    {
        // Canceled: keep the strip and resume on the same page.
        GetWorker().Start (m_shell.m_machine.GetRefs().printerCard->GetByteRing(), job->GetRaster());
    }
    else
    {
        // Confirmed: play the tear-off (a random paper-tear), start a fresh
        // sheet, and drop the persisted pending copy. The problem page (if
        // any) went with it, so a latched delivery error clears too.
        m_shell.m_audio->GetPrinterAudio().PlayTearOff();
        GetWorker().Start (m_shell.m_machine.GetRefs().printerCard->GetByteRing());
        PrintJobStore::Clear (m_shell.GetPendingPrintDir());
        NotePrinterDeliveryResult (false);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnPrinterDeliver
//
//  Print -> Windows printer; Save -> PNG via file dialog. Both are
//  non-destructive: the paper stays so it can be delivered again, which is
//  why every arm here restarts the worker with the SAME raster.
//
////////////////////////////////////////////////////////////////////////////////

void ShellPrinter::OnPrinterDeliver (PrinterJob * job, bool print)
{
    HRESULT       hr              = S_OK;
    PrintOutcome  outcome         = PrintOutcome::Delivered;
    fs::path      file;
    std::wstring  failedStage;
    wchar_t       forceClassic[8] = {};
    bool          modernUp        = false;



    if (print && GetEnvironmentVariableW (L"CASSO_CLASSIC_PRINT", forceClassic, 8) == 0)
    {
        // DCR-1: the modern OS print UI with a live preview is the default --
        // it follows the documented PrintManagerInterop contract and delivers a
        // real paginated preview, then posts IDM_PRINTER_MODERN_SENT / _FAILED
        // on completion. ShowAsync returns S_OK once the experience is up; a
        // hard failure returns FAILED and we fall back to the classic dialog
        // (which prints with honest error reporting, just no preview pane). The
        // CASSO_CLASSIC_PRINT env var forces the classic path -- a support
        // hatch for the rare machine whose print stack misbehaves.
        const GlobalUserPrefs &  prefs  = m_shell.m_settings->GetPrefs();
        HRESULT                  hrShow = GetPrintDialog().ShowAsync (m_shell.m_hwnd, job->GetRaster(),
                                                                                         PrintDpiFromPrefs (prefs),
                                                                                         PrintDotStyleFromPrefs (prefs));

        // The async session owns the outcome from here; resume and let its
        // completion callback post the result.
        modernUp = SUCCEEDED (hrShow);
    }

    if (!modernUp)
    {
        hr = print ? PrintToWindowsPrinter (job->GetRaster(), failedStage, outcome)
                   : SavePrintoutAs (job->GetRaster(), file, outcome);
    }

    if (modernUp || outcome == PrintOutcome::Canceled)
    {
        // Modern session up, or the user canceled the print / save dialog:
        // keep the strip either way, no clear.
        GetWorker().Start (m_shell.m_machine.GetRefs().printerCard->GetByteRing(), job->GetRaster());
    }
    else if (SUCCEEDED (hr))
    {
        std::wstring   msg = print
                                 ? std::wstring (L"Sent the printout to the printer.")
                                 : (L"Saved printout to:\n" + file.wstring());

        NotePrinterDeliveryResult (false);
        DxuiMessageBox (GetPrinterDialogOwner(), &m_shell.m_chrome->GetTheme(), msg.c_str(), L"Casso printer", MB_OK | MB_ICONINFORMATION);

        // Non-destructive: keep the paper so it can also be saved / printed.
        GetWorker().Start (m_shell.m_machine.GetRefs().printerCard->GetByteRing(), job->GetRaster());
    }
    else
    {
        // Human sentence first, then a "Details:" trailer with the failing stage
        // + hr + OS message so a nerd (or a bug report) can see exactly what
        // failed without the dialog reading like a stack trace.
        std::wstring   msg = print
                                 ? std::wstring (L"Something went wrong while sending your printout, so it is still waiting in the printer. Please try printing again.")
                                 : std::wstring (L"Something went wrong while saving your printout, so it is still waiting in the printer. Please try again.");

        msg += L"\n\nDetails: ";
        if (!failedStage.empty())
        {
            msg += failedStage + L"\n";
        }

        msg += WindowCommandManager::FormatSystemError (hr);

        NotePrinterDeliveryResult (true);   // toolbar LED: red until resolved

        DxuiMessageBox (GetPrinterDialogOwner(), &m_shell.m_chrome->GetTheme(), msg.c_str(),
                     L"Casso printer", MB_OK | MB_ICONWARNING);

        // Keep the strip so the user can retry -- reseed the worker with it
        // (copied before the old job is replaced). It re-persists on exit.
        GetWorker().Start (m_shell.m_machine.GetRefs().printerCard->GetByteRing(), job->GetRaster());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnModernPrintResult
//
//  Posted back by the modern print session's completion callback (which runs
//  on a print-system thread and must not raise UI itself). The strip was
//  copied into the session and the worker already resumed, so there is
//  nothing to reseed here -- just tell the user how it went. Cancel posts
//  nothing, matching the classic dialog's silent cancellation.
//
////////////////////////////////////////////////////////////////////////////////

void ShellPrinter::OnModernPrintResult (bool succeeded)
{
    NotePrinterDeliveryResult (!succeeded);   // toolbar LED tracks the outcome

    if (succeeded)
    {
        DxuiMessageBox (GetPrinterDialogOwner(), &m_shell.m_chrome->GetTheme(),
                        L"Sent the printout to the printer.",
                        L"Casso printer", MB_OK | MB_ICONINFORMATION);
    }
    else
    {
        DxuiMessageBox (GetPrinterDialogOwner(), &m_shell.m_chrome->GetTheme(),
                        L"Something went wrong while sending your printout, so it is still "
                        L"waiting in the printer. Please try printing again.",
                        L"Casso printer", MB_OK | MB_ICONWARNING);
    }
}
