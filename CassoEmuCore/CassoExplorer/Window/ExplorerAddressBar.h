#pragma once

#include "Pch.h"

#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/CassoExplorerCommands.h"
#include "CassoExplorer/CassoExplorerNamedControl.h"
#include "CassoExplorer/CassoExplorerNewDiskDialog.h"
#include "CassoExplorer/ICassoExplorerHost.h"
#include "CassoExplorer/Model/FocusRing.h"
#include "CassoExplorer/Model/CassoExplorerPrefs.h"
#include "Config/IFileSystem.h"
#include "Seams/Win32HostDialogs.h"
#include "Seams/Win32IntentChannel.h"
#include "Seams/Win32ProcessLauncher.h"
#include "CassoExplorer/CrtPictureRenderer.h"
#include "Seams/Win32ContentDetails.h"
#include "Ui/Debugger/SourceSyntax.h"
#include "Ui/ThemeLoader.h"
#include "Config/CrtResolver.h"
#include "Config/CrtPresets.h"
#include "Seams/Win32ShellIcons.h"
#include "Seams/Win32InfoTips.h"
#include "Seams/Win32ShellItemVerbs.h"
#include "Seams/Win32PackagedCommands.h"
#include "Ui/Chrome/CassoTheme.h"
#include "CassoExplorer/Model/FolderWatch.h"
#include "CassoExplorer/Model/RefreshAnchor.h"

class CassoLinkConnections;
class FirewallMonitor;
class RunningCassoRegistry;
class ILinkAnnouncements;
struct CassoExplorerSessionWindow;
class CassoExplorerWindow;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar
//
//  The address bar and the search box: the path's segments and their
//  menus, typed paths and what they could go on to be, the history and
//  roots menus, and a search over the folder on display.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerAddressBar
{
public:
    explicit ExplorerAddressBar (CassoExplorerWindow & window);

    static constexpr const wchar_t *  kExplorerTypedPathsKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\TypedPaths";
    static constexpr size_t           kMaxCompletions        = 200;
    void  GoToLocation      (const Location & location);
    void  FillAddress();
    void  FillNavTips();
    std::wstring  GetNavLabel (const Location & location);
    std::vector<BrowserModel::AddressSegment>  GetAddressSegmentsFor (const Location & location);
    void  SubmitAddress (const std::wstring & text);
    void  AddTypedPath  (const std::wstring & text);
    void  ShowAddressMenu (int index, const RECT & anchor);
    void  ShowAddressOverflowMenu (const RECT & anchor);
    void  ShowAddressRootsMenu    (const RECT & anchor);
    void  SearchLocation          (const std::wstring & query);
    void  LeaveSearch             ();
    std::wstring  GetCassoSearchScope();
    bool  OnFindBoxKey            (const DxuiKeyEvent & ev);
    void  ShowAddressHistoryMenu  (const RECT & anchor);

    //  The list under the address bar: the history when nothing is typed,
    //  what the typed path could go on to be once something is.
    void  ShowAddressSuggestions  (const std::wstring & typed);
    bool  OnAddressKey            (WPARAM vk);
    std::vector<std::wstring>  GetAddressHistory () const;

    static std::vector<std::wstring>  GetCompletions            (const std::wstring & typed);
    void                              ShowAddressContextMenu    (int x, int y);
    static std::vector<std::wstring>  ReadExplorerTypedPaths    ();
    static void                       WriteExplorerTypedPath    (const std::wstring & path);
    void  ShowHistoryMenu (bool forward, const RECT & anchor);

    static std::wstring  EscapeMnemonics (const std::wstring & text);
    static BrowserModel::AddressRoot  GetProfileRoot();

private:
    friend class CassoExplorerWindow;

    CassoExplorerWindow  & m_window;
};
