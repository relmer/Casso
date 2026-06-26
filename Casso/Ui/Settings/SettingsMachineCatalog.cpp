#include "Pch.h"

#include "SettingsMachineCatalog.h"

#include "MachinePage.h"
#include "ThemePage.h"

#include "../ThemeManager.h"
#include "../../EmulatorShell.h"
#include "../../AssetBootstrap.h"
#include "../../Config/UserConfigStore.h"
#include "../../Config/IFileSystem.h"
#include "../../Config/GlobalUserPrefs.h"

#include "Core/MachineScanner.h"
#include "Core/PathResolver.h"

#include "resource.h"


namespace fs = std::filesystem;


namespace
{
    std::string NarrowMachineName (const std::wstring & wideName)
    {
        std::string  narrowName;



        narrowName.reserve (wideName.size());
        for (wchar_t c : wideName)
        {
            narrowName.push_back ((char) (unsigned char) c);
        }
        return narrowName;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Bind
//
////////////////////////////////////////////////////////////////////////////////

void SettingsMachineCatalog::Bind (
    EmulatorShell      * emuShell,
    UserConfigStore    * ucs,
    GlobalUserPrefs    * prefs,
    IFileSystem        * fs,
    ThemeManager       * themes,
    SettingsPanelState * state,
    MachinePage        * machinePage,
    ThemePage          * themePage)
{
    m_emuShell    = emuShell;
    m_ucs         = ucs;
    m_prefs       = prefs;
    m_fs          = fs;
    m_themes      = themes;
    m_state       = state;
    m_machinePage = machinePage;
    m_themePage   = themePage;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadCurrentMachineIntoState
//
//  Re-snapshot the active machine's default JSON + user JSON into
//  the panel state so the panel always reflects whatever the shell
//  is presently emulating. Failures fall back silently to the prior
//  state (the panel still opens).
//
////////////////////////////////////////////////////////////////////////////////

void SettingsMachineCatalog::LoadCurrentMachineIntoState ()
{
    std::wstring                        machineNameW;
    std::string                         machineName;
    std::vector<std::filesystem::path>  searchPaths;
    std::filesystem::path               rel;
    std::filesystem::path               configPath;
    std::ifstream                       configFile;
    std::stringstream                   ss;
    std::string                         jsonText;
    JsonValue                           defaultJson;
    JsonValue                           mergedJson;
    JsonParseError                      parseErr;
    HRESULT                             hr = S_OK;



    if (m_emuShell == nullptr || m_ucs == nullptr || m_fs == nullptr || m_state == nullptr)
    {
        return;
    }

    machineNameW = m_emuShell->CurrentMachineName();
    machineName  = NarrowMachineName (machineNameW);
    if (machineName.empty())
    {
        return;
    }

    searchPaths = PathResolver::BuildSearchPaths (PathResolver::GetExecutableDirectory(),
                                                  PathResolver::GetWorkingDirectory());
    rel         = std::filesystem::path ("Machines") / machineName / (machineName + ".json");
    configPath  = PathResolver::FindFile (searchPaths, rel);
    if (configPath.empty())
    {
        return;
    }

    configFile.open (configPath);
    if (!configFile.good())
    {
        return;
    }
    ss << configFile.rdbuf();
    jsonText = ss.str();

    hr = JsonParser::Parse (jsonText, defaultJson, parseErr);
    if (FAILED (hr))
    {
        return;
    }

    hr = m_ucs->Load (machineName, defaultJson, *m_fs, mergedJson);
    if (FAILED (hr))
    {
        mergedJson = defaultJson;
    }

    hr = m_state->LoadFromMachine (machineName, defaultJson, mergedJson);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PopulateMachineList
//
////////////////////////////////////////////////////////////////////////////////

void SettingsMachineCatalog::PopulateMachineList ()
{
    std::vector<std::filesystem::path>  searchPaths;
    std::vector<MachineInfo>            machinesInfo;
    std::vector<std::string>            machineIds;
    std::vector<std::wstring>           displayNames;
    std::string                         activeMachine;
    int                                 activeIndex = -1;
    int                                 i           = 0;



    if (m_emuShell == nullptr || m_machinePage == nullptr)
    {
        return;
    }

    activeMachine = NarrowMachineName (m_emuShell->CurrentMachineName());
    searchPaths  = PathResolver::BuildSearchPaths (PathResolver::GetExecutableDirectory(),
                                                   PathResolver::GetWorkingDirectory());
    machinesInfo = MachineScanner::Scan (searchPaths,
                                         MachineScanner::ListDirectory,
                                         MachineScanner::ReadFile);

    for (const MachineInfo & info : machinesInfo)
    {
        std::string  machineId   = NarrowMachineName (info.fileName);
        std::wstring displayName = info.displayName.empty() ? std::wstring (info.fileName) : info.displayName;

        if (machineId == activeMachine)
        {
            activeIndex = i;
        }

        machineIds.push_back   (machineId);
        displayNames.push_back (std::move (displayName));
        i++;
    }

    if (machineIds.empty() && !activeMachine.empty())
    {
        machineIds.push_back (activeMachine);
        displayNames.emplace_back (activeMachine.begin(), activeMachine.end());
        activeIndex = 0;
    }

    m_machinePage->SetMachineList (std::move (machineIds), std::move (displayNames), activeIndex);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PopulateThemeList
//
//  Walks the ThemeManager's discovered themes, builds parallel id +
//  display-name vectors, and hands them to the theme page so the user
//  can pick from the live catalogue rather than a hardcoded list.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsMachineCatalog::PopulateThemeList ()
{
    std::vector<std::string>   themeIds;
    std::vector<std::wstring>  displayNames;
    std::string                activeName;
    int                        activeIndex = -1;
    int                        i           = 0;



    if (m_themes == nullptr || m_themePage == nullptr)
    {
        return;
    }

    activeName = m_themes->GetActiveThemeName();

    for (const LoadedTheme & t : m_themes->GetAvailableThemes())
    {
        if (t.name == activeName)
        {
            activeIndex = i;
        }
        themeIds.push_back (t.name);
        displayNames.emplace_back (t.name.begin(), t.name.end());
        i++;
    }

    if (themeIds.empty() && !activeName.empty())
    {
        themeIds.push_back (activeName);
        displayNames.emplace_back (activeName.begin(), activeName.end());
        activeIndex = 0;
    }

    m_themePage->SetThemes (std::move (themeIds), std::move (displayNames), activeIndex);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DoMachineSelect
//
//  Pre-flights ROM + Disk II audio for the target machine via
//  AssetBootstrap (which may surface modal download dialogs), then
//  posts IDM_FILE_OPEN with the new machine name to the emulator
//  shell's command queue. Returns false on user-cancel (Exit) or
//  bootstrap failure so the caller can leave the active machine alone.
//
////////////////////////////////////////////////////////////////////////////////

bool SettingsMachineCatalog::DoMachineSelect (const std::string & machineName)
{
    HRESULT           hr          = S_OK;
    std::wstring      wideName (machineName.begin(), machineName.end());
    HINSTANCE         hInstance   = (HINSTANCE) GetModuleHandleW (nullptr);
    HWND              hwndParent  = (m_emuShell != nullptr && m_emuShell->m_hwnd != nullptr)
                                        ? m_emuShell->m_hwnd
                                        : GetActiveWindow();
    std::vector<fs::path>  searchPaths;
    fs::path          assetBaseDir;
    std::string       bootstrapError;



    if (m_emuShell == nullptr || machineName.empty())
    {
        return false;
    }

    // Pre-flight: ensure ROMs and (if applicable) Disk II audio for
    // the target machine exist on disk before asking
    // MachineManager::SwitchMachine to load the config. Without this,
    // picking an uninstalled machine throws a "ROM file not found"
    // error dialog. Mirrors the unified startup flow in Main.cpp.
    searchPaths  = PathResolver::BuildSearchPaths (PathResolver::GetExecutableDirectory(),
                                                   PathResolver::GetWorkingDirectory());
    assetBaseDir = AssetBootstrap::GetAssetBaseDirectory();

    {
        bool          hasDisk            = false;
        std::string   hasDiskErr;
        HRESULT       hrHasDisk          = AssetBootstrap::HasDiskController (hInstance, wideName,
                                                                              hasDisk, hasDiskErr);
        IGNORE_RETURN_VALUE (hrHasDisk, S_OK);

        // Switch-machine flow: never auto-download a boot disk -- the
        // user explicitly picks the disk via File menu / settings.
        hr = AssetBootstrap::RunStartupDownloader (hInstance, wideName, hwndParent,
                                                   searchPaths, assetBaseDir, hasDisk,
                                                   *m_prefs, bootstrapError);

        // Audio consent may have been updated; flush prefs regardless.
        if (m_ucs != nullptr && m_fs != nullptr)
        {
            HRESULT  hrSave = m_ucs->SaveAll (*m_prefs, *m_fs);
            IGNORE_RETURN_VALUE (hrSave, S_OK);
        }
    }

    if (hr == S_FALSE)
    {
        // User chose Exit; leave the active machine alone.
        return false;
    }
    if (FAILED (hr))
    {
        std::wstring     wErr (bootstrapError.begin(), bootstrapError.end());
        DialogDefinition def  = {};

        def.title = L"Casso";
        def.icon  = DialogIcon::Error;
        def.body.push_back ({ std::format (L"Asset download failed:\n{}", wErr), false, L"" });
        def.buttons.push_back ({ L"OK", 0, true, true });
        (void) m_emuShell->ShowModalDialog (def);
        return false;
    }

    // SwitchMachine mutates CPU/bus/device state and MUST run on the
    // CPU thread (same as the File > Open Machine menu path). Posting
    // IDM_FILE_OPEN routes through CpuManager's command queue so the
    // teardown/recreate happens between CPU frames with no UI/CPU
    // race. Caller is expected to hide the panel before any subsequent
    // re-open.
    m_emuShell->PostCommand (IDM_FILE_OPEN, std::string (machineName));
    return true;
}
