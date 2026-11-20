#pragma once

#include "Pch.h"

#include "Core/TextEncoding.h"
#include "Devices/Disk/Inspector/Blocks/BlockInspectorModel.h"
#include "Devices/Disk/Inspector/Repair/RepairCopy.h"
#include "Seams/Win32Clipboard.h"
#include "Seams/Win32HostDialogs.h"
#include "Seams/Win32DriveSpace.h"
#include "Seams/Win32ShellIcons.h"
#include "Ui/DiskInspector/Blocks/BlockBrowser.h"
#include "Ui/DiskInspector/Blocks/BlockFileSession.h"
#include "Ui/DiskInspector/Components/InspectorCommands.h"
#include "Ui/DiskInspector/Components/InspectorMenuBar.h"
#include "Ui/DiskInspector/Components/InspectorStatusBar.h"

class BlockInspectorPanel;
class IDiskFileIo;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorWindow
//
//  The disk inspector in block mode, for an image file of blocks, laid out
//  as the track inspector is: the menu bar, the BlockInspectorPanel, and the
//  status band with the volume's chips, the selection and the window's zoom.
//  Editing matches the track inspector's: Edit block turns typing on, and
//  the edit bar has the unsaved changes and the actions on them. Save
//  writes at once; the user confirms each undo and redo of a save. A *
//  before the file's name in the title and a count in the status band mark
//  unsaved changes. Closing with unsaved changes offers Save, Discard and
//  Cancel.
//  The closed callback is called when the window closes.
//
//  Every read and write of the image runs on m_worker's thread: opening it,
//  each block, comparing, saving edits and reading it again. Each job's
//  completion runs on the next frame of the window set's timer. The
//  progress of a long job is drawn in the panel. Closing cancels a job that
//  can be canceled. While a job that can't be is running, the window
//  doesn't close.
//
////////////////////////////////////////////////////////////////////////////////

class BlockInspectorWindow : public DxuiWindow, public IBlockDialogs, public IBlockFrame
{
public:
    static constexpr int       kOpeningWidthDip  = 1280;
    static constexpr int       kOpeningHeightDip = 800;
    static constexpr int       kMinWidthDip      = 960;
    static constexpr int       kMinHeightDip     = 640;

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

    void  SetOnClosed    (std::function<void ()> fn) { m_onClosed = std::move (fn); }

    //  The file: its display name, set before Open, reading it again, and
    //  comparing it with another image.
    BlockFileSession  & GetFileSession () { return m_session; }

    void  ChangeTheme (const IDxuiTheme * theme);

    //  Once a frame: the finished jobs' completions, the busy state, and a
    //  repaint after a change.
    void  RenderIfChanged ();

    void     Layout            (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void     Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool     OnMouse           (const DxuiMouseEvent & ev) override;
    bool     OnKey             (const DxuiKeyEvent & ev) override;
    LPCWSTR  GetCursorForPoint (POINT clientPx) const override;

    void                 ShowWarning    (const std::wstring & text) override;
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
    using WriteFn = std::function<HRESULT (BlockInspectorModel & model, std::wstring & outError, BlockJobProgress & progress)>;

    void                                  BuildCommands       ();
    static bool                           RunCommand          (const InspectorCommands::Command & command);
    void                                  Save                ();
    void                                  QueueSave           ();
    void                                  StartSave           ();
    void                                  SaveToPreservedCopy ();
    void                                  UndoApplied         ();
    void                                  RedoApplied         ();
    void                                  QueueWrite          (const std::wstring & label, const std::wstring & done, WriteFn write);
    void                                  SaveEditedCopy      ();
    void                                  InitEditBar         ();
    void                                  UpdateEditing       ();
    bool                                  HandleEditKey       (const DxuiKeyEvent & ev);
    void                                  Discard             ();
    int                                   GetRepairVolume     () const;
    void                                  Repair              ();
    bool                                  PickRepairTarget    (const std::wstring & why, uint64_t bytes, fs::path & ioTarget);
    void                                  QueueRepairCopy     (int volume, const RepairPlan & plan, bool isCard, const fs::path & target);
    void                                  CopyBlock           ();
    void                                  PasteIntoBlock      ();
    void                                  GoToBlock           ();
    void                                  ShowLevel           (BlockPlace::Kind kind);
    void                                  UpdateTooltip       (POINT pointPx);
    void                                  StepDifference      (bool isForward);
    bool                                  ConfirmClose        ();

    //  m_worker follows m_model, so it is destroyed first and its thread
    //  ends before the model goes away.
    BlockInspectorModel       m_model;
    BlockInspectorWorker      m_worker;
    BlockFileSession          m_session         { m_model, *this, *this };
    Win32HostDialogs          m_dialogs;
    Win32Clipboard            m_clipboard;
    Win32DriveSpace           m_drives;
    InspectorCommands         m_commands;
    InspectorMenuBar          m_menus;
    InspectorStatusBar        m_status;
    BlockInspectorPanel     * m_panel           = nullptr;
    std::wstring              m_hint;
    std::wstring              m_editNote;                // the edit bar's note: the last save's result, or why it failed
    DxuiTooltip               m_tooltip;
    Win32ShellIcons           m_icons;
    std::wstring              m_tooltipText;
    const IDxuiTheme        * m_theme           = nullptr;
    std::function<void ()>    m_onClosed;
    double  m_zoom             = 1.0;
    bool    m_isDirty          = true;
    bool    m_isClosed         = false;
    bool    m_isCloseAfterSave = false;
    bool    m_isDarkSurface    = true;     // the surface the icons were last drawn for
};
