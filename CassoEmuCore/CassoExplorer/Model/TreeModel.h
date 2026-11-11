#pragma once

#include IPch.hI

#include ICassoExplorer/Model/FolderOptions.hI
#include ICassoExplorer/Model/Location.hI
#include IConfig/IFileSystem.hI
#include ISeams/IShellItemVerbs.hI

class FilesystemChoices;





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
    enum class Kind { CassoRoot, ThisPcRoot, RecycleBinRoot, KnownFolder, Drive, HostFolder, DiskImage, DiskDirectory, ShellRoot, CassoLinkConnection, AppleDrive, CassoLinkRoot, AddLink };

    std::wstring  id;
    Kind          kind         = Kind::HostFolder;
    std::wstring  label;
    Location      location;
    bool          canExpand    = false;
    bool          missing      = false;   // a known folder that is no longer there, drawn dimmed
    bool          pinned       = false;   // pinned to Quick access, drawn with a pin
    bool          archive      = false;   // a file the shell opens as a folder, such as a zip
    bool          hidden       = false;   // a hidden host folder, its icon drawn faded
    std::wstring  loadError;   // an image that failed to parse: the tooltip and the list-area message
    bool          broken       = false;   // an image that is not a disk at all, as against one with no file system
    bool          dividerAbove = false;   // a line above it, as between Explorer's navigation groups
    bool          offline      = false;   // a linked machine that is not connected, drawn in grays
    bool          warning      = false;   // a connection Windows Firewall blocks, drawn as a warning sign
    bool          transferring = false;   // a connection or its receiving folder during a transfer, drawn with a pulsing dot
    int           unseen       = 0;       // a receiving folder's files not yet listed, drawn as a count
    std::wstring  iconId;                 // the shell's name for the entry whose icon it shows
};





////////////////////////////////////////////////////////////////////////////////
//
//  StorageCardTreeEntry
//
//  A storage card with a partition Windows hasn't mounted, as the tree
//  lists it under This PC.
//
////////////////////////////////////////////////////////////////////////////////

struct StorageCardTreeEntry
{
    struct Partition
    {
        //  The partition's step, `#3.po`, and its label in the tree.
        std::wstring  step;
        std::wstring  label;
    };

    //  The card's path, `\\.\PhysicalDrive2`.
    std::wstring            path;
    std::wstring            label;

    //  The card's Apple partitions, once it is open. A card that is not open
    //  has no expander, as opening it raises the UAC prompt.
    std::vector<Partition>  partitions;

    //  The drive letters of the card's volumes that Windows can't read, as
    //  "I:\". The card's node replaces them, and they are left out of
    //  This PC and the tree's top level.
    std::vector<std::wstring>  hiddenRoots;
};





////////////////////////////////////////////////////////////////////////////////
//
//  LinkTreeEntry
//
//  An CassoLink as the tree lists it: a root with the link's name, and
//  under it the Apple's drives, or, while Casso's program is not replying,
//  the link's state and the reason in their place.
//
////////////////////////////////////////////////////////////////////////////////

struct LinkTreeEntry
{
    struct Drive
    {
        int           slot    = 6;
        int           drive   = 1;
        bool          empty   = false;
        std::wstring  volume;

        //  A ProDOS drive's top-level folders, once its catalog is read.
        std::vector<std::wstring>  folders;
    };

    std::wstring        name;
    std::wstring        model;   // the Apple's model once connected, as Apple //c
    bool                replying     = false;
    bool                probing      = false;   // a drive's catalog being read; its row is left out until it arrives
    bool                transferring = false;
    bool                fullSending  = false;   // the transfer is a Full Send, received in the Full Send folder
    std::wstring        status;
    std::vector<Drive>  drives;
    std::wstring        folder;   // an ADTPro server's shared folder, listed under it

    //  Why Windows Firewall blocks what the connection receives, for one that
    //  other computers connect or send to; empty when nothing is blocked.
    std::wstring        firewall;
    bool                firewallPublic = false;   // the blocking is on a public network, so making it private is offered
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

    //  A folder's localized name from its desktop.ini, or empty for none.
    using LocalizedNameLookup = std::function<std::wstring (const std::wstring & path)>;

    explicit TreeModel (IFileSystem & fs);

