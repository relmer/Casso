#include "Pch.h"

#include "CassoExplorer/InspectorWindowSet.h"
#include "CassoExplorer/ExplorerInspectorHost.h"
#include "CassoExplorer/ExplorerBlockInspectorWindow.h"
#include "CassoExplorer/ExplorerBlockInspectorWindow.h"
#include "CassoExplorer/ExplorerInspectorWindow.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/Inspector/Blocks/BlockImageFile.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/Inspector/Blocks/BlockImageFile.h"
#include "Seams/Win32IntentChannel.h"





std::map<UINT_PTR, InspectorWindowSet *>  InspectorWindowSet::s_timers;





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorWindowSet::InspectorWindowSet
//
//  Each seam left empty is the real window, file check or clock.
//
////////////////////////////////////////////////////////////////////////////////

InspectorWindowSet::InspectorWindowSet (IImageWatcher & watcher, DiskInspectorPrefs & prefs, Seams seams) :
    m_watcher (watcher),
    m_prefs   (prefs),
    m_seams   (std::move (seams))
{
    if (!m_seams.makeWindow)
    {
        m_seams.makeWindow = [] () { return std::unique_ptr<Window> (std::make_unique<ExplorerInspectorWindow>()); };
    }

    if (!m_seams.fileExists)
    {
        m_seams.fileExists = [] (const std::wstring & path) { std::error_code error; return fs::exists (fs::path (path), error); };
    }

    if (!m_seams.nowMs)
    {
        m_seams.nowMs = [] () { return GetTickCount64(); };
    }

    if (!m_seams.makeIntents)
    {
        m_seams.makeIntents = [] (HWND sender)
        {
            auto  intents = std::make_unique<Win32IntentChannel>();

            intents->SetSender (sender);
            return std::unique_ptr<IIntentChannel> (std::move (intents));
        };
    }

    if (m_seams.fileIo == nullptr)
    {
        m_seams.fileIo = &m_fileIo;
    }

    if (!m_seams.makeBlockWindow)
    {
        m_seams.makeBlockWindow = [this] (const std::wstring & path) { return std::unique_ptr<Window> (std::make_unique<ExplorerBlockInspectorWindow> (path, *m_seams.fileIo)); };
    }

    if (!m_seams.isBlockImage)
    {
        m_seams.isBlockImage = [this] (const std::wstring & path) { return BlockImageFile::IsBlockImage (*m_seams.fileIo, TextEncoding::WideToUtf8 (path)); };
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorWindowSet::~InspectorWindowSet
//
////////////////////////////////////////////////////////////////////////////////

InspectorWindowSet::~InspectorWindowSet()
{
    CloseAll();
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorWindowSet::MakeKey
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorWindowSet::MakeKey (const std::wstring & path)
{
    std::error_code  error;
    std::wstring     key = fs::absolute (fs::path (path), error).lexically_normal().wstring();



    if (error)
    {
        key = fs::path (path).lexically_normal().wstring();
    }

    std::transform (key.begin(), key.end(), key.begin(), [] (wchar_t c) { return static_cast<wchar_t> (towlower (c)); });

    return key;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorWindowSet::CanInspect
//
////////////////////////////////////////////////////////////////////////////////

bool InspectorWindowSet::CanInspect (const std::wstring & path)
{
    static constexpr std::array<const wchar_t *, 6>  s_kBlockExtensions = { L".hdv", L".2mg", L".2img", L".dc", L".dc42", L".image" };
    std::wstring                                      extension         = fs::path (path).extension().wstring();



    std::transform (extension.begin(), extension.end(), extension.begin(), [] (wchar_t c) { return static_cast<wchar_t> (towlower (c)); });

    return HasTrackFormat (path) || std::any_of (s_kBlockExtensions.begin(), s_kBlockExtensions.end(), [&extension] (const wchar_t * known) { return extension == known; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorWindowSet::HasTrackFormat
//
////////////////////////////////////////////////////////////////////////////////

bool InspectorWindowSet::HasTrackFormat (const std::wstring & path)
{
    static constexpr std::array<const wchar_t *, 6>  s_kExtensions = { L".dsk", L".do", L".po", L".nib", L".nb2", L".woz" };
    std::wstring                                      extension     = fs::path (path).extension().wstring();



    std::transform (extension.begin(), extension.end(), extension.begin(), [] (wchar_t c) { return static_cast<wchar_t> (towlower (c)); });

    return std::any_of (s_kExtensions.begin(), s_kExtensions.end(), [&extension] (const wchar_t * known) { return extension == known; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorWindowSet::Open
//
//  A file with a window brings it to the front; any other gets a new window
//  with the saved state, opened over `anchor` (FR-073, FR-080). The
//  walkthrough opens the first time either host opens the inspector
//  (FR-153).
//
////////////////////////////////////////////////////////////////////////////////

HRESULT InspectorWindowSet::Open (HINSTANCE hInstance, HWND anchor, const std::wstring & path, const DxuiTheme & theme)
{
    HRESULT                   hr      = S_OK;
    std::wstring              key     = MakeKey (path);
    Entry                   * found   = Find (key);
    std::unique_ptr<Entry>    entry;
    bool                      isBlock = false;



    if (found != nullptr)
    {
        found->window->Front();
        BAIL_OUT_IF (true, S_OK);
    }

    SetTheme (theme);

    entry         = std::make_unique<Entry>();
    entry->key    = key;
    entry->folder = fs::path (path).parent_path().wstring();
    entry->host   = std::make_unique<ExplorerInspectorHost> (path, *m_seams.fileIo);
    isBlock       = m_seams.isBlockImage (path);
    entry->window = isBlock ? m_seams.makeBlockWindow (path) : m_seams.makeWindow();

    hr = entry->window->Create (hInstance, anchor, m_theme, entry->host.get());
    CHR (hr);

    entry->intents = m_seams.makeIntents (entry->window->GetHwnd());
    entry->host->SetIntents (entry->intents.get());

    entry->host->SetOnClosed ([this, raw = entry.get()] ()
    {
        raw->window->GetPrefs (m_prefs);
        raw->isClosed = true;
    });

    entry->window->ApplyPrefs (m_prefs);

    if (!m_prefs.isWalkthroughSeen && !isBlock)
    {
        entry->window->OpenWalkthrough();
        m_prefs.isWalkthroughSeen = true;
    }

    hr = entry->window->ShowDisk();
    CHR (hr);

    WatchFolder (entry->folder);
    m_entries.push_back (std::move (entry));
    StartTimer();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorWindowSet::Compare
//
//  A is compared with B in A's window, which is opened or brought to the
//  front (FR-117).
//
////////////////////////////////////////////////////////////////////////////////

HRESULT InspectorWindowSet::Compare (HINSTANCE hInstance, HWND anchor, const std::wstring & pathA, const std::wstring & pathB, const DxuiTheme & theme)
{
    HRESULT  hr    = S_OK;
    Entry  * found = nullptr;



    hr = Open (hInstance, anchor, pathA, theme);
    CHR (hr);

    found = Find (MakeKey (pathA));
    CBRA (found != nullptr);

    found->window->CompareWith (pathB);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorWindowSet::SetTheme
//
//  The theme is copied as its most derived type, so a Casso theme's body
//  face is copied too.
//  The palette is resolved from the theme on every frame, so the windows
//  redraw in the new theme at once (FR-077, FR-078).
//
////////////////////////////////////////////////////////////////////////////////

void InspectorWindowSet::SetTheme (const DxuiTheme & theme)
{
    const CassoTheme      * casso = dynamic_cast<const CassoTheme *>     (&theme);
    const DxuiDarkTheme   * dark  = dynamic_cast<const DxuiDarkTheme *>  (&theme);
    const DxuiLightTheme  * light = dynamic_cast<const DxuiLightTheme *> (&theme);



    if (casso != nullptr)
    {
        m_cassoTheme = *casso;
        m_theme      = &m_cassoTheme;
    }
    else if (dark != nullptr)
    {
        m_darkTheme = *dark;
        m_theme     = &m_darkTheme;
    }
    else if (light != nullptr)
    {
        m_lightTheme = *light;
        m_theme      = &m_lightTheme;
    }
    else
    {
        m_otherTheme = theme;
        m_theme      = &m_otherTheme;
    }

    for (const std::unique_ptr<Entry> & entry : m_entries)
    {
        if (!entry->isClosed)
        {
            entry->window->ChangeTheme (m_theme);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorWindowSet::CloseAll
//
//  When Explorer exits (FR-075). The state saved is the last window's.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorWindowSet::CloseAll()
{
    for (const std::unique_ptr<Entry> & entry : m_entries)
    {
        if (!entry->isClosed)
        {
            entry->window->GetPrefs (m_prefs);
            entry->isClosed = true;
        }
    }

    Release();
    StopTimer();
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorWindowSet::GetCount
//
////////////////////////////////////////////////////////////////////////////////

size_t InspectorWindowSet::GetCount() const
{
    return static_cast<size_t> (std::count_if (m_entries.begin(), m_entries.end(), [] (const std::unique_ptr<Entry> & entry) { return !entry->isClosed; }));
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorWindowSet::Tick
//
////////////////////////////////////////////////////////////////////////////////

void InspectorWindowSet::Tick()
{
    HRESULT  hr = S_OK;



    TakeChanges();
    ReloadChanged();

    for (const std::unique_ptr<Entry> & entry : m_entries)
    {
        if (!entry->isClosed)
        {
            hr = entry->window->RenderFrame();
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
    }

    Release();

    if (m_entries.empty())
    {
        StopTimer();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorWindowSet::NoteChange
//
//  On the watcher's thread: the path is queued and matched on the next
//  frame.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorWindowSet::NoteChange (const std::string & path)
{
    std::lock_guard<std::mutex>  lock (m_changeLock);



    m_changes.push_back (fs::path (path).wstring());
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorWindowSet::OnTimer
//
////////////////////////////////////////////////////////////////////////////////

VOID CALLBACK InspectorWindowSet::OnTimer (HWND hwnd, UINT message, UINT_PTR id, DWORD timeMs)
{
    auto  it = s_timers.find (id);



    (void) hwnd;
    (void) message;
    (void) timeMs;

    if (it != s_timers.end())
    {
        it->second->Tick();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorWindowSet::Find
//
////////////////////////////////////////////////////////////////////////////////

InspectorWindowSet::Entry * InspectorWindowSet::Find (const std::wstring & key)
{
    for (const std::unique_ptr<Entry> & entry : m_entries)
    {
        if (!entry->isClosed && entry->key == key)
        {
            return entry.get();
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorWindowSet::WatchFolder
//
//  A folder is watched once, however many of its files have windows.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorWindowSet::WatchFolder (const std::wstring & folder)
{
    int  & count = m_folders[MakeKey (folder)];
    bool   isOn  = false;



    if (count++ == 0)
    {
        isOn = m_watcher.Watch (fs::path (folder).string(), [this] (const std::string & path) { NoteChange (path); });
        IGNORE_RETURN_VALUE (isOn, true);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorWindowSet::UnwatchFolder
//
////////////////////////////////////////////////////////////////////////////////

void InspectorWindowSet::UnwatchFolder (const std::wstring & folder)
{
    auto  it = m_folders.find (MakeKey (folder));



    if (it != m_folders.end() && --it->second == 0)
    {
        m_watcher.Unwatch (fs::path (folder).string());
        m_folders.erase (it);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorWindowSet::TakeChanges
//
//  Each change to a window's file restarts its wait, so a burst of changes,
//  such as a save's temporary file renamed over the image, is read once.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorWindowSet::TakeChanges()
{
    vector<std::wstring>  changes;
    ULONGLONG             nowMs = m_seams.nowMs();



    {
        std::lock_guard<std::mutex>  lock (m_changeLock);

        changes.swap (m_changes);
    }

    for (const std::wstring & change : changes)
    {
        Entry  * entry = Find (MakeKey (change));

        if (entry != nullptr)
        {
            entry->changedMs = nowMs;
            entry->isChanged = true;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorWindowSet::ReloadChanged
//
//  A file whose size and time match the window's last read or save is not
//  read again, so a save from the window does not clear its applied-edit
//  history (FR-107).
//
////////////////////////////////////////////////////////////////////////////////

void InspectorWindowSet::ReloadChanged()
{
    ULONGLONG  nowMs = m_seams.nowMs();



    for (const std::unique_ptr<Entry> & entry : m_entries)
    {
        if (entry->isClosed || !entry->isChanged || nowMs - entry->changedMs < kSettleMs)
        {
            continue;
        }

        entry->isChanged = false;

        if (!m_seams.fileExists (entry->host->GetPath()))
        {
            entry->window->NoteFileGone();
        }
        else if (!entry->host->IsFileCurrent())
        {
            entry->host->Reload();
            entry->window->ReloadDisk();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorWindowSet::Release
//
//  Called between messages, never inside a message to one of the windows.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorWindowSet::Release()
{
    for (const std::unique_ptr<Entry> & entry : m_entries)
    {
        if (entry->isClosed)
        {
            UnwatchFolder (entry->folder);
        }
    }

    std::erase_if (m_entries, [] (const std::unique_ptr<Entry> & entry) { return entry->isClosed; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorWindowSet::StartTimer
//
////////////////////////////////////////////////////////////////////////////////

void InspectorWindowSet::StartTimer()
{
    if (m_timer == 0)
    {
        m_timer = SetTimer (nullptr, 0, kFrameMs, &OnTimer);

        if (m_timer != 0)
        {
            s_timers[m_timer] = this;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorWindowSet::StopTimer
//
////////////////////////////////////////////////////////////////////////////////

void InspectorWindowSet::StopTimer()
{
    BOOL  isKilled = FALSE;



    if (m_timer != 0)
    {
        isKilled = KillTimer (nullptr, m_timer);
        IGNORE_RETURN_VALUE (isKilled, TRUE);
        s_timers.erase (m_timer);
        m_timer = 0;
    }
}
