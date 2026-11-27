#include "Pch.h"

#include "CassoExplorer/Model/FolderThumbnails.h"
#include "FakeFolderSeams.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FolderThumbnailsTests
//
//  The sweep over fake folders, a fake thumbnail store and a fake queue. No
//  test starts the thread; each calls what the thread calls.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr uint64_t   s_kDskBytes   = 143360;
static constexpr uint64_t   s_kWozBytes   = 1024 * 1024;
static constexpr uint64_t   s_kHardBytes  = 819200;
static constexpr int64_t    s_kTime       = 1000;
static constexpr int64_t    s_kLaterTime  = 2000;
static constexpr ULONGLONG  s_kStartMs    = 5000;





////////////////////////////////////////////////////////////////////////////////
//
//  MakeWindow
//
////////////////////////////////////////////////////////////////////////////////

static HWND MakeWindow (uintptr_t id)
{
    return reinterpret_cast<HWND> (id);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TakeAll
//
//  Each folder in the order TryTakeFolder returns it at nowMs, with " +sub"
//  after one to be swept with its subfolders.
//
////////////////////////////////////////////////////////////////////////////////

static std::vector<std::wstring> TakeAll (FolderThumbnails & sweep, ULONGLONG nowMs)
{
    std::vector<std::wstring>          folders;
    FolderThumbnails::FolderRequest    request;



    while (sweep.TryTakeFolder (nowMs, request))
    {
        folders.push_back (request.withSubfolders ? request.folder + L" +sub" : request.folder);
    }

    return folders;
}





TEST_CLASS (FolderThumbnailsTests)
{
public:

    TEST_METHOD (Sweep_QueuesEachFloppyImageWithNoThumbnail)
    {
        FakeFolderSeams       fake;
        FolderThumbnails  sweep (fake.MakeSeams(), s_kStartMs);



        fake.files[L"C:\\Disks"] =
        {
            { L"C:\\Disks\\Game.dsk",   s_kDskBytes,  s_kTime },
            { L"C:\\Disks\\Pictured.dsk",  s_kDskBytes,  s_kTime },
            { L"C:\\Disks\\Hard.po",    s_kHardBytes, s_kTime },
            { L"C:\\Disks\\Flux.woz",   s_kWozBytes,  s_kTime },
            { L"C:\\Disks\\Empty.dsk",  0,            s_kTime },
        };
        fake.thumbnails.insert (L"C:\\Disks\\Pictured.dsk");

        sweep.Sweep ({ L"C:\\Disks", false });

        Assert::AreEqual (size_t (2), fake.queued.size());
        Assert::AreEqual (std::wstring (L"C:\\Disks\\Game.dsk"), fake.queued[0]);
        Assert::AreEqual (std::wstring (L"C:\\Disks\\Flux.woz"), fake.queued[1]);
    }

    TEST_METHOD (Sweep_LooksForAThumbnailOnlyForAFloppySizedImage)
    {
        FakeFolderSeams       fake;
        FolderThumbnails  sweep (fake.MakeSeams(), s_kStartMs);



        fake.files[L"C:\\Disks"] =
        {
            { L"C:\\Disks\\Hard.po",  s_kHardBytes, s_kTime },
            { L"C:\\Disks\\Game.dsk", s_kDskBytes,  s_kTime },
        };

        sweep.Sweep ({ L"C:\\Disks", false });

        Assert::AreEqual (size_t (1), fake.checked.size());
        Assert::AreEqual (std::wstring (L"C:\\Disks\\Game.dsk"), fake.checked[0]);
    }

    TEST_METHOD (Sweep_QueuesAnImageOnce_UntilItsFileChanges)
    {
        FakeFolderSeams       fake;
        FolderThumbnails  sweep (fake.MakeSeams(), s_kStartMs);



        fake.files[L"C:\\Disks"] = { { L"C:\\Disks\\Game.dsk", s_kDskBytes, s_kTime } };

        sweep.Sweep ({ L"C:\\Disks", false });
        sweep.Sweep ({ L"C:\\Disks", false });

        Assert::AreEqual (size_t (1), fake.queued.size());

        fake.files[L"C:\\Disks"][0].time = s_kLaterTime;
        sweep.Sweep ({ L"C:\\Disks", false });

        Assert::AreEqual (size_t (2), fake.queued.size());
    }

    TEST_METHOD (GetRank_PutsActiveTabsFirst_ThenOtherTabs_ThenTheRest)
    {
        FakeFolderSeams       fake;
        FolderThumbnails  sweep (fake.MakeSeams(), s_kStartMs);



        sweep.SetWindowTabs (MakeWindow (1), { L"C:\\Active", { L"C:\\Other" } });
        sweep.SetWindowTabs (MakeWindow (2), { L"D:\\Second", {} });

        Assert::AreEqual (FolderThumbnails::kActiveRank, sweep.GetRank (L"C:\\Active"));
        Assert::AreEqual (FolderThumbnails::kActiveRank, sweep.GetRank (L"c:\\active\\"));
        Assert::AreEqual (FolderThumbnails::kActiveRank, sweep.GetRank (L"D:\\Second"));
        Assert::AreEqual (FolderThumbnails::kTabRank,    sweep.GetRank (L"C:\\Other"));
        Assert::AreEqual (FolderThumbnails::kOtherRank,  sweep.GetRank (L"C:\\Left"));

        sweep.SetWindowTabs (MakeWindow (1), { L"C:\\Other", { L"C:\\Active" } });
        sweep.RemoveWindow (MakeWindow (2));

        Assert::AreEqual (FolderThumbnails::kTabRank,    sweep.GetRank (L"C:\\Active"));
        Assert::AreEqual (FolderThumbnails::kActiveRank, sweep.GetRank (L"C:\\Other"));
        Assert::AreEqual (FolderThumbnails::kOtherRank,  sweep.GetRank (L"D:\\Second"));
    }

    TEST_METHOD (AWindowsFolders_AreSweptWhenFirstShown_AndAgainWhenListed)
    {
        FakeFolderSeams                fake;
        FolderThumbnails           sweep (fake.MakeSeams(), s_kStartMs);
        std::vector<std::wstring>  first;
        std::vector<std::wstring>  again;
        std::vector<std::wstring>  added;
        std::vector<std::wstring>  listed;



        sweep.SetWindowTabs (MakeWindow (1), { L"C:\\Active", { L"C:\\Other" } });
        first = TakeAll (sweep, s_kStartMs);

        sweep.SetWindowTabs (MakeWindow (1), { L"C:\\Active", { L"C:\\Other" } });
        again = TakeAll (sweep, s_kStartMs);

        sweep.SetWindowTabs (MakeWindow (1), { L"C:\\Active", { L"C:\\Other", L"C:\\New" } });
        added = TakeAll (sweep, s_kStartMs);

        sweep.SweepFolder (L"C:\\Active");
        listed = TakeAll (sweep, s_kStartMs);

        Assert::AreEqual (size_t (2), first.size());
        Assert::AreEqual (std::wstring (L"C:\\Active"), first[0]);
        Assert::AreEqual (std::wstring (L"C:\\Other"),  first[1]);
        Assert::AreEqual (size_t (0), again.size());
        Assert::AreEqual (size_t (1), added.size());
        Assert::AreEqual (std::wstring (L"C:\\New"),    added[0]);
        Assert::AreEqual (size_t (1), listed.size());
        Assert::AreEqual (std::wstring (L"C:\\Active"), listed[0]);
    }

    TEST_METHOD (KnownFolders_WaitForTheSettleTime_BehindTheWindowsFolders)
    {
        FakeFolderSeams                fake;
        FolderThumbnails           sweep (fake.MakeSeams(), s_kStartMs);
        std::vector<std::wstring>  early;
        std::vector<std::wstring>  settled;



        sweep.SetKnownFolders ({ L"C:\\Known1", L"C:\\Known2" });
        sweep.SetWindowTabs (MakeWindow (1), { L"C:\\Browsed", {} });

        early   = TakeAll (sweep, s_kStartMs + FolderThumbnails::kSettleMs - 1);
        settled = TakeAll (sweep, s_kStartMs + FolderThumbnails::kSettleMs);

        Assert::AreEqual (size_t (1), early.size());
        Assert::AreEqual (std::wstring (L"C:\\Browsed"), early[0]);
        Assert::AreEqual (size_t (2), settled.size());
        Assert::AreEqual (std::wstring (L"C:\\Known1 +sub"), settled[0]);
        Assert::AreEqual (std::wstring (L"C:\\Known2 +sub"), settled[1]);
    }

    TEST_METHOD (StartKnownFolders_EndsTheWait)
    {
        FakeFolderSeams                fake;
        FolderThumbnails           sweep (fake.MakeSeams(), s_kStartMs);
        std::vector<std::wstring>  folders;



        sweep.SetKnownFolders ({ L"C:\\Known" });
        sweep.StartKnownFolders();

        folders = TakeAll (sweep, s_kStartMs);

        Assert::AreEqual (size_t (1), folders.size());
        Assert::AreEqual (std::wstring (L"C:\\Known +sub"), folders[0]);
    }

    TEST_METHOD (WithTheSettingOff_NothingIsSwept_AndTheWaitingImagesAreRemoved)
    {
        FakeFolderSeams                fake;
        FolderThumbnails           sweep (fake.MakeSeams(), s_kStartMs);
        std::vector<std::wstring>  whileOff;
        std::vector<std::wstring>  onAgain;



        sweep.SetKnownFolders ({ L"C:\\Known" });
        sweep.StartKnownFolders();
        sweep.SetWindowTabs (MakeWindow (1), { L"C:\\Browsed", {} });

        fake.enabled = false;
        sweep.ApplySetting();
        whileOff = TakeAll (sweep, s_kStartMs);

        fake.enabled = true;
        sweep.ApplySetting();
        onAgain = TakeAll (sweep, s_kStartMs);

        Assert::AreEqual (1, fake.removals);
        Assert::AreEqual (size_t (0), whileOff.size());
        Assert::AreEqual (size_t (2), onAgain.size());
        Assert::AreEqual (std::wstring (L"C:\\Browsed"), onAgain[0]);
        Assert::AreEqual (std::wstring (L"C:\\Known +sub"), onAgain[1]);
    }

    TEST_METHOD (TheCassoNodesImages_RankWithTheTabAtTheNode)
    {
        FakeFolderSeams   fake;
        FolderThumbnails  sweep (fake.MakeSeams(), s_kStartMs);



        sweep.SetKnownFolders ({ L"C:\\Known" });
        sweep.SetWindowTabs (MakeWindow (1), { L"", {}, ThumbnailTabs::CassoNode::Active });

        Assert::AreEqual (FolderThumbnails::kActiveRank, sweep.GetRank (L"C:\\Known"));
        Assert::AreEqual (FolderThumbnails::kActiveRank, sweep.GetRank (L"C:\\Known\\Games\\Arcade"));
        Assert::AreEqual (FolderThumbnails::kOtherRank,  sweep.GetRank (L"C:\\KnownNot"));

        sweep.SetWindowTabs (MakeWindow (1), { L"C:\\Browsed", {}, ThumbnailTabs::CassoNode::Other });

        Assert::AreEqual (FolderThumbnails::kTabRank,    sweep.GetRank (L"C:\\Known\\Games"));
        Assert::AreEqual (FolderThumbnails::kActiveRank, sweep.GetRank (L"C:\\Browsed"));
    }

    TEST_METHOD (TheSameKnownFoldersAgain_AreNotSweptAgain)
    {
        FakeFolderSeams            fake;
        FolderThumbnails           sweep (fake.MakeSeams(), s_kStartMs);
        std::vector<std::wstring>  first;
        std::vector<std::wstring>  again;
        std::vector<std::wstring>  changed;



        sweep.StartKnownFolders();

        sweep.SetKnownFolders ({ L"C:\\Known" });
        first = TakeAll (sweep, s_kStartMs);

        sweep.SetKnownFolders ({ L"c:\\known\\" });
        again = TakeAll (sweep, s_kStartMs);

        sweep.SetKnownFolders ({ L"C:\\Known", L"C:\\Added" });
        changed = TakeAll (sweep, s_kStartMs);

        Assert::AreEqual (size_t (1), first.size());
        Assert::AreEqual (size_t (0), again.size());
        Assert::AreEqual (size_t (2), changed.size());
    }

    TEST_METHOD (Sweep_PassesTheRequestToTheList)
    {
        FakeFolderSeams   fake;
        FolderThumbnails  sweep (fake.MakeSeams(), s_kStartMs);



        sweep.Sweep ({ L"C:\\Known", true });

        Assert::AreEqual (size_t (1), fake.listed.size());
        Assert::AreEqual (std::wstring (L"C:\\Known"), fake.listed[0].folder);
        Assert::IsTrue   (fake.listed[0].withSubfolders);
    }

    TEST_METHOD (IsInFolder_MatchesTheFolderAndWhatIsInsideIt)
    {
        Assert::IsTrue  (FolderThumbnails::IsInFolder (L"C:\\Known",            L"C:\\Known"));
        Assert::IsTrue  (FolderThumbnails::IsInFolder (L"c:\\known\\Sub",     L"C:\\Known\\"));
        Assert::IsFalse (FolderThumbnails::IsInFolder (L"C:\\KnownNot",         L"C:\\Known"));
        Assert::IsFalse (FolderThumbnails::IsInFolder (L"C:\\Kno",              L"C:\\Known"));
        Assert::IsFalse (FolderThumbnails::IsInFolder (L"C:\\Known",            L""));
    }

    TEST_METHOD (TheCassoNodesImages_AreSweptAsListed_BeforeItsFolders)
    {
        FakeFolderSeams                    fake;
        FolderThumbnails                   sweep (fake.MakeSeams(), s_kStartMs);
        FolderThumbnails::FolderRequest    first;
        FolderThumbnails::FolderRequest    second;
        bool                               hasFirst  = false;
        bool                               hasSecond = false;



        sweep.SetKnownFolders ({ L"C:\\Known" });
        sweep.SetCassoImages  ({ L"C:\\Stock\\Master.dsk", L"C:\\Demo\\Sixies.dsk" });
        sweep.StartKnownFolders();

        hasFirst  = sweep.TryTakeFolder (s_kStartMs, first);
        hasSecond = sweep.TryTakeFolder (s_kStartMs, second);

        Assert::IsTrue   (hasFirst);
        Assert::AreEqual (size_t (2), first.files.size());
        Assert::AreEqual (std::wstring (L"C:\\Stock\\Master.dsk"), first.files[0]);
        Assert::AreEqual (std::wstring (L"C:\\Demo\\Sixies.dsk"),  first.files[1]);
        Assert::IsTrue   (hasSecond);
        Assert::AreEqual (std::wstring (L"C:\\Known"), second.folder);
        Assert::IsTrue   (second.withSubfolders);
    }

    TEST_METHOD (TheCassoNodesImages_WaitForTheSettleTime)
    {
        FakeFolderSeams            fake;
        FolderThumbnails           sweep (fake.MakeSeams(), s_kStartMs);
        std::vector<std::wstring>  early;
        std::vector<std::wstring>  settled;



        sweep.SetCassoImages ({ L"C:\\Demo\\Sixies.dsk" });

        early   = TakeAll (sweep, s_kStartMs);
        settled = TakeAll (sweep, s_kStartMs + FolderThumbnails::kSettleMs);

        Assert::AreEqual (size_t (0), early.size());
        Assert::AreEqual (size_t (1), settled.size());
    }

    TEST_METHOD (AFolderWithACassoNodeImage_RanksWithTheTabAtTheNode)
    {
        FakeFolderSeams   fake;
        FolderThumbnails  sweep (fake.MakeSeams(), s_kStartMs);



        sweep.SetCassoImages ({ L"C:\\Demo\\Sixies.dsk" });
        sweep.SetWindowTabs  (MakeWindow (1), { L"C:\\Browsed", {}, ThumbnailTabs::CassoNode::Other });

        Assert::AreEqual (FolderThumbnails::kTabRank,   sweep.GetRank (L"C:\\Demo"));
        Assert::AreEqual (FolderThumbnails::kOtherRank, sweep.GetRank (L"C:\\Demo\\Sub"));
    }
};