    void                         SetKnownFolders        (std::vector<std::wstring> folders);
    bool                         IsKnownFolder          (const std::wstring & path) const;
    void                         SetDrives              (std::vector<std::wstring> driveRoots);

    //  The storage cards listed under This PC after its drives.
    void                         SetStorageCards        (std::vector<StorageCardTreeEntry> cards);
    void                         SetDirectoryProbe      (DirectoryProbe probe) { m_directoryProbe = std::move (probe); }
    bool                         IsDirectory            (const std::wstring & path) const { return m_directoryProbe && m_directoryProbe (path); }
    void                         SetLocalizedNameLookup (LocalizedNameLookup lookup) { m_localizedName = std::move (lookup); }
    const LocalizedNameLookup  & GetLocalizedNameLookup () const { return m_localizedName; }

    //  Which hidden folders and images are listed. A change applies to the
    //  folders listed after it; the caller invalidates what is cached.
    void  SetFolderOptions  (const FolderOptions & options) { m_folderOptions = options; }

    //  The file systems chosen for images detection doesn't recognize.
    void  SetFilesystemChoices (const FilesystemChoices * choices) { m_filesystemChoices = choices; }


    //  The shell's navigation roots and pinned folders, read again on each
    //  call: Explorer's sections below Casso's own root. Without the shell the
    //  tree has Casso, This PC and the Recycle Bin alone.
    void  SetShellVerbs      (IShellItemVerbs * verbs) { m_shellVerbs = verbs; RefreshShellRoots(); }

    //  How the tree reads a shell folder: false while it is being read on a
    //  thread of its own, when the node shows nothing yet and asks again.
    using ShellFetch = std::function<bool (const std::wstring & id, std::vector<IShellItemVerbs::ShellFolderItem> & outItems)>;
    void  SetShellFetch      (ShellFetch fetch)        { m_shellFetch = std::move (fetch); }

    //  How the tree reads a host folder on a network path, the same way:
    //  false while it is being read, so an unreachable share never stops the
    //  window. Without one, every host folder is read at once.
    using HostFetch = std::function<bool (const std::wstring & path, std::vector<FileSystemEntry> & outEntries, HRESULT & outResult)>;
    void  SetHostFetch       (HostFetch fetch)         { m_hostFetch = std::move (fetch); }
    void  RefreshShellRoots  ();
    bool  IsPinnedFolder     (const std::wstring & path) const;

    //  Whether a file opens as a folder, as File Explorer opens a zip. The
    //  shell is asked once for each extension; a disk image is Casso's own.
    bool  IsArchive          (const std::wstring & path) const;

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

    //  The CassoLink connections, each under CassoLink. Their drives are listed
    //  again on the next expand.
    void  SetLinks           (std::vector<LinkTreeEntry> links);

    //  Where every connection's Full Send sessions write, listed under CassoLink once it
    //  exists.
    void  SetFullSendFolder  (std::wstring folder);

    //  Whether a folder is one disks arrive in over CassoLink: the Full Send
    //  folder, or an ADTPro server's shared folder.
    bool  IsLinkFolder       (const std::wstring & path) const;

    //  The Full Send folder and each ADTPro server's. Each has a count of files
    //  received and not yet listed, and only files that a transfer delivers are
    //  counted, not the manifest or a list of unreadable sectors.
    std::vector<std::wstring>  GetLinkFolders () const;
    void                       SetUnseenFiles (const std::wstring & folder, int count);
    int                        GetUnseenFiles (const std::wstring & folder) const;
    static bool                IsReceivedFile (const std::wstring & name);

    static constexpr const wchar_t *  kNotReceived[] = { LI.fullsendI, LI.csvI, LI.txtI, LI.mdI };

    //  A link's state while its Apple is not replying, for the list in place
    //  of its drives; false once it replies.
    bool  GetLinkStatus      (const std::wstring & name, std::wstring & outStatus) const;

    //  Whether the link's Apple is replying to Casso's program.
    bool                                IsLinkReplying (const std::wstring & name) const;
    const std::vector<LinkTreeEntry>  & GetLinks       () const { return m_links; }
    bool                                IsNavRootShown (NavRoot root) const;

    void     GetRoots    (std::vector<TreeNode> & outNodes) const;
    HRESULT  GetChildren (const std::wstring & id, std::vector<TreeNode> & outNodes);

    void  Invalidate    (const std::wstring & id);
    void  InvalidateAll ();

