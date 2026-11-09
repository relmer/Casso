#include "Pch.h"
#include "../EhmTestHelper.h"

#include "CassoExplorer/Model/CassoExplorerPrefs.h"
#include "CassoExplorer/Model/IconSizeSteps.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

using View = DxuiListView::View;





////////////////////////////////////////////////////////////////////////////////
//
//  IconSizeStepsTests
//
//  Ctrl with the wheel: the run of icon sizes, the view for each size, and
//  the views past the smallest size.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (IconSizeStepsTests)
{
public:

    static IconSizeSteps::Size  Make (View view, int iconDip = 0)
    {
        return IconSizeSteps::Size { view, iconDip };
    }



    //  File Explorer's sizes at 150%.
    TEST_METHOD (Smaller_FollowsTheMeasuredSizes)
    {
        Assert::IsTrue (IconSizeSteps::Step (Make (View::ExtraLargeIcons), false, 144) == Make (View::LargeIcons, 239));
        Assert::IsTrue (IconSizeSteps::Step (Make (View::LargeIcons), false, 144) == Make (View::LargeIcons, 89));
        Assert::IsTrue (IconSizeSteps::Step (Make (View::LargeIcons, 72), false, 144) == Make (View::MediumIcons, 67));
        Assert::IsTrue (IconSizeSteps::Step (Make (View::MediumIcons), false, 144) == Make (View::MediumIcons, 44), L"Medium's default is off the walk from 256");
        Assert::IsTrue (IconSizeSteps::Step (Make (View::MediumIcons, 33), false, 144) == Make (View::SmallIcons, 31));
        Assert::IsTrue (IconSizeSteps::Step (Make (View::SmallIcons, 17), false, 144) == Make (View::SmallIcons), L"16 is Small's default");
    }



    //  The sizes up differ from the sizes down, and by scale.
    TEST_METHOD (Larger_FollowsTheMeasuredSizes_ForEachScale)
    {
        Assert::IsTrue (IconSizeSteps::Step (Make (View::LargeIcons), true, 96) == Make (View::LargeIcons, 102));
        Assert::IsTrue (IconSizeSteps::Step (Make (View::LargeIcons, 102), true, 96) == Make (View::LargeIcons, 110));
        Assert::IsTrue (IconSizeSteps::Step (Make (View::LargeIcons, 102), true, 120) == Make (View::LargeIcons, 111));
        Assert::IsTrue (IconSizeSteps::Step (Make (View::LargeIcons, 106), true, 144) == Make (View::LargeIcons, 113));
    }


    TEST_METHOD (PastTheSmallest_MovesThroughTheOtherViews_AndStopsAtContent)
    {
        Assert::IsTrue (IconSizeSteps::Step (Make (View::SmallIcons), false, 144) == Make (View::List));
        Assert::IsTrue (IconSizeSteps::Step (Make (View::List), false, 144) == Make (View::Details));
        Assert::IsTrue (IconSizeSteps::Step (Make (View::Details), false, 144) == Make (View::Tiles));
        Assert::IsTrue (IconSizeSteps::Step (Make (View::Tiles), false, 144) == Make (View::Content));
        Assert::IsTrue (IconSizeSteps::Step (Make (View::Content), false, 144) == Make (View::Content));
    }



    //  File Explorer's icons at 125% and 150%: scaled up to 96, then by even
    //  steps to the 256 pixels of the shell's largest.
    TEST_METHOD (SizedIcon_ScalesAsFileExplorersDoes)
    {
        Assert::AreEqual (72,  DxuiListItemLayout::GetSizedIconPx (48,  144));
        Assert::AreEqual (144, DxuiListItemLayout::GetSizedIconPx (96,  144));
        Assert::AreEqual (168, DxuiListItemLayout::GetSizedIconPx (130, 144));
        Assert::AreEqual (256, DxuiListItemLayout::GetSizedIconPx (256, 144));
        Assert::AreEqual (120, DxuiListItemLayout::GetSizedIconPx (96,  120));
        Assert::AreEqual (256, DxuiListItemLayout::GetSizedIconPx (256, 120));
        Assert::AreEqual (256, DxuiListItemLayout::GetSizedIconPx (200, 288), L"Past 267% Large's icon is already the largest");
    }



    //  A folder's icon size is saved with its view, and only when it has one.
    TEST_METHOD (IconSize_IsSavedWithTheFoldersView)
    {
        CassoExplorerPrefs  prefs;
        CassoExplorerPrefs  read;

        prefs.folderViews.Remember (L"C:\\Sized", View::MediumIcons, 58);
        prefs.folderViews.Remember (L"C:\\Plain", View::LargeIcons);

        AssertSucceeded (read.FromJson (prefs.ToJson()));

        Assert::AreEqual (58, read.folderViews.GetEntry (L"C:\\Sized", FolderViews::FolderType::Generic).iconDip);
        Assert::AreEqual (0,  read.folderViews.GetEntry (L"C:\\Plain", FolderViews::FolderType::Generic).iconDip);
    }



    TEST_METHOD (Larger_ComesBackTheSameWay_AndStopsAtExtraLarge)
    {
        Assert::IsTrue (IconSizeSteps::Step (Make (View::Content), true, 144) == Make (View::Tiles));
        Assert::IsTrue (IconSizeSteps::Step (Make (View::Details), true, 144) == Make (View::List));
        Assert::IsTrue (IconSizeSteps::Step (Make (View::List), true, 144) == Make (View::SmallIcons));
        Assert::IsTrue (IconSizeSteps::Step (Make (View::SmallIcons), true, 144) == Make (View::SmallIcons, 18));
        Assert::IsTrue (IconSizeSteps::Step (Make (View::SmallIcons, 31), true, 144) == Make (View::MediumIcons, 33));
        Assert::IsTrue (IconSizeSteps::Step (Make (View::MediumIcons), true, 144) == Make (View::MediumIcons, 50));
        Assert::IsTrue (IconSizeSteps::Step (Make (View::MediumIcons, 67), true, 144) == Make (View::LargeIcons, 72));
        Assert::IsTrue (IconSizeSteps::Step (Make (View::LargeIcons, 239), true, 144) == Make (View::ExtraLargeIcons));
        Assert::IsTrue (IconSizeSteps::Step (Make (View::ExtraLargeIcons), true, 144) == Make (View::ExtraLargeIcons));
    }
};