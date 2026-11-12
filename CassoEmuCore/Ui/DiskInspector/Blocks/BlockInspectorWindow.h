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
////////////////////////////////////////////////////////////////////////////////

class BlockInspectorWindow : public DxuiDialogWindow
{
public:
    static constexpr int  kWidthDip     = 1180;
    static constexpr int  kHeightDip    = 760;
    static constexpr int  kMinWidthDip  = 960;
    static constexpr int  kMinHeightDip = 600;
    static constexpr int  kApplyId      = 1001;
    static constexpr int  kDiscardId    = 1002;

    BlockInspectorWindow  ();
    ~BlockInspectorWindow () override;

    //  Opens the file and the window over `owner`. Fails, with the
    //  reason, for a file that doesn't open in block mode.
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

private:
    HRESULT  OpenModel     ();
    void     Apply         ();
    void     UpdateButtons ();
    bool     ConfirmClose  ();
    void     Close         ();
    void     SaveRun       (const LeftoverRun & run);
    void     ShowMessage   (const std::wstring & text);

    BlockInspectorModel       m_model;
    Win32HostDialogs          m_dialogs;
    BlockInspectorPanel     * m_panel   = nullptr;
    DxuiButton              * m_apply   = nullptr;
    DxuiButton              * m_discard = nullptr;
    IDiskFileIo             * m_fileIo  = nullptr;
    std::wstring              m_path;
    const IDxuiTheme        * m_theme   = nullptr;
    std::function<void ()>    m_onClosed;
    bool                      m_isDirty  = true;
    bool                      m_isClosed = false;
};
