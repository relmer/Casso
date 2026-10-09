#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/FolderOptions.h"
#include "CassoExplorer/Model/Location.h"
#include "Config/IFileSystem.h"
#include "Seams/IShellItemVerbs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TreeNode
//
//  One row the tree can show. The id is stable across refreshes and says
//  which root the node hangs from, so the same folder reached under Casso
//  and under This PC is two nodes with two ids and one location.
//
////////////////////////////////////////////////////////////////////////////////

struct TreeNode
{
    enum class Kind { CassoRoot, ThisPcRoot, RecycleBinRoot, KnownFolder, Drive, HostFolder, DiskImage, DiskDirectory, ShellRoot };

    std::wstring  id;
    Kind          kind         = Kind::HostFolder;
    std::wstring  label;
    Location      location;
    bool          canExpand    = false;
    bool          missing      = false;   // a known folder that is no longer there, drawn dimmed
    bool          hidden       = false;   // a hidden host folder, its icon drawn faded
    std::wstring  loadError;   // an image that failed to parse: the tooltip and the list-area message
    bool          broken       = false;   // an image that is not a disk at all, as against one with no file system
    bool          dividerAbove = false;   // a line above it, as between Explorer's navigation groups
    std::wstring  iconId;                 // the shell's name for the entry whose icon it shows
};





////////////////////////////////////////////////////////////////////////////////
//
//  TreeModel
//
//  The two roots and everything under them, produced on demand.
//
//  CHILDREN ARE FETCHED ON FIRST EXPAND AND KEPT until the caller invalidates
//  them; a folder of two hundred images is read once, not on every repaint.
//  Ids record the root and the path in text, so a node can be rebuilt from its
//  id alone after a refresh.
//
//  Whether a host folder exists is asked through a probe the caller supplies,
//  because the file-system seam speaks only of files; the shell probes the
//  real disk and a test answers from a list.
//
////////////////////////////////////////////////////////////////////////////////

class TreeModel
{
public:
    using DirectoryProbe = std::function<bool (const std::wstring & path)>;

    explicit TreeModel (IFileSystem & fs);

    void  SetKnownFolders   (std::vector<std::wstring> folders);
    bool  IsKnownFolder     (const std::wstring & path) const;
    void  SetDrives         (std::vector<std::wstring> driveRoots);
    void  SetDirectoryProbe (DirectoryProbe probe) { m_directoryProbe = std::move (probe); }
    bool  IsDirectory       (const std::wstring & path) const { return m_directoryProbe && m_directoryProbe (path); }

    //  Which hidden folders and images are listed. A change applies to the
    //  folders listed after it; the caller invalidates what is cached.
    void  SetFolderOptions  (const FolderOptions & options) { m_folderOptions = options; }

    //  The shell's navigation roots and pinned folders, read again on each
    //  call: Explorer's sections below Casso's own root. Without the shell the
    //  tree has Casso, This PC and the Recycle Bin alone.
    void  SetShellVerbs      (IShellItemVerbs * verbs) { m_shellVerbs = verbs; RefreshShellRoots(); }

    //  How the tree reads a shell folder: false while it is being read on a
    //  thread of its own, when the node shows nothing yet and asks again.
    using ShellFetch = std::function<bool (const std::wstring & id, std::vector<IShellItemVerbs::ShellFolderItem> & outItems)>;
    void  SetShellFetch      (ShellFetch fetch)        { m_shellFetch = std::move (fetch); }
    void  RefreshShellRoots  ();
    bool  IsPinnedFolder     (const std::wstring & path) const;

    //  The roots the navigation pane's own menu shows or hides, as
    //  Explorer's does; all folders adds the Recycle Bin. A root with no
    //  value of its own shows as File Explorer's pane shows it.
    struct NavPaneOptions
    {
        std::optional<bool>  showThisPc;
        std::optional<bool>  showNetwork;
        std::optional<bool>  showLibraries;
        bool                 showAllFolders = false;
    };

    enum class NavRoot { ThisPc, Network, Libraries };

    void  SetNavPaneOptions  (const NavPaneOptions & options) { m_navOptions = options; }
    bool  IsNavRootShown     (NavRoot root) const;

    void     GetRoots    (std::vector<TreeNode> & outNodes) const;
    HRESULT  GetChildren (const std::wstring & id, std::vector<TreeNode> & outNodes);

