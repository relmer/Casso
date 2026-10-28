#include "Pch.h"

#include "Shell/Components/ShellFrameRateReadout.h"

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
//  ShellFrameRateReadout::SyncFrameRateReadout
//
//  The frame rate over the picture, in the top-left corner.
//
//  COUNTED AT THE PRESENT, not here: DxuiHwndSource ticks its counter when
//  a frame actually reaches the screen, so a paint the shell skipped is not
//  a dropped frame and a present that waited on vsync reports the interval
//  the user saw. This only reads the figure and places it.
//
//  One decimal, because whether the scene holds sixty is the question and a
//  rounded integer answers it ambiguously at the boundary.
//
////////////////////////////////////////////////////////////////////////////////

void ShellFrameRateReadout::SyncFrameRateReadout()
{
    RECT     client   = {};
    RECT     rc       = {};
    wchar_t  text[32] = {};



    if (!m_shell.m_settings->GetPrefs().showFrameRate || m_shell.m_host == nullptr
        || m_shell.m_hwnd == nullptr || !GetClientRect (m_shell.m_hwnd, &client))
    {
        m_fpsReadout.SetVisible (false);
        return;
    }

    // ABOVE THE BOTTOM CHROME, not on it: hung from the client edge the
    // readout straddles the switch bar, half over the scene and half over a
    // shell band, reading as neither.
    //
    // MEASURED OFF THE CHROME, NEVER OFF THE SCENE. Anchored to the toolbar
    // band this drifted up and down while the scene was being orbited: the
    // bands report different bounds as the composition changes under them,
    // so a readout hung off one wanders with the thing it is measuring. The
    // switch bar does not move, and the client edge behind it does not
    // either.
    {
        RECT  bar    = m_chrome.m_switchBand.GetBounds();
        LONG  bottom = (!m_shell.m_renderer->GetD3D().IsFullscreen() && bar.bottom > bar.top)
                     ? bar.top : client.bottom;

        rc.left   = client.left + m_shell.m_scaler.ToPx (s_kFrameRateInsetDp);
        rc.right  = rc.left + m_shell.m_scaler.ToPx (s_kFrameRateWidthDp);
        rc.bottom = bottom - m_shell.m_scaler.ToPx (s_kFrameRateInsetDp);
        rc.top    = rc.bottom - m_shell.m_scaler.ToPx (s_kFrameRateHeightDp);
    }

    swprintf_s (text, L"%.1f fps", m_shell.m_host->GetFramesPerSecond());

    m_fpsReadout.SetText        (text);
    m_fpsReadout.SetFontSizeDip (DxuiShadowedText::kFontDip);
    m_fpsReadout.SetAlign       (DxuiTextHAlign::Left, DxuiTextVAlign::Center);
    m_fpsReadout.SetDpi         (m_shell.m_scaler.GetDpi());
    m_fpsReadout.Layout         (rc, m_shell.m_scaler);
    m_fpsReadout.SetVisible     (true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellFrameRateReadout::AdoptFrameRateReadout
//
////////////////////////////////////////////////////////////////////////////////

void ShellFrameRateReadout::AdoptFrameRateReadout (DxuiPanel & root)
{
    root.Adopt (m_fpsReadout);
}
