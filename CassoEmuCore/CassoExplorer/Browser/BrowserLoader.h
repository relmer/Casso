#pragma once

#include "Pch.h"

#include "CassoExplorer/CassoExplorerShellListings.h"
#include "CassoExplorer/Model/PreviewDecoder.h"
#include "CassoExplorer/Model/TreeModel.h"

class CassoExplorerBrowser;





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
    explicit BrowserLoader (CassoExplorerBrowser & browser);

    bool  LeaveMissingLocation ();

    bool  PrepareCassoLinkItems ();

    bool  TryGetSelectedCassoLinkPaths (std::vector<std::wstring> & outPaths) const;

    //  CassoLink's help, as the What's CassoLink item has it.
    static const wchar_t *  GetCassoLinkHelp ();

    //  An ADTPro-compatible server's help, its virtual item's text.
    static const wchar_t *  GetAdtProHelp ();

    //  The help text a CassoLink root's or an ADTPro server's help item has.
    static const wchar_t *  GetHelpText   (CatalogRow::CassoLinkItem item);

    void  GetSelectedRecycledIds (std::vector<std::wstring> & outIds) const;
    void  GetAllRecycledIds      (std::vector<std::wstring> & outIds) const;

    bool  TryGetRowEntry (int row, FileEntry & outEntry) const;

    //  What the disk runner says of a whole disk it finds no file system on.
    static constexpr const char *  s_kNoFileSystem = "does not have a DOS or ProDOS file system";

    std::wstring  GetImageProblem (const std::wstring & imagePath);

    //  Refresh for a navigation, whose failure is already in the list error.
    void     ReloadAfterNavigation();
    bool     IsOnMissingDrive (const Location & location) const;
    HRESULT  LoadHostFolder (const std::wstring & path);
    HRESULT  LoadRoot       (const std::wstring & id);
    HRESULT  LoadRecycleBin ();
    HRESULT  LoadShellFolder (const std::wstring & id);
    HRESULT  LoadImage      (const std::wstring & path, const std::string & directory);
    HRESULT  LoadAppleDrive (const Location & location);
    void     AddCassoLinkItems ();
    void     AddAdtProHelpItem ();
    bool     IsAdtProServer    (const std::string & name) const;
    void     RevisitLink       (const Location & location);
    bool     CanListImage   (const std::wstring & imagePath);
    bool     TryGetImageLocation   (int row, Location & outLocation) const;

private:
    friend class CassoExplorerBrowser;

    CassoExplorerBrowser  & m_browser;

    //  Where Up goes from the shell folder listed; none at the top.
    Location                          m_shellParent;
    CassoExplorerHostListings         m_hostListings;
    Location                          m_linkVisit;   // the linked machine or drive last listed
    std::map<std::wstring, TreeNode>  m_nodes;
    std::vector<FileSystemEntry>      m_hostEntries;
    FolderOptions                     m_folderOptions;
    std::vector<TreeNode>             m_rootChildren;
    bool                              m_writeProtected = false;
    std::wstring                      m_listError;
};
