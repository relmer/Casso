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
//  New's choices are the shell's own New menu, read once on a thread of their
//  own, since the shell's handler for it takes a few hundred milliseconds to
//  list them. Choosing one creates a fresh copy of the handler for the folder
//  and runs the choice there, as File Explorer runs it.
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

    //  One of the shell's New choices. The verb is the shell's for it: a
    //  file type's extension, such as ".txt", or NewFolder or NewLink.
    struct NewChoice
    {
        std::wstring  title;
        std::wstring  verb;
    };

    static constexpr const wchar_t *  kNewFolderVerb = L"NewFolder";
    static constexpr const wchar_t *  kNewLinkVerb   = L"NewLink";

    ~Win32PackagedCommands();

    //  Starts reading the installed packages' manifests and New's choices,
    //  and posts `message` to `notify` once the choices are read.
    void  Start (HWND notify, UINT message);

    //  Returns the commands that apply to the items; empty until the
    //  manifests are read.
    std::vector<Entry>  GetEntries (const std::vector<std::wstring> & paths, ItemKind kind);

    //  Returns Windows' own commands for the items, in File Explorer's order.
    std::vector<Entry>  GetWindowsEntries (const std::vector<std::wstring> & paths, ItemKind kind);

    //  Returns Start's Pin or Unpin command for one item, with the title from
    //  the shell's handler; false when the handler has neither.
    bool  GetStartPinEntry (const std::wstring & path, Entry & outEntry);

    //  Returns New's choices, in the shell's order; empty until read.
    std::vector<NewChoice>  GetNewChoices ();

    //  Runs a New choice in the folder and returns the path of the item it
    //  created, or an empty path when none can be found.
    HRESULT  CreateNew (HWND owner, const std::wstring & folder, const std::wstring & verb, std::wstring & outPath);

    HRESULT  Invoke (HWND owner, const Entry & entry, const std::vector<std::wstring> & paths);

private:
    struct Registration
    {
        std::wstring  itemType;     // "*", ".png", "Directory", "Directory\Background" or "Drive"
        CLSID         clsid = {};
        std::wstring  package;      // the package's full name
        std::wstring  appName;      // the package's display name
        std::wstring  sortName;     // the first application's display name, or empty
        std::wstring  logo;         // the package's small logo file, or empty
    };

    static constexpr int   s_kMaxDepth          = 2;
    static constexpr UINT  s_kFirstNewCommandId = 1;
    static constexpr UINT  s_kLastNewCommandId  = 0x7FFF;

    void  ReadAll ();
    void  ReadNewChoices ();

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
    static HRESULT  AddStoreVerb     (const wchar_t * name, IShellItemArray * items, std::vector<Entry> & outEntries);
    static HRESULT  ReadString       (HKEY root, const std::wstring & key, const wchar_t * value, std::wstring & outText);
    static bool     HasValue         (const std::wstring & key, const wchar_t * value);
    static HRESULT  OpenNewMenu      (const std::wstring & folder, IContextMenu3 ** outMenu, HMENU & outPopup, HMENU & outChoices);
    static std::wstring  GetMenuVerb (IContextMenu * menu, UINT id);

    std::thread                 m_reader;
    std::thread                 m_newReader;
    std::mutex                  m_lock;
    bool                        m_read          = false;
    std::vector<Registration>   m_registrations;
    std::vector<NewChoice>      m_newChoices;
    HWND                        m_notify        = nullptr;
    UINT                        m_notifyMessage = 0;
};