    void  Invalidate    (const std::wstring & id);
    void  InvalidateAll ();

    //  How many times a listing was actually produced, so a test can prove
    //  the cache answers the second expand.
    int   GetFetchCount () const { return m_fetchCount; }

    static bool  IsSupportedImage (const std::wstring & fileName);

    //  The id grammar. A root tag, then the host path, then for a directory
    //  inside an image the directory's name after a bar, and after another,
    //  for the second and later entries of the same name, which one it is.
    //  An image's error, the detail on a line after what is wrong.
    static std::wstring  BreakAfterFirstSentence (std::wstring text);

    //  The known folders' names as the tree shows them: each folder's own name,
    //  and for names two folders share, the name of the folder above in
    //  parentheses -- further up where those match too -- so the two differ.
    static std::vector<std::wstring>  GetKnownFolderLabels (const std::vector<std::wstring> & paths);

    static std::wstring  MakeFolderId    (bool underCasso, const std::wstring & path);
    static std::wstring  MakeImageId     (bool underCasso, const std::wstring & path);
    static std::wstring  MakeDirectoryId (bool underCasso, const std::wstring & path, const std::string & inner, size_t occurrence = 0);

    static constexpr const wchar_t *  kCassoRootId  = L"casso:";
    static constexpr const wchar_t *  kThisPcRootId = L"pc:";
    static constexpr const wchar_t *  kShellRootTag = L"nav:";

    //  A root's label in the tree, the tab and the address bar, or empty for
    //  an id that is no root.
    static std::wstring  GetRootLabel (const std::wstring & id);

private:
    static constexpr const wchar_t *  kImagePrefix     = L"img:";
    static constexpr const wchar_t *  kDirectoryPrefix = L"dir:";
    static constexpr const wchar_t *  kCassoTag        = L"casso";
    static constexpr const wchar_t *  kThisPcTag       = L"pc";
    static constexpr wchar_t          kSeparator       = L'|';

    HRESULT  ListRoot       (bool underCasso, std::vector<TreeNode> & outNodes);
    HRESULT  ListShellFolders (const std::wstring & id, std::vector<TreeNode> & outNodes);

    //  The node for an item of the shell's: a folder on a disk is a host
    //  folder, with all a host folder's children; anything else opens through
    //  the shell.
    static TreeNode  MakeShellNode (const IShellItemVerbs::ShellFolderItem & item);
    HRESULT  ListHostFolder (bool underCasso, const std::wstring & path, std::vector<TreeNode> & outNodes);
    bool     HasTreeChildren (const std::wstring & path);
    HRESULT  ListImage      (bool underCasso, const std::wstring & path, std::vector<TreeNode> & outNodes);
    HRESULT  ListImageDirectory (bool underCasso, const std::wstring & path, const std::string & inner, std::vector<TreeNode> & outNodes);

    static void  ListDirectories (bool underCasso, const std::wstring & path, const std::vector<Byte> & sectors,
                                  const std::string & inner, std::vector<TreeNode> & outNodes);

    //  Reads and parses one image, producing either its directory children
    //  or the error on the node.
    HRESULT  DescribeImage (bool underCasso, const std::wstring & path, TreeNode & inOutNode);

    static std::wstring  JoinPath (const std::wstring & folder, const std::wstring & name);
    static std::wstring  GetLeaf  (const std::wstring & path);
    static bool  TryParseImageOrDirectoryId (const std::wstring & id, bool & outUnderCasso,
                                             std::wstring & outPath, std::string & outInner);

    IFileSystem                                    & m_fs;
    std::vector<std::wstring>                        m_knownFolders;
    std::vector<std::wstring>                        m_drives;
    DirectoryProbe                                   m_directoryProbe;
    FolderOptions                                    m_folderOptions;
    std::map<std::wstring, std::vector<TreeNode>>    m_children;
    int                                              m_fetchCount = 0;
    IShellItemVerbs                                * m_shellVerbs = nullptr;
    ShellFetch                                       m_shellFetch;
    std::vector<IShellItemVerbs::ShellFolderItem>    m_navRoots;
    std::vector<IShellItemVerbs::ShellFolderItem>    m_pinned;
    NavPaneOptions                                   m_navOptions;
};
