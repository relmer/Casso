#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/BrowserModel.h"
#include "CassoExplorer/Model/Location.h"
#include "CassoExplorer/Model/TypedPathHistory.h"

class HostFolderReader;
class IBrowserListing;
class IFileSystem;
class IShellItemVerbs;





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserNavigator
//
//  The browser's tabs and where each one goes: opening, closing and
//  switching tabs, Back, Forward and Up, and a typed address. Each move
//  loads the list again through IBrowserListing.
//
////////////////////////////////////////////////////////////////////////////////

class BrowserNavigator
{
public:
    BrowserNavigator (BrowserModel & model, IFileSystem & fs, HostFolderReader & hostFolders, IBrowserListing & listing);

    //  Tabs. A new tab opens at Home. The last tab does not close, since the
    //  window always lists somewhere. Switching restores the tab's selection
    //  by name. Restoring opens one tab per location and makes the first
    //  active; with an empty list, the tabs are not changed.
    size_t  NewTab      ();
    bool    CloseTab    (size_t index);
    bool    MoveTab     (size_t from, size_t to)                          { return m_model.MoveTab (from, to); }
    bool    SwitchTab   (size_t index);
    void    RestoreTabs (const std::vector<Location> & locations);
    void    RestoreTabs (std::vector<BrowserModel::Tab> tabs, size_t active);

    //  A tab arriving from another window goes in at `index`, history and view
    //  included, and becomes the active tab.
    size_t  InsertTab   (size_t index, BrowserModel::Tab tab);

    //  Opens a location in a new tab and switches to it. The tab menu's other
    //  commands: a copy of a tab where it is, and closing every tab but one or
    //  every tab after one. The last tab never closes.
    size_t  OpenInNewTab     (const Location & location);
    size_t  DuplicateTab     (size_t index);
    bool    CloseOtherTabs   (size_t index);
    bool    CloseTabsToRight (size_t index);

    //  Where Up goes from the active tab, for its tooltip and for GoUp.
    bool  CanGoUp        () const;
    bool  TryGetUpTarget (Location & outTarget) const;
    bool  GoUp           ();
    bool  GoBack         ();
    bool  GoForward      ();

    //  Several steps at once, as a pick from the Back or Forward list takes.
    bool  GoBackBy    (size_t steps);
    bool  GoForwardBy (size_t steps);

    //  Navigates the active tab to a location, as a click on an address bar
    //  segment does, or to a typed path. False, with nothing changed, for a
    //  path that is not a folder, an image or a directory inside one.
    void  NavigateToLocation (const Location & location);
    bool  NavigateToAddress  (const std::wstring & text);

    //  The paths typed into the address bar, added once a typed path's
    //  navigation succeeds. Seeded from the stored copy at startup.
    TypedPathHistory        & GetTypedPaths ()                             { return m_typedPaths; }
    const TypedPathHistory  & GetTypedPaths () const                       { return m_typedPaths; }

    void  SetShellVerbs (IShellItemVerbs * verbs)                          { m_shellVerbs = verbs; }

    //  A typed path with a root and no drive, put on the system drive.
    static std::wstring  RootOnSystemDrive (const std::wstring & text);

    //  A typed path with its environment variables expanded, as Explorer
    //  reads "%TEMP%". An undefined variable stays as typed.
    static std::wstring  ExpandVariables   (const std::wstring & text);

private:
    Location  GetLocation         () const                                 { return m_model.HasTabs() ? m_model.GetActiveTab().location : Location(); }
    void      ReloadWithSelection ();

    BrowserModel      & m_model;
    IFileSystem       & m_fs;
    HostFolderReader  & m_hostFolders;
    IBrowserListing   & m_listing;
    IShellItemVerbs   * m_shellVerbs = nullptr;
    TypedPathHistory    m_typedPaths;
};
