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
    explicit ExplorerDragDrop (ExplorerContext & context);

    //  A dragged item's icon, as File Explorer draws it, and the DPI it is
    //  drawn at, the list's.
    static constexpr int  kDragImageDip = 96;
    void  SetIconDpi (UINT dpi);

    //  How long a drag rests on a collapsed tree node before it opens,
    //  measured against File Explorer's: between 970 and 1,090 ms.
    static constexpr UINT  kTreeDropExpandMs      = 1000;

    //  The files in a drag, and whether a drop on a folder or item would
    //  put them back where they are or onto themselves.
    static std::vector<std::wstring>  GetDraggedPaths     (IDataObject * data);
    static bool                       IsDraggedFromFolder (IDataObject * data, const std::wstring & folder);
    static bool                       IsDraggedItem       (IDataObject * data, const std::wstring & target);

    //  A drag from a node in the tree: a linked Apple's whole drive, or a
    //  folder or disk image on the PC, which drags as Explorer's does.
    void  BeginTreeDrag     (const Location & location);

    //  The linked Apple's drive a drop would land on: a drive's node or row,
    //  or the drive in the list.
    bool  TryGetAppleDropDrive (int tag, POINT screen, Location & outDrive);
    void  BeginDiskDrag        (const std::wstring & driveId);

    //  The first selected row's icon at the drag image's size.
    std::shared_ptr<const DxuiIconImage>  GetRowDragImage ();
    void                                  BeginDragOut    ();
    void                                  HandleDropFile  (const std::wstring & path);

    //  Drag-in: the image location under a drop point, what a drop there
    //  would do, and the drop.
    bool  TryGetDropLocation     (int tag, POINT screen, Location & outLocation);
    DWORD GetDropEffect          (IDataObject * data, int tag, POINT screen);

    //  A host folder under the drag takes it as Explorer would: the drag goes
    //  to the folder's own shell drop target, which copies or moves it with
    //  Explorer's progress, conflict and undo handling.
    bool   TryGetHostDropFolder (int tag, POINT screen, std::wstring & outFolder);
    DWORD  ForwardHostDrag      (IDataObject * data, const std::wstring & folder, POINT screen);
    void   LeaveHostDrop        ();
    void   ShowDropTarget       (int tag, POINT screen, bool accepted);
    void   ShowDropMenu         ();
    void   RunDrop              (CassoExplorerActions::Conversion conversion);
    void   ClearDropTarget      ();
    void   HandleDrop           (IDataObject * data, int tag, POINT screen);

    //  What a drag holds: another image's entries, or host files.
    struct DropSource
    {
        bool                       fromImage = false;
        std::string                image;
        VolumeKind                 kind      = VolumeKind::Unknown;
        std::vector<std::string>   catalogPaths;
        std::vector<std::wstring>  hostPaths;
    };

    bool   ReadDropSource   (IDataObject * data, DropSource & outSource);
    DWORD  ChooseDropEffect (const DropSource & source, const Location & target) const;
    void   DescribeDrop     (IDataObject * data, DWORD effect, const Location & target);
    void   HoverDropTab     (POINT screen);
    bool   IsShellDropFile  (int row, std::wstring & outPath) const;

    static bool  IsProgramPath (const std::wstring & path);

    //  Files the shell runs, with what is dropped on them as arguments.
    static constexpr const wchar_t *  kProgramExtensions[] = { L".exe", L".com", L".bat", L".cmd" };

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

    DxuiDragDropTarget  & GetDropTarget () { return m_dropTarget; }

private:

    ExplorerContext  & m_context;

    Win32ShellIcons        m_dragIcons;   // drawn as each drag starts, so not in the background

    DxuiDragDropTarget                        m_dropTarget;
    ComPtr<IDropTarget>                       m_hostDrop;   // the host folder the drag is over, if any
    std::wstring                              m_hostDropFolder;

    PendingDrop                                  m_pendingDrop;

    int                                          m_treeDropRow       = -1;
    ULONGLONG                                    m_treeDropSinceMs   = 0;
};
