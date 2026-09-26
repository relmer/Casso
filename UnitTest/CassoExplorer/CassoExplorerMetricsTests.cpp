#include "Pch.h"
#include "CassoExplorer/CassoExplorerShell.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerMetricsTests
//
//  File Explorer's list row height, measured on 2026-09-23 at nine display
//  scales. Its rows are not one size scaled, so each measured height is
//  checked against the rule the window uses.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoExplorerMetricsTests)
{
public:

    TEST_METHOD (ListRowHeight_MatchesExplorerAtEveryMeasuredScale)
    {
        struct Measured
        {
            UINT  dpi;
            int   rowPx;
        };

        static constexpr Measured  s_kMeasured[] =
        {
            {  96, 28 },
            { 120, 37 },
            { 144, 43 },
            { 168, 51 },
            { 192, 56 },
            { 216, 65 },
            { 240, 71 },
            { 288, 84 },
            { 336, 99 },
        };

        for (const Measured & m : s_kMeasured)
        {
            Assert::AreEqual (m.rowPx, CassoExplorerWindow::GetListRowHeightPx (m.dpi), std::format (L"at {} dpi", m.dpi).c_str());
        }
    }


    TEST_METHOD (FitPanes_ListGivesWayFirst_ThenPreview_ThenTree)
    {
        using W = CassoExplorerWindow;

        static constexpr int  s_kSashes = 2 * DxuiSplitter::kSashDip;
        static constexpr int  s_kTree   = 300;
        static constexpr int  s_kView   = 400;

        int            listFull = s_kTree + s_kView + W::kMinListWidthDip + s_kSashes;
        W::PaneWidths  p        = {};



        //  Room to spare: the list takes it all.
        p = W::FitPanes (listFull + 500, s_kTree, s_kView);
        Assert::AreEqual (s_kTree, p.tree);
        Assert::AreEqual (s_kView, p.preview);
        Assert::AreEqual (W::kMinListWidthDip + 500, p.list);

        //  The list at its minimum: the preview gives up the next 100.
        p = W::FitPanes (listFull - 100, s_kTree, s_kView);
        Assert::AreEqual (s_kTree,                p.tree);
        Assert::AreEqual (s_kView - 100,          p.preview);
        Assert::AreEqual ((int) W::kMinListWidthDip, p.list);

        //  The preview at its minimum too: the tree gives up the rest.
        p = W::FitPanes (listFull - (s_kView - W::kMinPreviewWidthDip) - 50, s_kTree, s_kView);
        Assert::AreEqual ((int) W::kMinPreviewWidthDip, p.preview);
        Assert::AreEqual (s_kTree - 50,                 p.tree);
        Assert::AreEqual ((int) W::kMinListWidthDip,    p.list);
    }


    TEST_METHOD (FitPanes_BelowEveryMinimum_ShrinksAllThreeTogether)
    {
        using W = CassoExplorerWindow;

        W::PaneWidths  p         = W::FitPanes (200, 300, 400);
        int            available = 200 - 2 * DxuiSplitter::kSashDip;



        Assert::AreEqual (available, p.tree + p.list + p.preview, L"the three fill the body");
        Assert::IsTrue   (p.tree > 0 && p.list > 0 && p.preview > 0, L"and none vanishes");
        Assert::IsTrue   (p.preview > p.tree, L"in proportion to their minimums");
    }


    TEST_METHOD (ListColumnShown_NameAlways_CatalogOnlyInsideAnImage)
    {
        using W = CassoExplorerWindow;
        using C = CatalogModel::Column;

        Assert::IsTrue  (W::IsListColumnShown ((size_t) C::Name,     false, false), L"Name shows even when unchecked");
        Assert::IsTrue  (W::IsListColumnShown ((size_t) C::Type,     true,  false), L"a chosen column shows");
        Assert::IsFalse (W::IsListColumnShown ((size_t) C::Type,     false, false), L"an unchosen one does not");
        Assert::IsFalse (W::IsListColumnShown ((size_t) C::Address,  true,  false), L"a catalog column hides in a host folder");
        Assert::IsTrue  (W::IsListColumnShown ((size_t) C::Locked,   true,  true),  L"and shows inside an image");
        Assert::IsFalse (W::IsListColumnShown ((size_t) C::Locked,   false, true),  L"unless unchosen");
    }


    TEST_METHOD (FitPanes_HiddenPreview_TakesNothing)
    {
        using W = CassoExplorerWindow;

        W::PaneWidths  p = W::FitPanes (1000, 300, 0);



        Assert::AreEqual (0,   p.preview);
        Assert::AreEqual (300, p.tree);
        Assert::AreEqual (1000 - DxuiSplitter::kSashDip - 300, p.list);
    }
};
