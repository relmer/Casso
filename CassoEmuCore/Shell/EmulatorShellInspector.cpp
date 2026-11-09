#include "Pch.h"

#include "Shell/EmulatorShell.h"
#include "Shell/Components/ShellChrome.h"
#include "Shell/Components/ShellDisks.h"
#include "Shell/Components/ShellSettings.h"
#include "Shell/Components/ShellWindow.h"
#include "Ui/DiskInspector/DiskInspectorWindow.h"





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::OpenDiskInspector
//
//  Opens the disk inspector on a drive, creating the window the first time
//  and reusing it after. The window asks for a copy of the drive's disk and
//  fills in as the copy is analyzed. It takes the foreground only from Casso
//  itself, so a command that reaches Casso while another program is in front
//  does not take focus from it.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OpenDiskInspector (int drive)
{
    HWND  foreground = GetForegroundWindow();
    bool  activate   = foreground != nullptr && (GetAncestor (foreground, GA_ROOTOWNER) == m_hwnd ||
                       (m_diskInspector != nullptr && GetAncestor (foreground, GA_ROOTOWNER) == m_diskInspector->GetHwnd()));



    ShowDiskInspector (drive, activate);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ShowDiskInspector
//
//  The saved state is applied to a new window before ShowDrive (FR-080).
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ShowDiskInspector (int drive, bool activate)
{
    HRESULT    hr        = S_OK;
    HINSTANCE  hInstance = nullptr;



    if (m_diskInspector == nullptr || m_diskInspector->GetHwnd() == nullptr)
    {
        hInstance       = reinterpret_cast<HINSTANCE> (GetWindowLongPtr (m_hwnd, GWLP_HINSTANCE));
        m_diskInspector = std::make_unique<DiskInspectorWindow>();

        hr = m_diskInspector->Create (hInstance, m_hwnd, &m_chrome->GetTheme(), &m_inspectorHost, activate);
        CHRF (hr, m_diskInspector.reset());

        m_window->ApplyAppIconToWindow (m_diskInspector->GetHwnd());
        m_inspectorHost.SetOnClosed ([this] () { SaveInspectorPrefs (false); });
        m_inspectorHost.SetDriveQueries ([this] (int index) { return index == 0 || m_disks->ShouldShowExternalDrive(); },
                                         [this] () { return HasOtherDiskController(); });
        m_diskInspector->ApplyPrefs (m_settings->GetPrefs().diskInspector);

        //  The walkthrough opens the first time the inspector does (FR-153).
        if (!m_settings->GetPrefs().diskInspector.isWalkthroughSeen)
        {
            m_diskInspector->OpenWalkthrough();
            m_settings->GetPrefs().diskInspector.isWalkthroughSeen = true;
            m_settings->SaveGlobalPrefsDeferred();
        }
    }
    else
    {
        m_diskInspector->Show (activate);
    }

    hr = m_diskInspector->ShowDrive (drive);
    CHR (hr);

Error:
    return;
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
//  With pending edits in the disk inspector, a confirmation comes first,
//  and Casso stays open when the user cancels (FR-108).
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::OnClose()
{
    bool  isGoingOn = ConfirmInspectorDiscard (-1, L"Quitting Casso");



    if (isGoingOn && m_diskInspector != nullptr && m_diskInspector->GetHwnd() != nullptr)
    {
        SaveInspectorPrefs (IsWindowVisible (m_diskInspector->GetHwnd()) != FALSE);
    }

    return isGoingOn ? DxuiMessageResult::NotHandled : DxuiMessageResult::Handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SaveInspectorPrefs
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SaveInspectorPrefs (bool isOpen)
{
    DiskInspectorPrefs &  prefs = m_settings->GetPrefs().diskInspector;



    if (m_diskInspector != nullptr && m_diskInspector->GetHwnd() != nullptr)
    {
        m_diskInspector->GetPrefs (prefs);
        prefs.isOpen = isOpen;
        m_settings->SaveGlobalPrefsDeferred();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ReopenDiskInspector
//
//  At launch, the inspector on its last drive when it was open at exit,
//  behind Casso's main window (FR-080).
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ReopenDiskInspector()
{
    const DiskInspectorPrefs &  prefs = m_settings->GetPrefs().diskInspector;



    if (prefs.isOpen)
    {
        ShowDiskInspector (std::clamp (prefs.lastDrive, 0, ShellInspectorHost::kDriveCount - 1), false);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ConfirmInspectorDiscard
//
//  The confirmation dialog is modal, so it appears only on the UI thread.
//  On any other thread the function returns true.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::ConfirmInspectorDiscard (int drive, const std::wstring & action)
{
    bool  isOpen    = m_diskInspector != nullptr && m_diskInspector->GetHwnd() != nullptr;
    bool  isUi      = m_hwnd != nullptr && GetWindowThreadProcessId (m_hwnd, nullptr) == GetCurrentThreadId();
    bool  isMatched = isOpen && (drive < 0 || drive == m_diskInspector->GetDrive());



    return !isMatched || !isUi || m_diskInspector->ConfirmDiscardEdits (action);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::HasOtherDiskController
//
//  Only slot 6's drives are inspected, and the drive selector's
//  tooltip indicates so when another slot has a Disk II (FR-060).
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
