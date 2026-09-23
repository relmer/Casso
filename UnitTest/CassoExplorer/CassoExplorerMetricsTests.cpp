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
};
