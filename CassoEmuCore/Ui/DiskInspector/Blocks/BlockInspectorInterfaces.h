#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/Blocks/BlockInspectorWorker.h"

class BlockHexView;
class BlockMapView;
class BlockNavigator;
class EditBar;
class IDiskFileIo;
class IHostDialogs;
struct EditBarState;





////////////////////////////////////////////////////////////////////////////////
//
//  Block inspector interfaces
//
//  The parts of block mode call the window, the panel and each other only
//  through these, so a test can host a part with fakes.
//
////////////////////////////////////////////////////////////////////////////////

//  A job's result, written by its work and read by its completion.
struct BlockJobResult
{
    HRESULT       hr = S_OK;
    std::wstring  message;
};


//  The worker's jobs, as the panel queues them.
class IBlockJobs
{
public:
    virtual ~IBlockJobs() = default;

    virtual void  QueueJob   (BlockInspectorWorker::Job job) = 0;
    virtual bool  IsBusy     () const = 0;
    virtual void  CancelJobs () = 0;
};


//  The panel's block and its editing.
class IBlockView
{
public:
    virtual ~IBlockView() = default;

    virtual void            Refresh        () = 0;
    virtual void            SetEditing     (bool isEditing) = 0;
    virtual bool            IsEditing      () const = 0;
    virtual void            SetEditState   (const EditBarState & state) = 0;
    virtual EditBar       & GetEditBar     () const = 0;
    virtual BlockHexView  * GetHexView     () const = 0;
    virtual std::wstring    BuildBlockText () const = 0;
};


//  The panel's places: the tree's levels, and steps between blocks.
class IBlockNavigation
{
public:
    virtual ~IBlockNavigation() = default;

    virtual BlockNavigator        & GetNavigator ()       = 0;
    virtual const BlockNavigator  & GetNavigator () const = 0;
    virtual void                    SelectBlock  (uint32_t block) = 0;
    virtual void                    GoToBlock    (uint32_t block) = 0;
    virtual void                    Step         (int64_t delta) = 0;
    virtual void                    GoBack       () = 0;
    virtual void                    GoForward    () = 0;
};


//  What the panel draws: the map, the overlays, and the tips over them.
class IBlockDisplay
{
public:
    virtual ~IBlockDisplay() = default;

    virtual BlockMapView  & GetMap           () const = 0;
    virtual bool            IsFilesOn        () const = 0;
    virtual void            SetFilesOn       (bool isOn) = 0;
    virtual bool            IsDiffsOn        () const = 0;
    virtual void            SetDiffsOn       (bool isOn) = 0;
    virtual std::wstring    GetSelectionLine () const = 0;
    virtual bool            GetHexTooltip    (POINT pointPx, std::wstring & outText, RECT & outAnchorPx, vector<DxuiTooltipSwatch> & outSwatches) const = 0;
};


//  The answer to the question of what to do with unsaved edits.
enum class BlockUnsavedChoice
{
    Cancel,
    Save,
    Discard,
};


//  The window's message box, its questions and its file dialogs.
class IBlockDialogs
{
public:
    virtual ~IBlockDialogs() = default;

    virtual void                 ShowWarning     (const std::wstring & text) = 0;
    virtual BlockUnsavedChoice   AskUnsaved      (const std::wstring & line) = 0;
    virtual HWND                 GetDialogOwner  () const = 0;
    virtual const IDxuiTheme   * GetDialogTheme  () const = 0;
    virtual IHostDialogs       & GetHostDialogs  () = 0;
};


//  The window itself: its title, its zoom, a repaint, and closing.
class IBlockFrame
{
public:
    virtual ~IBlockFrame() = default;

    virtual void    CloseWindow  () = 0;
    virtual void    RequestClose () = 0;
    virtual void    MarkDirty    () = 0;
    virtual void    SetCaption   (const std::wstring & title, size_t markAt) = 0;
    virtual double  GetZoom      () const = 0;
    virtual void    SetZoom      (double zoom) = 0;
};


//  The file the window opened.
class IBlockFileInfo
{
public:
    virtual ~IBlockFileInfo() = default;

    virtual const std::wstring  & GetPath         () const = 0;
    virtual std::wstring          GetFileName     () const = 0;
    virtual std::wstring          GetHostFileName () const = 0;
    virtual bool                  IsOnCard        () const = 0;
    virtual IDiskFileIo         & GetFileIo       () const = 0;
};


//  Reading the file again, and comparing it with another image.
class IBlockFileSession
{
public:
    virtual ~IBlockFileSession() = default;

    virtual void  Reload            () = 0;
    virtual void  CompareWithPicked () = 0;
    virtual void  EndComparison     () = 0;
};


//  The edits and their saves, for the commands.
class IBlockEditing
{
public:
    virtual ~IBlockEditing() = default;

    virtual void  Save           () = 0;
    virtual void  SaveEditedCopy () = 0;
    virtual void  UndoApplied    () = 0;
    virtual void  RedoApplied    () = 0;
    virtual void  Discard        () = 0;
    virtual void  CopyBlock      () = 0;
};


//  The edit bar's note, as the last save's result or a repair's.
class IBlockEditNote
{
public:
    virtual ~IBlockEditNote() = default;

    virtual void  SetNote (const std::wstring & note) = 0;
};


//  Repair of the current volume.
class IBlockRepair
{
public:
    virtual ~IBlockRepair() = default;

    virtual bool  CanRepair () const = 0;
    virtual void  Repair    () = 0;
};
