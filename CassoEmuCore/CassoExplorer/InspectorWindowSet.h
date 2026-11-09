#pragma once

#include "Pch.h"

#include "Config/DiskInspectorPrefs.h"
#include "Devices/Disk/IImageWatcher.h"
#include "Seams/Win32DiskFileIo.h"
#include "Ui/Chrome/CassoTheme.h"

class ExplorerInspectorHost;
class IDiskInspectorHost;
class IIntentChannel;





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorWindowSet
//
//  Casso Explorer's disk inspector windows, one per image file (FR-073 to
//  FR-075). Opening a file that has a window brings that window to the
//  front. Each window's folder is watched, and a changed file is read again
//  once no change has arrived for kSettleMs; for a file that is gone, the
//  last analysis stays, with a note. The windows outlast the Explorer window
//  they were opened from and close with the app, so the set has a copy of
//  the theme rather than a pointer to an Explorer window's. A thread timer
//  drives their frames, since Explorer has no frame loop. A file a window
//  saved is not read again. Each window has a separate intent channel, so
//  Casso's reply to a save is sent to that window (FR-105).
//
////////////////////////////////////////////////////////////////////////////////

class InspectorWindowSet
{
public:
    static constexpr UINT       kFrameMs  = 16;
    static constexpr ULONGLONG  kSettleMs = 250;

    //  One inspector window, as the set drives it: DiskInspectorWindow in
    //  the app, a fake in tests. The window calls its host's
    //  OnInspectorClosed when the user closes it.
    class Window
    {
    public:
        virtual ~Window() = default;

        virtual HRESULT  Create          (HINSTANCE hInstance, HWND anchor, const DxuiTheme * theme, IDiskInspectorHost * host) = 0;
        virtual void     Front           ()                                    = 0;
        virtual void     ApplyPrefs      (const DiskInspectorPrefs & prefs)    = 0;
        virtual void     GetPrefs        (DiskInspectorPrefs & inOut) const    = 0;
        virtual void     OpenWalkthrough ()                                    = 0;
        virtual HRESULT  ShowDisk        ()                                    = 0;
        virtual HRESULT  RenderFrame     ()                                    = 0;
        virtual void     ReloadDisk      ()                                    = 0;
        virtual void     NoteFileGone    ()                                    = 0;
        virtual void     ChangeTheme     (const DxuiTheme * theme)             = 0;
        virtual HWND     GetHwnd         () const                              = 0;
    };

    //  The functions for the window, the file check, the clock and the
    //  channel to Casso, and the file access the windows read and save
    //  through. Each one left empty is the real one.
    struct Seams
    {
        std::function<std::unique_ptr<Window> ()>                  makeWindow;
        std::function<bool (const std::wstring &)>                 fileExists;
        std::function<ULONGLONG ()>                                nowMs;
        std::function<std::unique_ptr<IIntentChannel> (HWND)>      makeIntents;
        IDiskFileIo                                              * fileIo = nullptr;
    };

    InspectorWindowSet (IImageWatcher & watcher, DiskInspectorPrefs & prefs, Seams seams = {});
    ~InspectorWindowSet ();

    InspectorWindowSet (const InspectorWindowSet &)             = delete;
    InspectorWindowSet & operator= (const InspectorWindowSet &) = delete;

    HRESULT  Open     (HINSTANCE hInstance, HWND anchor, const std::wstring & path, const DxuiTheme & theme);
    void     SetTheme (const DxuiTheme & theme);
    void     CloseAll ();
    size_t   GetCount () const;

    //  One frame: changed files read again, every window drawn, and closed
    //  windows released. The timer calls it; tests call it directly.
    void     Tick     ();

    //  A path under a watched folder changed. Any thread.
    void     NoteChange (const std::string & path);

    //  The key a path is matched by: full, and lowercase as NTFS compares.
    static std::wstring  MakeKey    (const std::wstring & path);

    //  Whether a file is in a format the inspector opens, by its extension
    //  (FR-003, FR-073).
    static bool          CanInspect (const std::wstring & path);

private:
    struct Entry
    {
        std::wstring                            key;
        std::wstring                            folder;
        std::unique_ptr<ExplorerInspectorHost>  host;
        std::unique_ptr<Window>                 window;
        std::unique_ptr<IIntentChannel>         intents;
        ULONGLONG                               changedMs = 0;
        bool                                    isChanged = false;
        bool                                    isClosed  = false;
    };

    static VOID CALLBACK  OnTimer (HWND hwnd, UINT message, UINT_PTR id, DWORD timeMs);

    Entry  * Find          (const std::wstring & key);
    void     WatchFolder   (const std::wstring & folder);
    void     UnwatchFolder (const std::wstring & folder);
    void     TakeChanges   ();
    void     ReloadChanged ();
    void     Release       ();
    void     StartTimer    ();
    void     StopTimer     ();

    IImageWatcher                        & m_watcher;
    DiskInspectorPrefs                   & m_prefs;
    Seams                                  m_seams;
    Win32DiskFileIo                        m_fileIo;
    vector<std::unique_ptr<Entry>>         m_entries;
    std::map<std::wstring, int>            m_folders;
    std::mutex                             m_changeLock;
    vector<std::wstring>                   m_changes;
    UINT_PTR                               m_timer = 0;
    DxuiLightTheme                         m_lightTheme;
    DxuiDarkTheme                          m_darkTheme;
    CassoTheme                             m_cassoTheme;
    DxuiTheme                              m_otherTheme;
    const DxuiTheme                      * m_theme = &m_darkTheme;

    static std::map<UINT_PTR, InspectorWindowSet *>  s_timers;
};
