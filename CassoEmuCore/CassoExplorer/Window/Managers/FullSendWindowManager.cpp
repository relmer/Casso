#include "Pch.h"

#include "CassoExplorer/Window/Managers/FullSendWindowManager.h"
#include "CassoExplorer/Window/Managers/ExplorerOpen.h"
#include "CassoExplorer/Window/Managers/ExplorerProperties.h"
#include "CassoExplorer/Window/Panes/ExplorerAddressBar.h"
#include "CassoExplorer/Window/Panes/ExplorerListPane.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "CassoExplorer/WozPropertiesDialog.h"
#include "CassoLink/FullSend/FullSendManifest.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendWindowManager::FullSendWindowManager
//
////////////////////////////////////////////////////////////////////////////////

FullSendWindowManager::FullSendWindowManager (ExplorerContext & context)
    : m_context (context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendWindowManager::~FullSendWindowManager
//
////////////////////////////////////////////////////////////////////////////////

FullSendWindowManager::~FullSendWindowManager() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendWindowManager::GetDefaultFullSendFolder
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FullSendWindowManager::GetDefaultFullSendFolder()
{
    wchar_t      * downloads = nullptr;
    std::wstring   folder;
    HRESULT        hr        = SHGetKnownFolderPath (FOLDERID_Downloads, 0, nullptr, &downloads);



    if (SUCCEEDED (hr))
    {
        folder = (std::filesystem::path (downloads) / L"Casso Full Send").wstring();
    }

    CoTaskMemFree (downloads);

    return folder;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendWindowManager::ApplyFullSendPrefs
//
//  The folder is made when a session starts, not here, so one never used is
//  never made.
//
////////////////////////////////////////////////////////////////////////////////

void FullSendWindowManager::ApplyFullSendPrefs()
{
    std::wstring  folder = GetFullSendFolder();



    m_context.browser.GetTreeModel().SetFullSendFolder (folder);

    if (m_context.app.links != nullptr)
    {
        CassoLinkFullSendOptions  & options = m_context.app.links->GetFullSendOptions();

        options.SetAppleBootSounds (m_context.prefs.bootSoundPlace == CassoExplorerPrefs::kBootSoundApple
                                    || m_context.prefs.bootSoundPlace == CassoExplorerPrefs::kBootSoundBoth);

        options.SetProtection (m_context.prefs.fullSendProtection);

        options.SetTarget (TextEncoding::WideToNarrow (folder), m_context.prefs.fullSendExtension, [] (const std::string & path)
        {
            std::error_code  error;

            std::filesystem::create_directories (TextEncoding::NarrowToWide (path), error);

            return error ? HRESULT_FROM_WIN32 (error.value()) : S_OK;
        });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendWindowManager::FillFullSendView
//
//  The view of the connection with a session in progress, or with none, of
//  one whose session ended. False, and the view as it was, when no
//  connection has either.
//
////////////////////////////////////////////////////////////////////////////////

bool FullSendWindowManager::FillFullSendView (FullSendPanel::View & ioView)
{
    bool  found   = false;
    bool  running = false;



    if (m_context.app.links == nullptr)
    {
        return false;
    }

    for (const LinkTreeEntry & entry : m_context.app.links->BuildTreeEntries())
    {
        CassoLinkSession  * session = m_context.app.links->GetSession (TextEncoding::WideToNarrow (entry.name));
        FullSendSnapshot    snapshot;

        if (session == nullptr || running)
        {
            continue;
        }

        snapshot = session->GetFullSendStatus().GetSnapshot();

        if (snapshot.activity == FullSendActivity::None)
        {
            continue;
        }

        running = FullSendDialog::IsRunning (snapshot);

        if (running || !found)
        {
            ioView.connection = entry.name;
            ioView.machine    = session->GetModelName();
            ioView.snapshot   = std::move (snapshot);
            found             = true;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendWindowManager::FillConnectionView
//
//  The view of a connection's session, in progress or ended. False, and the
//  view as it was, for a connection with neither.
//
////////////////////////////////////////////////////////////////////////////////

bool FullSendWindowManager::FillConnectionView (const std::wstring & connection, FullSendPanel::View & ioView)
{
    CassoLinkSession  * session = (m_context.app.links != nullptr) ? m_context.app.links->GetSession (TextEncoding::WideToNarrow (connection)) : nullptr;
    FullSendSnapshot    snapshot;



    if (session == nullptr)
    {
        return false;
    }

    snapshot = session->GetFullSendStatus().GetSnapshot();

    if (snapshot.activity == FullSendActivity::None)
    {
        return false;
    }

    ioView.connection = connection;
    ioView.machine    = session->GetModelName();
    ioView.snapshot   = std::move (snapshot);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendWindowManager::GetRunningViews
//
//  The view of each connection with a session in progress.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<FullSendPanel::View> FullSendWindowManager::GetRunningViews()
{
    std::vector<FullSendPanel::View>  views;



    if (m_context.app.links == nullptr)
    {
        return views;
    }

    for (const LinkTreeEntry & entry : m_context.app.links->BuildTreeEntries())
    {
        FullSendPanel::View  view;

        if (FillConnectionView (entry.name, view) && FullSendDialog::IsRunning (view.snapshot))
        {
            views.push_back (std::move (view));
        }
    }

    return views;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendWindowManager::OpenFullSendOnStart
//
//  A connection's Full Send window opens once for each session, in the
//  first window whose connections change after it starts, without taking
//  the keyboard focus.
//
////////////////////////////////////////////////////////////////////////////////

void FullSendWindowManager::OpenFullSendOnStart()
{
    static std::set<std::wstring>  s_opened;
    std::set<std::wstring>         running;



    for (const FullSendPanel::View & view : GetRunningViews())
    {
        running.insert (view.connection);
    }

    std::erase_if (s_opened, [&running] (const std::wstring & connection) { return !running.contains (connection); });

    for (const std::wstring & connection : running)
    {
        if (s_opened.insert (connection).second)
        {
            ShowConnectionWindow (connection, false);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendWindowManager::ShowFullSendWindow
//
////////////////////////////////////////////////////////////////////////////////

void FullSendWindowManager::ShowFullSendWindow (bool activate)
{
    std::vector<FullSendPanel::View>  running = GetRunningViews();
    FullSendPanel::View               ended;



    for (const FullSendPanel::View & view : running)
    {
        ShowConnectionWindow (view.connection, activate);
    }

    if (running.empty())
    {
        ShowConnectionWindow (FillFullSendView (ended) ? ended.connection : std::wstring(), activate);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendWindowManager::ShowConnectionWindow
//
//  A connection's Full Send window, or the folder's for an empty name, which
//  has the disks in the folder's manifest. A window opened while another is
//  open is placed down and to the right of the saved place, so neither
//  covers the other.
//
////////////////////////////////////////////////////////////////////////////////

void FullSendWindowManager::ShowConnectionWindow (const std::wstring & connection, bool activate)
{
    std::unique_ptr<FullSendDialog>  & window   = m_fullSendWindows[connection];
    POINT                              position = { m_context.prefs.fullSendWindow.x, m_context.prefs.fullSendWindow.y };
    const POINT                      * saved    = m_context.prefs.fullSendWindow.valid ? &position : nullptr;
    int                                step     = MulDiv (kCascadeDip, (int) GetDpiForWindow (m_context.window.GetHwnd()), USER_DEFAULT_SCREEN_DPI);



    if (window != nullptr && window->IsCreated())
    {
        window->ShowModelessDialog (IDCANCEL, activate);
        return;
    }

    for (const auto & [other, dialog] : m_fullSendWindows)
    {
        if (other != connection && dialog != nullptr && dialog->IsCreated() && IsWindowVisible (dialog->GetHwnd()))
        {
            position.x += step;
            position.y += step;
        }
    }

    if (connection.empty())
    {
        m_fullSendFilled = false;
    }

    window = FullSendDialog::Open (m_context.window.GetHwnd(), m_context.theme, activate,
                                   [this, connection] (FullSendPanel::View & ioView) { return connection.empty() ? FillFromManifest (ioView) : FillConnectionView (connection, ioView); },
                                   [this, connection]() { EndFullSend (connection); },
                                   [this]() { m_context.addressBar.GoToLocation (Location::MakeHostFolder (GetFullSendFolder())); },
                                   saved, connection);

    if (window == nullptr)
    {
        return;
    }

    window->SetDiskAction ([this] (const std::string & file, FullSendDialog::DiskAction action) { RunFullSendDisk (file, action); });
    window->SetOnMoved ([this] (POINT topLeft)
    {
        m_context.prefs.fullSendWindow.x     = topLeft.x;
        m_context.prefs.fullSendWindow.y     = topLeft.y;
        m_context.prefs.fullSendWindow.valid = true;
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendWindowManager::GetFullSendFolder
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FullSendWindowManager::GetFullSendFolder() const
{
    return m_context.prefs.fullSendFolder.empty() ? GetDefaultFullSendFolder() : m_context.prefs.fullSendFolder;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendWindowManager::IsFullSendRunning
//
////////////////////////////////////////////////////////////////////////////////

bool FullSendWindowManager::IsFullSendRunning()
{
    return !GetRunningViews().empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendWindowManager::FillFromManifest
//
//  With no session, the window has the disks in the Full Send folder's
//  manifest, read once while no session runs.
//
////////////////////////////////////////////////////////////////////////////////

bool FullSendWindowManager::FillFromManifest (FullSendPanel::View & ioView)
{
    HRESULT            hr     = S_OK;
    std::string        path   = TextEncoding::WideToNarrow (GetFullSendFolder()) + "\\" + FullSendManifest::kFileName;
    std::vector<Byte>  bytes;
    bool               filled = false;



    BAIL_OUT_IF (m_fullSendFilled, S_OK);

    m_fullSendFilled = true;

    hr = m_context.browser.GetOperations().GetFileIo().ReadAllBytes (path, bytes);
    CHR (hr);

    filled = FullSendPanel::MakeManifestView (bytes, ioView.snapshot);

    ioView.connection.clear();
    ioView.machine.clear();

Error:
    return filled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendWindowManager::RunFullSendDisk
//
//  The image is selected in the Full Send folder, then opened, inspected, or
//  opened in Disk properties, as from its menu there.
//
////////////////////////////////////////////////////////////////////////////////

void FullSendWindowManager::RunFullSendDisk (const std::string & file, FullSendDialog::DiskAction action)
{
    std::wstring  folder   = GetFullSendFolder();
    std::wstring  name     = TextEncoding::NarrowToWide (file);
    bool          selected = false;



    m_context.addressBar.GoToLocation (Location::MakeHostFolder (folder));
    m_context.listPane.FillList();

    selected = m_context.listPane.SelectRowNamed (name);

    //  A row's file renamed or deleted outside Casso Explorer is in the
    //  manifest under its old name.
    if (!selected)
    {
        m_context.window.ShowMessage (std::format (L"{} isn't in the Full Send folder. It may have been renamed or deleted outside Casso Explorer.", name), MB_ICONINFORMATION);
        return;
    }

    switch (action)
    {
        case FullSendDialog::DiskAction::Open:       m_context.opener.OpenSelectedEntries();                                                         break;
        case FullSendDialog::DiskAction::Inspect:    m_context.opener.InspectSelected();                                                             break;
        case FullSendDialog::DiskAction::Properties: m_context.properties.ShowWozProperties ((std::filesystem::path (folder) / name).wstring(), WozPropertiesDialog::kInfoTab); break;
        default:                                                                                                                                     break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendWindowManager::EndFullSend
//
//  The connection's session is canceled, as Cancel ends it, and its window
//  closed. A confirmation comes first only while a disk is being read, as
//  that disk isn't saved.
//
////////////////////////////////////////////////////////////////////////////////

void FullSendWindowManager::EndFullSend (const std::wstring & connection)
{
    FullSendPanel::View    view;
    CassoLinkSession     * session = nullptr;
    auto                   found   = m_fullSendWindows.find (connection);
    HWND                   owner   = (found != m_fullSendWindows.end() && found->second != nullptr) ? found->second->GetHwnd() : m_context.window.GetHwnd();



    if (!FillConnectionView (connection, view) || !FullSendDialog::IsRunning (view.snapshot))
    {
        return;
    }

    if (FullSendDialog::IsReadingDisk (view.snapshot)
        && DxuiMessageBox (owner, m_context.theme, L"End the Full Send session? A disk being read is not saved.", L"Full Send",
                           MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2, { L"End session", L"Cancel" }) != IDOK)
    {
        return;
    }

    session = m_context.app.links->GetSession (TextEncoding::WideToNarrow (connection));

    if (session != nullptr)
    {
        session->Cancel();
    }

    if (found != m_fullSendWindows.end() && found->second != nullptr)
    {
        PostMessageW (found->second->GetHwnd(), WM_CLOSE, 0, 0);
    }
}
