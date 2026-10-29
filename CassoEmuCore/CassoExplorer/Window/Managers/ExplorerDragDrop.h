#pragma once

#include "Pch.h"

#include "Seams/Win32ShellIcons.h"

#include "CassoExplorer/CassoExplorerActions.h"

struct ExplorerContext;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDragDrop
//
//  Drag and drop: drags started from the list, the tree and a drive, the
//  window as a drop target for host files and other images' entries, the
//  shell drop target of a host folder under the pointer, the right-drag
//  menu, and dragging out to File Explorer.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerDragDrop
{
public:
    //  What a drag holds: another image's entries, or host files.
    struct DropSource
    {
        bool                       fromImage = false;
        std::string                image;
        VolumeKind                 kind      = VolumeKind::Unknown;
        std::vector<std::string>   catalogPaths;
        std::vector<std::wstring>  hostPaths;
    };

    //  What a right-drag dropped, held between the drop and the menu's
    //  answer: where it landed, and either the host files or the entries of
    //  the image they came from.
    struct PendingDrop
    {
        bool                       valid       = false;
        Location                   location;
        VolumeKind                 targetKind  = VolumeKind::Unknown;
        std::string                inner;
        POINT                      screen      = {};
        std::vector<std::wstring>  hostPaths;
        bool                       fromImage   = false;
        std::string                sourceImage;
        VolumeKind                 sourceKind  = VolumeKind::Unknown;
        std::vector<std::string>   catalogPaths;
    };

    explicit ExplorerDragDrop (ExplorerContext & context);

    //  A dragged item's icon, as File Explorer draws it, and the DPI it is
    //  drawn at, the list's.
    static constexpr int  kDragImageDip = 96;

    //  How long a drag rests on a collapsed tree node before it opens,
    //  measured against File Explorer's: between 970 and 1,090 ms.
    static constexpr UINT  kTreeDropExpandMs      = 1000;

    //  Files the shell runs, with what is dropped on them as arguments.
    static constexpr const wchar_t *  kProgramExtensions[] = { L".exe", L".com", L".bat", L".cmd" };

    void                                    SetIconDpi           (UINT dpi);
    static std::vector<std::wstring>        GetDraggedPaths      (IDataObject * data);
    static bool                             IsDraggedFromFolder  (IDataObject * data, const std::wstring & folder);
    static bool                             IsDraggedItem        (IDataObject * data, const std::wstring & target);
    void                                    BeginTreeDrag        (const Location & location);
    bool                                    TryGetAppleDropDrive (int tag, POINT screen, Location & outDrive);
    void                                    BeginDiskDrag        (const std::wstring & driveId);
    std::shared_ptr<const DxuiIconImage>    GetRowDragImage      ();
    void                                    BeginDragOut         ();
    bool                                    TryGetDropLocation   (int tag, POINT screen, Location & outLocation);
    DWORD                                   GetDropEffect        (IDataObject * data, int tag, POINT screen);
    bool                                    TryGetHostDropFolder (int tag, POINT screen, std::wstring & outFolder);
    DWORD                                   ForwardHostDrag      (IDataObject * data, const std::wstring & folder, POINT screen);
    void                                    LeaveHostDrop        ();
    void                                    ShowDropTarget       (int tag, POINT screen, bool accepted);
    void                                    RefreshDropTarget    ();
    void                                    ShowDropMenu         ();
    void                                    RunDrop              (CassoExplorerActions::Conversion conversion);
    void                                    ClearDropTarget      ();
    bool                                    ReadDropSource       (IDataObject * data, DropSource & outSource);
    DWORD                                   ChooseDropEffect     (const DropSource & source, const Location & target) const;
    void                                    DescribeDrop         (IDataObject * data, DWORD effect, const Location & target);
    void                                    HoverDropTab         (POINT screen);
    bool                                    IsShellDropFile      (int row, std::wstring & outPath) const;
    static bool                             IsProgramPath        (const std::wstring & path);
    DxuiDragDropTarget                    & GetDropTarget        () { return m_dropTarget; }

    void  HandleDropFile (const std::wstring & path);
    void  HandleDrop     (IDataObject * data, int tag, POINT screen);

private:
    ExplorerContext      & m_context;
    Win32ShellIcons        m_dragIcons;   // drawn as each drag starts, so not in the background
    DxuiDragDropTarget     m_dropTarget;
    ComPtr<IDropTarget>    m_hostDrop;   // the host folder the drag is over, if any
    std::wstring           m_hostDropFolder;
    PendingDrop            m_pendingDrop;
    int                    m_treeDropRow     = -1;
    ULONGLONG              m_treeDropSinceMs = 0;
    bool                   m_dropOver        = false;   // a drag is over the window, at m_dropTag and m_dropScreen
    int                    m_dropTag         = 0;
    POINT                  m_dropScreen      = {};
    bool                   m_dropAccepted    = false;
};
