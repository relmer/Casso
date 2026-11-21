#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/Blocks/BlockInspectorInterfaces.h"

class BlockInspectorModel;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFileSession
//
//  The image file block mode opened: opening it, reading it again after it
//  changes outside the window, comparing it with another image, and the
//  window's title with its notes.
//
////////////////////////////////////////////////////////////////////////////////

class BlockFileSession : public IBlockFileInfo, public IBlockFileSession
{
public:
    BlockFileSession (BlockInspectorModel & model, IBlockDialogs & dialogs, IBlockFrame & frame);

    void  SetFile          (IDiskFileIo & fileIo, const std::wstring & path);
    void  SetDisplayName   (const std::wstring & name) { m_displayName = name; }
    void  Attach           (IBlockJobs & jobs, IBlockView & view);
    void  Open             ();
    void  RunPendingReload ();
    void  NoteFileGone     ();
    void  CompareWithFile  (const std::wstring & path);
    void  UpdateTitle      ();

    void  Reload            () override;
    void  CompareWithPicked () override;
    void  EndComparison     () override;

    const std::wstring  & GetPath         () const override { return m_path; }
    std::wstring          GetFileName     () const override;
    std::wstring          GetHostFileName () const override;
    bool                  IsOnCard        () const override;
    IDiskFileIo         & GetFileIo       () const override { return *m_fileIo; }

private:
    static constexpr const wchar_t *  kpszFileChangedNote = L" (the file changed; save or discard to read it again)";

    BlockInspectorModel  & m_model;
    IBlockDialogs        & m_dialogs;
    IBlockFrame          & m_frame;
    IBlockJobs           * m_jobs            = nullptr;
    IBlockView           * m_view            = nullptr;
    IDiskFileIo          * m_fileIo          = nullptr;
    std::wstring           m_path;
    std::wstring           m_displayName;
    std::wstring           m_titleNote;                  // after the name in the title, as the image compared with
    std::wstring           m_title;
    size_t                 m_titleMark       = std::wstring::npos;
    bool                   m_isReloadPending = false;
};
