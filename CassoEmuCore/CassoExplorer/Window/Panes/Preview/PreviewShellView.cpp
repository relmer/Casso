#include "Pch.h"

#include "CassoExplorer/Window/Panes/Preview/PreviewShellView.h"
#include "CassoExplorer/CassoExplorerWindow.h"
#include "Core/AppTimerIds.h"
#include "CassoExplorer/Model/PreviewDecoder.h"
#include "CassoExplorer/Window/Managers/ExplorerPreviewManager.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "Core/AppMessages.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewShellView::PreviewShellView
//
////////////////////////////////////////////////////////////////////////////////

PreviewShellView::PreviewShellView (ExplorerPreviewManager & manager, ExplorerContext & context)
    : PreviewView (manager, context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewShellView::Configure
//
//  The preview helper posts its results to the window, so with no window,
//  as in a test, the helper is not started.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewShellView::Configure()
{
    HWND     hwnd = m_context.window.GetHwnd();
    HRESULT  hr   = S_OK;



    if (hwnd == nullptr)
    {
        return;
    }

    hr = m_shellPreview.Start (hwnd, (UINT) AppMessage::ExplorerShellPreview);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewShellView::Layout
//
////////////////////////////////////////////////////////////////////////////////

void PreviewShellView::Layout (const RECT & content)
{
    if (!m_path.empty())
    {
        m_shellPreview.SetRect (content);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewShellView::Hide
//
////////////////////////////////////////////////////////////////////////////////

void PreviewShellView::Hide()
{
    if (!m_path.empty())
    {
        Unload();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewShellView::Update
//
//  A shell preview is loaded once for its file and reused while the same
//  file is previewed; any other preview, or the pane hidden, unloads it. A
//  load that takes longer than a few seconds is replaced by the bytes.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewShellView::Update (const PreviewContent & preview, const RECT & content)
{
    bool      shell      = preview.kind == PreviewContent::Kind::Shell && m_context.prefs.previewVisible;
    uint32_t  background = m_context.theme->ContentBackground();
    uint32_t  text       = m_context.theme->Foreground();



    if (!shell)
    {
        if (!m_path.empty())
        {
            KillTimer (m_context.window.GetHwnd(), (UINT_PTR) AppTimerId::ExplorerShellPreview);
            Unload();
        }

        return;
    }

    //  A pane just opening has no size yet, and a thumbnail is made at its size.
    if (content.right <= content.left || content.bottom <= content.top)
    {
        return;
    }

    if (preview.hostPath == m_path && (!m_isPicture || EqualRect (&content, &m_pictureRect)))
    {
        return;
    }

    m_pictureRect = content;
    m_path        = preview.hostPath;
    m_isPicture = false;
    m_manager.GetPicture().Clear();

    m_shellPreview.Load (m_path, content,
                         RGB ((background >> 16) & 0xFF, (background >> 8) & 0xFF, background & 0xFF),
                         RGB ((text >> 16) & 0xFF, (text >> 8) & 0xFF, text & 0xFF));

    SetTimer (m_context.window.GetHwnd(), (UINT_PTR) AppTimerId::ExplorerShellPreview, kShellPreviewTimeoutMs, nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewShellView::HandleResult
//
//  A handler's window becomes visible over the pane; a thumbnail is drawn in
//  the picture view, fitted; after a failure the bytes are drawn.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewShellView::HandleResult()
{
    Win32ShellPreview::Result  result;



    if (m_path.empty() || !m_shellPreview.TakeResult (result))
    {
        return;
    }

    KillTimer (m_context.window.GetHwnd(), (UINT_PTR) AppTimerId::ExplorerShellPreview);

    switch (result.outcome)
    {
        case Win32ShellPreview::Outcome::Shown:
            m_shellPreview.ShowHost (true);
            break;

        case Win32ShellPreview::Outcome::Picture:
            m_manager.GetPicture().ShowFramebuffer (result.bgra, result.width, result.height);
            m_isPicture = true;
            break;

        case Win32ShellPreview::Outcome::Failed:
        default:
            ShowBytes();
            break;
    }

    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewShellView::ShowBytes
//
////////////////////////////////////////////////////////////////////////////////

void PreviewShellView::ShowBytes()
{
    KillTimer (m_context.window.GetHwnd(), (UINT_PTR) AppTimerId::ExplorerShellPreview);
    Unload();

    m_context.browser.ShowShellPreviewBytes();
    m_manager.FillPreview();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewShellView::Unload
//
////////////////////////////////////////////////////////////////////////////////

void PreviewShellView::Unload()
{
    m_shellPreview.Unload();
    m_path.clear();
    m_isPicture = false;
}
