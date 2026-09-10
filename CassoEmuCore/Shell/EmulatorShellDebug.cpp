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
//  TraceProgressWindow
//
//  Minimal GDI-painted progress window for the --trace file dump. Drawn
//  entirely in WM_PAINT (no common controls) so it stays robust even when
//  raised from inside the unhandled-exception filter on the CPU thread.
//  Shows the reason, the full output path, and "N of M instructions (P%)"
//  with a fill bar. SetProgress repaints synchronously and pumps pending
//  messages so the window keeps redrawing during a multi-second write.
//
////////////////////////////////////////////////////////////////////////////////

class TraceProgressWindow
{
public:
    HRESULT Create (const std::wstring & reason, const std::wstring & path, uint64_t total)
    {
        HRESULT     hr      = S_OK;
        WNDCLASSEXW wc      = { sizeof (wc) };
        HINSTANCE   hInst   = GetModuleHandleW (nullptr);
        DWORD       style   = WS_POPUP | WS_BORDER | WS_CAPTION;
        RECT        wr      = {};
        int         winW    = 0;
        int         winH    = 0;
        int         screenW = GetSystemMetrics (SM_CXSCREEN);
        int         screenH = GetSystemMetrics (SM_CYSCREEN);

        m_reason = reason;
        m_path   = path;
        m_total  = total;
        m_done   = 0;

        wc.lpfnWndProc   = &TraceProgressWindow::WndProc;
        wc.hInstance     = hInst;
        wc.hCursor       = LoadCursorW (nullptr, IDC_WAIT);
        wc.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
        wc.lpszClassName = s_kpszClass;
        RegisterClassExW (&wc);            // benign if already registered

        // Create at a provisional position so the window's monitor DPI is
        // known, then size the client area to fit the DPI-scaled content
        // and recenter. Sizing the *client* rect (via AdjustWindowRect)
        // keeps a uniform margin around the content at any DPI -- a fixed
        // window height let the caption eat the client area and clipped
        // the progress bar against the bottom edge on high-DPI displays.
        m_hwnd = CreateWindowExW (WS_EX_TOPMOST,
                                  s_kpszClass,
                                  L"Casso \x2014 Writing trace",
                                  style,
                                  0, 0, s_kClientWPx, s_kClientHPx,
                                  nullptr, nullptr, hInst, this);
        CWR (m_hwnd);

        m_dpi = GetDpiForWindow (m_hwnd);
        if (m_dpi == 0)
        {
            m_dpi = 96;
        }

        // Use the OS themed message font (Segoe UI on Win10/11) sized for
        // this window's DPI. Without an explicit font GDI falls back to the
        // ancient bitmap SYSTEM_FONT, which looks aliased, ignores DPI, and
        // can't render the U+2026 ellipsis.
        {
            NONCLIENTMETRICSW  ncm = { sizeof (ncm) };

            if (SystemParametersInfoForDpi (SPI_GETNONCLIENTMETRICS, sizeof (ncm), &ncm, 0, m_dpi))
            {
                m_font = CreateFontIndirectW (&ncm.lfMessageFont);
            }
        }

        wr.left   = 0;
        wr.top    = 0;
        wr.right  = Scaled (s_kClientWPx);
        wr.bottom = Scaled (s_kClientHPx);
        AdjustWindowRectExForDpi (&wr, style, FALSE, WS_EX_TOPMOST, m_dpi);

        winW = wr.right - wr.left;
        winH = wr.bottom - wr.top;

        SetWindowPos (m_hwnd, HWND_TOPMOST,
                      (screenW - winW) / 2, (screenH - winH) / 2,
                      winW, winH, SWP_NOACTIVATE);

        ShowWindow   (m_hwnd, SW_SHOW);
        UpdateWindow (m_hwnd);

    Error:
        return hr;
    }

    void SetProgress (uint64_t done, uint64_t total)
    {
        HRESULT  hr  = S_OK;
        MSG      msg = {};



        m_done  = done;
        m_total = total;

        BAIL_OUT_IF (m_hwnd == nullptr, S_OK);

        InvalidateRect (m_hwnd, nullptr, FALSE);
        UpdateWindow   (m_hwnd);

        while (PeekMessageW (&msg, m_hwnd, 0, 0, PM_REMOVE))
        {
            TranslateMessage (&msg);
            DispatchMessageW  (&msg);
        }

    Error:
        return;
    }

    void Destroy()
    {
        if (m_hwnd != nullptr)
        {
            DestroyWindow (m_hwnd);
            m_hwnd = nullptr;
        }

        if (m_font != nullptr)
        {
            DeleteObject (m_font);
            m_font = nullptr;
        }
    }

private:
    static LRESULT CALLBACK WndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        LRESULT                result = 0;



