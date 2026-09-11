#pragma once

#include "Pch.h"

#include "Cassque/Model/Location.h"
#include "Config/IFileSystem.h"





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
    enum class Kind { CassoRoot, ThisPcRoot, KnownFolder, Drive, HostFolder, DiskImage, DiskDirectory };

    std::wstring  id;
    Kind          kind      = Kind::HostFolder;
    std::wstring  label;
    Location      location;
    bool          canExpand = false;
    bool          missing   = false;     // a known folder that is no longer there, drawn dimmed
    std::wstring  loadError;             // an image that failed to parse: the tooltip and the list-area message
};





////////////////////////////////////////////////////////////////////////////////
//
//  TreeModel
//
//  The two roots and everything under them, produced on demand.
//
//  CHILDREN ARE FETCHED ON FIRST EXPAND AND KEPT until the caller invalidates
//  them; a folder of two hundred images is read once, not on every repaint.
//  Ids carry the root and the path in text, so a node can be rebuilt from its
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
    void  SetDrives         (std::vector<std::wstring> driveRoots);
    void  SetDirectoryProbe (DirectoryProbe probe) { m_directoryProbe = std::move (probe); }

    void     GetRoots    (std::vector<TreeNode> & outNodes) const;
    HRESULT  GetChildren (const std::wstring & id, std::vector<TreeNode> & outNodes);

    void  Invalidate    (const std::wstring & id);
    void  InvalidateAll ();

    //  How many times a listing was actually produced, so a test can prove
    //  the cache answers the second expand.
    int   GetFetchCount () const { return m_fetchCount; }

    static bool  IsSupportedImage (const std::wstring & fileName);

    //  The id grammar. A root tag, then the host path, then for a directory
    //  inside an image the directory's name after a bar.
    static std::wstring  MakeFolderId    (bool underCasso, const std::wstring & path);
    static std::wstring  MakeImageId     (bool underCasso, const std::wstring & path);
    static std::wstring  MakeDirectoryId (bool underCasso, const std::wstring & path, const std::string & inner);

    static constexpr const wchar_t *  kCassoRootId  = L"casso:";
    static constexpr const wchar_t *  kThisPcRootId = L"pc:";

private:
    static constexpr const wchar_t *  kImagePrefix     = L"img:";
    static constexpr const wchar_t *  kDirectoryPrefix = L"dir:";
    static constexpr const wchar_t *  kCassoTag        = L"casso";
    static constexpr const wchar_t *  kThisPcTag       = L"pc";
    static constexpr wchar_t          kSeparator       = L'|';

    HRESULT  ListRoot       (bool underCasso, std::vector<TreeNode> & outNodes);
    HRESULT  ListHostFolder (bool underCasso, const std::wstring & path, std::vector<TreeNode> & outNodes);
    HRESULT  ListImage      (bool underCasso, const std::wstring & path, std::vector<TreeNode> & outNodes);

    //  Reads and parses one image, producing either its directory children
    //  or the error the node carries.
    HRESULT  DescribeImage (bool underCasso, const std::wstring & path, TreeNode & inOutNode);

    static std::wstring  JoinPath (const std::wstring & folder, const std::wstring & name);
    static std::wstring  GetLeaf  (const std::wstring & path);
    static bool  TryParseImageOrDirectoryId (const std::wstring & id, bool & outUnderCasso,
                                             std::wstring & outPath, std::string & outInner);

    IFileSystem                                    & m_fs;
    std::vector<std::wstring>                        m_knownFolders;
    std::vector<std::wstring>                        m_drives;
    DirectoryProbe                                   m_directoryProbe;
    std::map<std::wstring, std::vector<TreeNode>>    m_children;
    int                                              m_fetchCount = 0;
};
