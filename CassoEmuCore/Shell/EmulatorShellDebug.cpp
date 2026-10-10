#include "Pch.h"

#include "Shell/EmulatorShell.h"
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
//  The crash handler's trace write. One-shot, so a fault inside the write
//  cannot start a second one, and a no-op when --trace is off. Safe on the
//  CPU thread (the thread that owns the ring) since the process is already
//  halted at the fault.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::DumpTrace (const wstring & reason)
{
    HRESULT       hr         = S_OK;
    bool          expected   = false;
    bool          wonTheRace = false;
    std::wstring  path;



    wonTheRace = m_traceDumped.compare_exchange_strong (expected, true);

    BAIL_OUT_IF (!wonTheRace, S_OK);

    hr = WriteTrace (reason, path);
    IGNORE_RETURN_VALUE (hr, S_OK);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SaveTrace
//
//  Debug > Save CPU trace, run on the CPU thread between slices so the ring
//  holds still while it is written. The machine stops for the length of the
//  write, which for a hang is the state worth keeping anyway. Repeatable,
//  unlike the crash dump, and the notice gives where the file went.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SaveTrace()
{
    HRESULT       hr = S_OK;
    std::wstring  path;



    BAIL_OUT_IF (m_traceDumped.load(), S_OK);

    hr = WriteTrace (L"request", path);
    CHR (hr);

    PostNotice (L"CPU trace saved to " + path);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteTrace
//
//  Write the CPU execution-trace ring to a timestamped text file on the
//  desktop, headed by the power-on seed, showing a progress window. S_OK
//  without writing when --trace is off; a failed write is reported to the
//  user here, since from the crash handler the trace is the only artifact.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::WriteTrace (
    const wstring  & reason,
    std::wstring   & path)
{
    HRESULT       hr             = S_OK;
    SYSTEMTIME    st             = {};
    wchar_t       name[64]       = {};
    std::string   preamble;
    uint64_t      total          = 0;
    bool          hasTrace       = false;



    hasTrace = m_traceCapacity != 0 && m_machine.GetCpu() != nullptr && m_machine.GetCpu()->IsTraceEnabled();

    BAIL_OUT_IF (!hasTrace, S_OK);

    GetLocalTime (&st);
    swprintf_s (name, L"casso-trace-%04u%02u%02u-%02u%02u%02u.txt",
                st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    path     = GetTraceFolder() + L"\\" + name;
    preamble = std::format ("power-on seed: 0x{:016X}  (replay with --seed 0x{:016X})\n",
                            m_prngSeed, m_prngSeed);
    total    = m_machine.GetCpu()->GetTraceCount();

    // Scoped so the bails above never jump across the window's construction.
    {
        TraceProgressWindow  win;

        win.Create (reason, path, total);

        hr = m_machine.GetCpu()->DumpTraceToFile (path, preamble, [&win] (uint64_t done, uint64_t tot)
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
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetTraceFolder
//
//  The desktop, where someone reporting a bug will find the file without
//  knowing what the working directory was. A launch from a shortcut usually
//  starts in the install folder, which may not be writable. Falls back to the
//  working directory when the desktop cannot be resolved.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring EmulatorShell::GetTraceFolder()
{
    HRESULT       hr            = S_OK;
    PWSTR         pszDesktop    = nullptr;
    wchar_t       cwd[MAX_PATH] = {};
    std::wstring  folder        = L".";



    hr = SHGetKnownFolderPath (FOLDERID_Desktop, KF_FLAG_DEFAULT, nullptr, &pszDesktop);

    if (SUCCEEDED (hr))
    {
        folder = pszDesktop;
    }
    else if (GetCurrentDirectoryW (MAX_PATH, cwd) > 0)
    {
        folder = cwd;
    }

    CoTaskMemFree (pszDesktop);

    return folder;
}