        TraceProgressWindow *  self   = reinterpret_cast<TraceProgressWindow *> (
            GetWindowLongPtrW (hwnd, GWLP_USERDATA));

        if (msg == WM_CREATE)
        {
            CREATESTRUCTW *  cs = reinterpret_cast<CREATESTRUCTW *> (lParam);
            SetWindowLongPtrW (hwnd, GWLP_USERDATA,
                               reinterpret_cast<LONG_PTR> (cs->lpCreateParams));
        }
        else if (msg == WM_PAINT && self != nullptr)
        {
            self->OnPaint (hwnd);
        }
        else
        {
            result = DefWindowProcW (hwnd, msg, wParam, lParam);
        }

        return result;
    }

    void OnPaint (HWND hwnd)
    {
        PAINTSTRUCT  ps         = {};
        HDC          hdc        = BeginPaint (hwnd, &ps);
        RECT         rc         = {};
        RECT         bar        = {};
        int          pad        = Scaled (s_kPadPx);
        int          line       = Scaled (s_kLinePx);
        int          barH       = Scaled (s_kBarPx);
        int          gap        = Scaled (s_kGapSmallPx);
        int          pct        = (m_total > 0) ? (int) ((m_done * 100) / m_total) : 0;
        wchar_t      line1[128] = {};
        wchar_t      line3[160] = {};
        HBRUSH       fill       = CreateSolidBrush (RGB (0x2D, 0x7D, 0x46));
        RECT         r1         = {};
        RECT         r2         = {};
        RECT         r3         = {};
        RECT         filled     = {};
        HFONT        oldFont    = nullptr;

        GetClientRect (hwnd, &rc);

        oldFont = (m_font != nullptr) ? (HFONT) SelectObject (hdc, m_font) : nullptr;

        // SetProgress invalidates without erasing (bErase = FALSE) and the
        // text is drawn transparently, so wipe the client area first --
        // otherwise each new progress value piles up on the previous one.
        FillRect (hdc, &rc, (HBRUSH) (COLOR_WINDOW + 1));

        swprintf_s (line1, L"Writing execution trace (%s)\x2026", m_reason.c_str());
        swprintf_s (line3, L"%llu of %llu instructions  (%d%%)",
                    (unsigned long long) m_done, (unsigned long long) m_total, pct);

        SetBkMode   (hdc, TRANSPARENT);

        r1 = { pad, pad, rc.right - pad, pad + line };
        DrawTextW (hdc, line1, -1, &r1, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

        r2 = { pad, r1.bottom + gap, rc.right - pad, r1.bottom + gap + line };
        DrawTextW (hdc, m_path.c_str(), -1, &r2, DT_LEFT | DT_SINGLELINE | DT_PATH_ELLIPSIS);

        r3 = { pad, r2.bottom + gap, rc.right - pad, r2.bottom + gap + line };
        DrawTextW (hdc, line3, -1, &r3, DT_LEFT | DT_SINGLELINE);

        // Anchor the bar to the bottom with a margin equal to the top pad,
        // so the bottom whitespace mirrors the caption-to-text gap and the
        // bar can't be clipped by the window edge at any DPI.
        bar.left   = pad;
        bar.right  = rc.right - pad;
        bar.bottom = rc.bottom - pad;
        bar.top    = bar.bottom - barH;
        FrameRect (hdc, &bar, (HBRUSH) GetStockObject (GRAY_BRUSH));

        filled = bar;
        filled.right = bar.left + (LONG) (((bar.right - bar.left) * (LONGLONG) pct) / 100);
        FillRect (hdc, &filled, fill);

        DeleteObject (fill);

        if (oldFont != nullptr)
        {
            SelectObject (hdc, oldFont);
        }

        EndPaint (hwnd, &ps);
    }

    int Scaled (int px) const { return MulDiv (px, (int) m_dpi, 96); }

    static constexpr const wchar_t *  s_kpszClass   = L"CassoTraceProgress";
    static constexpr int              s_kClientWPx  = 556;
    static constexpr int              s_kPadPx      = 16;
    static constexpr int              s_kLinePx     = 22;
    static constexpr int              s_kBarPx      = 22;
    static constexpr int              s_kGapSmallPx = 4;
    static constexpr int              s_kGapBarPx   = 8;

    // Client height kept in lockstep with the OnPaint layout: top pad +
    // three text lines (each followed by a small gap) + the larger gap
    // above the bar + the bar + an equal bottom pad.
    static constexpr int              s_kClientHPx  = s_kPadPx
                                                    + 3 * s_kLinePx
                                                    + 2 * s_kGapSmallPx
                                                    + s_kGapBarPx
                                                    + s_kBarPx
                                                    + s_kPadPx;

    HWND          m_hwnd  = nullptr;
    HFONT         m_font  = nullptr;
    UINT          m_dpi   = 96;
    std::wstring  m_reason;
    std::wstring  m_path;
    uint64_t      m_done  = 0;
    uint64_t      m_total = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DumpTrace
//
//  Write the CPU execution-trace ring to a timestamped text file in the
//  working directory, showing a progress window. Best-effort and one-shot:
//  guarded so the graceful-exit path and the crash handler cannot both
//  write, and a no-op when --trace is off. Safe to call from the crash
//  handler on the CPU thread (the thread that owns the ring) since the
//  process is already halted at the fault.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::DumpTrace (const wstring & reason)
{
    HRESULT       hr             = S_OK;
    bool          expected       = false;
    SYSTEMTIME    st             = {};
    wchar_t       name[64]       = {};
    wchar_t       cwd[MAX_PATH]  = {};
    std::wstring  path;
    uint64_t      total          = 0;
    bool          wonTheRace     = false;
    bool          hasTrace       = false;



    // One-shot: the graceful-exit path and the crash handler both call this,
    // and only the first through gets to write.
    wonTheRace = m_traceDumped.compare_exchange_strong (expected, true);

    BAIL_OUT_IF (!wonTheRace, S_OK);

    hasTrace = m_traceCapacity != 0 && m_machine.GetCpu() != nullptr && m_machine.GetCpu()->IsTraceEnabled();

    BAIL_OUT_IF (!hasTrace, S_OK);

    GetLocalTime (&st);
    swprintf_s (name, L"casso-trace-%04u%02u%02u-%02u%02u%02u.txt",
                st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    if (GetCurrentDirectoryW (MAX_PATH, cwd) > 0)
    {
        path = std::wstring (cwd) + L"\\" + name;
    }
    else
    {
        path = name;
    }

    total = m_machine.GetCpu()->GetTraceCount();

    // Scoped so the bails above never jump across the window's construction.
    {
        TraceProgressWindow  win;

        win.Create (reason, path, total);

        hr = m_machine.GetCpu()->DumpTraceToFile (path, [&win] (uint64_t done, uint64_t tot)
        {
            win.SetProgress (done, tot);
        });

        win.Destroy();

        // Tear the progress window down FIRST, then report: the notify is modal,
        // and leaving a progress dialog stranded behind it looks like a hang.
        // This path also runs from the crash handler, where the trace is the only
        // artifact -- silently losing it is the worst possible outcome.
        CHRN (hr, L"Could not write the CPU trace file");
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpenDisk2DebugDialog
//
//  Spec-006 / FR-001 / FR-017 / FR-024. View -> Disk II Debug...
//  command handler and Ctrl+Shift+D accelerator target. Lazy-creates
//  the modeless dialog on first open, wires it as the controller's
//  event sink AND as the active Disk2AudioSource's audio-event
//  sink, applies the uptime anchor and the multi-controller title
//  hint, then shows + foregrounds the window. Subsequent calls
//  short-circuit to Show + SetForegroundWindow.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OpenDisk2DebugDialog()
{
    HRESULT            hr         = S_OK;
    Disk2Controller  * controller = nullptr;
    int                Disk2Count = 0;
    HINSTANCE          hInstance  = nullptr;
    size_t             i          = 0;



    controller = m_diskManager->FindSlot6Controller();

    // FR-001a should have grayed the menu item; the accelerator
    // bypasses that gate so we defend in depth.
    CBR (controller != nullptr);

    for (const SlotConfig & slot : m_machine.GetConfig().slots)
    {
        if (slot.device == "disk-ii")
        {
            Disk2Count++;
        }
    }

    if (m_disk2DebugPanel == nullptr || m_disk2DebugPanel->GetHwnd() == nullptr)
    {
        hInstance          = reinterpret_cast<HINSTANCE> (GetWindowLongPtr (m_hwnd, GWLP_HINSTANCE));
        m_disk2DebugPanel = std::make_unique<Disk2DebugPanel>();

        hr = m_disk2DebugPanel->Create (hInstance,
                                         m_hwnd,
                                         m_d3dRenderer.GetDevice(),
                                         m_d3dRenderer.GetContext(),
                                         &m_chromeTheme);
        CHRF (hr, m_disk2DebugPanel.reset());

        ApplyAppIconToWindow (m_disk2DebugPanel->GetHwnd());

        m_disk2DebugPanel->SetUptimeAnchor (m_uptimeAnchor);
        m_disk2DebugPanel->SetMultiControllerHint (Disk2Count > 1);

        if (m_machine.GetCpu() != nullptr)
        {
            m_disk2DebugPanel->SetCycleCounter (m_machine.GetCpu()->GetCycleCounterPtr());
        }

        controller->SetEventSink (m_disk2DebugPanel.get());

        for (auto & diskAudioSource : m_diskAudioSources)
        {
            if (diskAudioSource != nullptr)
            {
                diskAudioSource->SetAudioEventSink (m_disk2DebugPanel.get());
            }
        }
    }
    else
    {
        m_disk2DebugPanel->SetMultiControllerHint (Disk2Count > 1);
    }

    m_disk2DebugPanel->Show();
    SetForegroundWindow (m_disk2DebugPanel->GetHwnd());

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpenInputDebugDialog
//
//  Shows the input debug panel, creating it on first use.
//
//  The panel is created lazily and then kept: it is a diagnostic window most
//  sessions never open, but one that is toggled repeatedly when it is in use.
//  The re-create test covers a null panel AND a live panel whose HWND has
//  already been destroyed, since closing the window leaves the object behind.
//
//  Creation wires the panel in as an input event SINK on every device that
//  produces input -- keyboard, //e soft switches, game port -- so it observes
//  the real event stream rather than polling state and inventing its own
//  version of what happened.
//
//  A failed Create resets the pointer, so a subsequent open retries cleanly
//  instead of finding a half-built panel and short-circuiting the branch.
//
//  The uptime anchor and cycle counter are handed over so the panel timestamps
//  events in the emulator's own time base, not the host's.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OpenInputDebugDialog()
{
    HRESULT          hr        = S_OK;
    HINSTANCE        hInstance = nullptr;
    AppleKeyboard *  keyboard  = m_machine.GetRefs().keyboard;



    CBR (keyboard != nullptr);

    if (m_inputDebugPanel == nullptr || m_inputDebugPanel->GetHwnd() == nullptr)
    {
        Apple2eSoftSwitchBank * iieSwitches = nullptr;

        hInstance         = reinterpret_cast<HINSTANCE> (GetWindowLongPtr (m_hwnd, GWLP_HINSTANCE));
        m_inputDebugPanel = std::make_unique<InputDebugPanel>();

        hr = m_inputDebugPanel->Create (hInstance,
                                         m_hwnd,
                                         m_d3dRenderer.GetDevice(),
                                         m_d3dRenderer.GetContext(),
                                         &m_chromeTheme);
        CHRF (hr, m_inputDebugPanel.reset());

        ApplyAppIconToWindow (m_inputDebugPanel->GetHwnd());

        m_inputDebugPanel->SetUptimeAnchor (m_uptimeAnchor);

        // $C063 is the //c's active-low mouse button whenever a mouse device
        // is wired up (matching Apple2eKeyboard's own read), and the //e's
        // shift-key mod otherwise.
        m_inputDebugPanel->SetMouseButtonAtC063 (m_machine.GetMouse() != nullptr);

        if (m_machine.GetCpu() != nullptr)
        {
            m_inputDebugPanel->SetCycleCounter (m_machine.GetCpu()->GetCycleCounterPtr());
        }

        m_machine.GetRefs().keyboard->SetInputEventSink (m_inputDebugPanel.get());

        iieSwitches = m_machine.GetRefs().iieSoftSwitches;
        if (iieSwitches != nullptr)
        {
            iieSwitches->SetInputEventSink (m_inputDebugPanel.get());
        }

        if (m_machine.GetRefs().gamePort != nullptr)
        {
            m_machine.GetRefs().gamePort->SetInputEventSink (m_inputDebugPanel.get());
        }
    }

    m_inputDebugPanel->Show();
    SetForegroundWindow (m_inputDebugPanel->GetHwnd());

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AttachDebugSinksIfOpen
//
//  Spec-006 bug 15. A machine switch tears down the old controller and
//  audio source and builds new ones, but the panel's sink wiring only ran
//  inside OpenDisk2DebugDialog on first open -- the new controller starts
//  with a null event sink and the new audio source with a null audio event
//  sink, so the debug window goes silent after a switch. This re-points
//  whoever is open at the machine that now exists, and is a no-op when
//  neither panel has been opened.
//
//  Each panel is attached on its own. This used to return early unless the
//  disk panel was open, so someone running only the Input panel watched it
//  go quiet on every machine switch and never come back.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::AttachDebugSinksIfOpen()
{
    m_machine.AttachObservers ({ m_disk2DebugPanel.get(), m_inputDebugPanel.get() });

    // The drive audio sources are the emulator's, not the machine's -- the
    // machine has a drive, the emulator decides what it sounds like -- so
    // the panel is pointed at them from here.
    for (auto & diskAudioSource : m_diskAudioSources)
    {
        if (diskAudioSource != nullptr)
        {
            diskAudioSource->SetAudioEventSink (m_disk2DebugPanel.get());
        }
    }
}
