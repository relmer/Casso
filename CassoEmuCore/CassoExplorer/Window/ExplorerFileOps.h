#pragma once

#include "Pch.h"

#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/CassoExplorerCommands.h"
#include "CassoExplorer/CassoExplorerNamedControl.h"
#include "CassoExplorer/CassoExplorerNewDiskDialog.h"
#include "CassoExplorer/ICassoExplorerHost.h"
#include "CassoExplorer/Model/FocusRing.h"
#include "CassoExplorer/Model/CassoExplorerPrefs.h"
#include "Config/IFileSystem.h"
#include "Seams/Win32HostDialogs.h"
#include "Seams/Win32IntentChannel.h"
#include "Seams/Win32ProcessLauncher.h"
#include "CassoExplorer/CrtPictureRenderer.h"
#include "Seams/Win32ContentDetails.h"
#include "Ui/Debugger/SourceSyntax.h"
#include "Ui/ThemeLoader.h"
#include "Config/CrtResolver.h"
#include "Config/CrtPresets.h"
#include "Seams/Win32ShellIcons.h"
#include "Seams/Win32InfoTips.h"
#include "Seams/Win32ShellItemVerbs.h"
#include "Seams/Win32PackagedCommands.h"
#include "Ui/Chrome/CassoTheme.h"
#include "CassoExplorer/Model/FolderWatch.h"
#include "CassoExplorer/Model/RefreshAnchor.h"

class CassoLinkConnections;
class FirewallMonitor;
class RunningCassoRegistry;
class ILinkAnnouncements;
struct CassoExplorerSessionWindow;
class CassoExplorerWindow;





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
    explicit ExplorerFileOps (CassoExplorerWindow & window);

    static constexpr int  kMaxCatalogName = 30;
    static constexpr int  kRestoreRetries = 5;

    //  A machine's configuration with the user's changes to it from Casso's
    //  prefs file merged in, as Casso merges them; the file is only read.
    static HRESULT  MergeUserMachineDelta (const std::string & machine, IFileSystem & files, std::string & inoutJson);
    void  OpenSelectedEntries     ();

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

    void                              PushUndo                  (UndoStep step);
    void                              UndoLast                  ();
    static const wchar_t *            GetUndoLabel              (UndoStep::Kind kind);
    void                              PasteHere                 (const std::wstring & folder);
    void                              BeginTreeRename           (const std::wstring & id);
    void                              ShowHostProperties        (const std::vector<std::wstring> & paths);
    void                              PasteIntoVolume           ();
    void                              PushPutUndo               (const CassoExplorerActions::Outcome & outcome, const std::wstring & image, const std::string & inner);
    CassoExplorerActions::AddressFn    MakeAddressPrompt();
    CassoExplorerActions::CollisionFn  MakeCollisionPrompt();
    std::wstring  GetPasteFolder () const;
    void  CopyEntriesToClipboard (HostFileNaming::Style style);

    HRESULT  RecycleHostFiles (const std::vector<std::wstring> & paths);
    void  CopySelectedPaths      ();
    void  OpenEachSelected       ();
    void  ShowRowProperties      (int row);
    void  ShowLocationProperties (const Location & location);
    void  ShowHostProperties     (const std::wstring & path);
    bool  TryGetSelectedShellIds  (std::vector<std::wstring> & outIds) const;
    void  DeleteSelectionPermanently ();
    void  BeginRename         ();
    void  EndRename           (bool commit);
    void  InsertIntoDrive     (const std::wstring & imagePath, int drive);
    void  OpenInNewCasso      (const std::wstring & imagePath);
    HWND  FindCassoTarget     () const;
    int   GetDefaultMachineDriveCount();
    void  AskCassoToDescribe  ();
    std::wstring  GetSelectedImagePath() const;

private:
    friend class CassoExplorerWindow;

    CassoExplorerWindow  & m_window;

    Win32ProcessLauncher  m_launcher;
    int                   m_defaultDriveCount = -1;   // the default machine's, once read
    std::wstring          m_renameTreeId;
    int                   m_restoreTicks      = 0;
};