    //  How many times a listing was actually produced, so a test can prove
    //  the cache answers the second expand.
    int   GetFetchCount () const { return m_fetchCount; }

    static bool  IsSupportedImage (const std::wstring & fileName);

    //  What follows an image's file name when it loads but has no file system
    //  this browser reads.
    static constexpr const wchar_t *  kNoFilesystemText = LIcan't be previewed because it doesn't have a recognized file system.I;

    //  What follows a partition's label when the partition ends past the end
    //  of the image.
    static constexpr const wchar_t *  kDamagedPartitionText = LIis damaged: it ends past the end of the image.I;

    //  The id grammar. A root tag, then the host path, then for a directory
    //  inside an image the directory's name after a bar, and after another,
    //  for the second and later entries of the same name, which one it is.
    //  An image's error, the detail on a line after what is wrong.
    static std::wstring  BreakAfterFirstSentence (std::wstring text);

    //  The known folders' labels in the tree: each folder's name, or for a name
    //  two folders share, that name after as many of the folders above it as
    //  make the two differ, such as Casso\Disks and ADTPro-2.1.0\disks.
    static std::vector<std::wstring>  GetKnownFolderLabels (const std::vector<std::wstring> & paths);

    static std::wstring  MakeFolderId    (bool underCasso, const std::wstring & path);
    static std::wstring  MakeImageId     (bool underCasso, const std::wstring & path);
    static std::wstring  MakeDirectoryId (bool underCasso, const std::wstring & path, const std::string & inner, size_t occurrence = 0);

    //  Whether a node is below Casso: a folder, a disk image or a directory in
    //  one, though not the Casso node itself.
    static bool          IsUnderCasso    (const std::wstring & id);

    static constexpr const wchar_t *  kCassoRootId  = LIcasso:I;
    static constexpr const wchar_t *  kThisPcRootId = LIpc:I;
    static constexpr const wchar_t *  kShellRootTag = LInav:I;

    //  File Explorer's one root with the all-folders option on, the desktop's
    //  namespace, with Home, This PC and the rest under it.
    static constexpr const wchar_t *  kDesktopRootId = LIdesktop:I;

    //  An CassoLink's root, link:<name>, and its drives, link:<name>|<slot>,<drive>.
    static constexpr const wchar_t *  kLinkRootTag   = LIlink:I;

    //  CassoLink's root, after This PC, and its copy after Casso: every
    //  linked Apple, with CassoLink.dsk and its help beside them in the list.
    static constexpr const wchar_t *  kCassoLinkRootId = LIcassolink:I;

    //  A folder listed where it is not on the disk, adtpro:<path>: an ADTPro
    //  server's under its connection, or a Full Send's under CassoLink,
    //  which lists the folder there rather than where the folder is.
    static constexpr const wchar_t *  kAdtProId        = LIadtpro:I;

    //  The node that adds a CassoLink connection; selecting it opens the
    //  dialog, and the list stays where it was.
    static constexpr const wchar_t *  kAddLinkId       = LIaddlink:I;

    //  A storage card under This PC: card:<its path>.
    static constexpr const wchar_t *  kStorageCardTag  = LIcard:I;

    //  The copy of CassoLink's root after Casso, and of the links and drives
    //  under it: top:<id>, at the same location as the first.
    static constexpr const wchar_t *  kLinkCopyTag   = LItop:I;

    static std::wstring  StripLinkCopy    (const std::wstring & id);

    static std::wstring  MakeLinkId       (const std::wstring & name);
    static std::wstring  MakeLinkDriveId  (const std::wstring & name, int slot, int drive);
    static bool          TryParseLinkId   (const std::wstring & id, std::wstring & outName, int & outSlot, int & outDrive);

    //  ISlot 6, Drive 1I, and the volume in parentheses after it when known.
    static std::wstring  GetLinkDriveLabel (int slot, int drive);
    static std::wstring  GetLinkDriveLabel (const LinkTreeEntry::Drive & drive);

    //  A drive of a link, by its id, as the tree labels it: its slot, its drive
    //  and its volume once known.
    std::wstring  GetLinkDriveTitle (const std::wstring & driveId) const;

