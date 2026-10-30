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



    TEST_METHOD (Smaller_WalksTheRunThroughTheViews)
    {
        Assert::IsTrue (IconSizeSteps::Step (Make (View::ExtraLargeIcons), false) == Make (View::LargeIcons, 239));
        Assert::IsTrue (IconSizeSteps::Step (Make (View::LargeIcons, 107), false) == Make (View::LargeIcons), L"96 is Large's default");
        Assert::IsTrue (IconSizeSteps::Step (Make (View::LargeIcons), false) == Make (View::LargeIcons, 89));
        Assert::IsTrue (IconSizeSteps::Step (Make (View::LargeIcons, 72), false) == Make (View::MediumIcons, 67));
        Assert::IsTrue (IconSizeSteps::Step (Make (View::MediumIcons), false) == Make (View::MediumIcons, 47), L"Medium's default is off the run");
        Assert::IsTrue (IconSizeSteps::Step (Make (View::MediumIcons, 33), false) == Make (View::SmallIcons, 31));
        Assert::IsTrue (IconSizeSteps::Step (Make (View::SmallIcons, 17), false) == Make (View::SmallIcons), L"16 is Small's default");
    }



    TEST_METHOD (PastTheSmallest_MovesThroughTheOtherViews_AndStopsAtContent)
    {
        Assert::IsTrue (IconSizeSteps::Step (Make (View::SmallIcons), false) == Make (View::List));
        Assert::IsTrue (IconSizeSteps::Step (Make (View::List), false) == Make (View::Details));
        Assert::IsTrue (IconSizeSteps::Step (Make (View::Details), false) == Make (View::Tiles));
        Assert::IsTrue (IconSizeSteps::Step (Make (View::Tiles), false) == Make (View::Content));
        Assert::IsTrue (IconSizeSteps::Step (Make (View::Content), false) == Make (View::Content));
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
        Assert::IsTrue (IconSizeSteps::Step (Make (View::Content), true) == Make (View::Tiles));
        Assert::IsTrue (IconSizeSteps::Step (Make (View::Details), true) == Make (View::List));
        Assert::IsTrue (IconSizeSteps::Step (Make (View::List), true) == Make (View::SmallIcons));
        Assert::IsTrue (IconSizeSteps::Step (Make (View::SmallIcons), true) == Make (View::SmallIcons, 17));
        Assert::IsTrue (IconSizeSteps::Step (Make (View::SmallIcons, 31), true) == Make (View::MediumIcons, 33));
        Assert::IsTrue (IconSizeSteps::Step (Make (View::MediumIcons), true) == Make (View::MediumIcons, 50));
        Assert::IsTrue (IconSizeSteps::Step (Make (View::MediumIcons, 67), true) == Make (View::LargeIcons, 72));
        Assert::IsTrue (IconSizeSteps::Step (Make (View::LargeIcons, 239), true) == Make (View::ExtraLargeIcons));
        Assert::IsTrue (IconSizeSteps::Step (Make (View::ExtraLargeIcons), true) == Make (View::ExtraLargeIcons));
    }
};