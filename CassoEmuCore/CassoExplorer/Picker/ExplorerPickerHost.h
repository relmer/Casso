#pragma once

#include "Pch.h"

#include "CassoExplorer/ICassoExplorerHost.h"
#include "CassoExplorer/Model/CassoExplorerPrefs.h"
#include "CassoExplorer/Picker/PickerRequest.h"
#include "CassoExplorer/Window/ExplorerContext.h"

class CassoExplorerActions;
class CassoExplorerBrowser;
class GivenNames;
class IDiskFileIo;
class IDiskPictures;
class IFolderWatcher;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerHost
//
//  One picker: a Casso Explorer window in picker mode, created in the
//  process the request came from, with its browser, its preferences and its
//  request. It is apart from CassoExplorerShell: it has no session, no
//  CassoLink connections, no cards and no other windows, and its window's
//  class is not one a Casso Explorer launch is forwarded to.
//
//  Prepare builds everything but the window. Open creates the window and
//  makes it visible. A confirm or a close ends the picker: the view is saved, the
//  window is hidden, and the closed callback is called from inside the
//  window's message, so the host is released after that message returns.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerPickerHost : public ICassoExplorerHost
{
public:
    //  Optional services of the process the picker is in.
    struct Services
    {
        IDiskPictures  * pictures   = nullptr;
        GivenNames     * givenNames = nullptr;
    };

    ExplorerPickerHost (IFileSystem & fs, IDiskFileIo & fileIo, std::wstring baseDir);
    ExplorerPickerHost (IFileSystem & fs, IDiskFileIo & fileIo, std::wstring baseDir, Services services);
    ~ExplorerPickerHost() override;

    static constexpr const wchar_t *  kWindowClass      = L"CassoPickerWindow";
    static constexpr int              kDefaultWidthDip  = 1040;
    static constexpr int              kDefaultHeightDip = 660;

    //  `theme` is one of CassoExplorerPrefs' themes. `cascade` steps
    //  the window down and right of its saved place, for a second picker
    //  with the same view key.
    void     Prepare (PickerRequest request, const std::string & theme, HWND owner, bool cascade);
    HRESULT  Open    (HINSTANCE instance);

    //  The modal form: opens the picker with its owner disabled and runs
    //  the thread's messages until the picker has ended. The result is in
    //  the outcome.
    HRESULT  RunModal (HINSTANCE instance);

    bool     IsOpen       () const { return m_window != nullptr && !m_outcome.isClosed; }
    HWND     GetHwnd      () const;
    void     BringToFront ();

    //  As the close box: nothing is confirmed.
    void     Close        ();

    //  Called once, when the picker has ended.
    void     SetOnClosed  (std::function<void ()> onClosed) { m_onClosed = std::move (onClosed); }

    const PickerRequest       & GetRequest () const { return m_request; }
    const PickerOutcome       & GetOutcome () const { return m_outcome; }
    const CassoExplorerPrefs  & GetPrefs   () const { return m_prefs; }
    CassoExplorerBrowser      & GetBrowser ()       { return *m_browser; }
    CassoExplorerWindow       * GetWindow  ()       { return m_window.get(); }

    //  What the window is created with: the request and its starting
    //  location. The CassoLink connections, the running Cassos, the
    //  announcements, the firewall monitor and the cards are null.
    CassoExplorerWindowContext  MakeContext ();

    //  The location the request starts on: the Casso node's list for a
    //  drive, else the request's folder, else the folder last picked from.
    static bool  TryGetStartLocation (const PickerRequest & request, const CassoExplorerPrefs & prefs, Location & outLocation);

    HRESULT  SaveView ();

    //  ICassoExplorerHost
    CassoExplorerWindow  * OpenWindow          (CassoExplorerWindowRequest request) override;
    CassoExplorerWindow  * GetWindowFor        (HWND hwnd) override;
    void                   ForEachWindow       (const std::function<void (CassoExplorerWindow & window)> & fn) override;
    void                   CloseWindow         (CassoExplorerWindow & window) override;
    void                   ExitAll             () override;
    void                   SaveSession         () override {}
    void                   RunLaunch           (const std::wstring & arguments) override { (void) arguments; }
    void                   CheckForUpdates     (HWND hwnd) override { (void) hwnd; }
    bool                   OfferMouseToUpdater (HWND hwnd, const DxuiMouseEvent & ev) override { (void) hwnd; (void) ev; return false; }

private:
    void  End ();

    IFileSystem                            & m_fs;
    IDiskFileIo                            & m_fileIo;
    std::wstring                             m_baseDir;
    Services                                 m_services;
    PickerRequest                            m_request;
    PickerOutcome                            m_outcome;
    CassoExplorerPrefs                       m_prefs;
    HWND                                     m_owner         = nullptr;
    bool                                     m_cascade       = false;
    bool                                     m_ended         = false;
    bool                                     m_disabledOwner = false;   // a modal run disabled the owner, which End enables again
    bool                                     m_isModal       = false;
    std::function<void ()>                   m_onClosed;
    std::unique_ptr<IFolderWatcher>          m_watcher;
    std::unique_ptr<CassoExplorerBrowser>    m_browser;
    std::unique_ptr<CassoExplorerActions>    m_actions;
    std::unique_ptr<CassoExplorerWindow>     m_window;
};
