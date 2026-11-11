#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../EmuTests/FixtureProvider.h"
#include "../UiTests/InMemoryFileSystem.h"
#include "CassoExplorer/Model/TreeModel.h"
#include "Core/TextEncoding.h"

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
        AssertSucceeded (fs.WriteAllText (L"C:\\Disks\\dos33.dsk",    FixtureContent ("CassoExplorer/dos33.dsk")));
        AssertSucceeded (fs.WriteAllText (L"C:\\Disks\\prodos.po",    FixtureContent ("CassoExplorer/prodos.po")));
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



    TEST_METHOD (Roots_CassoThisPcAndRecycleBin)
    {
        InMemoryFileSystem     fs;
        TreeModel              model (fs);
        std::vector<TreeNode>      roots;
        std::vector<TreeNode>      binChildren;
        TreeModel::NavPaneOptions  options;

        //  Explorer's pane shows the bin only with all folders shown.
        model.GetRoots (roots);
        Assert::AreEqual ((size_t) 4, roots.size());
        Assert::IsTrue   (roots[0].kind == TreeNode::Kind::CassoLinkRoot, L"CassoLink at the top, with no machines yet");
        Assert::IsTrue   (roots[3].kind == TreeNode::Kind::CassoLinkRoot, L"and again after This PC");

        options.showAllFolders = true;
        model.SetNavPaneOptions (options);
        model.GetRoots (roots);

        Assert::AreEqual ((size_t) 5, roots.size());
        Assert::IsTrue   (roots[1].kind == TreeNode::Kind::CassoRoot);
        Assert::IsTrue   (roots[2].kind == TreeNode::Kind::ThisPcRoot);
        Assert::IsTrue   (roots[4].kind == TreeNode::Kind::RecycleBinRoot);
        Assert::AreEqual (std::wstring (TreeModel::kCassoRootId),  roots[1].id);
        Assert::AreEqual (std::wstring (TreeModel::kThisPcRootId), roots[2].id);
        Assert::IsTrue   (roots[4].location == Location::MakeRecycleBin());
        Assert::IsFalse  (roots[4].canExpand, L"The Recycle Bin has no folders under it in the tree");

        AssertSucceeded (model.GetChildren (roots[4].id, binChildren));
        Assert::IsTrue  (binChildren.empty());
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



    TEST_METHOD (ThisPc_ListsStorageCardsAfterItsDrives)
    {
        InMemoryFileSystem     fs;
        TreeModel              model (fs);
        std::vector<TreeNode>  nodes;

        SeedHost (fs);
        Configure (model);
        model.SetStorageCards ({ StorageCardTreeEntry { L"\\\\.\\PhysicalDrive2", L"Storage card (SanDisk Ultra, 29.7 GB)" } });

        AssertSucceeded (model.GetChildren (TreeModel::kThisPcRootId, nodes));

        Assert::AreEqual ((size_t) 2, nodes.size());
        Assert::IsTrue   (nodes[0].kind == TreeNode::Kind::Drive);
        Assert::IsTrue   (nodes[1].kind == TreeNode::Kind::DiskImage);
        Assert::AreEqual (std::wstring (L"Storage card (SanDisk Ultra, 29.7 GB)"), nodes[1].label);
        Assert::IsFalse  (nodes[1].canExpand, L"a card opens when selected, not when the tree is read");
        Assert::IsTrue   (nodes[1].location == Location::MakeDiskImage (L"\\\\.\\PhysicalDrive2"));
    }



    TEST_METHOD (ThisPc_LeavesOutADriveLetterTheCardNodeStandsFor)
    {
        InMemoryFileSystem     fs;
        TreeModel              model (fs);
        std::vector<TreeNode>  nodes;
        StorageCardTreeEntry   card { L"\\.\PhysicalDrive2", L"Storage card (I:, Test, 64 MB)" };

        SeedHost (fs);
        Configure (model);
        model.SetDrives ({ kDrive, L"I:\\" });
        card.hiddenRoots = { L"I:\\" };
        model.SetStorageCards ({ card });

        AssertSucceeded (model.GetChildren (TreeModel::kThisPcRootId, nodes));

        Assert::AreEqual ((size_t) 2, nodes.size());
        Assert::IsTrue   (nodes[0].kind == TreeNode::Kind::Drive);
        Assert::AreEqual (std::wstring (L"Storage card (I:, Test, 64 MB)"), nodes[1].label);
    }



    TEST_METHOD (ThisPc_AnOpenStorageCardHasItsPartitionsAsChildren)
    {
        InMemoryFileSystem     fs;
        TreeModel              model (fs);
        std::vector<TreeNode>  nodes;
        StorageCardTreeEntry   card { L"\\\\.\\PhysicalDrive2", L"Storage card (Test, 64 MB)" };
        std::wstring           cardId;

        SeedHost (fs);
        Configure (model);
        card.partitions = { { L"#1.po", L"Partition 1: CARD1 (32 MB)" }, { L"#2.po", L"Partition 2: CARD2 (32 MB)" } };
        model.SetStorageCards ({ card });

        AssertSucceeded (model.GetChildren (TreeModel::kThisPcRootId, nodes));
        Assert::IsTrue (nodes.back().canExpand);

        cardId = nodes.back().id;
        AssertSucceeded (model.GetChildren (cardId, nodes));

        Assert::AreEqual ((size_t) 2, nodes.size());
        Assert::AreEqual (std::wstring (L"Partition 2: CARD2 (32 MB)"), nodes[1].label);
        Assert::IsTrue   (nodes[1].location == Location::MakeDiskImage (L"\\\\.\\PhysicalDrive2|#2.po"));
        Assert::IsFalse  (nodes[1].canExpand);
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

        //  Not a disk: no affordance, and the loader's reason to show.
        Assert::IsFalse (nodes[2].canExpand);
        Assert::IsTrue  (nodes[2].loadError.starts_with (L"notadisk.dsk is "));
        Assert::IsTrue  (nodes[2].loadError.find (L"143,360 bytes") != std::wstring::npos, L"The size a .dsk image has, as the list gives it");

        //  ProDOS: the subdirectory is a child.
        Assert::IsTrue  (nodes[3].canExpand);
        Assert::IsTrue  (nodes[3].location == Location::MakeDiskImage (L"C:\\Disks\\prodos.po"));
    }



    TEST_METHOD (NetworkFolder_IsReadThroughTheFetchAndNothingIsKeptWhilePending)
    {
        InMemoryFileSystem     fs;
        TreeModel              model (fs);
        std::vector<TreeNode>  nodes;
        bool                   arrived = false;
        int                    fetches = 0;
        const wchar_t        * share   = L"\\\\server\\share";
        std::wstring           id      = TreeModel::MakeFolderId (false, share);

        model.SetHostFetch ([&] (const std::wstring & path, std::vector<FileSystemEntry> & outEntries, HRESULT & outResult)
        {
            FileSystemEntry  folder;

            fetches++;
            Assert::AreEqual (std::wstring (share), path);

            folder.name     = L"Games";
            folder.isFolder = true;
            outEntries      = { folder };
            outResult       = S_OK;

            return arrived;
        });

        Assert::AreEqual (S_FALSE, model.GetChildren (id, nodes), L"Being read: nothing yet");
        Assert::IsTrue   (nodes.empty());

        arrived = true;
        AssertSucceeded  (model.GetChildren (id, nodes));
        Assert::AreEqual ((size_t) 1, nodes.size(), L"The pending read was not cached, so the arrival is taken");
        Assert::AreEqual (std::wstring (L"Games"), nodes[0].label);
        Assert::IsTrue   (nodes[0].canExpand, L"A network folder has an expander without being looked into");
        Assert::AreEqual (2, fetches, L"and only the fetch is called for it, never the local file system");
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



    //  A directory inside an image expands to the directories inside it, each
    //  addressed by its full path from the volume directory, and can expand
    //  only when it has subdirectories.
    TEST_METHOD (ProDosDirectory_ChildrenAreTheDirectoriesInsideIt)
    {
        InMemoryFileSystem     fs;
        TreeModel              model (fs);
        std::vector<TreeNode>  folder;
        std::vector<TreeNode>  image;
        std::vector<TreeNode>  inside;

        AssertSucceeded (fs.WriteAllText (L"C:\\Disks\\merlin.dsk", FixtureContent ("Disks/Merlin-proProdos2.33-a.dsk")));
        Configure (model);

        AssertSucceeded (model.GetChildren (TreeModel::MakeFolderId (true, kDisks), folder));
        AssertSucceeded (model.GetChildren (FindNode (folder, L"merlin.dsk").id, image));

        Assert::IsFalse (image.empty(), L"this disk must have subdirectories");

        for (const TreeNode & directory : image)
        {
            AssertSucceeded  (model.GetChildren (directory.id, inside));
            Assert::AreEqual (!inside.empty(), directory.canExpand, L"a directory can expand only when it contains subdirectories");

            for (const TreeNode & child : inside)
            {
                const std::string &  parent   = directory.location.innerPath;
                const std::string &  actual   = child.location.innerPath;
                std::wstring         expected = TextEncoding::NarrowToWide (parent) + L"/" + child.label;

                Assert::IsTrue   (child.kind == TreeNode::Kind::DiskDirectory);
                Assert::AreEqual (expected, TextEncoding::NarrowToWide (actual));
            }
        }
    }



    //  A damaged directory holding two entries of the same name gives the
    //  second its own id, which still reaches the directory by that name.
    TEST_METHOD (ProDosDirectory_ADuplicateNameStillReachesTheDirectory)
    {
        InMemoryFileSystem     fs;
        TreeModel              model (fs);
        std::vector<TreeNode>  folder;
        std::vector<TreeNode>  image;
        std::vector<TreeNode>  plain;
        std::vector<TreeNode>  second;
        std::wstring           path = L"C:\\Disks\\merlin.dsk";
        std::string            inner;

        AssertSucceeded (fs.WriteAllText (path, FixtureContent ("Disks/Merlin-proProdos2.33-a.dsk")));
        Configure (model);

        AssertSucceeded (model.GetChildren (TreeModel::MakeFolderId (true, kDisks), folder));
        AssertSucceeded (model.GetChildren (FindNode (folder, L"merlin.dsk").id, image));
        Assert::IsFalse (image.empty(), L"this disk must have subdirectories");

        inner = image[0].location.innerPath;

        Assert::AreNotEqual (TreeModel::MakeDirectoryId (true, path, inner), TreeModel::MakeDirectoryId (true, path, inner, 1));

        AssertSucceeded  (model.GetChildren (TreeModel::MakeDirectoryId (true, path, inner),    plain));
        AssertSucceeded  (model.GetChildren (TreeModel::MakeDirectoryId (true, path, inner, 1), second));
        Assert::AreEqual (plain.size(), second.size());
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



    TEST_METHOD (KnownFolderLabels_SameNamesTakeTheFoldersAboveThatMakeThemDiffer)
    {
        std::vector<std::wstring>  labels = TreeModel::GetKnownFolderLabels ({ L"C:\\Users\\Rob\\AppData\\Local\\Casso\\Disks",
                                                                               L"C:\\Users\\Rob\\Downloads\\ADTPro-2.1.0\\disks",
                                                                               L"C:\\Games", L"C:\\",
                                                                               L"C:\\X\\Same\\Art", L"D:\\Y\\Same\\Art",
                                                                               L"C:\\Music", L"D:\\Music" });

        Assert::AreEqual (std::wstring (L"Casso\\Disks"),        labels[0], L"One folder above tells the two Disks apart");
        Assert::AreEqual (std::wstring (L"ADTPro-2.1.0\\disks"), labels[1], L"whatever the case of the name");
        Assert::AreEqual (std::wstring (L"Games"),               labels[2], L"A name no other folder has stands alone");
        Assert::AreEqual (std::wstring (L"C:\\"),                labels[3], L"A drive's root is its path");
        Assert::AreEqual (std::wstring (L"X\\Same\\Art"),        labels[4], L"Two folders above, where one is the same");
        Assert::AreEqual (std::wstring (L"Y\\Same\\Art"),        labels[5]);
        Assert::AreEqual (std::wstring (L"C:\\Music"),           labels[6], L"Folders that differ only by drive are their paths");
        Assert::AreEqual (std::wstring (L"D:\\Music"),           labels[7]);
    }


    static std::vector<LinkTreeEntry>  TwoLinks()
    {
        LinkTreeEntry  replying;
        LinkTreeEntry  silent;

        replying.name     = L"Apple II (COM3)";
        replying.replying = true;
        replying.drives   = { { 6, 1, false, L"DOS 3.3 volume 254" }, { 6, 2, true, L"" } };

        silent.name   = L"Apple II (Casso)";
        silent.status = L"Not connected: Casso is not running on port 6502";

        return { replying, silent };
    }



    TEST_METHOD (Links_FollowThisPc_AsRoots_AfterCassoLink)
    {
        InMemoryFileSystem     fs;
        TreeModel              model (fs);
        std::vector<TreeNode>  roots;

        model.SetLinks (TwoLinks());
        model.GetRoots (roots);

        Assert::AreEqual ((size_t) 4, roots.size());
        Assert::IsTrue   (roots[0].kind == TreeNode::Kind::CassoLinkRoot);
        Assert::AreEqual (std::wstring (L"top:cassolink:"), roots[0].id);
        Assert::IsTrue   (roots[0].location == Location::MakeRoot (TreeModel::kCassoLinkRootId), L"both CassoLink nodes open the same list");
        Assert::IsTrue   (roots[2].kind == TreeNode::Kind::ThisPcRoot);
        Assert::IsTrue   (roots[2].dividerAbove);
        Assert::IsTrue   (roots[3].kind == TreeNode::Kind::CassoLinkRoot);
        Assert::AreEqual (std::wstring (TreeModel::kCassoLinkRootId), roots[3].id);
    }



    //  An ADTPro server lists the folder it shares under its root.
    TEST_METHOD (AnAdtProServer_ListsItsFolder)
    {
        InMemoryFileSystem     fs;
        TreeModel              model (fs);
        std::vector<TreeNode>  nodes;
        LinkTreeEntry          server;

        server.name   = L"ADTPro server on COM3";
        server.folder = L"C:\\Disks\\ADTPro";

        model.SetLinks ({ server });
        AssertSucceeded (model.GetChildren (TreeModel::MakeLinkId (server.name), nodes));

        Assert::AreEqual ((size_t) 1, nodes.size());
        Assert::AreEqual (std::wstring (L"adtpro:C:\\Disks\\ADTPro"), nodes[0].id, L"its id ends in its path");
        Assert::AreEqual (std::wstring (L"ADTPro"), nodes[0].label);
        Assert::IsTrue   (nodes[0].location == Location::MakeHostFolder (L"C:\\Disks\\ADTPro"));
    }


    //  CassoLink lists where every connection's Full Send sessions write, with no Apple
    //  connected, once the folder exists.
    TEST_METHOD (CassoLink_ListsTheFullSendFolder_OnceItExists)
    {
        InMemoryFileSystem     fs;
        TreeModel              model (fs);
        std::vector<TreeNode>  nodes;
        LinkTreeEntry          apple;

        apple.name = L"Apple //c";

        model.SetLinks ({ apple });
        model.SetFullSendFolder (L"C:\\Box");
        AssertSucceeded (model.GetChildren (TreeModel::kCassoLinkRootId, nodes));
        Assert::AreEqual ((size_t) 2, nodes.size(), L"the connection and the row that adds one");

        AssertSucceeded (fs.WriteAllText (L"C:\\Box\\0001.dsk", "x"));

        model.SetFullSendFolder (L"C:\\Box");
        AssertSucceeded (model.GetChildren (TreeModel::kCassoLinkRootId, nodes));

        Assert::AreEqual ((size_t) 3, nodes.size());
        Assert::AreEqual (std::wstring (L"Full Send"), nodes[1].label);
        Assert::IsTrue   (nodes[1].location == Location::MakeHostFolder (L"C:\\Box"));
        Assert::IsTrue   (nodes[2].kind == TreeNode::Kind::AddLink);
    }

    TEST_METHOD (ATransfer_MarksTheConnectionAndItsReceivingFolder)
    {
        InMemoryFileSystem         fs;
        TreeModel                  model (fs);
        LinkTreeEntry              apple;
        LinkTreeEntry              server;
        std::vector<TreeNode>      roots;
        std::vector<TreeNode>      shared;

        AssertSucceeded (fs.WriteAllText (L"C:\\Box\\0001.dsk", "x"));
        AssertSucceeded (fs.WriteAllText (L"C:\\Disks\\ADTPro\\in.dsk", "x"));

        apple.name         = L"Apple //c";
        apple.replying     = true;
        apple.transferring = true;
        apple.fullSending  = true;
        server.name        = L"ADTPro server";
        server.folder      = L"C:\\Disks\\ADTPro";
        server.transferring = true;

        model.SetLinks ({ apple, server });
        model.SetFullSendFolder (L"C:\\Box");
        model.SetUnseenFiles (L"C:\\Box", 3);
        model.SetUnseenFiles (L"C:\\Disks\\ADTPro", 2);

        AssertSucceeded (model.GetChildren (TreeModel::kCassoLinkRootId, roots));
        AssertSucceeded (model.GetChildren (TreeModel::MakeLinkId (L"ADTPro server"), shared));

        auto  sent = std::find_if (roots.begin(), roots.end(), [] (const TreeNode & node) { return node.label == L"Full Send"; });

        Assert::IsTrue (roots[0].transferring);
        Assert::IsTrue (sent != roots.end());
        Assert::IsTrue (sent->transferring);
        Assert::AreEqual (3, sent->unseen);
        Assert::IsTrue (shared[0].transferring);
        Assert::AreEqual (2, shared[0].unseen);
    }

    TEST_METHOD (IsReceivedFile_LeavesOutTheFilesATransferDoesNotDeliver)
    {
        Assert::IsTrue  (TreeModel::IsReceivedFile (L"0001 GAMES.dsk"));
        Assert::IsTrue  (TreeModel::IsReceivedFile (L"FROM APPLE.PO"));
        Assert::IsFalse (TreeModel::IsReceivedFile (L"manifest.fullsend"));
        Assert::IsFalse (TreeModel::IsReceivedFile (L"full-send.csv"));
        Assert::IsFalse (TreeModel::IsReceivedFile (L"0001 GAMES.dsk.unreadable.txt"));
        Assert::IsFalse (TreeModel::IsReceivedFile (L"README.md"));
    }

    TEST_METHOD (IsLinkFolder_MatchesTheFullSendFolderAndAnAdtProServersFolder)
    {
        InMemoryFileSystem  fs;
        TreeModel           model (fs);
        LinkTreeEntry       server;

        server.name   = L"ADTPro server on COM3";
        server.folder = L"C:\\Disks\\ADTPro";

        model.SetLinks ({ server });
        model.SetFullSendFolder (L"C:\\Box");

        Assert::IsTrue  (model.IsLinkFolder (L"c:\\box"));
        Assert::IsTrue  (model.IsLinkFolder (L"C:\\Disks\\ADTPro"));
        Assert::IsFalse (model.IsLinkFolder (L"C:\\Disks"));
    }

    TEST_METHOD (AConnection_DoesNotListTheFullSendFolder)
    {
        InMemoryFileSystem     fs;
        TreeModel              model (fs);
        std::vector<TreeNode>  nodes;
        LinkTreeEntry          apple;

        AssertSucceeded (fs.WriteAllText (L"C:\\Box\\0001.dsk", "x"));

        apple.name     = L"Apple //c";
        apple.replying = true;

        model.SetLinks ({ apple });
        model.SetFullSendFolder (L"C:\\Box");
        AssertSucceeded (model.GetChildren (TreeModel::MakeLinkId (apple.name), nodes));

        Assert::IsTrue (nodes.empty());
    }


    TEST_METHOD (CassoLink_ListsEveryLink_UnderIdsOfItsOwn)
    {
        InMemoryFileSystem     fs;
        TreeModel              model (fs);
        std::vector<TreeNode>  links;

        model.SetLinks (TwoLinks());
        AssertSucceeded (model.GetChildren (TreeModel::kCassoLinkRootId, links));

        Assert::AreEqual ((size_t) 3, links.size(), L"the links, then Add a CassoLink connection");
        Assert::IsTrue   (links[0].kind == TreeNode::Kind::CassoLinkConnection);
        Assert::AreEqual (std::wstring (L"link:Apple II (COM3)"), links[0].id);
        Assert::IsTrue   (links[0].location == Location::MakeRoot (L"link:Apple II (COM3)"));
        Assert::AreEqual (std::wstring (L"Apple II (Casso)"), links[1].label);

        AssertSucceeded (model.GetChildren (TreeModel::kLinkCopyTag + std::wstring (TreeModel::kCassoLinkRootId), links));

        Assert::AreEqual ((size_t) 3, links.size());
        Assert::AreEqual (std::wstring (L"top:link:Apple II (COM3)"), links[0].id);
        Assert::IsTrue   (links[0].location == Location::MakeRoot (L"link:Apple II (COM3)"), L"both copies open the same link");
        Assert::AreEqual (std::wstring (L"Apple II (Casso)"), links[1].label);
    }



    TEST_METHOD (LinkCopies_ListTheSameDrives_UnderTheirOwnIds)
    {
        InMemoryFileSystem     fs;
        TreeModel              model (fs);
        std::vector<TreeNode>  drives;
        std::wstring           name;
        int                    slot  = 0;
        int                    drive = 0;

        model.SetLinks (TwoLinks());
        AssertSucceeded (model.GetChildren (L"top:link:Apple II (COM3)", drives));

        Assert::AreEqual ((size_t) 2, drives.size());
        Assert::AreEqual (std::wstring (L"top:link:Apple II (COM3)|6,1"), drives[0].id);
        Assert::IsTrue   (drives[0].location == Location::MakeAppleDrive (L"link:Apple II (COM3)|6,1", std::string()));
        Assert::IsTrue   (TreeModel::TryParseLinkId (drives[0].id, name, slot, drive));
        Assert::AreEqual (std::wstring (L"Apple II (COM3)"), name);
        Assert::AreEqual (6, slot);
        Assert::AreEqual (1, drive);
    }



    TEST_METHOD (Links_ListTheirDrives_WithSlotDriveAndVolume)
    {
        InMemoryFileSystem     fs;
        TreeModel              model (fs);
        std::vector<TreeNode>  drives;

        model.SetLinks (TwoLinks());
        AssertSucceeded (model.GetChildren (L"link:Apple II (COM3)", drives));

        Assert::AreEqual ((size_t) 2, drives.size());
        Assert::IsTrue   (drives[0].kind == TreeNode::Kind::AppleDrive);
        Assert::AreEqual (std::wstring (L"Slot 6, Drive 1 (DOS 3.3 volume 254)"), drives[0].label);
        Assert::AreEqual (std::wstring (L"link:Apple II (COM3)|6,1"), drives[0].id);
        Assert::IsTrue   (drives[0].location == Location::MakeAppleDrive (L"link:Apple II (COM3)|6,1", std::string()));
        Assert::AreEqual (std::wstring (L"Slot 6, Drive 2 (no disk or unformatted)"), drives[1].label);
        Assert::IsFalse  (drives[1].missing, L"a drive with no readable disk can be formatted, so it is not dimmed");
    }



    TEST_METHOD (Links_WithNoAppleReplying_HaveNoChildren_AndTheirStateIsForTheList)
    {
        InMemoryFileSystem     fs;
        TreeModel              model (fs);
        std::vector<TreeNode>  children;
        std::wstring           status;

        model.SetLinks (TwoLinks());
        AssertSucceeded (model.GetChildren (L"link:Apple II (Casso)", children));

        Assert::IsTrue (children.empty(), L"no row under a machine that is not connected");
        Assert::IsTrue (model.GetLinkStatus (L"Apple II (Casso)", status));
        Assert::AreEqual (std::wstring (L"Not connected: Casso is not running on port 6502"), status);
    }



    TEST_METHOD (Links_SetAgain_ListTheirDrivesAgain)
    {
        InMemoryFileSystem          fs;
        TreeModel                   model (fs);
        std::vector<TreeNode>       children;
        std::vector<LinkTreeEntry>  links = TwoLinks();

        model.SetLinks (links);
        AssertSucceeded (model.GetChildren (L"link:Apple II (Casso)", children));

        links[1].replying = true;
        links[1].drives   = { { 6, 1, false, L"LINKED" } };
        model.SetLinks (links);
        AssertSucceeded (model.GetChildren (L"link:Apple II (Casso)", children));

        Assert::AreEqual ((size_t) 1, children.size());
        Assert::AreEqual (std::wstring (L"Slot 6, Drive 1 (LINKED)"), children[0].label);
    }



    TEST_METHOD (LinkIds_RoundTrip)
    {
        std::wstring  name;
        int           slot  = 0;
        int           drive = 0;

        Assert::IsTrue   (TreeModel::TryParseLinkId (TreeModel::MakeLinkDriveId (L"Apple", 6, 2), name, slot, drive));
        Assert::AreEqual (std::wstring (L"Apple"), name);
        Assert::AreEqual (6, slot);
        Assert::AreEqual (2, drive);

        Assert::IsTrue   (TreeModel::TryParseLinkId (TreeModel::MakeLinkId (L"Apple"), name, slot, drive));
        Assert::AreEqual (0, drive, L"a root has no drive");

        Assert::IsFalse  (TreeModel::TryParseLinkId (L"link:Apple|9,1", name, slot, drive));
        Assert::IsFalse  (TreeModel::TryParseLinkId (L"pc:C:\\", name, slot, drive));
    }



    TEST_METHOD (IsUnderCasso_TakesItsFoldersImagesAndDirectories)
    {
        Assert::IsTrue  (TreeModel::IsUnderCasso (TreeModel::MakeFolderId    (true, L"C:\\Disks")));
        Assert::IsTrue  (TreeModel::IsUnderCasso (TreeModel::MakeImageId     (true, L"C:\\Disks\\a.po")));
        Assert::IsTrue  (TreeModel::IsUnderCasso (TreeModel::MakeDirectoryId (true, L"C:\\Disks\\a.po", "MP")), L"A directory inside an image");
        Assert::IsFalse (TreeModel::IsUnderCasso (TreeModel::kCassoRootId), L"but not the Casso node itself");
        Assert::IsFalse (TreeModel::IsUnderCasso (TreeModel::MakeImageId     (false, L"C:\\Disks\\a.po")), L"nor This PC's");
        Assert::IsFalse (TreeModel::IsUnderCasso (TreeModel::MakeDirectoryId (false, L"C:\\Disks\\a.po", "MP")));
    }
};
