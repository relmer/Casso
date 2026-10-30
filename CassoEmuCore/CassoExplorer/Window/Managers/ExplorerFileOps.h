#pragma once

#include "Pch.h"

#include "CassoExplorer/CassoExplorerActions.h"
#include "Seams/Win32ProcessLauncher.h"

struct ExplorerContext;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerFileOps
//
//  File operations on the selection: rename in place, undo, paste,
//  deleting, copying paths, opening, Properties, and the Casso a disk is
//  inserted into.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerFileOps
{
public:
    //  What Ctrl+Z puts back, as Explorer's undo does: a deletion to the
    //  Recycle Bin, a rename, a new folder, and inside a disk image a
    //  deletion (from a copy kept aside), a rename or a new folder.
    struct UndoStep
    {
        enum class Kind { Recycle, HostRename, HostNewFolder, HostNewItem, ImageRename, ImageNewFolder, ImageDelete, HostCopy, HostMove, ImagePut };

        Kind                       kind = Kind::Recycle;
        Location                   location;    // where an image step was made
        std::vector<std::wstring>  paths;       // host paths, as they are now
        std::wstring               oldName;
        std::wstring               newName;
        std::wstring               savedDir;    // an image deletion's copy
        std::vector<std::wstring>  sources;     // where moved host items were
        std::vector<std::string>   entries;     // what a put or copy made in an image
    };

    explicit ExplorerFileOps (ExplorerContext & context);

    static constexpr int  kMaxCatalogName = 30;
    static constexpr int  kRestoreRetries = 5;

    void  Create            ();
    bool  IsRenaming        () const { return m_renameRow >= 0 || !m_renameTreeId.empty(); }
    int   GetRenameRow      () const { return m_renameRow; }

    //  How many drives the Casso the window targets has, from its reply.
    void                    SetCassoDriveCount    (int count) { m_cassoDriveCount = count; }
    int                     GetCassoDriveCount    () const { return m_cassoDriveCount; }
    static HRESULT          MergeUserMachineDelta (const std::string & machine, IFileSystem & files, std::string & inoutJson);
    void                    OpenSelectedEntries   ();
    void                    PushUndo              (UndoStep step);
    void                    UndoLast              ();
    static const wchar_t  * GetUndoLabel          (UndoStep::Kind kind);

    //  The file operations that can be undone, newest last.
    bool                                 CanUndo                     () const { return !m_undo.empty(); }
    const UndoStep                     & GetLastUndo                 () const { return m_undo.back(); }
    void                                 PasteHere                   (const std::wstring & folder);
    void                                 BeginTreeRename             (const std::wstring & id);
    void                                 ShowHostProperties          (const std::vector<std::wstring> & paths);
    void                                 PasteIntoVolume             ();
    void                                 PushPutUndo                 (const CassoExplorerActions::Outcome & outcome, const std::wstring & image, const std::string & inner);
    CassoExplorerActions::AddressFn      MakeAddressPrompt           ();
    CassoExplorerActions::CollisionFn    MakeCollisionPrompt         ();
    std::wstring                         GetPasteFolder              () const;
    void                                 CopyEntriesToClipboard      (HostFileNaming::Style style);
    HRESULT                              RecycleHostFiles            (const std::vector<std::wstring> & paths);
    void                                 CopySelectedPaths           ();
    void                                 OpenEachSelected            ();
    void                                 ShowRowProperties           (int row);
    void                                 ShowAppleProperties         ();
    void                                 ShowLocationProperties      (const Location & location);
    void                                 ShowHostProperties          (const std::wstring & path);
    bool                                 TryGetSelectedShellIds      (std::vector<std::wstring> & outIds) const;
    void                                 DeleteSelectionPermanently  ();
    void                                 BeginRename                 ();
    void                                 EndRename                   (bool commit);
    void                                 InsertIntoDrive             (const std::wstring & imagePath, int drive);
    void                                 OpenInNewCasso              (const std::wstring & imagePath);
    HWND                                 FindCassoTarget             () const;
    int                                  GetDefaultMachineDriveCount ();
    void                                 AskCassoToDescribe          ();
    std::wstring                         GetSelectedImagePath        () const;
    bool                                 CountDownRestore            () { return --m_restoreTicks <= 0; }

    bool  HandleRenameMouse (const DxuiMouseEvent & ev);
    void  HandleRenameKey   (const DxuiKeyEvent & ev);

private:
    ExplorerContext        & m_context;
    Win32ProcessLauncher     m_launcher;
    int                      m_defaultDriveCount = -1;   // the default machine's, once read
    int                      m_cassoDriveCount   = 0;   // the drives the Casso the window targets has
    std::vector<UndoStep>    m_undo;   // newest last

    //  Rename in place: the box over the name, and the list row being renamed,
    //  or -1, or the tree node.
    DxuiTextInput  * m_renameBox    = nullptr;
    int              m_renameRow    = -1;
    std::wstring     m_renameTreeId;
    int              m_restoreTicks = 0;
};
