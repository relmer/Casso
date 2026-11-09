#include "Pch.h"

#include "CassoExplorer/Window/Managers/ExplorerCassoTarget.h"
#include "CassoExplorer/Window/ExplorerContext.h"
#include "CassoExplorer/Window/IExplorerWindow.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "CassoExplorer/Model/CassoTargeting.h"
#include "CassoExplorer/Model/KnownFolderStore.h"
#include "CassoExplorer/Model/LaunchCommand.h"
#include "AssetBootstrap.h"
#include "Config/UserConfigStore.h"
#include "Core/JsonWriter.h"
#include "Core/MachineScanner.h"
#include "Core/TextEncoding.h"
#include "Core/AppMessages.h"
#include "Seams/Win32IntentChannel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerCassoTarget::ExplorerCassoTarget
//
////////////////////////////////////////////////////////////////////////////////

ExplorerCassoTarget::ExplorerCassoTarget (ExplorerContext & context)
    : m_context (context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerCassoTarget::GetSelectedImagePath
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ExplorerCassoTarget::GetSelectedImagePath() const
{
    std::vector<std::wstring>  paths;



    m_context.browser.GetSelectedHostPaths (paths);

    return (paths.size() == 1) ? paths[0] : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerCassoTarget::InsertIntoDrive
//
//  The Casso that launched this browser when it is still there, otherwise
//  any running Casso, otherwise a new one with the disk in drive 1. The
//  answer arrives later as a reply message.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerCassoTarget::InsertIntoDrive (const std::wstring & imagePath, int drive)
{
    HWND  target = nullptr;
    bool  sent   = false;



    if (imagePath.empty())
    {
        return;
    }

    target = FindCassoTarget();

    if (target == nullptr)
    {
        OpenInNewCasso (imagePath);
        return;
    }

    sent = Win32IntentChannel::SendTo (target, m_context.window.GetHwnd(), Win32IntentChannel::GetMessageId(),
                                       Win32IntentChannel::EncodeInsert (TextEncoding::WideToNarrow (imagePath), drive));

    if (!sent)
    {
        m_context.window.ShowMessage (L"Casso did not answer the request to insert the disk.", MB_ICONWARNING);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerCassoTarget::OpenInNewCasso
//
//  The folder is recorded here, since the new Casso learns of the disk from
//  its command line rather than from a hand-off it would record itself.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerCassoTarget::OpenInNewCasso (const std::wstring & imagePath)
{
    wchar_t       module[MAX_PATH] = {};
    DWORD         length           = GetModuleFileNameW (nullptr, module, ARRAYSIZE (module));
    std::wstring  exe;
    HRESULT       hr               = S_OK;



    if (imagePath.empty() || length == 0 || length >= ARRAYSIZE (module))
    {
        return;
    }

    exe = LaunchCommand::GetSiblingPath (std::filesystem::path (module).parent_path().wstring(), LaunchCommand::kCassoExe);

    if (!m_launcher.Exists (exe))
    {
        m_context.window.ShowMessage (LaunchCommand::DescribeMissing (exe), MB_ICONERROR);
        return;
    }

    hr = m_launcher.Launch (exe, LaunchCommand::MakeCassoArguments (imagePath, m_context.app.titlePrefix));

    if (FAILED (hr))
    {
        m_context.window.ShowMessage (L"Casso could not be started.", MB_ICONERROR);
        return;
    }

    if (m_context.app.fs != nullptr)
    {
        KnownFolderStore  store (*m_context.app.fs, m_context.app.baseDir);

        hr = store.Append (std::filesystem::path (imagePath).parent_path().wstring(), (int64_t) _time64 (nullptr));
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerCassoTarget::FindCassoTarget
//
//  The Casso that launched this browser while it is still there, otherwise
//  any running Casso, otherwise none.
//
////////////////////////////////////////////////////////////////////////////////

HWND ExplorerCassoTarget::FindCassoTarget() const
{
    std::vector<HWND>  running;
    HWND               window  = nullptr;
    CassoTarget        target;



    //  Top-level windows come back front to back, so the first Casso found
    //  is the one most recently active.
    while ((window = FindWindowExW (nullptr, window, Win32IntentChannel::kWindowClass, nullptr)) != nullptr)
    {
        running.push_back (window);
    }

    target = CassoTargeting::Choose (m_context.app.request.owner, m_context.app.request.owner != nullptr && IsWindow (m_context.app.request.owner) != FALSE,
                                     running, MachineConfig());

    return target.hwnd;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerCassoTarget::GetDefaultMachineDriveCount
//
//  The drives of the machine a new Casso would start, chosen as Casso chooses
//  it: the machine last selected, else the Apple //e. Read once, from its
//  configuration alone; its ROMs are not needed to count its drives. Both
//  drives when the configuration cannot be read.
//
////////////////////////////////////////////////////////////////////////////////

int ExplorerCassoTarget::GetDefaultMachineDriveCount()
{
    constexpr std::wstring_view  s_kPreferredDefaultMachine = L"Apple2e";
    HRESULT                hr          = S_OK;
    GlobalUserPrefs        prefs;
    Win32FileSystem        files;
    std::wstring           machine;
    std::vector<fs::path>  searchPaths;
    fs::path               configPath;
    std::string            jsonText;
    std::string            error;
    MachineConfig          config;
    int                    drives      = 0;



    BAIL_OUT_IF (m_defaultDriveCount >= 0, S_OK);

    m_defaultDriveCount = Win32IntentChannel::kMaxDriveCount;

    hr = prefs.Load (AssetBootstrap::GetAssetBaseDirectory().wstring(), files);
    IGNORE_RETURN_VALUE (hr, S_OK);

    searchPaths = PathResolver::BuildSearchPaths (PathResolver::GetExecutableDirectory(), PathResolver::GetWorkingDirectory());
    machine     = TextEncoding::NarrowToWide (prefs.lastSelectedMachine);
    machine     = MachineScanner::SelectCanonical (MachineScanner::Scan (searchPaths, &MachineScanner::ListDirectory, &MachineScanner::ReadFile),
                                                   machine, s_kPreferredDefaultMachine);
    configPath  = PathResolver::FindFile (searchPaths, fs::path ("Machines") / fs::path (machine) / (fs::path (machine).string() + ".json"));
    BAIL_OUT_IF (configPath.empty(), S_OK);

    hr = files.ReadAllText (configPath.wstring(), jsonText);
    CHR (hr);

    hr = MergeUserMachineDelta (TextEncoding::WideToNarrow (machine), files, jsonText);
    IGNORE_RETURN_VALUE (hr, S_OK);

    //  Only the slots are read here, never a ROM's bytes, so a ROM that is
    //  missing stands in as the config file itself rather than stopping the
    //  slots from being read; one that is there is found where Casso finds it.
    hr = MachineConfigLoader::Load (jsonText, fs::path (machine).string(), searchPaths,
                                    [&configPath] (const std::vector<fs::path> & paths, const fs::path & relative)
                                    {
                                        fs::path  found = PathResolver::FindFile (paths, relative);

                                        return found.empty() ? configPath : found;
                                    },
                                    config, error);
    CHR (hr);

    drives = config.AttachedDiskIiDriveCount();

    if (drives > 0)
    {
        m_defaultDriveCount = drives;
    }

Error:
    return m_defaultDriveCount;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerCassoTarget::MergeUserMachineDelta
//
//  The user's changes to the machine, such as a drive detached in Casso's
//  settings, merged into its configuration as Casso merges them. The prefs
//  file is only read: a delta Casso would migrate is merged as it is, which
//  is also Casso's own fallback when a migration fails.
//
//  A machine's configuration with the user's changes to it from Casso's
//  prefs file merged in, as Casso merges them; the file is only read.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ExplorerCassoTarget::MergeUserMachineDelta (const std::string & machine, IFileSystem & files, std::string & inoutJson)
{
    HRESULT            hr       = S_OK;
    UserConfigStore    store (AssetBootstrap::GetAssetBaseDirectory().wstring());
    std::string        prefsText;
    JsonValue          base;
    JsonValue          prefs;
    JsonParseError     error;
    const JsonValue  * machines = nullptr;
    const JsonValue  * delta    = nullptr;



    hr = files.ReadAllText (store.GetUserPrefsFilePath(), prefsText);
    CHR (hr);

    hr = JsonParser::Parse (prefsText, prefs, error);
    CHR (hr);

    hr = JsonParser::Parse (inoutJson, base, error);
    CHR (hr);

    BAIL_OUT_IF (!prefs.HasObject ("machines", machines), S_OK);
    BAIL_OUT_IF (!machines->HasObject (machine, delta), S_OK);

    inoutJson = JsonWriter::Write (UserConfigStore::MergeJson (base, *delta));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerCassoTarget::AskCassoToDescribe
//
//  The answer arrives as a reply message and updates the drive count the
//  menus read. With no Casso running, a new one opens with the default
//  machine, so its configuration says how many drives there are.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerCassoTarget::AskCassoToDescribe()
{
    HWND  target = FindCassoTarget();
    bool  sent   = false;



    if (target == nullptr)
    {
        m_cassoDriveCount = GetDefaultMachineDriveCount();
        return;
    }

    sent = Win32IntentChannel::SendTo (target, m_context.window.GetHwnd(), Win32IntentChannel::GetMessageId(), Win32IntentChannel::EncodeDescribe());
    IGNORE_RETURN_VALUE (sent, true);
}
