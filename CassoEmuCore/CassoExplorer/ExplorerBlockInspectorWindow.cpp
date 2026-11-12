#include "Pch.h"

#include "CassoExplorer/ExplorerBlockInspectorWindow.h"
#include "Ui/DiskInspector/Blocks/BlockInspectorWindow.h"
#include "Ui/DiskInspector/IDiskInspectorHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerBlockInspectorWindow::ExplorerBlockInspectorWindow
//
////////////////////////////////////////////////////////////////////////////////

ExplorerBlockInspectorWindow::ExplorerBlockInspectorWindow (const std::wstring & path, IDiskFileIo & fileIo) :
    m_path   (path),
    m_fileIo (fileIo),
    m_window (std::make_unique<BlockInspectorWindow>())
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerBlockInspectorWindow::~ExplorerBlockInspectorWindow
//
////////////////////////////////////////////////////////////////////////////////

ExplorerBlockInspectorWindow::~ExplorerBlockInspectorWindow() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerBlockInspectorWindow::Create
//
//  A file that doesn't open is reported in a message over the Explorer
//  window, and the window set closes the entry.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ExplorerBlockInspectorWindow::Create (HINSTANCE hInstance, HWND anchor, const DxuiTheme * theme, IDiskInspectorHost * host)
{
    HRESULT       hr    = S_OK;
    HICON         icon  = nullptr;
    std::wstring  error;
    int           shown = 0;



    m_window->SetOnClosed ([host]()
    {
        if (host != nullptr)
        {
            host->OnInspectorClosed();
        }
    });

    hr = m_window->Open (hInstance, anchor, theme, m_fileIo, m_path, error);

    if (FAILED (hr))
    {
        shown = DxuiMessageBox (anchor, theme, (error.empty() ? std::wstring (L"The image can't be opened in block mode.") : error).c_str(), L"Disk inspector", MB_OK | MB_ICONWARNING);
        IGNORE_RETURN_VALUE (shown, 0);
    }

    CHR (hr);

    icon = (anchor != nullptr) ? reinterpret_cast<HICON> (SendMessageW (anchor, WM_GETICON, ICON_BIG, 0)) : nullptr;

    if (icon != nullptr)
    {
        SendMessageW (m_window->GetHwnd(), WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM> (icon));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerBlockInspectorWindow::Front
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerBlockInspectorWindow::Front()
{
    BOOL  isFront = FALSE;



    m_window->Show (true);
    isFront = SetForegroundWindow (m_window->GetHwnd());
    IGNORE_RETURN_VALUE (isFront, TRUE);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerBlockInspectorWindow::RenderFrame
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ExplorerBlockInspectorWindow::RenderFrame()
{
    m_window->RenderIfChanged();

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerBlockInspectorWindow::ReloadDisk
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerBlockInspectorWindow::ReloadDisk()
{
    m_window->Reload();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerBlockInspectorWindow::NoteFileGone
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerBlockInspectorWindow::NoteFileGone()
{
    m_window->NoteFileGone();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerBlockInspectorWindow::ChangeTheme
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerBlockInspectorWindow::ChangeTheme (const DxuiTheme * theme)
{
    m_window->ChangeTheme (theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerBlockInspectorWindow::GetHwnd
//
////////////////////////////////////////////////////////////////////////////////

HWND ExplorerBlockInspectorWindow::GetHwnd() const
{
    return m_window->GetHwnd();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerBlockInspectorWindow::CompareWith
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerBlockInspectorWindow::CompareWith (const std::wstring & pathB)
{
    m_window->CompareWithFile (pathB);
}