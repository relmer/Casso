#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/Blocks/BlockInspectorInterfaces.h"

class BlockInspectorModel;
class IHostClipboard;
struct BlockJobProgress;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockEditController
//
//  Block mode's edits: the edit bar, the keys while editing, copy and
//  paste, and saving the edits, undoing and redoing a save, and saving an
//  edited copy, each on the worker's thread.
//
////////////////////////////////////////////////////////////////////////////////

class BlockEditController : public IBlockEditing, public IBlockEditNote
{
public:
    //  The parts of block mode that the edits call.
    struct Parts
    {
        BlockInspectorModel  & model;
        IBlockJobs           & jobs;
        IBlockView           & view;
        IBlockDialogs        & dialogs;
        IBlockFrame          & frame;
        IBlockFileInfo       & file;
        IBlockFileSession    & session;
        IHostClipboard       & clipboard;
    };

    explicit BlockEditController (const Parts & parts);

    void  ConnectEditBar ();
    void  UpdateEditBar  ();
    bool  HandleKey      (const DxuiKeyEvent & ev);
    bool  ConfirmUnsaved ();

    void  Save           () override;
    void  SaveEditedCopy () override;
    void  UndoApplied    () override;
    void  RedoApplied    () override;
    void  Discard        () override;
    void  CopyBlock      () override;

    void  SetNote (const std::wstring & note) override { m_note = note; }

private:
    using WriteFn = std::function<HRESULT (BlockInspectorModel & model, std::wstring & outError, BlockJobProgress & progress)>;

    void  StartSave           ();
    void  SaveToPreservedCopy ();
    void  QueueWrite          (const std::wstring & label, const std::wstring & done, WriteFn write);
    void  PasteIntoBlock      ();

    Parts         m_parts;
    std::wstring  m_note;                       // the edit bar's note: the last save's result, or why it failed
    bool          m_isCloseAfterSave = false;
};
