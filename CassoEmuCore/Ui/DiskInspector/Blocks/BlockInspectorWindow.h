#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/Blocks/BlockInspectorModel.h"
#include "Seams/Win32HostDialogs.h"

class BlockInspectorPanel;
class IDiskFileIo;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow
//
//  The disk inspector in block mode, for an image file of blocks: the
//  content panel, and Apply edits, Discard edits and Close along the bottom.
//  Applied edits are written to the file in place. Closing with edits
//  pending prompts to discard them. The window is modeless, and the closed
//  callback is called when it closes.
//
//  Every read and write of the image runs on m_worker's thread: opening it,
//  each block, Find leftover data, comparing, applying edits, reading it
//  again and saving leftover data. Each job's completion runs on the
//  dialog's tick, which is shortened while a job is queued. The progress of
//  a long job is drawn in the panel. Closing cancels a job that can be
//  canceled. While a job that can't be is running, the window doesn't close.
//
////////////////////////////////////////////////////////////////////////////////

class BlockInspectorWindow : public DxuiDialogWindow
{
public:
    static constexpr int   kWidthDip     = 1180;
    static constexpr int   kHeightDip    = 760;
    static constexpr int   kMinWidthDip  = 960;
    static constexpr int   kMinHeightDip = 600;
    static constexpr int   kApplyId      = 1001;
    static constexpr int   kDiscardId    = 1002;
    static constexpr UINT  kBusyTickMs   = 16;

    BlockInspectorWindow  ();
    ~BlockInspectorWindow () override;

    //  Opens the window over `owner`, and the file on the worker's thread.
    //  A file that doesn't open in block mode is reported in a message, and
    //  the window closes. Fails, with the reason, when the window can't be
    //  created.
    HRESULT  Open (HINSTANCE             hInstance,
                   HWND                  owner,
                   const IDxuiTheme    * theme,
                   IDiskFileIo         & fileIo,
                   const std::wstring  & path,
                   std::wstring        & outError);

    void  SetOnClosed (std::function<void ()> fn) { m_onClosed = std::move (fn); }

    //  The file read again when it changed and no edits are pending; with
    //  edits pending, a note in the title.
    void  Reload       ();
    void  NoteFileGone ();

    //  Compares the window's image, as A, with another image of blocks.
    void  CompareWithFile (const std::wstring & path);
    void  ChangeTheme     (const IDxuiTheme * theme);

    //  Repaints after a change the panel reported.
    void  RenderIfChanged ();

protected:
    void  OnCreate      () override;
    void  OnWindowClose () override;
    void  OnDialogTick  () override;

private:
    struct JobResult
    {
        HRESULT       hr = S_OK;
        std::wstring  message;
    };

    void          QueueOpen     ();
    void          Apply         ();
    void          UpdateButtons ();
    bool          ConfirmClose  ();
    void          Close         ();
    void          SaveRun       (LeftoverRun run);
    void          ShowMessage   (const std::wstring & text);
    std::wstring  GetFileName   () const { return fs::path (m_path).filename().wstring(); }

    //  m_worker follows m_model, so it is destroyed first and its thread
    //  ends before the model goes away.
    BlockInspectorModel       m_model;
    BlockInspectorWorker      m_worker;
    Win32HostDialogs          m_dialogs;
    BlockInspectorPanel     * m_panel           = nullptr;
    DxuiButton              * m_apply           = nullptr;
    DxuiButton              * m_discard         = nullptr;
    IDiskFileIo             * m_fileIo          = nullptr;
    std::wstring              m_path;
    const IDxuiTheme        * m_theme           = nullptr;
    std::function<void ()>    m_onClosed;
    bool                      m_isDirty         = true;
    bool                      m_isClosed        = false;
    bool                      m_isReloadPending = false;
};
