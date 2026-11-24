#pragma once

#include "Pch.h"

#include "Seams/Win32ContextMenuLayout.h"
#include "Seams/Win32ExplorerCommandEntries.h"
#include "Seams/Win32PackageWatcher.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackagedCommands
//
//  The commands installed apps add to File Explorer's first-level context
//  menu: Open with Code, Edit in Notepad, PowerRename and the like, and the
//  commands Windows lists there itself: Send to My Phone, Run with
//  PowerShell, Add to Favorites, Compress to.
//
//  EACH APP DECLARES ITS COMMANDS IN ITS PACKAGE MANIFEST, by item type and
//  COM class, and File Explorer reads them from there, as this class does.
//  A command is an IExplorerCommand: GetTitle, GetIcon and GetState are
//  called against the items, and Invoke runs it on them.
//
//  Windows' own commands are either IExplorerCommand handlers in the shell's
//  command store or static verbs registered for the item's type, of which
//  only Windows' own few appear on File Explorer's first level.
//
//  The manifests are read on a worker thread at startup, and again whenever
//  a package is installed, updated or removed, or a file association
//  changes. After each read the worker creates every command once and
//  releases it, so the first menu doesn't load their DLLs. A menu creates a
//  command the first time it applies and reuses it until the next read;
//  creating one takes up to 60 ms, while its state and title take well under
//  one, so they are requested again for each menu.
//
////////////////////////////////////////////////////////////////////////////////

class Win32PackagedCommands
{
public:
    using MenuLayout = Win32ContextMenuLayout;

    ~Win32PackagedCommands();

    //  Starts reading the installed packages' manifests. The shell sends
    //  `assocMessage` to `notify` when a file association changes.
    void  Start (HWND notify, UINT assocMessage);

    //  Reads the manifests again, as after a package change; called when a
    //  file association changes.
    void  Refresh ();

    //  Returns the commands that apply to the items; empty until the
    //  manifests are read.
    std::vector<Win32ExplorerCommandEntries::Entry>  GetEntries (const std::vector<std::wstring> & paths, Win32ExplorerCommandEntries::ItemKind kind);

    //  Returns Windows' own commands for the items, in File Explorer's order.
    std::vector<Win32ExplorerCommandEntries::Entry>  GetWindowsEntries (const std::vector<std::wstring> & paths, Win32ExplorerCommandEntries::ItemKind kind);

    //  The shell items the commands are asked about in place of the paths: Home's rows, whose commands answer for them as they are listed there.
    void                SetItemIds        (std::vector<std::wstring> ids) { m_itemIds = std::move (ids); }

    //  Returns the Context menu page's choices, read from the registry for
    //  each menu once Start has run, and the defaults before then.
    MenuLayout  GetMenuLayout () const { return (m_notify != nullptr) ? Win32ContextMenuLayout::Read() : MenuLayout(); }

    HRESULT         Invoke         (HWND owner, const Win32ExplorerCommandEntries::Entry & entry, const std::vector<std::wstring> & paths);
    static HRESULT  InvokeGuarded  (IExplorerCommand * command, IShellItemArray * items);
    static HRESULT  SetSiteGuarded (IObjectWithSite * sited, IUnknown * site);

private:
    std::vector<std::wstring>  m_itemIds;
    struct Registration
    {
        std::wstring  itemType;     // "*", ".png", "Directory", "Directory\Background" or "Drive"
        CLSID         clsid = {};
        std::wstring  package;      // the package's full name
        std::wstring  appName;      // the package's display name
        std::wstring  sortName;     // the first application's display name, or empty
        std::wstring  logo;         // the package's small logo file, or empty
    };

    struct CachedCommand
    {
        CLSID                                       clsid = {};
        Microsoft::WRL::ComPtr<IExplorerCommand>    command;
    };

    void     ReadAll        ();
    HRESULT  GetCommand     (const CLSID & clsid, IExplorerCommand ** outCommand);

    static void  PrepareCommands (const std::vector<Registration> & registrations);
    static bool  IsDisconnected  (HRESULT hr);

    static HRESULT       OpenManifest      (const wchar_t * fullName, IStream ** outStream);
    static HRESULT       ReadManifest      (IStream * stream, const wchar_t * fullName, std::vector<Registration> & outRegistrations);
    static std::wstring  LoadAppName       (const wchar_t * fullName, const std::wstring & displayName);
    static std::wstring  FindLogoFile      (const wchar_t * fullName, const std::wstring & logo);
    static HRESULT       GetAttribute      (IXmlReader * reader, const wchar_t * attribute, std::wstring & outValue);
    static HRESULT       ParseClsid        (const std::wstring & text, CLSID & outClsid);
    static void          AddRegistration   (const Registration & registration, std::vector<Registration> & outRegistrations);
    static bool          Applies           (const Registration & registration, const std::vector<std::wstring> & paths, Win32ExplorerCommandEntries::ItemKind kind);
    static bool          IsInSyncRootOf    (const std::wstring & package, const std::wstring & path);
    static void          AddPackageEntries (const Registration & package, size_t applies, std::vector<Win32ExplorerCommandEntries::Entry> & own, std::vector<Win32ExplorerCommandEntries::Entry> & outEntries);
    static void          AddStoreEntry     (const wchar_t * name, const wchar_t * explorerIcon, IShellItemArray * items, std::vector<Win32ExplorerCommandEntries::Entry> & outEntries);
    static void          AddStaticVerbs    (const std::wstring & path, IShellItemArray * items, std::vector<Win32ExplorerCommandEntries::Entry> & outEntries);
    static HRESULT       AddStaticVerb     (const std::wstring & classKey, const wchar_t * verb, IShellItemArray * items, std::vector<Win32ExplorerCommandEntries::Entry> & outEntries);
    static HRESULT       AddStoreVerb      (const wchar_t * name, IShellItemArray * items, std::vector<Win32ExplorerCommandEntries::Entry> & outEntries);
    static HRESULT       ReadString        (HKEY root, const std::wstring & key, const wchar_t * value, std::wstring & outText);
    static bool          HasValue          (const std::wstring & key, const wchar_t * value);

    std::thread                 m_reader;
    std::mutex                  m_lock;
    bool                        m_read               = false;
    unsigned                    m_generation         = 0;        // advanced by each read of the manifests
    std::vector<Registration>   m_registrations;
    std::vector<CachedCommand>  m_commands;                      // used only on the thread that opens menus
    unsigned                    m_commandsGeneration = 0;
    HWND                        m_notify             = nullptr;
    Win32PackageWatcher         m_watcher;                       // last, so it stops before ReadAll's members are destroyed
};
