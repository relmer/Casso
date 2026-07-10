#include "Pch.h"

#include "../Casso/Shell/RepoCheckout.h"



using namespace Microsoft::VisualStudio::CppUnitTestFramework;




////////////////////////////////////////////////////////////////////////////////
//
//  RepoCheckoutTests
//
//  Pure/lexical classification of which repo checkout a disk path belongs to.
//  The disk picker uses this to hide recent disks that live in a checkout
//  other than the running one (a sibling worktree, or the main tree when we
//  run from a worktree) while always showing the user's own / %LOCALAPPDATA%
//  disks. Boundary cases (repo-root look-alikes, case folding, running from
//  the main tree vs a worktree) are the whole point, so lock them down.
//
////////////////////////////////////////////////////////////////////////////////

namespace RepoCheckoutTests
{
    namespace fs = std::filesystem;

    // A representative multi-checkout layout on one machine.
    static const wchar_t * const  kMainRoot   = L"C:\\repo\\Casso";
    static const wchar_t * const  kWt1Key     = L"C:\\repo\\Casso\\.claude\\worktrees\\wt1";
    static const wchar_t * const  kWt2Key     = L"C:\\repo\\Casso\\.claude\\worktrees\\wt2";

    static const wchar_t * const  kMainDisk   = L"C:\\repo\\Casso\\Apple2\\Demos\\a.dsk";
    static const wchar_t * const  kWt1Disk    = L"C:\\repo\\Casso\\.claude\\worktrees\\wt1\\Apple2\\Demos\\b.woz";
    static const wchar_t * const  kWt2Disk    = L"C:\\repo\\Casso\\.claude\\worktrees\\wt2\\Apple2\\Demos\\c.dsk";
    static const wchar_t * const  kLocalDisk  = L"C:\\Users\\me\\AppData\\Local\\Casso\\Disks\\d.woz";
    static const wchar_t * const  kLookAlike  = L"C:\\repo\\CassoOther\\x.dsk";


