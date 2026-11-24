#pragma once

#include "Pch.h"

#include "CassoExplorer/Browser/BrowserParts.h"
#include "CassoExplorer/CassoExplorerShellListings.h"
#include "CassoExplorer/Model/BrowserModel.h"
#include "CassoExplorer/Model/PreviewDecoder.h"
#include "CassoExplorer/Model/TreeModel.h"
#include "Devices/Disk/Partitions/PartitionAccess.h"

class BrowserArchives;
class IBrowserLoaderHost;





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserLoader
//
//  Each kind of location read into the list's rows: a host folder, a
//  shell folder, the Recycle Bin, a root, a disk image, a linked Apple's
//  drive, and the CassoLink items under a connection.
//
////////////////////////////////////////////////////////////////////////////////

class BrowserLoader
{
public:
    BrowserLoader (const BrowserParts & parts, const BrowserArchives & archives, IBrowserLoaderHost & host);

    //  When the folder or image the active tab shows is gone from the disk,
    //  moves to the nearest folder above it that is still there, as Explorer
    //  does when the folder it shows is deleted. False, with nothing
    //  changed, when the location still exists or is not on the host.
    bool  LeaveMissingLocation ();

    //  The partitions of an image, or of a card that is open.
    HRESULT  ListPartitions (const std::wstring & path, PartitionLayoutKind & outKind, std::vector<PartitionEntry> & outEntries);

    //  The selected CassoLink items written to their files, so they open and
    //  drag as files; false when any could not be written.
    bool  PrepareCassoLinkItems ();

    //  The paths of the selection when every row is a CassoLink item.
    bool  TryGetSelectedCassoLinkPaths (std::vector<std::wstring> & outPaths) const;

    //  CassoLink's help, as the What's CassoLink item has it.
    static const wchar_t *  GetCassoLinkHelp ();

    //  An ADTPro-compatible server's help, its virtual item's text.
    static const wchar_t *  GetAdtProHelp ();

    //  The help text a CassoLink root's or an ADTPro server's help item has.
    static const wchar_t *  GetHelpText   (CatalogRow::CassoLinkItem item);

    //  The shell's ids for the Recycle Bin items selected.
    void  GetSelectedRecycledIds (std::vector<std::wstring> & outIds) const;
    void  GetAllRecycledIds      (std::vector<std::wstring> & outIds) const;

    //  The catalog entry a row inside an image stands for.
    bool  TryGetRowEntry (int row, FileEntry & outEntry) const;

    //  What the disk runner says of a whole disk it finds no file system on.
    static constexpr const char *  s_kNoFileSystem = "does not have a recognized file system";

    //  Why an image cannot be read as a disk at all, as the list's message
    //  says it; empty for one that can, a whole disk with no file system to
    //  list included, since that is a disk and not a broken file.
    std::wstring  GetImageProblem (const std::wstring & imagePath);

    //  Refresh for a navigation, whose failure is already in the list error.
    void     ReloadAfterNavigation ();
    bool     IsOnMissingDrive      (const Location & location) const;
    HRESULT  LoadHostFolder        (const std::wstring & path);
    HRESULT  LoadRoot              (const std::wstring & id);
    HRESULT  LoadCassoImages       ();
    HRESULT  LoadRecycleBin        ();
    HRESULT  LoadShellFolder       (const std::wstring & id);
    HRESULT  LoadImage             (const std::wstring & path, const std::string & directory);
    HRESULT  LoadPartition         (const std::wstring & path);
    bool     AddPartitionRows      (const std::wstring & path, PartitionLayoutKind layout, const std::vector<PartitionEntry> & entries);
    HRESULT  LoadCard              (const std::wstring & path);
    HRESULT  LoadAppleDrive        (const Location & location);
    void     AddCassoLinkItems     ();
    void     AddAdtProHelpItem     ();
    bool     IsAdtProServer        (const std::string & name) const;
    void     RevisitLink           (const Location & location);

    static constexpr ULONGLONG  kLinkRevisitMs = 30000;   // a visit sooner than this to a drive lists what was read
    bool     CanListImage          (const std::wstring & imagePath);
    bool     TryGetImageLocation   (int row, Location & outLocation) const;

    std::wstring  MakeContainerPath (const std::wstring & folderId, const std::wstring & name) const;

    //  The tree nodes read so far, by id, and the readers of host folders on
    //  a network path.
    std::map<std::wstring, TreeNode>        & GetNodes        ()       { return m_nodes; }
    const std::map<std::wstring, TreeNode>  & GetNodes        () const { return m_nodes; }
    CassoExplorerHostListings               & GetHostListings ()       { return m_hostListings; }

    //  The linked machine or drive last listed. Returning to a tab lists what
    //  was last read there; forgetting the visit reads its disks again.
    void  SetLinkVisit    (const Location & location) { m_linkVisit = location; }
    void  ForgetLinkVisit ()                          { m_linkVisit = Location(); m_linkReadAt.clear(); }
    //  The path a shell folder's image row opens by: its path on the disk, or
    //  in a ZIP, its ContainerPath's text.
    static const std::wstring &  GetImagePath (const CatalogRow & row) { return row.hostPath.empty() ? row.containerPath : row.hostPath; }

private:
    struct ImageProblem
    {
        uint64_t      time = 0;
        uint64_t      size = 0;
        std::wstring  problem;
    };

    std::unordered_map<std::wstring, ImageProblem>  m_imageProblems;

    Location  GetLocation      () const { return m_parts.model.HasTabs() ? m_parts.model.GetActiveTab().location : Location(); }
    bool      IsVolumeLocation () const { return m_parts.list.isImage || m_parts.list.isAppleDrive; }
    bool      IsCardInDrive    (int number);

    BrowserParts                         m_parts;
    const BrowserArchives              & m_archives;
    IBrowserLoaderHost                 & m_host;
    CassoExplorerHostListings            m_hostListings;
    Location                             m_linkVisit;    // the linked machine or drive last listed
    std::map<std::wstring, ULONGLONG>    m_linkReadAt;   // when each linked machine or drive was last read again
    std::map<std::wstring, TreeNode>     m_nodes;
};
