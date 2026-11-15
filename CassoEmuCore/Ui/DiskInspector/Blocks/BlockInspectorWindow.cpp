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
    HRESULT                   hr = S_OK;
    DxuiWindow::CreateParams  params;



    m_fileIo = &fileIo;
    m_path   = path;
    m_theme  = theme;

    outError.clear();

    params.title                    = GetFileName() + L" - Disk inspector";
    params.hInstance                = hInstance;
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { kWidthDip, kHeightDip };
    params.minSizeDip               = { kMinWidthDip, kMinHeightDip };
    params.resizable                = true;
    params.insetContentBelowCaption = true;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = Create (params);
    CHRAF (hr, outError = L"The disk inspector's window can't be created.");

    SetTheme (theme);
    ShowModelessDialog (kApplyId);
    QueueOpen();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::QueueOpen
//
//  A file that doesn't open is reported, and the window closes.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::QueueOpen()
{
    auto                   result = std::make_shared<JobResult>();
    IDiskFileIo          * fileIo = m_fileIo;
    std::string            path   = TextEncoding::WideToUtf8 (m_path);
    BlockInspectorModel  * model  = &m_model;



    m_panel->QueueJob ({ L"Opening " + GetFileName(), false,
                         [result, fileIo, path, model] (BlockJobProgress & progress)
                         {
                             std::unique_ptr<BlockImageFile>  file;

                             (void) progress;
                             result->hr = BlockImageFile::Open (*fileIo, path, file, result->message);

                             if (SUCCEEDED (result->hr))
                             {
                                 result->hr = model->Open (std::move (file));
                                 result->message = FAILED (result->hr) ? L"The image's blocks can't be read." : L"";
                             }
                         },
                         [this, result] ()
                         {
                             if (FAILED (result->hr))
                             {
                                 ShowMessage (result->message.empty() ? L"The image can't be opened in block mode." : result->message);
                                 Close();
                                 return;
                             }

                             m_panel->Refresh();
                             UpdateButtons();
                         } });
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

    m_panel->Init (&m_model, &m_worker, m_theme, GetHwnd(), GetTextRenderer());
    m_panel->SetOnChildPressed ([this] (IDxuiControl * child) { FocusControl (child); });
    m_panel->SetOnChanged      ([this]() { m_isDirty = true; UpdateButtons(); });
    m_panel->SetOnSaveRun      ([this] (const LeftoverRun & run) { SaveRun (run); });
    m_panel->SetOnJobQueued    ([this]() { SetDialogTickIntervalMs (kBusyTickMs); });

    m_apply->SetOnClick   ([this]() { Apply(); });
    m_discard->SetOnClick ([this]()
    {
        if (!m_worker.IsBusy())
        {
            m_model.DiscardEdits();
            m_panel->Refresh();
        }
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
//  BlockInspectorWindow::OnDialogTick
//
//  Each finished job's completion, then a reload that came in while a job
//  ran. The tick goes back to its usual interval once no job is left.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::OnDialogTick()
{
    m_worker.RunFinished();

    if (m_isReloadPending && !m_worker.IsBusy())
    {
        m_isReloadPending = false;
        Reload();
    }

    if (m_panel != nullptr)
    {
        m_panel->UpdateBusy();
    }

    if (!m_worker.IsBusy())
    {
        SetDialogTickIntervalMs (kDefaultDialogTickMs);
    }

    UpdateButtons();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::UpdateButtons
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::UpdateButtons()
{
    bool  hasEdits = !m_worker.IsBusy() && m_model.IsOpen() && !m_model.GetEdits().IsEmpty();



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
    auto                   result = std::make_shared<JobResult>();
    BlockInspectorModel  * model  = &m_model;



    if (m_worker.IsBusy())
    {
        return;
    }

    m_panel->QueueJob ({ L"Writing the edits", false,
                         [result, model] (BlockJobProgress & progress) { result->hr = model->ApplyEdits (result->message, &progress); },
                         [this, result] ()
                         {
                             if (FAILED (result->hr))
                             {
                                 ShowMessage (result->message.empty() ? L"The edits weren't written." : result->message);
                             }

                             m_panel->Refresh();
                             UpdateButtons();
                         } });

    UpdateButtons();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::ConfirmClose
//
//  A job that can be canceled is canceled; while one that can't be is
//  running, the window stays open.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockInspectorWindow::ConfirmClose()
{
    int           bytes     = 0;
    int           answer    = IDYES;
    std::wstring  label;
    bool          canCancel = false;
    uint64_t      done      = 0;
    uint64_t      total     = 0;



    if (m_worker.TryGetLongJob (label, canCancel, done, total) && !canCancel)
    {
        ShowMessage (label + L" hasn't finished. Close the window after it finishes.");
        return false;
    }

    m_worker.Cancel();

    bytes = m_model.IsOpen() ? m_model.GetEdits().CountChangedBytes() : 0;

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

    m_worker.Cancel();
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
//  A file this window wrote is not read again. A reload that comes in while
//  a job runs is made once the job ends.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::Reload()
{
    enum class Outcome
    {
        Unchanged,
        EditsPending,
        Gone,
        Reopened,
    };

    auto                   outcome = std::make_shared<Outcome> (Outcome::Unchanged);
    IDiskFileIo          * fileIo  = m_fileIo;
    std::string            path    = TextEncoding::WideToUtf8 (m_path);
    BlockInspectorModel  * model   = &m_model;
    uint32_t               block   = 0;



    if (m_worker.IsBusy())
    {
        m_isReloadPending = true;
        return;
    }

    if (!m_model.IsOpen())
    {
        return;
    }

    block = m_model.GetBlock();

    m_panel->QueueJob ({ L"Reading the file again", false,
                         [outcome, fileIo, path, model, block] (BlockJobProgress & progress)
                         {
                             std::unique_ptr<BlockImageFile>  file;
                             std::wstring                     error;
                             HRESULT                          hr = S_OK;

                             (void) progress;

                             if (model->GetFile().IsFileUnchanged())
                             {
                                 return;
                             }

                             if (!model->GetEdits().IsEmpty())
                             {
                                 *outcome = Outcome::EditsPending;
                                 return;
                             }

                             hr = BlockImageFile::Open (*fileIo, path, file, error);

                             if (SUCCEEDED (hr))
                             {
                                 hr = model->Open (std::move (file));
                             }

                             if (SUCCEEDED (hr))
                             {
                                 model->SelectBlock (block);
                             }

                             *outcome = SUCCEEDED (hr) ? Outcome::Reopened : Outcome::Gone;
                         },
                         [this, outcome] ()
                         {
                             if (*outcome == Outcome::EditsPending)
                             {
                                 SetTitle (GetFileName() + L" - Disk inspector (the file changed; apply or discard to read it again)");
                             }
                             else if (*outcome == Outcome::Gone)
                             {
                                 NoteFileGone();
                             }

                             m_panel->Refresh();
                         } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::NoteFileGone
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::NoteFileGone()
{
    SetTitle (GetFileName() + L" - Disk inspector (the file can't be read)");
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





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::ShowMessage
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::ShowMessage (const std::wstring & text)
{
    int  shown = DxuiMessageBox (GetHwnd(), m_theme, text.c_str(), L"Disk inspector", MB_OK | MB_ICONWARNING);



    IGNORE_RETURN_VALUE (shown, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::CompareWithFile
//
//  A comparison canceled from its progress has no message.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::CompareWithFile (const std::wstring & path)
{
    auto                   result = std::make_shared<JobResult>();
    IDiskFileIo          * fileIo = m_fileIo;
    std::string            other  = TextEncoding::WideToUtf8 (path);
    BlockInspectorModel  * model  = &m_model;
    std::wstring           name   = fs::path (path).filename().wstring();



    m_panel->QueueJob ({ L"Comparing with " + name, true,
                         [result, fileIo, other, model] (BlockJobProgress & progress)
                         {
                             std::unique_ptr<BlockImageFile>  file;

                             if (!BlockImageFile::IsBlockImage (*fileIo, other))
                             {
                                 result->hr      = E_INVALIDARG;
                                 result->message = L"Only another image of blocks can be compared with this one.";
                                 return;
                             }

                             result->hr = BlockImageFile::Open (*fileIo, other, file, result->message);

                             if (FAILED (result->hr))
                             {
                                 result->message = result->message.empty() ? L"The other image can't be read." : result->message;
                                 return;
                             }

                             result->hr      = model->CompareWith (std::move (file), &progress);
                             result->message = FAILED (result->hr) && result->hr != E_ABORT ? L"The other image's blocks can't be read." : L"";
                         },
                         [this, result, name] ()
                         {
                             if (!result->message.empty())
                             {
                                 ShowMessage (result->message);
                             }
                             else if (SUCCEEDED (result->hr))
                             {
                                 SetTitle (GetFileName() + L" compared with " + name + L" - Disk inspector");
                             }

                             m_panel->Refresh();
                         } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow::SaveRun
//
//  A run of free blocks with leftover data, saved as a file the user picks.
//
////////////////////////////////////////////////////////////////////////////////

void BlockInspectorWindow::SaveRun (LeftoverRun run)
{
    HRESULT                hr       = S_OK;
    FileDialogSpec         spec;
    std::filesystem::path  path;
    bool                   isPicked = false;
    bool                   isText   = run.kind == LeftoverKind::Text;
    auto                   result   = std::make_shared<JobResult>();
    IDiskFileIo          * fileIo   = m_fileIo;
    BlockInspectorModel  * model    = &m_model;



    spec.title            = L"Save leftover data";
    spec.defaultExtension = isText ? L"txt" : L"bin";
    spec.defaultFileName  = L"Blocks " + std::to_wstring (run.first) + L" to " + std::to_wstring (run.first + run.count - 1) + (isText ? L".txt" : L".bin");
    spec.initialFolder    = fs::path (m_path).parent_path();
    spec.filters          = { { isText ? L"Text" : L"Binary", isText ? L"*.txt" : L"*.bin" }, { L"All files", L"*.*" } };

    hr = m_dialogs.PickFileToSave (GetHwnd(), spec, path, isPicked);
    CHRF (hr, ShowMessage (L"The save dialog can't be shown."));
    BAIL_OUT_IF (!isPicked, S_OK);

    m_panel->QueueJob ({ L"Saving the leftover data", false,
                         [result, fileIo, model, run, target = TextEncoding::WideToUtf8 (path.wstring())] (BlockJobProgress & progress)
                         {
                             vector<Byte>  bytes;

                             (void) progress;
                             result->hr = model->ReadRun (run, bytes);

                             if (FAILED (result->hr))
                             {
                                 result->message = L"The leftover blocks can't be read.";
                                 return;
                             }

                             result->hr      = fileIo->WriteAllBytes (target, bytes);
                             result->message = FAILED (result->hr) ? L"The file can't be written." : L"";
                         },
                         [this, result] ()
                         {
                             if (!result->message.empty())
                             {
                                 ShowMessage (result->message);
                             }
                         } });

Error:
    return;
}