    TEST_CLASS (RepoCheckoutTests)
    {
    public:

        //////////////////////////////////////////////////////////////////////
        //  WorktreeKeyOf
        //////////////////////////////////////////////////////////////////////

        TEST_METHOD (WorktreeKeyOf_InsideWorktree_ReturnsKey)
        {
            auto  key = RepoCheckout::WorktreeKeyOf (fs::path (kWt1Disk));

            Assert::IsTrue (fs::path (key) == fs::path (kWt1Key));
        }

        TEST_METHOD (WorktreeKeyOf_MainTreePath_ReturnsEmpty)
        {
            Assert::IsTrue (RepoCheckout::WorktreeKeyOf (fs::path (kMainDisk)).empty());
        }

        TEST_METHOD (WorktreeKeyOf_NonRepoPath_ReturnsEmpty)
        {
            Assert::IsTrue (RepoCheckout::WorktreeKeyOf (fs::path (kLocalDisk)).empty());
        }


        //////////////////////////////////////////////////////////////////////
        //  MainRootOfWorktreeKey
        //////////////////////////////////////////////////////////////////////

        TEST_METHOD (MainRootOfWorktreeKey_StripsWorktreeSuffix)
        {
            auto  root = RepoCheckout::MainRootOfWorktreeKey (kWt1Key);

            Assert::IsTrue (root == fs::path (kMainRoot));
        }

        TEST_METHOD (MainRootOfWorktreeKey_Empty_ReturnsEmpty)
        {
            Assert::IsTrue (RepoCheckout::MainRootOfWorktreeKey (L"").empty());
        }


        //////////////////////////////////////////////////////////////////////
        //  IsUnderOrEqual
        //////////////////////////////////////////////////////////////////////

        TEST_METHOD (IsUnderOrEqual_ChildBeneathAncestor_True)
        {
            Assert::IsTrue (RepoCheckout::IsUnderOrEqual (fs::path (kMainDisk), fs::path (kMainRoot)));
        }

        TEST_METHOD (IsUnderOrEqual_SamePath_True)
        {
            Assert::IsTrue (RepoCheckout::IsUnderOrEqual (fs::path (kMainRoot), fs::path (kMainRoot)));
        }

        TEST_METHOD (IsUnderOrEqual_RootLookAlike_False)
        {
            // "C:\repo\CassoOther" must NOT count as under "C:\repo\Casso":
            // the compare is component-wise, not a raw string prefix.
            Assert::IsFalse (RepoCheckout::IsUnderOrEqual (fs::path (kLookAlike), fs::path (kMainRoot)));
        }

        TEST_METHOD (IsUnderOrEqual_CaseInsensitive_True)
        {
            Assert::IsTrue (RepoCheckout::IsUnderOrEqual (
                fs::path (L"C:\\REPO\\casso\\Apple2\\z.dsk"), fs::path (kMainRoot)));
        }

        TEST_METHOD (IsUnderOrEqual_EmptyAncestor_False)
        {
            Assert::IsFalse (RepoCheckout::IsUnderOrEqual (fs::path (kMainDisk), fs::path ()));
        }


        //////////////////////////////////////////////////////////////////////
        //  IsForeignCheckoutDisk — running FROM a worktree (wt1)
        //////////////////////////////////////////////////////////////////////

        TEST_METHOD (Foreign_FromWorktree_MainTreeDisk_IsForeign)
        {
            Assert::IsTrue (RepoCheckout::IsForeignCheckoutDisk (fs::path (kMainDisk), kWt1Key),
                L"main-tree repo disk must be hidden when running from a worktree");
        }

        TEST_METHOD (Foreign_FromWorktree_OwnDisk_NotForeign)
        {
            Assert::IsFalse (RepoCheckout::IsForeignCheckoutDisk (fs::path (kWt1Disk), kWt1Key),
                L"a disk in the running worktree must show");
        }

        TEST_METHOD (Foreign_FromWorktree_SiblingWorktreeDisk_IsForeign)
        {
            Assert::IsTrue (RepoCheckout::IsForeignCheckoutDisk (fs::path (kWt2Disk), kWt1Key),
                L"a disk in a sibling worktree must be hidden");
        }

        TEST_METHOD (Foreign_FromWorktree_NonRepoDisk_NotForeign)
        {
            Assert::IsFalse (RepoCheckout::IsForeignCheckoutDisk (fs::path (kLocalDisk), kWt1Key),
                L"the user's own / %LOCALAPPDATA% disks must always show");
        }

        TEST_METHOD (Foreign_FromWorktree_RootLookAlikeDisk_NotForeign)
        {
            // A path that merely shares a string prefix with the repo root is
            // a different tree entirely and must not be hidden.
            Assert::IsFalse (RepoCheckout::IsForeignCheckoutDisk (fs::path (kLookAlike), kWt1Key));
        }


        //////////////////////////////////////////////////////////////////////
        //  IsForeignCheckoutDisk — running FROM the main tree (empty key)
        //////////////////////////////////////////////////////////////////////

        TEST_METHOD (Foreign_FromMain_MainTreeDisk_NotForeign)
        {
            Assert::IsFalse (RepoCheckout::IsForeignCheckoutDisk (fs::path (kMainDisk), L""),
                L"main-tree disks are 'here' when running from the main tree");
        }

        TEST_METHOD (Foreign_FromMain_WorktreeDisk_IsForeign)
        {
            Assert::IsTrue (RepoCheckout::IsForeignCheckoutDisk (fs::path (kWt1Disk), L""),
                L"any worktree disk is foreign when running from the main tree");
        }

        TEST_METHOD (Foreign_FromMain_NonRepoDisk_NotForeign)
        {
            Assert::IsFalse (RepoCheckout::IsForeignCheckoutDisk (fs::path (kLocalDisk), L""));
        }
    };
}
