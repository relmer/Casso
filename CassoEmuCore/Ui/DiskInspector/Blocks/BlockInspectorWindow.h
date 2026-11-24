#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/Blocks/BlockInspectorModel.h"
#include "Seams/Win32Clipboard.h"
#include "Seams/Win32DriveSpace.h"
#include "Seams/Win32HostDialogs.h"
#include "Seams/Win32ShellIcons.h"
#include "Ui/DiskInspector/Blocks/BlockFileSession.h"
#include "Ui/DiskInspector/Components/InspectorCommands.h"
#include "Ui/DiskInspector/Components/InspectorMenuBar.h"
#include "Ui/DiskInspector/Components/InspectorStatusBar.h"

class BlockCommandSet;
class BlockEditController;
class BlockInspectorPanel;
class BlockInspectorTips;
class BlockRepairController;
class IDiskFileIo;
struct InspectorRawTransfer;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow
//
//  The disk inspector in block mode, for an image file of blocks, laid out
//  as the track inspector is: the menu bar, the BlockInspectorPanel, and the
//  status band with the volume's chips, the selection and the window's zoom.
//  The window creates the parts of block mode and routes input to them:
//  BlockFileSession for the file, BlockEditController for the edits,
//  BlockRepairController for repair, BlockInspectorTips for the tips and
//  BlockCommandSet for the commands and their keys.
//  The closed callback is called when the window closes.
//
//  Every read and write of the image runs on m_worker's thread. Each job's
//  completion runs on the next frame of the window set's timer. Closing
//  cancels a job that can be canceled. While a job that can't be is
//  running, the window doesn't close.
//
////////////////////////////////////////////////////////////////////////////////

class BlockInspectorWindow : public DxuiWindow, public IBlockDialogs, public IBlockFrame
{
public:
    static constexpr int  kOpeningWidthDip  = 1280;
    static constexpr int  kOpeningHeightDip = 800;
    static constexpr int  kMinWidthDip      = 960;
    static constexpr int  kMinHeightDip     = 640;

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

    void                SetOnClosed     (std::function<void ()> fn) { m_onClosed = std::move (fn); }

    //  Before Open: the host's save of blocks to a file and overwrite of them
    //  from one. With it set, the File menu has commands for them.
    void                SetOnTransferRaw (std::function<void (const InspectorRawTransfer &)> fn) { m_onTransferRaw = std::move (fn); }

    bool                HasUnsavedEdits () const                    { return m_model.IsOpen() && !m_model.GetEdits().IsEmpty(); }
    BlockFileSession  & GetFileSession  ()                          { return m_session; }
    void                ChangeTheme     (const IDxuiTheme * theme);
    void                RenderIfChanged ();

    //  True when the window can close: no job that can't be canceled is
    //  running, and the user confirms the unsaved edits.
    bool  ConfirmClose ();

    void  Layout  (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void  Paint   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool  OnMouse (const DxuiMouseEvent & ev) override;
    bool  OnKey   (const DxuiKeyEvent & ev) override;

    void                 ShowWarning    (const std::wstring & text) override;
    BlockUnsavedChoice   AskUnsaved     (const std::wstring & line) override;
    HWND                 GetDialogOwner () const override { return GetHwnd(); }
    const IDxuiTheme   * GetDialogTheme () const override { return m_theme; }
    IHostDialogs       & GetHostDialogs () override       { return m_dialogs; }

    void    CloseWindow  () override;
    void    RequestClose () override                                         { OnWindowClose(); }
    void    MarkDirty    () override                                         { m_isDirty = true; }
    void    SetCaption   (const std::wstring & title, size_t markAt) override { SetTitle (title, markAt); }
    double  GetZoom      () const override                                   { return m_zoom; }
    void    SetZoom      (double zoom) override;

protected:
    void  OnCreate      () override;
    void  OnWindowClose () override;

private:
    void  CreateParts  ();
    void  TransferRaw  (bool isSave);
    void  UpdateNotice ();

    //  m_worker follows m_model, so it is destroyed first and its thread
    //  ends before the model goes away.
    BlockInspectorModel                     m_model;
    BlockInspectorWorker                    m_worker;
    BlockFileSession                        m_session       { m_model, *this, *this };
    Win32HostDialogs                        m_dialogs;
    Win32Clipboard                          m_clipboard;
    Win32DriveSpace                         m_drives;
    InspectorCommands                       m_commands;
    InspectorMenuBar                        m_menus;
    InspectorStatusBar                      m_status;
    BlockInspectorPanel                   * m_panel         = nullptr;
    std::unique_ptr<BlockEditController>    m_edits;
    std::unique_ptr<BlockRepairController>  m_repair;
    std::unique_ptr<BlockInspectorTips>     m_tips;
    std::unique_ptr<BlockCommandSet>        m_commandSet;
    Win32ShellIcons                         m_icons;
    const IDxuiTheme                      * m_theme         = nullptr;
    std::function<void ()>                  m_onClosed;
    std::function<void (const InspectorRawTransfer &)>  m_onTransferRaw;
    double                                  m_zoom          = 1.0;
    bool                                    m_isDirty       = true;
    bool                                    m_isClosed      = false;
    bool                                    m_isDarkSurface = true;     // the surface the icons were last drawn for
};
