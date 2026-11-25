#include "Pch.h"
#include "CassoExplorer/Model/PreviewText.h"
#include "Core/UnicodeSymbols.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  RecordTableRowsTests
//
//  A document's record table as text rows: a table in a wide view, cards in
//  a narrow one, each record's details under it in both.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (RecordTableRowsTests)
{
public:

    static std::vector<Paragraph> MakeDocument()
    {
        auto                 table = std::make_shared<RecordTable>();
        RecordTable::Record  record;
        Paragraph            card;
        Paragraph            detail;
        Paragraph            title;
        Paragraph            holder;



        table->headings = { L"Name", L"Class" };

        for (const wchar_t * name : { L"ELVE", L"SHADE" })
        {
            record       = RecordTable::Record();
            record.cells = { Run { name, true }, Run { L"Bishop" } };
            card.runs    = { Run { name, true }, Run { L"   Bishop" } };
            detail.runs  = { Run { L"      MACE" }, Run { L"\t" }, Run { L"equipped" } };
            record.card    = { card };
            record.details = { detail };
            table->records.push_back (record);
        }

        title.runs   = { Run { L"Roster" } };
        holder.table = table;

        return { title, holder };
    }


    TEST_METHOD (Wide_IsAHeadingRowAndARowARecord)
    {
        std::vector<int>                outParagraph;
        std::vector<DxuiTextView::Row>  rows = PreviewText::BuildDocumentRows (MakeDocument(), {}, TableForm::Table, &outParagraph);



        Assert::AreEqual ((size_t) 7, rows.size(), L"The title, the headings, and a row and a detail for each record, a blank row between");
        Assert::IsTrue   (rows[4].cells.empty(), L"The blank row between records with details");
        Assert::IsTrue   (rows[5].cells == std::vector<std::wstring> ({ L"SHADE", L"Bishop" }));
        Assert::IsTrue   (rows[1].cells == std::vector<std::wstring> ({ L"Name", L"Class" }));
        Assert::IsTrue   (rows[1].spans[0].bold, L"The headings are bold");
        Assert::IsTrue   (rows[2].cells == std::vector<std::wstring> ({ L"ELVE", L"Bishop" }));
        Assert::IsTrue   (rows[3].cells == std::vector<std::wstring> ({ L"      MACE", L"equipped" }), L"The record's detail is under it, its tab a column");
        Assert::IsTrue   (outParagraph == std::vector<int> ({ 0, 1, 1, 1, 1, 1, 1 }), L"Each row's paragraph");
    }


    TEST_METHOD (Fields_AreTheFirstCellThenHeadingAndValuePairs)
    {
        std::vector<DxuiTextView::Row>  rows = PreviewText::BuildDocumentRows (MakeDocument(), {}, TableForm::TwoFields);



        Assert::AreEqual ((size_t) 8, rows.size(), L"The title, then the name, its fields and a detail for each record, a blank line between");
        Assert::AreEqual (std::wstring (L"ELVE"), rows[1].cells[0]);
        Assert::IsTrue   (rows[2].cells == std::vector<std::wstring> ({ L"   Class Bishop" }));
        Assert::AreEqual (std::wstring (L""),      rows[4].cells[0]);
        Assert::AreEqual (std::wstring (L"SHADE"), rows[5].cells[0]);
    }


    TEST_METHOD (Narrow_IsEachRecordsCard)
    {
        std::vector<DxuiTextView::Row>  rows = PreviewText::BuildDocumentRows (MakeDocument(), {}, TableForm::Cards);



        Assert::AreEqual ((size_t) 6, rows.size(), L"The title, then a card and a detail for each record, a blank line between");
        Assert::AreEqual (std::wstring (L"ELVE   Bishop"), rows[1].cells[0]);
        Assert::AreEqual (std::wstring (L""),             rows[3].cells[0]);
        Assert::AreEqual (std::wstring (L"SHADE   Bishop"), rows[4].cells[0]);
    }


    TEST_METHOD (Title_IsBold_WithARuleUnderItAboveTheCards)
    {
        std::vector<Paragraph>          document = MakeDocument();
        auto                            table    = std::make_shared<RecordTable> (*document[1].table);
        std::vector<DxuiTextView::Row>  narrow;
        std::vector<DxuiTextView::Row>  wide;



        table->title       = L"Party";
        document[1].table  = table;
        narrow = PreviewText::BuildDocumentRows (document, {}, TableForm::Cards);
        wide   = PreviewText::BuildDocumentRows (document, {}, TableForm::Table);

        Assert::AreEqual (std::wstring (L"Party"), narrow[1].cells[0]);
        Assert::IsTrue   (narrow[1].spans[0].bold && narrow[1].rule, L"Above the cards, bold with a rule");
        Assert::AreEqual (std::wstring (L"Party"), wide[1].cells[0]);
        Assert::IsFalse  (wide[1].rule, L"Above the table, the heading row is the rule");
        Assert::AreEqual (std::wstring (L"Party"), GetFirstText (FlattenRecordTables (document)[1]), L"and a copy out has the title");
    }


    //  A run with a swatch has a square of its color before its text, and a
    //  hatched swatch has the crosshatched square.
    TEST_METHOD (Swatch_IsASquareOfItsColorBeforeTheText)
    {
        Paragraph                       legend;
        std::vector<DxuiTextView::Row>  rows;



        legend.runs = { Run { L"Water ", false, false, false, RunColor::Text, 0xFF0000FF }, Run { L"Lost", false, false, false, RunColor::Text, kHatchedSwatch } };
        rows        = PreviewText::BuildDocumentRows ({ legend });

        Assert::AreEqual (std::wstring (s_kpszSwatchSquare) + L" Water " + s_kpszHatchedSquare + L" Lost", rows[0].cells[0]);
        Assert::AreEqual (0xFF0000FFu, rows[0].spans[0].argb, L"The square is in the swatch's color");
        Assert::AreEqual (1, rows[0].spans[0].length);
    }

    static std::wstring GetFirstText (const Paragraph & paragraph)
    {
        return paragraph.runs.empty() ? std::wstring() : paragraph.runs[0].text;
    }
};