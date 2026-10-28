#include "Pch.h"

#include "Shell/Components/ShellToolbarEntries.h"

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
//  RefreshToolbarThemeList
//
//  Fills the toolbar's theme picker from the discovered catalog and keeps the
//  parallel id vector that turns a picked row back into a theme id.
//
//  The rows are REPLACED only when the catalog itself changed. This runs from
//  the theme-change listener, which also fires for every live preview -- and
//  replacing the rows resets the selection, which would destroy the row the
//  picker has to snap back to when the list is dismissed. An unchanged
//  catalog therefore only moves the selection, which the toolbar in turn
//  drops while its list is open.
//
////////////////////////////////////////////////////////////////////////////////

void ShellToolbarEntries::RefreshToolbarThemeList()
{
    std::vector<std::wstring>  displayNames;
    std::vector<std::string>   ids;
    std::string                activeName;
    int                        activeIndex = -1;
    int                        row         = 0;



    if (m_shell.m_settings->GetThemeManager() == nullptr)
    {
        return;
    }

    activeName = m_shell.m_settings->GetThemeManager()->GetActiveThemeName();

    for (const LoadedTheme & theme : m_shell.m_settings->GetThemeManager()->GetAvailableThemes())
    {
        if (theme.name == activeName)
        {
            activeIndex = row;
        }

        ids.push_back (theme.name);
        displayNames.emplace_back (theme.name.begin(), theme.name.end());
        row++;
    }

    // An OPEN picker is mid-preview and owns the index, so a sync from here is
    // dropped rather than fighting the highlight the user is moving -- the
    // preview itself arrives back here as a sync.
    if (ids != m_toolbarThemeIds)
    {
        m_toolbarThemeIds = std::move (ids);
        m_chrome.m_mainMenu.GetCommands().SetThemeNames (displayNames);
        m_chrome.m_mainMenu.GetCommands().SetThemeIndex (activeIndex);
        m_chrome.m_toolbar.SetDropDownItems (EmulatorCommands::kIdTheme, m_chrome.m_mainMenu.GetCommands().GetThemeItems());
    }
    else if (activeIndex >= 0 && !m_chrome.m_toolbar.IsMenuOpen())
    {
        m_chrome.m_mainMenu.GetCommands().SetThemeIndex (activeIndex);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SyncToolbarState
//
//  Pushes the state the toolbar mirrors rather than owns: which machine the
//  Reset / Power tips talk about, which way the fullscreen button points, and
//  where the two pickers sit. The pickers can be moved from the menu, from
//  Settings and from a machine switch, so the sync runs every UI frame; the
//  toolbar drops it while a list is open, since an open list is mid-preview
//  and owns its own value until it closes.
//
////////////////////////////////////////////////////////////////////////////////

void ShellToolbarEntries::SyncToolbarState()
{
    ColorMode  mode       = m_shell.m_renderer->GetColorMode();
    RECT       client     = {};
    int        colorIndex = 0;
    int        themeIndex = -1;
    int        row        = 0;



    switch (mode)
    {
        case ColorMode::GreenMono: colorIndex = 1; break;
        case ColorMode::AmberMono: colorIndex = 2; break;
        case ColorMode::WhiteMono: colorIndex = 3; break;
        default:                   colorIndex = 0; break;
    }

    if (m_shell.m_settings->GetThemeManager() != nullptr)
    {
        const std::string &  activeName = m_shell.m_settings->GetThemeManager()->GetActiveThemeName();

        for (const std::string & id : m_toolbarThemeIds)
        {
            if (id == activeName)
            {
                themeIndex = row;
            }

            row++;
        }
    }

    if (m_shell.m_hwnd != nullptr && GetClientRect (m_shell.m_hwnd, &client))
    {
        m_chrome.m_toolbar.SetHostClientRect (client);
    }

    m_chrome.m_mainMenu.GetCommands().SetMachineDisplayName (std::wstring (m_shell.m_machine.GetConfig().name.begin(), m_shell.m_machine.GetConfig().name.end()));
    m_chrome.m_switchBar.SetMachineDisplayName (std::wstring (m_shell.m_machine.GetConfig().name.begin(), m_shell.m_machine.GetConfig().name.end()));
    m_chrome.m_mainMenu.GetCommands().SetFullscreen (m_shell.m_renderer->GetD3D().IsFullscreen());

    // An OPEN picker is mid-preview and owns its index; see RefreshToolbarThemeList.
    if (!m_chrome.m_toolbar.IsMenuOpen())
    {
        m_chrome.m_mainMenu.GetCommands().SetMonitorColorIndex (colorIndex);

        if (themeIndex >= 0)
        {
            m_chrome.m_mainMenu.GetCommands().SetThemeIndex (themeIndex);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellToolbarEntries::BuildToolbar
//
//  A toolbar click dispatches through the same command table as a menu row.
//  The volume flyout drives the master output gain and saves the level in
//  GlobalUserPrefs, deferred, since the slider reports every intermediate
//  value and a save per report would rewrite the prefs file on each step of
//  a drag.
//
////////////////////////////////////////////////////////////////////////////////

void ShellToolbarEntries::BuildToolbar (PrinterStatusLed & printerLed)
{
    GlobalUserPrefs &  prefs = m_shell.m_settings->GetPrefs();



    m_chrome.m_mainMenu.GetCommands().BuildToolbar (m_chrome.m_toolbar, printerLed, m_volumeFlyout);

    m_volumeFlyout.SetSink ([this] (float volume01, bool muted)
    {
        m_shell.m_settings->GetPrefs().masterVolume = volume01;
        m_shell.m_settings->GetPrefs().masterMuted  = muted;
        m_shell.m_audio->GetOutput().SetMasterGain (muted ? 0.0f : volume01);
        m_chrome.m_mainMenu.GetCommands().SetMuted (muted);
        m_shell.m_settings->SaveGlobalPrefsDeferred();
    });

    m_volumeFlyout.SetVolume (prefs.masterVolume, prefs.masterMuted);
    m_chrome.m_mainMenu.GetCommands().SetMuted (prefs.masterMuted);
    m_shell.m_audio->GetOutput().SetMasterGain (prefs.masterMuted ? 0.0f : prefs.masterVolume);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellToolbarEntries::WireToolbarPickers
//
//  The command bar's theme and monitor-color pickers, wired to the same
//  live-apply channels the Settings panel uses, so a pick has the same effect
//  in either.
//
//  A HIGHLIGHT PREVIEWS: it activates the theme (or the color treatment)
//  without writing anything down, because moving the pointer down a list is
//  not a choice. The toolbar replays that same sink with the row the list
//  opened on when the list is dismissed, which is what snaps the chrome and
//  the picture back.
//
//  A PICK COMMITS, and only a pick writes the prefs file: the theme into
//  GlobalUserPrefs, the color mode into the machine's UI prefs.
//
////////////////////////////////////////////////////////////////////////////////

void ShellToolbarEntries::WireToolbarPickers()
{
    m_chrome.m_toolbar.SetPopupHost (m_shell.m_host.get());

    //  A CLICK ON A MENU TITLE SWITCHES TO THAT MENU, rather than being spent
    //  closing the drop-down that was open. The drop-down has the mouse
    //  capture, so the click goes to it and not to the title, and every other
    //  application on the desktop moves from one menu to the next on it.
    //
    //  Only the titles. Anywhere else the click closes the picker and stops
    //  there, which is what a menu does everywhere: dismissing is not a
    //  reason to fire the button that happened to be underneath.
    m_chrome.m_toolbar.SetDropDownClickOutsideFn (
        [this] (POINT screenPx)
        {
            POINT  client = screenPx;

            if (m_shell.m_hwnd == nullptr || !ScreenToClient (m_shell.m_hwnd, &client))
            {
                return;
            }

            for (int i = 0; i < m_chrome.m_mainMenu.GetMenuCount(); i++)
            {
                RECT  title = m_chrome.m_mainMenu.GetMenuRect (i);

                if (PtInRect (&title, client))
                {
                    m_chrome.m_mainMenu.Open (i, false);
                    return;
                }
            }
        });

    m_chrome.m_toolbar.SetDropDownSinks (EmulatorCommands::kIdTheme,
        [this] (int index)
        {
            HRESULT  hrTheme = S_OK;
            bool     inRange = index >= 0 && index < (int) m_toolbarThemeIds.size();

            if (inRange)
            {
                hrTheme = m_shell.m_settings->ApplyThemeLive (m_toolbarThemeIds[index]);
                IGNORE_RETURN_VALUE (hrTheme, S_OK);
            }
        },
        [this] (int index)
        {
            HRESULT  hrTheme = S_OK;
            bool     inRange = index >= 0 && index < (int) m_toolbarThemeIds.size();

            if (inRange)
            {
                m_chrome.m_mainMenu.GetCommands().SetThemeIndex (index);
                hrTheme = m_shell.m_settings->ApplyAndPersistTheme (m_toolbarThemeIds[index]);
                IGNORE_RETURN_VALUE (hrTheme, S_OK);
            }
        });

    m_chrome.m_toolbar.SetDropDownSinks (EmulatorCommands::kIdColor,
        [this] (int index)
        {
            m_shell.m_renderer->SetColorModeLive (index);
        },
        [this] (int index)
        {
            m_chrome.m_mainMenu.GetCommands().SetMonitorColorIndex  (index);
            m_shell.m_renderer->SetColorModeLive           (index);
            m_shell.m_settings->PersistColorModeForMachine (index);
        });
}
