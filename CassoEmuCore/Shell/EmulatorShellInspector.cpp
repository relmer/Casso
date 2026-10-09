#include "Pch.h"

#include "Shell/EmulatorShell.h"
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
    HRESULT    hr         = S_OK;
    HINSTANCE  hInstance  = nullptr;
    HWND       foreground = GetForegroundWindow();
    bool       activate   = foreground != nullptr && GetAncestor (foreground, GA_ROOTOWNER) == m_hwnd;



    if (m_diskInspector == nullptr || m_diskInspector->GetHwnd() == nullptr)
    {
        hInstance       = reinterpret_cast<HINSTANCE> (GetWindowLongPtr (m_hwnd, GWLP_HINSTANCE));
        m_diskInspector = std::make_unique<DiskInspectorWindow>();

        hr = m_diskInspector->Create (hInstance, m_hwnd, &m_chromeTheme, &m_inspectorHost, activate);
        CHRF (hr, m_diskInspector.reset());

        ApplyAppIconToWindow (m_diskInspector->GetHwnd());
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
