#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../UiTests/InMemoryFileSystem.h"
#include "CassoExplorer/Model/TreeChildChecks.h"
#include "CassoExplorer/Model/TreeModel.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TreeChildChecksTests
//
//  The tree's subfolder checks, run on demand in place of the worker.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (TreeChildChecksTests)
{
public:

    static constexpr const wchar_t *  kDisks = L"C:\\Disks";



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



    TEST_METHOD (Check_IsQueuedUntilRun_AndStoredByModifiedTime)
    {
        TreeChildChecks  checks;
        int              calls       = 0;
        bool             hasChildren = false;

        checks.SetManualRun (true);
        checks.SetChecker ([&calls] (const std::wstring &) { calls++; return true; });

        Assert::IsFalse (checks.TryGet (L"C:\\A", 10, hasChildren));
        Assert::IsFalse (checks.TryGet (L"C:\\A", 10, hasChildren), L"a queued check isn't queued twice");
        Assert::AreEqual (0, calls);

        checks.RunQueued();
        Assert::AreEqual (1, calls);

        std::vector<TreeChildChecks::Result>  results = checks.TakeResults();

        Assert::AreEqual ((size_t) 1, results.size());
        Assert::IsTrue   (results[0].hasChildren);

        Assert::IsTrue  (checks.TryGet (L"C:\\A", 10, hasChildren));
        Assert::IsTrue  (hasChildren);
        Assert::AreEqual (1, calls, L"a folder with the same modified time isn't checked again");

        Assert::IsFalse (checks.TryGet (L"C:\\A", 11, hasChildren), L"a modified folder is checked again");
        checks.RunQueued();
        Assert::AreEqual (2, calls);
    }



    TEST_METHOD (Clear_DropsStoredResultsAndQueuedChecks)
    {
        TreeChildChecks  checks;
        int              calls       = 0;
        bool             hasChildren = false;

        checks.SetManualRun (true);
        checks.SetChecker ([&calls] (const std::wstring &) { calls++; return true; });

        checks.TryGet (L"C:\\A", 10, hasChildren);
        checks.RunQueued();
        checks.TakeResults();

        checks.TryGet (L"C:\\B", 10, hasChildren);
        checks.Clear();
        checks.RunQueued();

        Assert::AreEqual (1, calls, L"a check queued before the clear doesn't run");
        Assert::IsTrue   (checks.TakeResults().empty());
        Assert::IsFalse  (checks.TryGet (L"C:\\A", 10, hasChildren), L"the stored result is dropped");
    }



    TEST_METHOD (Tree_RowsAppearBeforeTheChecks_AndEachResultSetsItsRow)
    {
        InMemoryFileSystem     fs;
        HostFolderReader       hostFolders;
        TreeModel              model (fs, hostFolders);
        std::vector<TreeNode>  nodes;
        std::wstring           folderId = TreeModel::MakeFolderId (true, kDisks);

        AssertSucceeded (fs.WriteAllText (L"C:\\Disks\\Plain\\x.txt",   "x"));
        AssertSucceeded (fs.WriteAllText (L"C:\\Disks\\Images\\a.dsk",  "a"));

        model.SetKnownFolders ({ kDisks });
        model.GetChildChecks().SetManualRun (true);

        AssertSucceeded (model.GetChildren (folderId, nodes));

        Assert::AreEqual ((size_t) 2, nodes.size());
        Assert::IsFalse  (FindNode (nodes, L"Images").canExpand, L"no expander until the check finishes");
        Assert::IsFalse  (FindNode (nodes, L"Plain").canExpand);
        Assert::AreEqual ((size_t) 0, model.GetChildChecks().GetCheckCount());

        model.GetChildChecks().RunQueued();

        std::vector<std::pair<std::wstring, bool>>  applied = model.ApplyChildChecks();

        Assert::AreEqual ((size_t) 2, applied.size());

        for (const auto & [id, expands] : applied)
        {
            Assert::AreEqual (id == FindNode (nodes, L"Images").id, expands);
        }

        AssertSucceeded (model.GetChildren (folderId, nodes));
        Assert::IsTrue  (FindNode (nodes, L"Images").canExpand, L"the cached row is set");
        Assert::IsFalse (FindNode (nodes, L"Plain").canExpand);

        model.Invalidate (folderId);
        AssertSucceeded (model.GetChildren (folderId, nodes));

        Assert::IsTrue   (FindNode (nodes, L"Images").canExpand, L"a reload uses the stored result");
        Assert::AreEqual ((size_t) 2, model.GetChildChecks().GetCheckCount(), L"and opens neither folder again");
    }



    //  When a subfolder is made or deleted inside a closed folder, the
    //  parent's listing is the same, but the closed folder's expander changes.
    TEST_METHOD (Tree_ACurrentListingRechecksAModifiedSubfolder)
    {
        InMemoryFileSystem     fs;
        HostFolderReader       hostFolders;
        TreeModel              model (fs, hostFolders);
        std::vector<TreeNode>  nodes;
        std::wstring           folderId = TreeModel::MakeFolderId (true, kDisks);

        AssertSucceeded (fs.WriteAllText (L"C:\\Disks\\Plain\\x.txt", "x"));

        model.SetKnownFolders ({ kDisks });
        model.GetChildChecks().SetManualRun (true);

        AssertSucceeded (model.GetChildren (folderId, nodes));
        model.GetChildChecks().RunQueued();
        model.ApplyChildChecks();
        Assert::IsTrue  (model.IsHostListingCurrent (folderId));
        model.GetChildChecks().RunQueued();
        Assert::AreEqual ((size_t) 0, model.ApplyChildChecks().size(), L"an unmodified subfolder isn't checked again");

        AssertSucceeded (fs.WriteAllText (L"C:\\Disks\\Plain\\Sub\\y.txt", "y"));
        fs.SetFolderModifiedUnix (L"C:\\Disks\\Plain", 50);

        Assert::IsTrue (model.IsHostListingCurrent (folderId), L"the parent lists the same subfolder");

        model.GetChildChecks().RunQueued();

        std::vector<std::pair<std::wstring, bool>>  applied = model.ApplyChildChecks();

        Assert::AreEqual ((size_t) 1, applied.size());
        Assert::IsTrue   (applied[0].second, L"Plain has a subfolder now");
        Assert::AreEqual ((size_t) 2, model.GetChildChecks().GetCheckCount());
    }



    TEST_METHOD (Tree_AListedFolderUsesItsChildren)
    {
        InMemoryFileSystem     fs;
        HostFolderReader       hostFolders;
        TreeModel              model (fs, hostFolders);
        std::vector<TreeNode>  nodes;
        std::vector<TreeNode>  children;
        std::wstring           folderId = TreeModel::MakeFolderId (true, kDisks);

        AssertSucceeded (fs.WriteAllText (L"C:\\Disks\\Images\\a.dsk", "a"));

        model.SetKnownFolders ({ kDisks });
        model.GetChildChecks().SetManualRun (true);

        AssertSucceeded (model.GetChildren (folderId, nodes));
        AssertSucceeded (model.GetChildren (FindNode (nodes, L"Images").id, children));

        model.Invalidate (folderId);
        AssertSucceeded (model.GetChildren (folderId, nodes));

        Assert::IsTrue (FindNode (nodes, L"Images").canExpand);
    }
};
