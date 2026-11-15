#include "Pch.h"

#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "CassoExplorer/CassoExplorerWindow.h"
#include "CassoExplorer/Model/CassoImagesListing.h"
#include "CassoExplorer/Model/KnownFolderStore.h"
#include "CassoExplorer/Picker/ExplorerPickerHost.h"
#include "CassoExplorer/Picker/PickerPrefs.h"
#include "Seams/Win32FolderWatcher.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerHost::ExplorerPickerHost
//
////////////////////////////////////////////////////////////////////////////////

ExplorerPickerHost::ExplorerPickerHost (IFileSystem & fs, IDiskFileIo & fileIo, std::wstring baseDir)
    : ExplorerPickerHost (fs, fileIo, std::move (baseDir), Services())
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerHost::ExplorerPickerHost
//
////////////////////////////////////////////////////////////////////////////////

ExplorerPickerHost::ExplorerPickerHost (IFileSystem & fs, IDiskFileIo & fileIo, std::wstring baseDir, Services services)
    : m_fs       (fs),
      m_fileIo   (fileIo),
      m_baseDir  (std::move (baseDir)),
      m_services (services)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerHost::~ExplorerPickerHost
//
//  The window first: it has references to the browser and the actions.
//
////////////////////////////////////////////////////////////////////////////////

