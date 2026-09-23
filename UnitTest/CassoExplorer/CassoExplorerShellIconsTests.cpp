#include "Pch.h"

#include "CassoExplorer/CassoExplorerBrowser.h"
#include "Seams/IShellIcons.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  RecordingShellIcons
//
//  Records each request and returns one marker image for a path and another
//  for a kind, so a test can tell which request produced the icon on a node or
//  a row.
//
////////////////////////////////////////////////////////////////////////////////

class RecordingShellIcons : public IShellIcons
{
public:
    std::shared_ptr<const DxuiIconImage>  GetForPath (const std::wstring & path, bool isDirectory) override
    {
        paths.push_back (path);
        directoryFlags.push_back (isDirectory);
        return pathIcon;
    }

    std::shared_ptr<const DxuiIconImage>  GetForKind (Kind kind) override
    {
        kinds.push_back (kind);
        return kindIcon;
    }

    std::vector<std::wstring>             paths;
    std::vector<bool>                     directoryFlags;
    std::vector<Kind>                     kinds;
    std::shared_ptr<const DxuiIconImage>  pathIcon = std::make_shared<DxuiIconImage>();
    std::shared_ptr<const DxuiIconImage>  kindIcon = std::make_shared<DxuiIconImage>();
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShellIconsTests
//
//  A real path gets the shell's icon for that path; anything without a host
//  path (an entry inside an image, This PC, Casso) gets a kind icon. Only the
//  name column has an icon.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoExplorerShellIconsTests)
{
public:

    TEST_METHOD (HostFolderRow_AsksForTheRealPath)
    {
        RecordingShellIcons              icons;
        CatalogRow                       row;
        std::vector<DxuiListView::Cell>  cells;


        row.name = L"GAMES.DSK";
        cells    = CassoExplorerBrowser::ToCells (row, Location::MakeHostFolder (L"C:\\Disks"), &icons);

        Assert::AreEqual ((size_t) 1, icons.paths.size(), L"One question for one row");
        Assert::AreEqual (CassoExplorerBrowser::JoinPath (L"C:\\Disks", L"GAMES.DSK"), icons.paths[0],
            L"and it is the file's own path, so a disk image shows what its extension is registered to");
        Assert::IsTrue (cells[0].icon == icons.pathIcon, L"The name column shows it");

        for (size_t c = 1; c < cells.size(); c++)
        {
            Assert::IsFalse ((bool) cells[c].icon, L"and no other column does");
        }
    }


    TEST_METHOD (HostFolderRow_SaysWhetherItIsADirectory)
    {
        RecordingShellIcons  icons;
        CatalogRow           folder;
        CatalogRow           file;
        Location             at = Location::MakeHostFolder (L"C:\\Disks");

        folder.name        = L"SUBDIR";
        folder.isDirectory = true;
        file.name          = L"GAMES.DSK";

        Assert::IsFalse (CassoExplorerBrowser::ToCells (folder, at, &icons).empty());
        Assert::IsFalse (CassoExplorerBrowser::ToCells (file,   at, &icons).empty());

        //  Told, so the icons never have to ask the file system which it is.
        Assert::AreEqual ((size_t) 2, icons.directoryFlags.size());
        Assert::IsTrue   (icons.directoryFlags[0], L"A folder says so");
        Assert::IsFalse  (icons.directoryFlags[1], L"and a file says so");
    }


    TEST_METHOD (RowInsideAnImage_GetsAPlainFolderOrFile)
    {
        RecordingShellIcons  icons;
        CatalogRow           folder;
        CatalogRow           file;


        folder.name        = L"UTILS";
        folder.isDirectory = true;
        file.name          = L"HELLO";

        Assert::IsFalse (CassoExplorerBrowser::ToCells (folder, Location::MakeDiskImage (L"C:\\Disks\\A.po"), &icons).empty());
        Assert::IsFalse (CassoExplorerBrowser::ToCells (file,   Location::MakeDiskImage (L"C:\\Disks\\A.po"), &icons).empty());

        Assert::IsTrue (icons.paths.empty(), L"An entry inside an image has no path to ask about");
        Assert::AreEqual ((size_t) 2, icons.kinds.size());
        Assert::IsTrue (icons.kinds[0] == IShellIcons::Kind::Folder, L"A directory gets the plain folder");
        Assert::IsTrue (icons.kinds[1] == IShellIcons::Kind::File,   L"and a file the plain file");
    }


    TEST_METHOD (TreeNodes_MapByKind)
    {
        RecordingShellIcons  icons;
        TreeNode             casso;
        TreeNode             thisPc;
        TreeNode             inner;
        TreeNode             drive;
        DxuiTreeNode         out;


        casso.kind     = TreeNode::Kind::CassoRoot;
        thisPc.kind    = TreeNode::Kind::ThisPcRoot;
        inner.kind     = TreeNode::Kind::DiskDirectory;
        drive.kind     = TreeNode::Kind::Drive;
        drive.location = Location::MakeHostFolder (L"C:\\");

        out = CassoExplorerBrowser::ToTreeNode (casso,  &icons);
        out = CassoExplorerBrowser::ToTreeNode (thisPc, &icons);
        out = CassoExplorerBrowser::ToTreeNode (inner,  &icons);
        out = CassoExplorerBrowser::ToTreeNode (drive,  &icons);

        Assert::AreEqual ((size_t) 3, icons.kinds.size());
        Assert::IsTrue (icons.kinds[0] == IShellIcons::Kind::Casso,  L"Casso's node gets the emulator's own icon");
        Assert::IsTrue (icons.kinds[1] == IShellIcons::Kind::ThisPc, L"This PC gets the shell's This PC");
        Assert::IsTrue (icons.kinds[2] == IShellIcons::Kind::Folder, L"a directory inside an image the plain folder");

        Assert::AreEqual ((size_t) 1, icons.paths.size());
        Assert::AreEqual (std::wstring (L"C:\\"), icons.paths[0], L"and a drive its own, by path");
        Assert::IsTrue (out.icon == icons.pathIcon, L"The node holds what came back");
    }


    TEST_METHOD (NoProvider_NoIcons)
    {
        CatalogRow    row;
        TreeNode      node;


        row.name  = L"GAMES.DSK";
        node.kind = TreeNode::Kind::CassoRoot;

        Assert::IsFalse ((bool) CassoExplorerBrowser::ToCells (row)[0].icon,
            L"A caller that asks for no icons, as every existing one does, gets none");
        Assert::IsFalse ((bool) CassoExplorerBrowser::ToTreeNode (node).icon);
    }
};
