#include "Pch.h"

#include "Shell/Components/ShellNotices.h"

#include "Shell/Components/ShellInput.h"
#include "Shell/Components/ShellChrome.h"
#include "Shell/EmulatorShell.h"
#include "Shell/ShellDebugger.h"
#include "Shell/Components/ShellRenderer.h"
#include "Shell/Components/ShellDeskScene.h"
#include "Ui/ThemeManager.h"
#include "Config/UserConfigStore.h"
#include "Shell/Components/ShellSettings.h"
#include "Shell/Components/ShellDisks.h"
#include "Shell/Components/ShellSerialCard.h"
#include "Shell/DiskManager.h"
#include "Shell/Components/ShellTapeDeck.h"
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
#include "Shell/Layout/ChromeBandLayout.h"
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
#include "Ui/Settings/SettingsSheet.h"   // TEMP (T162 3a dev trigger)"
#include "Seams/Win32IntentChannel.h"
#include "Devices/Disk/PreservedCopy.h"
#include "Ui/ThemeManager.h"
#include "Shell/DiskManager.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ShowNotice
//
//  Show a notice over the picture for a few seconds. UI thread only.
//
//  The text arrives already composed -- by CaptureOutcome::DescribeResult or
//  WriteProtectChange::DescribeResult -- which is deliberate: every branch of
//  what to say is decided in core where a test can reach it, and this
//  function chooses no wording.
//
//  A notice already up keeps its full time; this one goes below it.
//
////////////////////////////////////////////////////////////////////////////////

void ShellNotices::ShowNotice (const std::wstring & text)
{
    int64_t   nowMs = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                          std::chrono::steady_clock::now().time_since_epoch()).count();



    m_notices.Push (text, nowMs);

    SyncNotice();

    m_shell.m_renderer->GetD3D().MarkRedrawNeeded();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PostNotice
//
//  Hand a notice to the window from any thread. The notice is Dxui and Dxui
//  asserts UI-thread affinity, so a caller on the CPU thread cannot show it
//  directly. With no window there is nothing to show it over, and the notice
//  is dropped: it only confirms a change the indicators already show.
//
////////////////////////////////////////////////////////////////////////////////

void ShellNotices::PostNotice (const std::wstring & text)
{
    wstring *  carried = nullptr;



    if (m_shell.m_hwnd == nullptr)
    {
        return;
    }

    carried = new (std::nothrow) wstring (text);

    if (carried != nullptr && !PostMessageW (m_shell.m_hwnd, WM_APP_SHOW_NOTICE, 0,
                                             reinterpret_cast<LPARAM> (carried)))
    {
        delete carried;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SyncNotice
//
//  Lay the notice out while it is live, and drop it once it expires.
//
//  ACROSS THE TOP, UNDER EVERYTHING DOCKED THERE. The chrome at the top of
//  the window is where this window puts what it has to say about itself, and
//  a screenshot's filename is exactly that -- the one part of a capture that
//  is about the application rather than about the machine. Put over the
//  picture it lands on whatever the user just photographed, and read as a
//  caption on it.
//
//  IT OVERLAYS RATHER THAN DOCKS, which is the one way it differs from the
//  pointer-capture bar beside it. That bar tracks a state and is worth the
//  height it takes from the picture; this one is up for four seconds, and a
//  band that appears and vanishes on a timer would reflow the machine twice
//  for every screenshot. So it hangs UNDER the last docked band and covers a
//  strip of picture, dimmed by a scrim rather than hidden behind a panel.
//
//  Separate from the pointer-capture bar, not a reuse of it: a screenshot
//  taken with the paddle captured must not replace the words telling the user
//  how to get their cursor back.
//
//  AN EXPIRY OR A SLIDE ASKS FOR ITS OWN FRAMES. A notice leaving, and the
//  ones below it moving up, change the picture with nothing else asking for
//  a present; a paused machine would otherwise leave a stale notice up until
//  something unrelated repainted. WaitForFrameOrMessage wakes for the next
//  expiry for the same reason.
//
////////////////////////////////////////////////////////////////////////////////

void ShellNotices::SyncNotice()
{
    RECT                 client   = {};
    RECT                 rc       = {};
    IDxuiTextRenderer *  text     = (m_shell.m_host != nullptr) ? m_shell.m_host->GetTextRenderer() : nullptr;
    float                width    = 0.0f;
    size_t               countWas = m_notices.GetCount();
    int64_t              nowMs    = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                                        std::chrono::steady_clock::now().time_since_epoch()).count();



    //  The system's animation setting is read here and passed in, so the
    //  stack moves notices at once when the user has turned animations off.
    m_notices.SetAnimationsEnabled (DxuiSystemSettings::Instance().AreMenuAnimationsEnabled());
    m_notices.Tick (nowMs);

    if (m_notices.GetCount() != countWas || m_notices.IsAnimating (nowMs))
    {
        m_shell.m_renderer->GetD3D().MarkRedrawNeeded();
    }

    if (!m_notices.IsShowing (nowMs) || m_shell.m_hwnd == nullptr || !GetClientRect (m_shell.m_hwnd, &client))
    {
        m_notices.SetVisible (false);
        return;
    }

    width = (float) (client.right - client.left);

    rc.left  = client.left;
    rc.right = client.right;
    rc.top   = m_chrome.ComputeTopOverlayEdgePx (client);

    //  Measured where there is a renderer to ask; the estimate is the
    //  fallback for the frames before the renderer exists.
    m_notices.SetDpi (m_shell.m_scaler.GetDpi());

    rc.bottom = rc.top + (LONG) m_notices.MeasureHeightPx (text, width, m_shell.m_scaler);

    m_notices.Layout     (rc, m_shell.m_scaler);
    m_notices.SetVisible (true);
}
