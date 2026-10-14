#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackagedCommands
//
//  The commands installed apps add to File Explorer's first-level context
//  menu: Open with Code, Edit in Notepad, PowerRename and the like.
//
//  EACH APP DECLARES ITS COMMANDS IN ITS PACKAGE MANIFEST, by item type and
//  COM class, and File Explorer reads them from there, as this class does.
//  A command is an IExplorerCommand: GetTitle, GetIcon and GetState are
//  called against the items, and Invoke runs it on them.
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
        std::vector<Entry>                          children;
    };

    ~Win32PackagedCommands();

    //  Starts reading the installed packages' manifests.
    void  Start ();

    //  Returns the commands that apply to the items; empty until the
    //  manifests are read.
    std::vector<Entry>  GetEntries (const std::vector<std::wstring> & paths, ItemKind kind);

    HRESULT  Invoke (HWND owner, const Entry & entry, const std::vector<std::wstring> & paths);

private:
    struct Registration
    {
        std::wstring  itemType;     // "*", ".png", "Directory", "Directory\Background" or "Drive"
        CLSID         clsid = {};
    };

    static constexpr int  s_kMaxDepth = 2;

    void  ReadAll ();

    static HRESULT  OpenManifest     (const std::wstring & folder, IStream ** outStream);
    static HRESULT  ReadManifest     (IStream * stream, std::vector<Registration> & outRegistrations);
    static HRESULT  GetAttribute     (IXmlReader * reader, const wchar_t * attribute, std::wstring & outValue);
    static HRESULT  ParseClsid       (const std::wstring & text, CLSID & outClsid);
    static void     AddRegistration  (const Registration & registration, std::vector<Registration> & outRegistrations);
    static bool     Applies          (const Registration & registration, const std::vector<std::wstring> & paths, ItemKind kind);
    static HRESULT  MakeItems        (const std::vector<std::wstring> & paths, IShellItemArray ** outItems);
    static void     AddEntry         (IExplorerCommand * command, IShellItemArray * items, int depth, std::vector<Entry> & outEntries);

    std::thread                 m_reader;
    std::mutex                  m_lock;
    bool                        m_read = false;
    std::vector<Registration>   m_registrations;
};
