#pragma once

#include "Pch.h"





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
//  The manifests are read once on a worker thread, and the commands are
//  created when a menu opens, as File Explorer creates them.
//
////////////////////////////////////////////////////////////////////////////////

class Win32PackagedCommands
{
public:
    //  The kind of item a registration applies to.
    enum class ItemKind { File, Folder, Background, Drive };

    struct Entry
    {
        std::wstring                                title;
        std::wstring                                icon;       // a "path,index" resource, or empty
        bool                                        enabled = true;
        Microsoft::WRL::ComPtr<IExplorerCommand>    command;
        std::string                                 verb;           // a static verb, invoked by name, when there is no command
        std::wstring                                explorerIcon;   // File Explorer's own icon for one of Windows' commands
        std::vector<Entry>                          children;
    };

    ~Win32PackagedCommands();

    //  Starts reading the installed packages' manifests.
    void  Start ();

    //  Returns the commands that apply to the items; empty until the
    //  manifests are read.
    std::vector<Entry>  GetEntries (const std::vector<std::wstring> & paths, ItemKind kind);

    //  Returns Windows' own commands for the items, in File Explorer's order.
    std::vector<Entry>  GetWindowsEntries (const std::vector<std::wstring> & paths, ItemKind kind);

    HRESULT  Invoke (HWND owner, const Entry & entry, const std::vector<std::wstring> & paths);

private:
    struct Registration
    {
        std::wstring  itemType;     // "*", ".png", "Directory", "Directory\Background" or "Drive"
        CLSID         clsid = {};
        std::wstring  package;      // the package's full name
        std::wstring  appName;      // the package's display name
        std::wstring  logo;         // the package's small logo file, or empty
    };

    static constexpr int  s_kMaxDepth = 2;

    void  ReadAll ();

    static HRESULT  OpenManifest     (const wchar_t * fullName, IStream ** outStream);
    static HRESULT  ReadManifest     (IStream * stream, const wchar_t * fullName, std::vector<Registration> & outRegistrations);
    static std::wstring  LoadAppName        (const wchar_t * fullName, const std::wstring & displayName);
    static std::wstring  FindLogoFile       (const wchar_t * fullName, const std::wstring & logo);
    static std::wstring  LoadIndirectString (const std::wstring & text);
    static HRESULT  GetAttribute     (IXmlReader * reader, const wchar_t * attribute, std::wstring & outValue);
    static HRESULT  ParseClsid       (const std::wstring & text, CLSID & outClsid);
    static void     AddRegistration  (const Registration & registration, std::vector<Registration> & outRegistrations);
    static bool     Applies          (const Registration & registration, const std::vector<std::wstring> & paths, ItemKind kind);
    static HRESULT  MakeItems        (const std::vector<std::wstring> & paths, IShellItemArray ** outItems);
    static void     AddEntry         (IExplorerCommand * command, IShellItemArray * items, int depth, const std::wstring & fallbackTitle,
                                      const std::wstring & fallbackIcon, std::vector<Entry> & outEntries);
    static void     AddPackageEntries (const Registration & package, size_t applies, std::vector<Entry> & own, std::vector<Entry> & outEntries);
    static void     AddStoreEntry    (const wchar_t * name, const wchar_t * explorerIcon, IShellItemArray * items, std::vector<Entry> & outEntries);
    static void     AddStaticVerbs   (const std::wstring & path, IShellItemArray * items, std::vector<Entry> & outEntries);
    static HRESULT  AddStaticVerb    (const std::wstring & classKey, const wchar_t * verb, IShellItemArray * items, std::vector<Entry> & outEntries);
    static HRESULT  ReadString       (HKEY root, const std::wstring & key, const wchar_t * value, std::wstring & outText);
    static bool     HasValue         (const std::wstring & key, const wchar_t * value);

    std::thread                 m_reader;
    std::mutex                  m_lock;
    bool                        m_read = false;
    std::vector<Registration>   m_registrations;
};
