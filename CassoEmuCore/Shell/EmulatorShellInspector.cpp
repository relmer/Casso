#include "Pch.h"

#include "Shell/EmulatorShell.h"
#include "Shell/Components/ShellChrome.h"
#include "Shell/Components/ShellDisks.h"
#include "Shell/Components/ShellRenderer.h"
#include "Shell/Components/ShellSettings.h"
#include "Shell/Components/ShellWindow.h"
#include "Shell/ShellInspectorWindowHost.h"
#include "Ui/DiskInspector/DiskInspectorWindow.h"
#include "Ui/Chrome/ChromeMetrics.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskThumbnailCommand.h"
#include "DiskCheck/ThumbnailStore.h"





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::DiskInspectorEntry::DiskInspectorEntry
//
////////////////////////////////////////////////////////////////////////////////

EmulatorShell::DiskInspectorEntry::DiskInspectorEntry()  = default;
EmulatorShell::DiskInspectorEntry::~DiskInspectorEntry() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::OpenDiskInspector
//
//  Opens the disk inspector on the disk in a drive: the window that already
//  inspects that disk, or a new one. The window requests a copy of the
//  disk and fills in as the copy is analyzed. It is activated only when
//  Casso or one of its windows is in front, so a command sent to Casso
//  while another program is in front doesn't change the foreground.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OpenDiskInspector (int drive)
{
    HWND  foreground = GetForegroundWindow();
    HWND  root       = (foreground != nullptr) ? GetAncestor (foreground, GA_ROOTOWNER) : nullptr;
    bool  activate   = root != nullptr && (root == m_hwnd || IsDiskInspectorWindow (root));



    ShowDiskInspector (drive, activate);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SetDiskThumbnail
//
//  The frame last drawn, as Casso Explorer draws it in place of the image's
//  icon and in its preview.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetDiskThumbnail (int drive)
{
    static constexpr int  kWidth  = ChromeMetrics::kFramebufferWidthPx;
    static constexpr int  kHeight = ChromeMetrics::kFramebufferHeightPx;



    HRESULT            hr       = S_OK;
    std::wstring       image    = m_disks->GetMountedImagePath (drive);
    const uint32_t   * pixels   = m_renderer->GetUiFramebufferPixels();
    bool               hasImage = !image.empty();
    bool               hasFrame = pixels != nullptr;
    ThumbnailStore     store (ThumbnailStore::GetDefaultFolder());



    CBR (hasImage);
    CBR (hasFrame);

    hr = DiskThumbnailCommand::SetFromScreen (store, image, std::span<const uint32_t> (pixels, (size_t) kWidth * kHeight), kWidth, kHeight);
    CHRF (hr, DxuiMessageBox (m_hwnd, &m_chrome->GetTheme(), L"The thumbnail couldn't be saved.", L"Save screenshot as thumbnail", MB_OK | MB_ICONWARNING));

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ShowDiskInspector
//
//  A window for the drive's disk, or for the drive when it is empty, is
//  brought forward. Otherwise a new window opens on it, with the saved
//  state applied before ShowDrive (FR-080).
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ShowDiskInspector (int drive, bool activate)
{
    HRESULT                              hr        = S_OK;
    HINSTANCE                            hInstance = nullptr;
    std::wstring                         path      = m_disks->GetMountedImagePath (drive);
    DiskInspectorEntry                 * existing  = FindDiskInspector (drive, path);
    std::unique_ptr<DiskInspectorEntry>  entry;
    DiskInspectorEntry                 * created   = nullptr;



    if (existing != nullptr)
    {
        existing->window->Show (activate);
    }
    else
    {
        hInstance    = reinterpret_cast<HINSTANCE> (GetWindowLongPtr (m_hwnd, GWLP_HINSTANCE));
        entry        = std::make_unique<DiskInspectorEntry>();
        created      = entry.get();
        entry->host  = std::make_unique<ShellInspectorWindowHost> (m_inspectorHost, drive, path,
                                                                   [this] (const std::wstring & file) { return FindDiskDrive (file); },
                                                                   [this] () { return TextEncoding::Utf8ToWide (m_machine.GetConfig().name); });
        entry->window = std::make_unique<DiskInspectorWindow>();

        m_inspectorHost.SetDriveQueries ([this] (int index) { return index == 0 || m_disks->ShouldShowExternalDrive(); },
                                         [this] () { return HasOtherDiskController(); });

        hr = entry->window->Create (hInstance, m_hwnd, &m_chrome->GetTheme(), entry->host.get(), activate);
        CHR (hr);

        m_window->ApplyAppIconToWindow (entry->window->GetHwnd());
        entry->host->SetOnClosed ([this, created] () { created->isClosed = true; SaveInspectorPrefs (created); });
        //  The first window open takes the saved position, and the others
        //  are placed by Windows.
        entry->window->ApplyPrefs (m_settings->GetPrefs().diskInspector, !std::any_of (m_diskInspectors.begin(), m_diskInspectors.end(),
                                                                                       [] (const std::unique_ptr<DiskInspectorEntry> & open) { return !open->isClosed; }));

        //  The walkthrough opens the first time the inspector does (FR-153).
        if (!m_settings->GetPrefs().diskInspector.isWalkthroughSeen)
        {
            entry->window->OpenWalkthrough();
            m_settings->GetPrefs().diskInspector.isWalkthroughSeen = true;
            m_settings->SaveGlobalPrefsDeferred();
        }

        hr = entry->window->ShowDrive (drive);
        CHR (hr);

        m_diskInspectors.push_back (std::move (entry));
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::FindDiskInspector
//
//  The open window that inspects a file, or for no file, the open window
//  waiting on the empty drive.
//
////////////////////////////////////////////////////////////////////////////////

EmulatorShell::DiskInspectorEntry * EmulatorShell::FindDiskInspector (int drive, const std::wstring & path)
{
    for (std::unique_ptr<DiskInspectorEntry> & entry : m_diskInspectors)
    {
        const std::wstring &  inspected = entry->host->GetPath();
        bool                  isMatch   = path.empty() ? (inspected.empty() && entry->host->GetDrive() == drive) : _wcsicmp (inspected.c_str(), path.c_str()) == 0;



        if (!entry->isClosed && isMatch)
        {
            return entry.get();
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::FindDiskDrive
//
//  The slot 6 drive a file is mounted in, or -1.
//
////////////////////////////////////////////////////////////////////////////////

int EmulatorShell::FindDiskDrive (const std::wstring & path) const
{
    int  found = -1;



    for (int drive = 0; found < 0 && drive < ShellInspectorHost::kDriveCount; drive++)
    {
        const std::wstring &  mounted = m_disks->GetMountedImagePath (drive);

        found = (!mounted.empty() && _wcsicmp (mounted.c_str(), path.c_str()) == 0) ? drive : -1;
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::RenderDiskInspectors
//
//  One frame of every open window. A window the user closed is released
//  here, outside the window's message handling.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::RenderDiskInspectors()
{
    HRESULT  hr = S_OK;



    for (std::unique_ptr<DiskInspectorEntry> & entry : m_diskInspectors)
    {
        if (!entry->isClosed)
        {
            hr = entry->window->RenderFrame();
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
    }

    ReleaseClosedInspectors();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ReleaseClosedInspectors
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ReleaseClosedInspectors()
{
    std::erase_if (m_diskInspectors, [] (const std::unique_ptr<DiskInspectorEntry> & entry) { return entry->isClosed; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::IsDiskInspectorWindow
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::IsDiskInspectorWindow (HWND hwnd) const
{
    return hwnd != nullptr && std::any_of (m_diskInspectors.begin(), m_diskInspectors.end(),
                                           [hwnd] (const std::unique_ptr<DiskInspectorEntry> & entry) { return entry->window->GetHwnd() == hwnd; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ServiceInspectorRequests
//
//  CPU thread: answers the inspector's waiting requests between passes.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ServiceInspectorRequests()
{
    m_inspectorHost.Service();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::OnClose
//
//  With unsaved changes in a disk inspector window, a confirmation comes
//  first, and Casso stays open when the user cancels (FR-108).
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnClose()
{
    bool  isGoingOn = ConfirmInspectorDiscard ([hwnd = m_hwnd] () { PostMessageW (hwnd, WM_CLOSE, 0, 0); });



    if (isGoingOn)
    {
        SaveInspectorPrefs (nullptr);
    }

    return isGoingOn ? DxuiMessageResult::NotHandled : DxuiMessageResult::Handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SaveInspectorPrefs
//
//  The layout comes from the window given, or else from the first open
//  one. The drives of the open windows whose disks are in a drive are
//  reopened at the next launch.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SaveInspectorPrefs (const DiskInspectorEntry * from)
{
    DiskInspectorPrefs        & prefs  = m_settings->GetPrefs().diskInspector;
    const DiskInspectorEntry  * source = from;
    int                         drives = 0;
    bool                        isOpen = false;



    for (const std::unique_ptr<DiskInspectorEntry> & entry : m_diskInspectors)
    {
        isOpen = !entry->isClosed && entry->window->GetHwnd() != nullptr && IsWindowVisible (entry->window->GetHwnd());
        drives = (isOpen && entry->host->IsInDrive()) ? (drives | (1 << entry->host->GetDrive())) : drives;
        source = (source == nullptr && isOpen) ? entry.get() : source;
    }

    if (source != nullptr && source->window->GetHwnd() != nullptr)
    {
        source->window->GetPrefs (prefs);
    }

    prefs.openDrives = drives;
    prefs.isOpen     = drives != 0;
    m_settings->SaveGlobalPrefsDeferred();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ReopenDiskInspector
//
//  At launch, a window for each drive that had one open at exit, behind
//  Casso's main window (FR-080). Prefs saved before Casso had a window per
//  disk have the last drive and whether its window was open.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ReopenDiskInspector()
{
    const DiskInspectorPrefs &  prefs  = m_settings->GetPrefs().diskInspector;
    int                         drives = (prefs.openDrives >= 0) ? prefs.openDrives
                                       : prefs.isOpen            ? (1 << std::clamp (prefs.lastDrive, 0, ShellInspectorHost::kDriveCount - 1))
                                                                 : 0;



    for (int drive = 0; drive < ShellInspectorHost::kDriveCount; drive++)
    {
        if ((drives & (1 << drive)) != 0)
        {
            ShowDiskInspector (drive, false);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ConfirmInspectorDiscard
//
//  ConfirmDiscardEdits is called on each window in turn. After a Save,
//  afterSave runs, and the next window's confirmation follows. The confirmation dialog is modal,
//  so it appears only on the UI thread. On any other thread the function
//  returns true.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::ConfirmInspectorDiscard (std::function<void()> afterSave)
{
    bool  isUi      = m_hwnd != nullptr && GetWindowThreadProcessId (m_hwnd, nullptr) == GetCurrentThreadId();
    bool  isGoingOn = true;



    for (std::unique_ptr<DiskInspectorEntry> & entry : m_diskInspectors)
    {
        if (isUi && isGoingOn && !entry->isClosed && entry->window->GetHwnd() != nullptr)
        {
            isGoingOn = entry->window->ConfirmDiscardEdits (L"Quitting Casso", afterSave);
        }
    }

    return isGoingOn;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::HasOtherDiskController
//
//  Only slot 6's drives are inspected (FR-060).
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::HasOtherDiskController() const
{
    for (const SlotConfig & entry : m_machine.GetConfig().slots)
    {
        if (entry.enabled && entry.device == kpszDiskIiDevice && entry.slot != ShellInspectorHost::kDiskSlot)
        {
            return true;
        }
    }

    return false;
}
