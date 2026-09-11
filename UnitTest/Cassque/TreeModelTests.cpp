#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../EmuTests/FixtureProvider.h"
#include "../UiTests/InMemoryFileSystem.h"
#include "Cassque/Model/TreeModel.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TreeModelTests
//
//  The two roots over an in-memory host holding the scratch images, a file
//  wearing a disk extension that is no disk, and a folder that is gone.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (TreeModelTests)
{
public:

    static constexpr const wchar_t *  kDisks   = L"C:\\Disks";
    static constexpr const wchar_t *  kGone    = L"D:\\Gone";
    static constexpr const wchar_t *  kDrive   = L"C:\\";



    static std::string FixtureContent (const char * name)
    {
        FixtureProvider    fixtures;
        std::vector<Byte>  bytes;

        AssertSucceeded (fixtures.OpenFixture (name, bytes));

        return std::string (bytes.begin(), bytes.end());
    }



    static void SeedHost (InMemoryFileSystem & fs)
    {
        AssertSucceeded (fs.WriteAllText (L"C:\\Disks\\dos33.dsk",    FixtureContent ("Cassque/dos33.dsk")));
        AssertSucceeded (fs.WriteAllText (L"C:\\Disks\\prodos.po",    FixtureContent ("Cassque/prodos.po")));
        AssertSucceeded (fs.WriteAllText (L"C:\\Disks\\notadisk.dsk", "this is not a disk image"));
        AssertSucceeded (fs.WriteAllText (L"C:\\Disks\\readme.txt",   "plain file, not a node"));
        AssertSucceeded (fs.WriteAllText (L"C:\\Disks\\Sub\\x.txt",   "x"));
    }



    static void Configure (TreeModel & model)
    {
        model.SetKnownFolders ({ kDisks, kGone });
        model.SetDrives ({ kDrive });
        model.SetDirectoryProbe ([] (const std::wstring & path) { return _wcsicmp (path.c_str(), kDisks) == 0 || _wcsicmp (path.c_str(), kDrive) == 0; });
    }



    static const TreeNode & FindNode (const std::vector<TreeNode> & nodes, const wchar_t * label)
    {
        for (const TreeNode & node : nodes)
        {
            if (node.label == label)
            {
                return node;
            }
        }

        Assert::Fail (L"node not found");

        return nodes[0];
    }



    TEST_METHOD (Roots_CassoAndThisPc)
    {
        InMemoryFileSystem     fs;
        TreeModel              model (fs);
        std::vector<TreeNode>  roots;

        model.GetRoots (roots);

        Assert::AreEqual ((size_t) 2, roots.size());
        Assert::IsTrue   (roots[0].kind == TreeNode::Kind::CassoRoot);
        Assert::IsTrue   (roots[1].kind == TreeNode::Kind::ThisPcRoot);
        Assert::AreEqual (std::wstring (TreeModel::kCassoRootId),  roots[0].id);
        Assert::AreEqual (std::wstring (TreeModel::kThisPcRootId), roots[1].id);
    }



    TEST_METHOD (CassoRoot_ListsKnownFoldersAndDimsTheMissingOne)
    {
        InMemoryFileSystem     fs;
        TreeModel              model (fs);
        std::vector<TreeNode>  nodes;

        SeedHost (fs);
        Configure (model);

        AssertSucceeded (model.GetChildren (TreeModel::kCassoRootId, nodes));

        Assert::AreEqual ((size_t) 2, nodes.size());
        Assert::IsTrue   (nodes[0].kind == TreeNode::Kind::KnownFolder);
        Assert::IsFalse  (nodes[0].missing);
        Assert::IsTrue   (nodes[0].canExpand);
        Assert::IsTrue   (nodes[1].missing, L"the folder that is gone is still listed, dimmed");
        Assert::IsFalse  (nodes[1].canExpand);
        Assert::IsTrue   (nodes[0].location == Location::MakeHostFolder (kDisks));
    }



    TEST_METHOD (Folder_ListsSubfoldersThenImagesAndNotPlainFiles)
    {
        InMemoryFileSystem     fs;
        TreeModel              model (fs);
        std::vector<TreeNode>  nodes;

        SeedHost (fs);
        Configure (model);

        AssertSucceeded (model.GetChildren (TreeModel::MakeFolderId (true, kDisks), nodes));

        Assert::AreEqual ((size_t) 4, nodes.size());
        Assert::AreEqual (std::wstring (L"Sub"),          nodes[0].label);
        Assert::IsTrue   (nodes[0].kind == TreeNode::Kind::HostFolder);
        Assert::AreEqual (std::wstring (L"dos33.dsk"),    nodes[1].label);
        Assert::AreEqual (std::wstring (L"notadisk.dsk"), nodes[2].label);
        Assert::AreEqual (std::wstring (L"prodos.po"),    nodes[3].label);

        //  DOS 3.3: an image with no children and no error.
        Assert::IsTrue  (nodes[1].kind == TreeNode::Kind::DiskImage);
        Assert::IsFalse (nodes[1].canExpand);
        Assert::IsTrue  (nodes[1].loadError.empty());

        //  Not a disk: no affordance, an error to show.
        Assert::IsFalse (nodes[2].canExpand);
        Assert::IsFalse (nodes[2].loadError.empty());

        //  ProDOS: the subdirectory is a child.
        Assert::IsTrue  (nodes[3].canExpand);
        Assert::IsTrue  (nodes[3].location == Location::MakeDiskImage (L"C:\\Disks\\prodos.po"));
    }



    TEST_METHOD (ProDosImage_ChildrenAreItsDirectories_Dos33HasNone)
    {
        InMemoryFileSystem     fs;
        TreeModel              model (fs);
        std::vector<TreeNode>  folder;
        std::vector<TreeNode>  prodos;
        std::vector<TreeNode>  dos33;

        SeedHost (fs);
        Configure (model);

        AssertSucceeded (model.GetChildren (TreeModel::MakeFolderId (true, kDisks), folder));
        AssertSucceeded (model.GetChildren (FindNode (folder, L"prodos.po").id, prodos));
        AssertSucceeded (model.GetChildren (FindNode (folder, L"dos33.dsk").id, dos33));

        Assert::AreEqual ((size_t) 1, prodos.size());
        Assert::AreEqual (std::wstring (L"SUBDIR"), prodos[0].label);
        Assert::IsTrue   (prodos[0].kind == TreeNode::Kind::DiskDirectory);
        Assert::IsTrue   (prodos[0].location == Location::MakeDiskDirectory (L"C:\\Disks\\prodos.po", "SUBDIR"));
        Assert::IsFalse  (prodos[0].canExpand);

        Assert::AreEqual ((size_t) 0, dos33.size());
    }



    TEST_METHOD (Children_AreFetchedOnceUntilInvalidated)
    {
        InMemoryFileSystem     fs;
        TreeModel              model (fs);
        std::vector<TreeNode>  first;
        std::vector<TreeNode>  second;
        std::wstring           id = TreeModel::MakeFolderId (true, kDisks);

        SeedHost (fs);
        Configure (model);

        AssertSucceeded (model.GetChildren (id, first));
        AssertSucceeded (model.GetChildren (id, second));

        Assert::AreEqual (1, model.GetFetchCount());
        Assert::AreEqual (first.size(), second.size());

        model.Invalidate (id);

        AssertSucceeded (model.GetChildren (id, second));
        Assert::AreEqual (2, model.GetFetchCount());
    }



    TEST_METHOD (ThisPc_ListsDrivesAndTheirFolders)
    {
        InMemoryFileSystem     fs;
        TreeModel              model (fs);
        std::vector<TreeNode>  drives;
        std::vector<TreeNode>  folders;

        SeedHost (fs);
        Configure (model);

        AssertSucceeded (model.GetChildren (TreeModel::kThisPcRootId, drives));

        Assert::AreEqual ((size_t) 1, drives.size());
        Assert::IsTrue   (drives[0].kind == TreeNode::Kind::Drive);
        Assert::AreEqual (std::wstring (kDrive), drives[0].label);

        AssertSucceeded (model.GetChildren (drives[0].id, folders));

        Assert::AreEqual ((size_t) 1, folders.size());
        Assert::AreEqual (std::wstring (L"Disks"), folders[0].label);
        Assert::AreEqual (TreeModel::MakeFolderId (false, L"C:\\Disks"), folders[0].id);
    }



    TEST_METHOD (Ids_DistinguishTheRootTheSameFolderIsReachedUnder)
    {
        Assert::AreNotEqual (TreeModel::MakeFolderId (true, kDisks), TreeModel::MakeFolderId (false, kDisks));
        Assert::AreNotEqual (TreeModel::MakeImageId (true, L"C:\\a.dsk"), TreeModel::MakeImageId (false, L"C:\\a.dsk"));
        Assert::IsTrue      (TreeModel::IsSupportedImage (L"GAME.WOZ"));
        Assert::IsTrue      (TreeModel::IsSupportedImage (L"disk.po"));
        Assert::IsFalse     (TreeModel::IsSupportedImage (L"notes.txt"));
        Assert::IsFalse     (TreeModel::IsSupportedImage (L"noext"));
    }
};
