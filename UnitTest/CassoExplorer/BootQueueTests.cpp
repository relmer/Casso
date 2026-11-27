#include "Pch.h"

#include "CassoExplorer/Model/BootQueue.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BootQueueTests
//
//  The order images are booted in. Folder A is ranked ahead of folder B.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr int  s_kRankA = 0;
static constexpr int  s_kRankB = 1;





////////////////////////////////////////////////////////////////////////////////
//
//  RankAAheadOfB
//
////////////////////////////////////////////////////////////////////////////////

static int RankAAheadOfB (const std::wstring & folder)
{
    return _wcsicmp (folder.c_str(), L"C:\\A") == 0 ? s_kRankA : s_kRankB;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakeItem
//
////////////////////////////////////////////////////////////////////////////////

static BootQueue::Item MakeItem (const wchar_t * path, BootQueue::Urgency urgency)
{
    return BootQueue::Item { path, std::string(), urgency == BootQueue::Urgency::Connection ? std::string ("link") : std::string(), urgency };
}





////////////////////////////////////////////////////////////////////////////////
//
//  TakeAll
//
//  Each path in the order TryTakeNext returns it.
//
////////////////////////////////////////////////////////////////////////////////

static std::vector<std::wstring> TakeAll (BootQueue & queue)
{
    std::vector<std::wstring>  paths;
    BootQueue::Item            item;



    while (queue.TryTakeNext (RankAAheadOfB, item))
    {
        paths.push_back (item.path);
    }

    return paths;
}





TEST_CLASS (BootQueueTests)
{
public:

    TEST_METHOD (AConnectionsImage_GoesFirst_ThenInView_ThenFolderImages)
    {
        BootQueue                  queue;
        std::vector<std::wstring>  order;



        queue.TryAdd (MakeItem (L"C:\\A\\folder.dsk", BootQueue::Urgency::Folder));
        queue.TryAdd (MakeItem (L"C:\\B\\view.dsk",   BootQueue::Urgency::InView));
        queue.TryAdd (MakeItem (L"C:\\B\\link.dsk",   BootQueue::Urgency::Connection));

        order = TakeAll (queue);

        Assert::AreEqual (size_t (3), order.size());
        Assert::AreEqual (std::wstring (L"C:\\B\\link.dsk"),   order[0]);
        Assert::AreEqual (std::wstring (L"C:\\B\\view.dsk"),   order[1]);
        Assert::AreEqual (std::wstring (L"C:\\A\\folder.dsk"), order[2]);
    }

    TEST_METHOD (FolderImages_GoByTheirFoldersRank_ThenByWhenAdded)
    {
        BootQueue                  queue;
        std::vector<std::wstring>  order;



        queue.TryAdd (MakeItem (L"C:\\B\\one.dsk", BootQueue::Urgency::Folder));
        queue.TryAdd (MakeItem (L"C:\\A\\two.dsk", BootQueue::Urgency::Folder));
        queue.TryAdd (MakeItem (L"C:\\B\\three.dsk", BootQueue::Urgency::Folder));
        queue.TryAdd (MakeItem (L"C:\\A\\four.dsk", BootQueue::Urgency::Folder));

        order = TakeAll (queue);

        Assert::AreEqual (size_t (4), order.size());
        Assert::AreEqual (std::wstring (L"C:\\A\\two.dsk"),   order[0]);
        Assert::AreEqual (std::wstring (L"C:\\A\\four.dsk"),  order[1]);
        Assert::AreEqual (std::wstring (L"C:\\B\\one.dsk"),   order[2]);
        Assert::AreEqual (std::wstring (L"C:\\B\\three.dsk"), order[3]);
    }

    TEST_METHOD (AFolderImageComingIntoView_MovesAheadOfTheOtherFolderImages)
    {
        BootQueue                  queue;
        std::vector<std::wstring>  order;
        bool                       isMoved = false;



        queue.TryAdd (MakeItem (L"C:\\A\\first.dsk", BootQueue::Urgency::Folder));
        queue.TryAdd (MakeItem (L"C:\\B\\later.dsk", BootQueue::Urgency::Folder));
        queue.TryAdd (MakeItem (L"C:\\A\\view.dsk",  BootQueue::Urgency::InView));

        isMoved = queue.TryAdd (MakeItem (L"c:\\b\\LATER.dsk", BootQueue::Urgency::InView));
        order   = TakeAll (queue);

        Assert::IsTrue   (isMoved);
        Assert::AreEqual (size_t (3), order.size());
        Assert::AreEqual (std::wstring (L"C:\\A\\view.dsk"),  order[0]);
        Assert::AreEqual (std::wstring (L"c:\\b\\LATER.dsk"), order[1]);
        Assert::AreEqual (std::wstring (L"C:\\A\\first.dsk"), order[2]);
    }

    TEST_METHOD (AnImageAlreadyWaiting_IsNotAddedAgain)
    {
        BootQueue  queue;



        Assert::IsTrue   (queue.TryAdd (MakeItem (L"C:\\A\\one.dsk", BootQueue::Urgency::InView)));
        Assert::IsFalse  (queue.TryAdd (MakeItem (L"C:\\A\\ONE.dsk", BootQueue::Urgency::InView)));
        Assert::IsFalse  (queue.TryAdd (MakeItem (L"C:\\A\\one.dsk", BootQueue::Urgency::Folder)));
        Assert::AreEqual (size_t (1), queue.GetCount());
        Assert::IsTrue   (queue.Contains (L"c:\\a\\one.dsk"));
    }

    TEST_METHOD (RemoveFolderImages_LeavesTheOthers)
    {
        BootQueue                  queue;
        std::vector<std::wstring>  order;
        size_t                     removed = 0;



        queue.TryAdd (MakeItem (L"C:\\A\\folder.dsk", BootQueue::Urgency::Folder));
        queue.TryAdd (MakeItem (L"C:\\A\\view.dsk",   BootQueue::Urgency::InView));
        queue.TryAdd (MakeItem (L"C:\\B\\folder.dsk", BootQueue::Urgency::Folder));

        removed = queue.RemoveFolderImages();
        order   = TakeAll (queue);

        Assert::AreEqual (size_t (2), removed);
        Assert::AreEqual (size_t (1), order.size());
        Assert::AreEqual (std::wstring (L"C:\\A\\view.dsk"), order[0]);
    }
};
