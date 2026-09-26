#include "Pch.h"
#include "CassoExplorer/Model/FolderViews.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FolderViewsTests
//
//  Explorer's view for each folder: the type's default until a view is
//  chosen, the choice after, and the oldest choices dropped past the cap.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FolderViewsTests)
{
public:

    TEST_METHOD (Defaults_FollowTheFolderType)
    {
        Assert::IsTrue (FolderViews::GetDefaultView (FolderViews::FolderType::Drives)    == DxuiListView::View::Tiles);
        Assert::IsTrue (FolderViews::GetDefaultView (FolderViews::FolderType::Pictures)  == DxuiListView::View::LargeIcons);
        Assert::IsTrue (FolderViews::GetDefaultView (FolderViews::FolderType::Videos)    == DxuiListView::View::LargeIcons);
        Assert::IsTrue (FolderViews::GetDefaultView (FolderViews::FolderType::Music)     == DxuiListView::View::Details);
        Assert::IsTrue (FolderViews::GetDefaultView (FolderViews::FolderType::Documents) == DxuiListView::View::Details);
        Assert::IsTrue (FolderViews::GetDefaultView (FolderViews::FolderType::Generic)   == DxuiListView::View::Details);
    }


    TEST_METHOD (AChosenView_IsTheFoldersAlone)
    {
        FolderViews  views;

        views.Remember (L"C:\\Games", DxuiListView::View::List);

        Assert::IsTrue (views.GetView (L"c:\\games\\", FolderViews::FolderType::Generic) == DxuiListView::View::List, L"Whatever the case and slash");
        Assert::IsTrue (views.GetView (L"C:\\Other",   FolderViews::FolderType::Generic) == DxuiListView::View::Details);

        views.Remember (L"C:\\GAMES", DxuiListView::View::Tiles);

        Assert::AreEqual ((size_t) 1, views.GetEntries().size(), L"A folder chosen again is kept once");
        Assert::IsTrue   (views.GetView (L"C:\\Games", FolderViews::FolderType::Generic) == DxuiListView::View::Tiles);
    }


    TEST_METHOD (TheOldestChoices_AreDroppedPastTheCap)
    {
        FolderViews  views;
        size_t       i = 0;

        for (i = 0; i <= FolderViews::kMaxEntries; i++)
        {
            views.Remember (std::format (L"C:\\F{}", i), DxuiListView::View::List);
        }

        Assert::AreEqual (FolderViews::kMaxEntries, views.GetEntries().size());
        Assert::IsTrue   (views.GetView (L"C:\\F0", FolderViews::FolderType::Generic) == DxuiListView::View::Details, L"The first chosen is gone");
        Assert::IsTrue   (views.GetView (std::format (L"C:\\F{}", FolderViews::kMaxEntries), FolderViews::FolderType::Generic) == DxuiListView::View::List);
    }
};
