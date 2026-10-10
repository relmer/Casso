#include "Pch.h"

#include "Core/UnicodeSymbols.h"
#include "Ui/DiskInspector/PlatterLegendView.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterLegendViewTests
//
//  The platter's legend (FR-030): a swatch per kind in Structure mode, and
//  fast, nominal and slow with the range in Timing mode.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (PlatterLegendViewTests)
{
public:

    TEST_METHOD (StructureModeListsEveryKind)
    {
        DiskInspectorPalette  palette = DiskInspectorPalette::MakeFallback (true);
        vector<LegendEntry>   entries = PlatterLegendView::BuildEntries (palette, false, 0.05);



        Assert::AreEqual (size_t (12), entries.size());
        Assert::AreEqual (std::wstring (L"Sync"), entries.front().label);
        Assert::AreEqual (palette.colors.sync, entries.front().argb);
        Assert::AreEqual (std::wstring (L"Analyzing"), entries.back().label);
    }



    TEST_METHOD (TimingModeShowsTheRange)
    {
        DiskInspectorPalette  palette = DiskInspectorPalette::MakeFallback (true);
        vector<LegendEntry>   entries = PlatterLegendView::BuildEntries (palette, true, 0.10);



        Assert::AreEqual (size_t (3), entries.size());
        Assert::AreEqual (std::wstring (L"Fast cells (") + s_kpszMinus + L"10.0%)", entries[0].label);
        Assert::AreEqual (palette.colors.timingNominal, entries[1].argb);
        Assert::AreEqual (std::wstring (L"Slow cells (+10.0%)"), entries[2].label);
    }



    TEST_METHOD (OnlyTimingModeNotesFluxTracks)
    {
        Assert::IsTrue (PlatterLegendView::GetNote (true).find (L"flux") != std::wstring::npos);
        Assert::IsTrue (PlatterLegendView::GetNote (false).find (L"flux") == std::wstring::npos);
    }
};