ExplorerPickerHost::~ExplorerPickerHost()
{
    m_window.reset();
    m_actions.reset();
    m_browser.reset();
    m_watcher.reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerHost::Prepare
//
//  The tree has the Casso node's folders, as Casso Explorer's does.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPickerHost::Prepare (PickerRequest request, const std::string & theme, HWND owner, bool cascade)
{
    HRESULT                               hr = S_OK;
    KnownFolderStore                      store (m_fs, m_baseDir);
    std::vector<KnownFolderStore::Entry>  entries;



    m_request = std::move (request);
    m_outcome = PickerOutcome();
    m_owner   = owner;
    m_cascade = cascade;
    m_ended   = false;

    PickerPrefs::Load (m_baseDir, m_fs, m_request, theme, m_prefs);

    m_browser = std::make_unique<CassoExplorerBrowser> (m_fs, m_fileIo);
    m_actions = std::make_unique<CassoExplorerActions> (*m_browser, m_fs);

    hr = store.Load (entries);
    IGNORE_RETURN_VALUE (hr, S_OK);

    //  Before the window lists the tree: the stock disks' and demo disks'
    //  nodes are the Casso node's first children.
    m_browser->GetTreeModel().SetCassoLinkShown (false);
    m_browser->GetTreeModel().SetStockDisksShown (!m_request.stockDisks.empty());
    m_browser->SetStockDisks (m_request.stockDisks);
    m_browser->GetTreeModel().SetDemoDisksShown (!m_request.demoDisks.empty());
    m_browser->SetDemoDisks (m_request.demoDisks);
    m_browser->GetTreeModel().SetKnownFolders (KnownFolderStore::ListRootFolders (m_fs, m_baseDir, entries));
    m_browser->SetRecentLoads ([this] () { return CassoImagesListing::LoadRecent (m_fs, m_baseDir); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerHost::TryGetStartLocation
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPickerHost::TryGetStartLocation (const PickerRequest & request, const CassoExplorerPrefs & prefs, Location & outLocation)
{
    outLocation = Location();

    if (request.IsForDrive())
    {
        outLocation = Location::MakeRoot (TreeModel::kCassoRootId);
    }
    else if (!request.initialFolder.empty())
    {
        outLocation = Location::MakeHostFolder (request.initialFolder.wstring());
    }
    else if (!prefs.tabs.empty() && prefs.tabs[0].kind == Location::Kind::HostFolder)
    {
        outLocation = prefs.tabs[0];
    }

    return outLocation.kind != Location::Kind::None;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerHost::MakeContext
//
//  The CassoLink connections, the running Cassos, the announcements, the
//  firewall monitor and the cards are null in every picker.
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerWindowContext ExplorerPickerHost::MakeContext()
{
    CassoExplorerWindowContext  context;
    BrowserModel::Tab           tab;



    context.fs                = &m_fs;
    context.baseDir           = m_baseDir;
    context.host              = this;
    context.watcher           = m_watcher.get();
    context.pictures          = m_services.pictures;
    context.givenNames        = m_services.givenNames;
    context.picker            = &m_request;
    context.outcome           = &m_outcome;
    context.request.owner     = m_owner;
    context.request.placement = m_prefs.placement;
    context.request.cascade   = m_cascade;

    if (TryGetStartLocation (m_request, m_prefs, tab.location))
    {
        context.request.tabs.push_back (std::move (tab));
    }

    return context;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerHost::Open
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ExplorerPickerHost::Open (HINSTANCE instance)
{
    HRESULT  hr         = S_OK;
    HWND     foreground = GetForegroundWindow();
    bool     isInFront  = m_owner == nullptr || (foreground != nullptr && GetAncestor (foreground, GA_ROOTOWNER) == m_owner);



    CBRAEx (m_browser != nullptr && m_window == nullptr, E_UNEXPECTED);

    m_watcher = std::make_unique<Win32FolderWatcher>();

    m_browser->GetTreeModel().SetDrives (CassoExplorerShell::GetDriveRoots());
    m_browser->GetTreeModel().SetLocalizedNameLookup (&CassoExplorerShell::GetLocalizedFolderName);
    m_browser->GetTreeModel().SetDirectoryProbe ([] (const std::wstring & path)
    {
        DWORD  attributes = GetFileAttributesW (path.c_str());

        return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    });

    m_window = std::make_unique<CassoExplorerWindow> (*m_browser, *m_actions, m_prefs, MakeContext());

    //  A picker opened while another app is in front does not take the
    //  keyboard from it.
    hr = m_window->Open (instance, m_request.title, isInFront ? SW_SHOWNORMAL : SW_SHOWNOACTIVATE);
    CHR (hr);

Error:
    if (FAILED (hr))
    {
        m_window.reset();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerHost::RunModal
//
//  The owner is enabled again before the picker's window is hidden, so the
//  owner, not some other app's window, is activated when it closes.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ExplorerPickerHost::RunModal (HINSTANCE instance)
{
    HRESULT  hr      = S_OK;
    MSG      message = {};
    BOOL     got     = TRUE;



    hr = Open (instance);
    CHR (hr);

    m_disabledOwner = m_owner != nullptr && IsWindowEnabled (m_owner);

    if (m_disabledOwner)
    {
        EnableWindow (m_owner, FALSE);
    }

    while (!m_outcome.isClosed)
    {
        got = GetMessageW (&message, nullptr, 0, 0);

        //  A quit is for the loop this one runs inside.
        if (got == 0)
        {
            PostQuitMessage ((int) message.wParam);
            break;
        }

        if (got == -1)
        {
            break;
        }

        TranslateMessage (&message);
        DispatchMessageW (&message);
    }

    if (!m_outcome.isClosed)
    {
        Close();
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerHost::GetHwnd
//
////////////////////////////////////////////////////////////////////////////////

HWND ExplorerPickerHost::GetHwnd() const
{
    return (m_window != nullptr) ? m_window->GetHwnd() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerHost::BringToFront
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPickerHost::BringToFront()
{
    if (IsOpen())
    {
        CassoExplorerShell::FrontWindow (m_window->GetHwnd());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerHost::Close
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPickerHost::Close()
{
    if (IsOpen())
    {
        m_window->CloseWindow();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerHost::SaveView
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ExplorerPickerHost::SaveView()
{
    if (m_browser != nullptr && !m_request.IsForDrive())
    {
        m_prefs.tabs = { m_browser->GetLocation() };
    }

    return PickerPrefs::Save (m_baseDir, m_fs, m_request, m_prefs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerHost::End
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPickerHost::End()
{
    HRESULT  hr = S_OK;



    if (m_ended)
    {
        return;
    }

    m_ended = true;

    if (m_window != nullptr)
    {
        m_prefs.placement = m_window->GetPlacement();
    }

    hr = SaveView();
    IGNORE_RETURN_VALUE (hr, S_OK);

    if (m_disabledOwner)
    {
        EnableWindow (m_owner, TRUE);
        m_disabledOwner = false;
    }

    if (m_window != nullptr)
    {
        m_window->CloseForApp();
    }

    m_outcome.isClosed = true;

    if (m_onClosed)
    {
        m_onClosed();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerHost::OpenWindow
//
//  No other window is opened from a picker.
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerWindow * ExplorerPickerHost::OpenWindow (CassoExplorerWindowRequest request)
{
    (void) request;

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerHost::GetWindowFor
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerWindow * ExplorerPickerHost::GetWindowFor (HWND hwnd)
{
    return (IsOpen() && hwnd != nullptr && m_window->GetHwnd() == hwnd) ? m_window.get() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerHost::ForEachWindow
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPickerHost::ForEachWindow (const std::function<void (CassoExplorerWindow & window)> & fn)
{
    if (IsOpen())
    {
        fn (*m_window);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerHost::CloseWindow
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPickerHost::CloseWindow (CassoExplorerWindow & window)
{
    if (&window == m_window.get())
    {
        End();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerHost::ExitAll
//
//  Windows is signing out: the picker ends with nothing confirmed.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPickerHost::ExitAll()
{
    End();
}
