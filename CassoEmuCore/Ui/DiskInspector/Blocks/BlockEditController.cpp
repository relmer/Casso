#include "Pch.h"

#include "Core/TextEncoding.h"
#include "Devices/Disk/Inspector/Blocks/BlockInspectorModel.h"
#include "Devices/Disk/Inspector/InspectorClipboard.h"
#include "Devices/Disk/PreservedCopy.h"
#include "Seams/IHostClipboard.h"
#include "Seams/IHostDialogs.h"
#include "Ui/DiskInspector/Blocks/BlockEditController.h"
#include "Ui/DiskInspector/Blocks/BlockHeaderView.h"
#include "Ui/DiskInspector/Blocks/BlockHexView.h"
#include "Ui/DiskInspector/Blocks/BlockInspectorState.h"
#include "Ui/DiskInspector/InspectorText.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockEditController::BlockEditController
//
////////////////////////////////////////////////////////////////////////////////

BlockEditController::BlockEditController (const Parts & parts) :
    m_parts (parts)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockEditController::ConnectEditControls
//
//  The block pane's edit buttons: Edit and Done, Undo block changes, and
//  Use B's bytes. Saving and discarding every edit are in the File and Edit
//  menus.
//
////////////////////////////////////////////////////////////////////////////////

void BlockEditController::ConnectEditControls()
{
    BlockHeaderView  & header = m_parts.view.GetHeader();



    header.SetOnEdit     ([this] () { if (!m_parts.jobs.IsBusy()) { m_parts.view.SetEditing (!m_parts.view.IsEditing()); } });
    header.SetOnUndo     ([this] () { UndoBlock(); });
    header.SetOnUseOther ([this] () { if (!m_parts.jobs.IsBusy() && m_parts.model.UseOtherBlock (m_parts.model.GetBlock())) { m_parts.view.Refresh(); } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockEditController::UndoBlock
//
//  The current block's pending edits are dropped.
//
////////////////////////////////////////////////////////////////////////////////

void BlockEditController::UndoBlock()
{
    if (!m_parts.jobs.IsBusy() && m_parts.model.IsOpen())
    {
        m_parts.model.DiscardBlock (m_parts.model.GetBlock());
        m_parts.view.Refresh();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockEditController::UpdateEditControls
//
//  The edit controls from the pending edits, once a frame.
//
////////////////////////////////////////////////////////////////////////////////

void BlockEditController::UpdateEditControls()
{
    m_parts.view.SetEditState (BlockInspectorState::BuildEditState (m_parts.model, m_parts.jobs.IsBusy(), m_parts.view.IsEditing(), m_note));
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockEditController::HandleKey
//
//  While editing in the hex view: Ctrl+Z and Ctrl+Y for the pending edits,
//  Ctrl+V to paste, and Escape to stop editing, as the Sector data tab has.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockEditController::HandleKey (const DxuiKeyEvent & ev)
{
    bool  isHandled = true;



    if (ev.ctrl && !ev.alt && !ev.shift && ev.vk == 'Z')
    {
        Undo();
    }
    else if (ev.ctrl && !ev.alt && (ev.vk == 'Y' || (ev.shift && ev.vk == 'Z')))
    {
        Redo();
    }
    else if (ev.ctrl && !ev.alt && ev.vk == 'V')
    {
        PasteIntoBlock();
    }
    else if (!ev.ctrl && !ev.alt && ev.vk == VK_ESCAPE)
    {
        m_parts.view.SetEditing (false);
    }
    else
    {
        isHandled = false;
    }

    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockEditController::ConfirmUnsaved
//
//  With unsaved changes, the user picks Save, Discard or Cancel. True when
//  the window closes now; after Save, it closes once the save succeeds.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockEditController::ConfirmUnsaved()
{
    int                 blocks = m_parts.model.IsOpen() ? (int) m_parts.model.GetEdits().GetChangedBlocks().size() : 0;
    BlockUnsavedChoice  choice = BlockUnsavedChoice::Cancel;



    if (blocks == 0)
    {
        return true;
    }

    choice = m_parts.dialogs.AskUnsaved (InspectorText::FormatUnsavedChanges (blocks, L"block", L"blocks", m_parts.file.GetFileName()));

    if (choice == BlockUnsavedChoice::Save)
    {
        m_isCloseAfterSave = true;
        StartSave();
    }

    return choice == BlockUnsavedChoice::Discard;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockEditController::Save
//
//  The unsaved changes are counted in the status bar and marked in the
//  title, so a save writes them without a question.
//
////////////////////////////////////////////////////////////////////////////////

void BlockEditController::Save()
{
    if (!m_parts.jobs.IsBusy() && m_parts.model.IsOpen() && !m_parts.model.GetEdits().IsEmpty())
    {
        StartSave();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockEditController::StartSave
//
//  When an image file changed outside Casso after it was opened, the edits
//  go to a preserved copy beside it, as the disk inspector saves them, and
//  the file is left as it is.
//
////////////////////////////////////////////////////////////////////////////////

void BlockEditController::StartSave()
{
    if (!m_parts.file.IsOnCard() && !m_parts.model.GetFile().IsFileUnchanged())
    {
        SaveToPreservedCopy();
        return;
    }

    QueueWrite (L"Saving the edits", L"Saved to", [] (BlockInspectorModel & model, std::wstring & outError, BlockJobProgress & progress)
                { return model.ApplyEdits (outError, &progress); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockEditController::SaveToPreservedCopy
//
//  A copy of the file as it is now, with the edited blocks written into it.
//  The edits are then saved, so the window reads the changed file again.
//
////////////////////////////////////////////////////////////////////////////////

void BlockEditController::SaveToPreservedCopy()
{
    auto                   result = std::make_shared<BlockJobResult>();
    BlockInspectorModel  * model  = &m_parts.model;
    IDiskFileIo          * fileIo = &m_parts.file.GetFileIo();
    fs::path               image  = m_parts.file.GetPath();
    std::string            stamp  = PreservedCopy::MakeStamp (time (nullptr));
    fs::path               target;
    fs::path               candidate;
    std::error_code        error;



    for (int attempt = 0; attempt < PreservedCopy::kMaxAttempts && target.empty(); attempt++)
    {
        candidate = TextEncoding::Utf8ToWide (PreservedCopy::MakePath (TextEncoding::WideToUtf8 (image.wstring()), stamp, attempt));
        target    = fs::exists (candidate, error) ? fs::path() : candidate;
    }

    if (target.empty())
    {
        m_isCloseAfterSave = false;
        m_note             = L"Not saved: the file changed outside Casso, and a preserved copy can't be named.";
        m_parts.view.Refresh();
        return;
    }

    m_note = L"Saving...";
    m_parts.view.SetEditing (false);

    m_parts.jobs.QueueJob ({ L"Saving a preserved copy", false,
                             [result, model, fileIo, image, target] (BlockJobProgress & progress)
                             {
                                 (void) progress;

                                 if (!CopyFileW (image.c_str(), target.c_str(), TRUE))
                                 {
                                     result->hr = HRESULT_FROM_WIN32 (GetLastError());
                                     return;
                                 }

                                 result->hr = model->WriteEditsTo (*fileIo, TextEncoding::WideToUtf8 (target.wstring()), result->message);
                             },
                             [this, result, target] ()
                             {
                                 bool  isClosing = m_isCloseAfterSave && SUCCEEDED (result->hr);

                                 m_isCloseAfterSave = false;
                                 m_note             = SUCCEEDED (result->hr) ? L"Saved to a preserved copy, since the file changed outside Casso: " + target.filename().wstring()
                                                                             : L"Not saved: the copy could not be written to " + target.filename().wstring();

                                 if (SUCCEEDED (result->hr))
                                 {
                                     m_parts.model.ClearEdits();
                                     m_parts.session.Reload();
                                 }

                                 m_parts.view.Refresh();

                                 if (isClosing)
                                 {
                                     m_parts.frame.CloseWindow();
                                 }
                             } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockEditController::Undo
//
//  Undoes the last unsaved edit, in or out of editing. With no unsaved edit
//  to undo, undoes the last save.
//
////////////////////////////////////////////////////////////////////////////////

void BlockEditController::Undo()
{
    bool      canUndo = m_parts.model.GetEdits().CanUndo();
    uint32_t  block   = 0;
    int       offset  = 0;



    if (m_parts.jobs.IsBusy())
    {
        return;
    }

    if (!canUndo)
    {
        UndoApplied();
        return;
    }

    m_note.clear();

    if (m_parts.model.GetEdits().GetUndoAt (block, offset) && m_parts.model.Undo())
    {
        m_parts.view.ShowBlockByte (block, offset);
        m_parts.view.Refresh();
    }
    else if (m_parts.model.Undo())
    {
        m_parts.view.Refresh();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockEditController::Redo
//
//  Redoes the last undone edit, in or out of editing. With no edit to redo,
//  redoes the last undone save.
//
////////////////////////////////////////////////////////////////////////////////

void BlockEditController::Redo()
{
    bool      canRedo = m_parts.model.GetEdits().CanRedo();
    uint32_t  block   = 0;
    int       offset  = 0;



    if (m_parts.jobs.IsBusy())
    {
        return;
    }

    if (!canRedo)
    {
        RedoApplied();
        return;
    }

    m_note.clear();

    if (m_parts.model.GetEdits().GetRedoAt (block, offset) && m_parts.model.Redo())
    {
        m_parts.view.ShowBlockByte (block, offset);
        m_parts.view.Refresh();
    }
    else if (m_parts.model.Redo())
    {
        m_parts.view.Refresh();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockEditController::UndoApplied
//
////////////////////////////////////////////////////////////////////////////////

void BlockEditController::UndoApplied()
{
    if (m_parts.jobs.IsBusy() || !m_parts.model.CanUndoApplied())
    {
        return;
    }

    QueueWrite (L"Undoing the last save", L"Undid the save to", [] (BlockInspectorModel & model, std::wstring & outError, BlockJobProgress & progress)
                { return model.UndoApplied (outError, &progress); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockEditController::RedoApplied
//
////////////////////////////////////////////////////////////////////////////////

void BlockEditController::RedoApplied()
{
    if (m_parts.jobs.IsBusy() || !m_parts.model.CanRedoApplied())
    {
        return;
    }

    QueueWrite (L"Saving the edits", L"Saved to", [] (BlockInspectorModel & model, std::wstring & outError, BlockJobProgress & progress)
                { return model.RedoApplied (outError, &progress); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockEditController::QueueWrite
//
//  A save, undo or redo on the worker's thread. Its result goes in the
//  edit controls' note, as "Saved to" the file. A save chosen when closing the
//  window closes it once the save succeeds.
//
////////////////////////////////////////////////////////////////////////////////

void BlockEditController::QueueWrite (const std::wstring & label, const std::wstring & done, WriteFn write)
{
    auto                   result = std::make_shared<BlockJobResult>();
    BlockInspectorModel  * model  = &m_parts.model;



    m_note.clear();
    m_parts.view.SetEditing (false);

    m_parts.jobs.QueueJob ({ label, false,
                             [result, model, write] (BlockJobProgress & progress) { result->hr = write (*model, result->message, progress); },
                             [this, result, done] ()
                             {
                                 bool  isClosing = m_isCloseAfterSave && SUCCEEDED (result->hr);

                                 m_isCloseAfterSave = false;
                                 m_note             = SUCCEEDED (result->hr) ? done + L" " + m_parts.file.GetFileName()
                                                                             : (result->message.empty() ? L"The edits weren't saved." : result->message);
                                 m_parts.view.Refresh();

                                 if (isClosing)
                                 {
                                     m_parts.frame.CloseWindow();
                                 }
                             } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockEditController::SaveEditedCopy
//
//  The image with the pending edits saved to a new file the user chooses,
//  never the image's file. The pending edits stay pending, and the image's
//  file is left as it was.
//
////////////////////////////////////////////////////////////////////////////////

void BlockEditController::SaveEditedCopy()
{
    auto                   result    = std::make_shared<BlockJobResult>();
    BlockInspectorModel  * model     = &m_parts.model;
    IDiskFileIo          * fileIo    = &m_parts.file.GetFileIo();
    fs::path               image     = m_parts.file.GetPath();
    std::wstring           extension = image.has_extension() ? image.extension().wstring().substr (1) : std::wstring (L"po");
    FileDialogSpec         spec;
    fs::path               target;
    bool                   isPicked  = false;
    bool                   isTaken   = true;
    std::error_code        error;
    HRESULT                hr        = S_OK;



    BAIL_OUT_IF (m_parts.jobs.IsBusy() || !m_parts.model.IsOpen() || m_parts.model.GetEdits().IsEmpty() || m_parts.file.IsOnCard(), S_OK);

    spec.filters          = { { L"Disk images", L"*." + extension } };
    spec.defaultExtension = extension;
    spec.defaultFileName  = image.stem().wstring() + L" edited." + extension;
    spec.initialFolder    = image.parent_path();

    //  The Save dialog opens again until the file isn't the image's.
    while (isTaken)
    {
        hr = m_parts.dialogs.GetHostDialogs().PickFileToSave (m_parts.dialogs.GetDialogOwner(), spec, target, isPicked);
        CHR (hr);
        BAIL_OUT_IF (!isPicked, S_OK);

        isTaken = fs::equivalent (target, image, error) || _wcsicmp (fs::absolute (target, error).wstring().c_str(), fs::absolute (image, error).wstring().c_str()) == 0;

        if (isTaken)
        {
            m_parts.dialogs.ShowWarning (L"That file is the image this window opened. Choose another file for the copy.");
        }
    }

    m_note = L"Preparing the copy...";

    m_parts.jobs.QueueJob ({ L"Saving the edited copy", false,
                             [result, model, fileIo, image, target] (BlockJobProgress & progress)
                             {
                                 (void) progress;

                                 if (!CopyFileW (image.c_str(), target.c_str(), FALSE))
                                 {
                                     result->hr = HRESULT_FROM_WIN32 (GetLastError());
                                     return;
                                 }

                                 result->hr = model->WriteEditsTo (*fileIo, TextEncoding::WideToUtf8 (target.wstring()), result->message);
                             },
                             [this, result, target] ()
                             {
                                 m_note = SUCCEEDED (result->hr) ? L"Saved a copy with the pending edits to " + target.filename().wstring()
                                                                 : L"Not saved: the copy could not be written to " + target.filename().wstring();
                                 m_parts.view.Refresh();
                             } });

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockEditController::Discard
//
////////////////////////////////////////////////////////////////////////////////

void BlockEditController::Discard()
{
    if (!m_parts.jobs.IsBusy() && m_parts.model.IsOpen())
    {
        m_parts.model.DiscardEdits();
        m_parts.view.Refresh();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockEditController::CopyBlock
//
////////////////////////////////////////////////////////////////////////////////

void BlockEditController::CopyBlock()
{
    std::wstring  text = m_parts.jobs.IsBusy() ? std::wstring() : m_parts.view.BuildBlockText();



    if (!text.empty())
    {
        (void) m_parts.clipboard.SetText (m_parts.dialogs.GetDialogOwner(), text);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockEditController::PasteIntoBlock
//
//  Ctrl+V while editing: the clipboard's hex digits, or its text in the
//  text column, over the block from the caret, as the sector editor
//  pastes.
//
////////////////////////////////////////////////////////////////////////////////

void BlockEditController::PasteIntoBlock()
{
    BlockHexView         * hex      = m_parts.view.GetHexView();
    BlockInspectorModel  & model    = m_parts.model;
    bool                   isText   = hex != nullptr && hex->GetActiveColumn() == DxuiHexView::Column::Text;
    std::wstring           text;
    vector<Byte>           values;
    int                    overflow = 0;
    int                    written  = 0;



    if (hex == nullptr || m_parts.jobs.IsBusy() || !model.IsOpen() || !model.IsLoaded (model.GetBlock()) || !m_parts.clipboard.GetText (m_parts.dialogs.GetDialogOwner(), text))
    {
        return;
    }

    written = model.Paste ((int) hex->GetCaret(), text, isText, overflow);
    m_note  = (written == 0) ? (isText ? std::wstring (L"Nothing to paste: the clipboard has no printable text") : InspectorClipboard::GetHexPasteNote (InspectorClipboard::ParseHex (text, values)))
            : (overflow > 0) ? InspectorText::FormatCount (overflow, L"byte", L"bytes") + L" did not fit before the end of the block"
                             : std::wstring();

    hex->ResetNibble();
    m_parts.view.Refresh();
}
