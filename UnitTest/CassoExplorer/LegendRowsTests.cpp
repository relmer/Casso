#include "Pch.h"
#include "CassoExplorer/Model/PreviewText.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  LegendRowsTests
//
//  A legend paragraph as text rows: its entries a cell each, as many to a
//  row as the columns asked for, in a column group apart from the rest.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (LegendRowsTests)
{
public:

    static std::vector<Paragraph> MakeDocument()
    {
        Paragraph  legend;
        Paragraph  text;



        legend.isLegend = true;
        legend.runs     = { Run { L"Water   ", false, false, false, RunColor::Muted, 0xFF2040C0 },
                            Run { L"Grass   ", false, false, false, RunColor::Muted, 0xFF40C040 },
                            Run { L"Forest",   false, false, false, RunColor::Muted, 0xFF206020 } };
        text.runs       = { Run { L"Party" } };

        return { legend, text };
    }


    TEST_METHOD (Columns_PutThatManyEntriesInARow)
    {
        std::vector<DxuiTextView::Row>  rows = PreviewText::BuildDocumentRows (MakeDocument(), {}, TableForm::Cards, nullptr, 2);

        Assert::AreEqual ((size_t) 3, rows.size(), L"two rows of the legend, then the text");
        Assert::AreEqual ((size_t) 2, rows[0].cells.size());
        Assert::AreEqual ((size_t) 1, rows[1].cells.size());
        Assert::AreEqual (1, rows[0].columnGroup, L"the legend lines up apart from the rest");
        Assert::AreEqual (0, rows[2].columnGroup);
    }


    TEST_METHOD (Entries_KeepTheirSwatchAndDropTheirSeparator)
    {
        std::vector<DxuiTextView::Row>  rows = PreviewText::BuildDocumentRows (MakeDocument(), {}, TableForm::Cards, nullptr, 3);

        Assert::AreEqual ((size_t) 3, rows[0].cells.size());
        Assert::IsTrue   (rows[0].cells[0].ends_with (L"Water"), L"the trailing spaces are dropped");
        Assert::IsTrue   (rows[0].cells[0].size() > 5, L"and the swatch is in the same cell as its name");
    }


    TEST_METHOD (NoColumns_IsOneRow)
    {
        std::vector<DxuiTextView::Row>  rows = PreviewText::BuildDocumentRows (MakeDocument(), {});

        Assert::AreEqual ((size_t) 3, rows[0].cells.size());
    }
};