    //  A root's label in the tree, the tab and the address bar, or empty for
    //  an id that is no root.
    static std::wstring  GetRootLabel (const std::wstring & id);

private:
    static constexpr const wchar_t *  kImagePrefix     = LIimg:I;
    static constexpr const wchar_t *  kDirectoryPrefix = LIdir:I;
    static constexpr const wchar_t *  kCassoTag        = LIcassoI;
    static constexpr const wchar_t *  kThisPcTag       = LIpcI;
    static constexpr wchar_t          kSeparator       = L'|';

    HRESULT  ListRoot        (bool underCasso, std::vector<TreeNode> & outNodes);
    void     ListStorageCard (const std::wstring & path, std::vector<TreeNode> & outNodes) const;
    HRESULT  ListLink        (const std::wstring & name, std::vector<TreeNode> & outNodes);
    HRESULT  ListLinkFolders (const std::wstring & driveId, std::vector<TreeNode> & outNodes) const;
    void     AppendLinkRoots (std::vector<TreeNode> & outNodes, bool copy) const;

    HRESULT   ListShellFolders (const std::wstring & id, std::vector<TreeNode> & outNodes);
    HRESULT   ListDesktopRoot  (std::vector<TreeNode> & outNodes);
    TreeNode  MakeRootNode     (const std::wstring & id) const;

    //  The node for an item of the shell's: a folder on a disk is a host
    //  folder, with all a host folder's children; anything else opens through
    //  the shell.
    TreeNode         MakeShellNode      (const IShellItemVerbs::ShellFolderItem & item) const;
    bool             IsHiddenDrive      (const std::wstring & path) const;
    HRESULT          ListHostFolder     (bool underCasso, const std::wstring & path, std::vector<TreeNode> & outNodes);
    bool             HasTreeChildren    (const std::wstring & path);
    HRESULT          ListImage          (bool underCasso, const std::wstring & path, std::vector<TreeNode> & outNodes);
    HRESULT          ListImageDirectory (bool underCasso, const std::wstring & path, const std::string & inner, std::vector<TreeNode> & outNodes);

    static void  ListDirectories (bool underCasso, const std::wstring & path, const std::vector<Byte> & sectors,
                                  const std::string & inner, std::vector<TreeNode> & outNodes);

    //  A node for each partition of an image file. The list is empty for a
    //  file with no partitions.
    static void  ListPartitions (bool underCasso, const std::wstring & path, const std::vector<Byte> & fileBytes,
                                 const std::string & name, std::vector<TreeNode> & outNodes);

    //  An image's bytes, and the file name whose extension indicates their
    //  format. `path` is a host file, or a container path to an image inside
    //  another file.
    HRESULT  ReadImage (const std::wstring & path, std::vector<Byte> & outBytes, std::string & outName);

    //  Reads and parses one image, producing either its directory children
    //  or the error on the node.
    HRESULT  DescribeImage (bool underCasso, const std::wstring & path, TreeNode & inOutNode);

    static std::wstring  JoinPath (const std::wstring & folder, const std::wstring & name);
    static std::wstring  GetLeaf  (const std::wstring & path);
    static bool  TryParseImageOrDirectoryId (const std::wstring & id, bool & outUnderCasso,
                                             std::wstring & outPath, std::string & outInner);

    IFileSystem                                    & m_fs;
    std::map<std::wstring, int>                      m_unseen;
    std::vector<std::wstring>                        m_knownFolders;
    std::vector<std::wstring>                        m_drives;
    std::vector<StorageCardTreeEntry>                m_storageCards;
    DirectoryProbe                                   m_directoryProbe;
    mutable std::map<std::wstring, bool>             m_archiveExtensions;
    LocalizedNameLookup                              m_localizedName;
    FolderOptions                                    m_folderOptions;
    const FilesystemChoices                        * m_filesystemChoices = nullptr;
    std::map<std::wstring, std::vector<TreeNode>>    m_children;
    int                                              m_fetchCount = 0;
    IShellItemVerbs                                * m_shellVerbs = nullptr;
    ShellFetch                                       m_shellFetch;
    HostFetch                                        m_hostFetch;
    std::vector<IShellItemVerbs::ShellFolderItem>    m_navRoots;
    std::vector<IShellItemVerbs::ShellFolderItem>    m_pinned;
    NavPaneOptions                                   m_navOptions;
    std::vector<LinkTreeEntry>                       m_links;
    std::wstring                                     m_fullSendFolder;
};
