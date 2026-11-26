#include "Pch.h"

#include "Core/TextEncoding.h"
#include "Devices/Disk/Inspector/Blocks/BlockInspectorModel.h"
#include "Devices/Disk/Inspector/Repair/RepairCopy.h"
#include "Devices/Disk/Partitions/PhysicalCards.h"
#include "Seams/IHostDialogs.h"
#include "Ui/DiskInspector/Blocks/BlockFileSession.h"
#include "Ui/DiskInspector/Blocks/BlockInspectorState.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFileSession::BlockFileSession
//
////////////////////////////////////////////////////////////////////////////////

BlockFileSession::BlockFileSession (BlockInspectorModel & model, IBlockDialogs & dialogs, IBlockFrame & frame) :
    m_model   (model),
    m_dialogs (dialogs),
    m_frame   (frame)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFileSession::SetFile
//
////////////////////////////////////////////////////////////////////////////////

void BlockFileSession::SetFile (IDiskFileIo & fileIo, const std::wstring & path)
{
    m_fileIo = &fileIo;
    m_path   = path;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFileSession::Attach
//
//  The panel's jobs and view, once the window has created the panel.
//
////////////////////////////////////////////////////////////////////////////////

void BlockFileSession::Attach (IBlockJobs & jobs, IBlockView & view)
{
    m_jobs = &jobs;
    m_view = &view;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFileSession::Open
//
//  A file that doesn't open is reported, and the window closes.
//
////////////////////////////////////////////////////////////////////////////////

void BlockFileSession::Open()
{
    auto                   result = std::make_shared<BlockJobResult>();
    IDiskFileIo          * fileIo = m_fileIo;
    std::string            path   = TextEncoding::WideToUtf8 (m_path);
    BlockInspectorModel  * model  = &m_model;



    m_jobs->QueueJob ({ L"Opening " + GetFileName(), false,
                        [result, fileIo, path, model] (BlockJobProgress & progress)
                        {
                            std::unique_ptr<BlockImageFile>  file;

                            (void) progress;
                            result->hr = BlockImageFile::Open (*fileIo, path, file, result->message);

                            if (SUCCEEDED (result->hr))
                            {
                                result->hr      = model->Open (std::move (file));
                                result->message = FAILED (result->hr) ? L"The image's blocks can't be read." : L"";
                            }
                        },
                        [this, result] ()
                        {
                            if (FAILED (result->hr))
                            {
                                m_dialogs.ShowWarning (result->message.empty() ? L"The image can't be opened in the block inspector." : result->message);
                                m_frame.CloseWindow();
                                return;
                            }

                            m_view->Refresh();
                        } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFileSession::Reload
//
//  A file this window wrote is not read again. A reload that comes in while
//  a job runs is made once the job ends.
//
////////////////////////////////////////////////////////////////////////////////

void BlockFileSession::Reload()
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



    if (m_jobs->IsBusy())
    {
        m_isReloadPending = true;
        return;
    }

    if (!m_model.IsOpen())
    {
        return;
    }

    block = m_model.GetBlock();

    m_jobs->QueueJob ({ L"Reading the file again", false,
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
                            if (*outcome == Outcome::Reopened && m_titleNote == kpszFileChangedNote)
                            {
                                m_titleNote.clear();
                                UpdateTitle();
                            }
                            else if (*outcome == Outcome::EditsPending)
                            {
                                m_titleNote = kpszFileChangedNote;
                                UpdateTitle();
                            }
                            else if (*outcome == Outcome::Gone)
                            {
                                NoteFileGone();
                            }

                            m_view->Refresh();
                        } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFileSession::RunPendingReload
//
//  A reload that came in while a job ran, once no job is running.
//
////////////////////////////////////////////////////////////////////////////////

void BlockFileSession::RunPendingReload()
{
    if (m_isReloadPending && !m_jobs->IsBusy())
    {
        m_isReloadPending = false;
        Reload();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFileSession::NoteFileGone
//
////////////////////////////////////////////////////////////////////////////////

void BlockFileSession::NoteFileGone()
{
    m_titleNote = L" (the file can't be read)";
    UpdateTitle();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFileSession::CompareWithPicked
//
//  The image chosen in an Open dialog, compared with this one.
//
////////////////////////////////////////////////////////////////////////////////

void BlockFileSession::CompareWithPicked()
{
    FileDialogSpec         spec;
    std::filesystem::path  picked;
    bool                   isPicked = false;
    HRESULT                hr       = S_OK;



    spec.title         = L"Compare with";
    spec.initialFolder = std::filesystem::path (m_path).parent_path();
    spec.filters       = { { L"Images of blocks", L"*.po;*.hdv;*.2mg;*.2img;*.dc;*.dc42;*.image" }, { L"All files", L"*.*" } };

    hr = m_dialogs.GetHostDialogs().PickFileToOpen (m_dialogs.GetDialogOwner(), spec, picked, isPicked);

    if (SUCCEEDED (hr) && isPicked)
    {
        CompareWithFile (picked.wstring());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFileSession::CompareWithFile
//
//  Compares the window's image, as A, with another image of blocks. A
//  comparison canceled from its progress has no message.
//
////////////////////////////////////////////////////////////////////////////////

void BlockFileSession::CompareWithFile (const std::wstring & path)
{
    auto                   result = std::make_shared<BlockJobResult>();
    IDiskFileIo          * fileIo = m_fileIo;
    std::string            other  = TextEncoding::WideToUtf8 (path);
    BlockInspectorModel  * model  = &m_model;
    std::wstring           name   = std::filesystem::path (path).filename().wstring();



    m_jobs->QueueJob ({ L"Comparing with " + name, true,
                        [result, fileIo, other, model, name] (BlockJobProgress & progress)
                        {
                            std::unique_ptr<BlockImageFile>  file;

                            if (!BlockImageFile::IsBlockImage (*fileIo, other))
                            {
                                result->hr      = E_INVALIDARG;
                                result->message = name + L" isn't an image of blocks, so it can't be compared with this one.";
                                return;
                            }

                            result->hr = BlockImageFile::Open (*fileIo, other, file, result->message);

                            if (FAILED (result->hr))
                            {
                                result->message = result->message.empty() ? name + L" can't be read." : result->message;
                                return;
                            }

                            result->hr      = model->CompareWith (std::move (file), &progress);
                            result->message = FAILED (result->hr) && result->hr != E_ABORT ? L"The blocks in " + name + L" can't be read." : L"";
                        },
                        [this, result, name] ()
                        {
                            if (!result->message.empty())
                            {
                                m_dialogs.ShowWarning (result->message);
                            }
                            else if (SUCCEEDED (result->hr))
                            {
                                m_titleNote = L" compared with " + name;
                                UpdateTitle();
                            }

                            m_view->Refresh();
                        } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFileSession::EndComparison
//
////////////////////////////////////////////////////////////////////////////////

void BlockFileSession::EndComparison()
{
    m_model.EndComparison();
    m_titleNote.clear();
    UpdateTitle();
    m_view->Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFileSession::UpdateTitle
//
//  The window's title, set only when it changes.
//
////////////////////////////////////////////////////////////////////////////////

void BlockFileSession::UpdateTitle()
{
    size_t        markAt = std::wstring::npos;
    std::wstring  title  = BlockInspectorState::BuildTitle (GetFileName(), m_titleNote, m_model.IsOpen() && !m_model.GetEdits().IsEmpty(), markAt);



    if (title != m_title || markAt != m_titleMark)
    {
        m_title     = title;
        m_titleMark = markAt;
        m_frame.SetCaption (title, markAt);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFileSession::GetFileName
//
//  The display name, such as a card's partition as Casso Explorer lists it,
//  or the file's name.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockFileSession::GetFileName() const
{
    return m_displayName.empty() ? std::filesystem::path (m_path).filename().wstring() : m_displayName;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFileSession::GetHostFileName
//
//  The name of the file a repaired copy is of: for an image in a ZIP, the
//  ZIP's.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockFileSession::GetHostFileName() const
{
    return IsOnCard() ? GetFileName() : std::filesystem::path (TextEncoding::Utf8ToWide (RepairCopy::GetHostPath (TextEncoding::WideToUtf8 (m_path)))).filename().wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFileSession::IsOnCard
//
//  A partition of a storage card is written in place; there is no image
//  file to copy for Save as.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockFileSession::IsOnCard() const
{
    return PhysicalCards::IsOnCard (m_path);
}
