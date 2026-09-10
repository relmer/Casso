#include "Pch.h"

#include "Shell/EmulatorShell.h"
#include "Shell/EmulatorShellInternal.h"
#include "AssetBootstrap.h"
#include "Config/MonitorCatalog.h"
#include "Config/MachineInputPrefs.h"
#include "Config/CrtPresets.h"
#include "Config/CrtResolver.h"
#include "Ui/Chrome/DriveLabelTruncation.h"
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
#include "Window/DxuiHwndSource.h"
#include "Ui/Dialogs/DialogBodyContent.h"
#include "Ui/Dialogs/MessageDialog.h"
#include "Ui/Dialogs/SalvageDialogContent.h"
#include "Ui/Settings/SettingsPanelState.h"
#include "Ui/Settings/SettingsSheet.h"   // TEMP (T162 3a dev trigger)
#include "Seams/Win32IntentChannel.h"
#include "Devices/Disk/PreservedCopy.h"





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ShowPrinterPanel
//
//  Lazily creates the printer panel / print preview window, wires its toolbar
//  callbacks to the existing delivery commands, pushes a fresh strip snapshot,
//  and brings it to the foreground. Mirrors ShowDisk2Debug's create pattern.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ShowPrinterPanel (bool activate)
{
    HRESULT     hr        = S_OK;
    HINSTANCE   hInstance = nullptr;



    DXUI_ASSERT_UI_THREAD();   // creates / shows a Dxui window


    if (m_printerPanel == nullptr || m_printerPanel->GetHwnd() == nullptr)
    {
        hInstance      = reinterpret_cast<HINSTANCE> (GetWindowLongPtr (m_hwnd, GWLP_HINSTANCE));
        m_printerPanel = std::make_unique<PrinterPanel> ();

        // No owner window: the preview is a peer of the main window, not an
        // owned popup. An owned window is permanently z-locked above its owner
        // (always-on-top of Casso); a peer can be sent behind Casso normally.
        hr = m_printerPanel->Create (hInstance,
                                     nullptr,
                                     m_hwnd,     // placement anchor only -- not an owner
                                     m_d3dRenderer.GetDevice(),
                                     m_d3dRenderer.GetContext(),
                                     &m_chromeTheme);
        CHRF (hr, m_printerPanel.reset());

        ApplyAppIconToWindow (m_printerPanel->GetHwnd());

        // Toolbar actions route through the existing command path (which
        // quiesces the worker, delivers/clears, and resumes), then re-snapshot.
        // Print / Save / Copy are non-destructive: they deliver the strip and
        // leave the paper in the printer, so one printout can be printed AND
        // saved AND copied. Discard is the one tear-off.
        m_printerPanel->SetOnPrint ([this] ()
        {
            m_windowCommandManager->HandleCommand (IDM_PRINTER_PRINT);
            SnapshotStripToPanel();
        });
        m_printerPanel->SetOnSaveAs ([this] ()
        {
            m_windowCommandManager->HandleCommand (IDM_PRINTER_SAVEAS);
            SnapshotStripToPanel();
        });
        m_printerPanel->SetOnCopy ([this] ()
        {
            m_windowCommandManager->HandleCommand (IDM_PRINTER_COPY);
            SnapshotStripToPanel();
        });
        m_printerPanel->SetOnDiscard ([this] ()
        {
            // The tear-off sound fires from the confirmed branch of the discard
            // handler (WindowCommandManager), NOT here -- so canceling the
            // confirmation dialog does not rip a page we are keeping.
            m_windowCommandManager->HandleCommand (IDM_PRINTER_DISCARD);
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
            m_printerAudio.PlayFormFeed (unused);

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
            TryPresentUiFrame();
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
        SetWindowPos (m_printerPanel->GetHwnd(), m_hwnd, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::UpdatePrinterStatus
//
//  Samples the worker's thread-safe status signals, recomputes the LED state
//  through the pure PrinterStatusModel, feeds the toolbar's printer button,
//  and marks a redraw only on a change so a static screen still repaints the
//  LED on a transition.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::UpdatePrinterStatus()
{
    int64_t        nowMs  = 0;
    PrinterStatus  status = PrinterStatus::Idle;



    if (m_machine.GetRefs().printerCard == nullptr)
    {
        m_toolbar.SetPrinterPresent (false);
        return;   // no card: the toolbar's printer button disables
    }

    m_toolbar.SetPrinterPresent (true);

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
        m_toolbar.SetPrinterStatus (status);
        m_d3dRenderer.MarkRedrawNeeded();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::UpdatePrinterPreview
//
//  Per-frame: auto-open the preview the moment the guest starts printing, then
//  refresh the strip live as bytes flow. The read is non-destructive (see
//  SnapshotStripToPanel), so watching a print in progress never perturbs it.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::UpdatePrinterPreview()
{
    static constexpr int64_t   s_kAutoOpenIdleMs = 1200;   // activity gap that re-arms auto-open



    HRESULT    hr        = S_OK;
    uint64_t   activity  = 0;
    int64_t    nowMs     = 0;
    bool       previewUp = false;

    BAIL_OUT_IF (m_machine.GetRefs().printerCard == nullptr, S_OK);   // machine has no printer card

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
        m_printerAudio.PublishReveal (progressDots, colDots, inkActive, sweepWidthDots);
    }

    // Printer-sound volume + mute (Settings > Printing audio, FR-034). Read from
    // prefs each frame so an OK / Cancel in Settings binds on the next update
    // without any live-apply plumbing; the shared "Drive Audio" master still
    // gates the whole bus above this.
    m_printerAudio.SetVolume (m_globalPrefs.printerAudioVolume);
    m_printerAudio.SetMuted  (!m_globalPrefs.printerAudioEnabled);

    // Position the printer sound in the stereo field. Manual override (Settings >
    // Printing) pins a fixed pan; otherwise it auto-follows where the preview
    // window sits relative to the main Casso window -- center-to-center X offset,
    // normalized so the two windows just touching side by side is a hard pan and
    // a fully overlapping (co-centered) window is dead center.
    {
        float  pan  = 0.0f;
        float  panL = 0.0f;
        float  panR = 0.0f;

        if (m_globalPrefs.printerAudioPanOverride)
        {
            pan = std::clamp (m_globalPrefs.printerAudioPan, -1.0f, 1.0f);
        }
        else
        {
            RECT  mainR    = {};
            RECT  printerR = {};

            if (GetWindowRect (m_hwnd, &mainR) &&
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
        m_printerAudio.SetPan (panL, panR);
    }

    // Hold a smooth present cadence while the carriage is sweeping or a pan/zoom
    // is easing. Without this the loop drops to Sleep(1) whenever the emulator
    // framebuffer is static (a guest that prints without touching the screen),
    // and that coarse, jittery tick makes the head step across the platen.
    if (m_printerPanel->NeedsAnimationFrame())
    {
        m_d3dRenderer.MarkRedrawNeeded();
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

std::wstring EmulatorShell::GetPrinterBannerMessage() const
{
    std::wstring  message;



    if (m_machine.GetConfig().HasEnabledSlotDevice ("parallel-printer"))
    {
        message = L"Emulating an Apple ImageWriter II connected via parallel interface.";
    }
    else
    {
        message = L"No printer is connected to this " + fs::path (m_machine.GetConfig().name).wstring() + L".";
    }

    return message;
}
