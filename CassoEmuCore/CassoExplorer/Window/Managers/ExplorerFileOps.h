#pragma once

#include "Pch.h"

#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoLink/FullSend/FullSendManifest.h"
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

    void                    OpenSelectedEntries   ();
    void                    PushUndo              (UndoStep step);

    //  Host files deleted, and files restored: each image in a Full Send
    //  folder is removed from the folder's manifest, and put back when it is
    //  restored while this window is open.
    void                    NoteManifestDeletes   (const std::vector<std::wstring> & paths);
    void                    NoteManifestRestores  (const std::vector<std::wstring> & paths);
    void                    UndoLast              ();
    static const wchar_t  * GetUndoLabel          (UndoStep::Kind kind);

    //  The file operations that can be undone, newest last.
    bool                                 CanUndo                     () const { return !m_undo.empty(); }
    const UndoStep                     & GetLastUndo                 () const { return m_undo.back(); }
    void                                 PasteHere                   (const std::wstring & folder);
    void                                 BeginTreeRename             (const std::wstring & id);
    void                                 PasteIntoVolume             ();
    void                                 PushPutUndo                 (const CassoExplorerActions::Outcome & outcome, const std::wstring & image, const std::string & inner);
    CassoExplorerActions::AddressFn      MakeAddressPrompt           ();
    CassoExplorerActions::CollisionFn    MakeCollisionPrompt         ();
    std::wstring                         GetPasteFolder              () const;
    void                                 CopyEntriesToClipboard      (HostFileNaming::Style style);

    //  Cut on an image's or an Apple drive's entries: they go on the
    //  clipboard as Copy puts them, and are deleted once a target reports
    //  the paste, in FinishCut.
    void                                 CutEntriesToClipboard       (HostFileNaming::Style style);
    void                                 FinishCut                   ();
    HRESULT                              RecycleHostFiles            (const std::vector<std::wstring> & paths);
    void                                 CopySelectedPaths           ();
    void                                 OpenEachSelected            ();

    //  Copy and the shell's commands reach a selected row by its place in the shell.
    static bool                          HasShellRows                (Location::Kind kind, bool home) { return kind == Location::Kind::ShellFolder || home; }
    bool                                 TryGetSelectedShellIds      (std::vector<std::wstring> & outIds) const;
    void                                 DeleteSelectionPermanently  ();
    void                                 BeginRename                 ();
    void                                 EndRename                   (bool commit);
    bool                                 CountDownRestore            () { return --m_restoreTicks <= 0; }

    bool  HandleRenameMouse (const DxuiMouseEvent & ev);
    void  HandleRenameKey   (const DxuiKeyEvent & ev);

private:
    //  What the last cut takes away once it is pasted, read when it was cut.
    struct CutSource
    {
        bool                              valid   = false;
        bool                              onDrive = false;
        std::string                       image;
        std::vector<std::string>          catalogPaths;
        ActionsAppleDrive::DriveDeletion  drive;
    };

    void  ReportPasted       (IDataObject * data, const CassoExplorerActions::Outcome & outcome);
    bool  IsCutFrom          (const std::string & inner) const;
    void  NoteManifestRename (const std::wstring & oldPath, const std::wstring & newName);

    ExplorerContext        & m_context;
    CutSource                m_cut;
    std::vector<UndoStep>    m_undo;   // newest last

    //  What a deleted image's deletion removed and changed in the manifest,
    //  and the folder as it was deleted from, by the image's path in
    //  lowercase, for when it is restored.
    struct RemovedRows
    {
        std::string      folder;
        FullSendRemoval  removal;
    };

    std::map<std::wstring, RemovedRows>  m_removedRows;

    //  Rename in place: the box over the name, and the list row being renamed,
    //  or -1, or the tree node.
    DxuiTextInput  * m_renameBox    = nullptr;
    int              m_renameRow    = -1;
    std::wstring     m_renameHiddenExtension;   // a shortcut's .lnk, left out of the box
    std::wstring     m_renameTreeId;
    int              m_restoreTicks = 0;
};
