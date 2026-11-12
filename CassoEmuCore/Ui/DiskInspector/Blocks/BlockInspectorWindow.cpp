#include "Pch.h"

#include "Core/TextEncoding.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "Ui/DiskInspector/Blocks/BlockHexView.h"
#include "Ui/DiskInspector/Blocks/BlockInspectorPanel.h"
#include "Ui/DiskInspector/Blocks/BlockInspectorWindow.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::BlockInspectorWindow
//
////////////////////////////////////////////////////////////////////////////////

BlockInspectorWindow::BlockInspectorWindow()  = default;
BlockInspectorWindow::~BlockInspectorWindow() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::OpenModel
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockInspectorWindow::OpenModel()
{
    HRESULT                          hr    = S_OK;
    std::unique_ptr<BlockImageFile>  file;
    std::wstring                     error;



    hr = BlockImageFile::Open (*m_fileIo, TextEncoding::WideToUtf8 (m_path), file, error);
    CHR (hr);

    hr = m_model.Open (std::move (file));
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::Open
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockInspectorWindow::Open (
    HINSTANCE             hInstance,
    HWND                  owner,
    const IDxuiTheme    * theme,
    IDiskFileIo         & fileIo,
    const std::wstring  & path,
    std::wstring        & outError)
{
    HRESULT                          hr     = S_OK;
    std::unique_ptr<BlockImageFile>  file;
    DxuiWindow::CreateParams         params;



    m_fileIo = &fileIo;
    m_path   = path;
    m_theme  = theme;

    hr = BlockImageFile::Open (fileIo, TextEncoding::WideToUtf8 (path), file, outError);
    CHR (hr);

    hr = m_model.Open (std::move (file));
    CHRF (hr, outError = L"The image's blocks can't be read.");

    params.title                    = fs::path (path).filename().wstring() + L" - Disk inspector";
    params.hInstance                = hInstance;
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { kWidthDip, kHeightDip };
    params.minSizeDip               = { kMinWidthDip, kMinHeightDip };
    params.resizable                = true;
    params.insetContentBelowCaption = true;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = Create (params);
    CHRA (hr);

    SetTheme (theme);
    ShowModelessDialog (kApplyId);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::OnCreate
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::OnCreate()
{
    DxuiButton  * close = nullptr;



    m_panel   = CreateDialogContent<BlockInspectorPanel>();
    m_apply   = AddDialogButton (L"Apply edits", kApplyId);
    m_discard = AddDialogButton (L"Discard edits", kDiscardId);
    close     = AddDialogButton (L"Close", IDCANCEL);

    m_panel->Init (&m_model, m_theme, GetHwnd(), GetTextRenderer());
    m_panel->SetOnChildPressed ([this] (IDxuiControl * child) { FocusControl (child); });
    m_panel->SetOnChanged      ([this]() { m_isDirty = true; UpdateButtons(); });

    m_apply->SetOnClick   ([this]() { Apply(); });
    m_discard->SetOnClick ([this]()
    {
        m_model.DiscardEdits();
        m_panel->Refresh();
    });

    close->SetOnClick ([this]()
    {
        if (ConfirmClose())
        {
            Close();
        }
    });

    SetInitialFocus (m_panel->GetHexView());
    UpdateButtons();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::UpdateButtons
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::UpdateButtons()
{
    bool  hasEdits = !m_model.GetEdits().IsEmpty();



    if (m_apply != nullptr)
    {
        m_apply->SetEnabled   (hasEdits);
        m_discard->SetEnabled (hasEdits);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::Apply
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::Apply()
{
    HRESULT       hr = S_OK;
    std::wstring  error;
    int           ok = 0;



    hr = m_model.ApplyEdits (error);

    if (FAILED (hr))
    {
        ok = DxuiMessageBox (GetHwnd(), m_theme, error.empty() ? L"The edits weren't written." : error.c_str(), L"Disk inspector", MB_OK | MB_ICONWARNING);
        IGNORE_RETURN_VALUE (ok, 0);
    }

    m_panel->Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::ConfirmClose
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorWindow::ConfirmClose()
{
    int  bytes  = m_model.GetEdits().CountChangedBytes();
    int  answer = IDYES;



    if (bytes > 0)
    {
        answer = DxuiMessageBox (GetHwnd(), m_theme,
                                 (std::to_wstring (bytes) + (bytes == 1 ? L" changed byte isn't" : L" changed bytes aren't") + L" written. Close and discard the edits?").c_str(),
                                 L"Disk inspector", MB_YESNO | MB_ICONWARNING, { L"Discard edits", L"Keep editing" }, 0);
    }

    return answer == IDYES;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::Close
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::Close()
{
    if (m_isClosed)
    {
        return;
    }

    m_isClosed = true;

    EndDialog (IDCANCEL);

    if (m_onClosed)
    {
        m_onClosed();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::OnWindowClose
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::OnWindowClose()
{
    if (ConfirmClose())
    {
        Close();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::Reload
//
//  A file this window wrote is not read again.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::Reload()
{
    HRESULT   hr    = S_OK;
    uint32_t  block = m_model.GetBlock();



    if (m_model.GetFile().IsFileUnchanged())
    {
        return;
    }

    if (!m_model.GetEdits().IsEmpty())
    {
        SetTitle (fs::path (m_path).filename().wstring() + L" - Disk inspector (the file changed; apply or discard to read it again)");
        return;
    }

    hr = OpenModel();

    if (FAILED (hr))
    {
        NoteFileGone();
        return;
    }

    m_model.SelectBlock (block);
    m_panel->Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::NoteFileGone
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::NoteFileGone()
{
    SetTitle (fs::path (m_path).filename().wstring() + L" - Disk inspector (the file can't be read)");
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::ChangeTheme
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::ChangeTheme (const IDxuiTheme * theme)
{
    m_theme = theme;

    SetTheme (theme);
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::RenderIfChanged
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::RenderIfChanged()
{
    if (m_isDirty && GetHwnd() != nullptr)
    {
        m_isDirty = false;
        Invalidate();
    }
}
