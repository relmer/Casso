#include "Pch.h"
#include "CassoExplorer/Model/PreviewText.h"

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
        std::vector<DxuiTextView::Row>  rows = PreviewText::BuildDocumentRows (MakeDocument(), {}, true, &outParagraph);



        Assert::AreEqual ((size_t) 6, rows.size(), L"The title, the headings, and a row and a detail for each record");
        Assert::IsTrue   (rows[1].cells == std::vector<std::wstring> ({ L"Name", L"Class" }));
        Assert::IsTrue   (rows[1].spans[0].bold, L"The headings are bold");
        Assert::IsTrue   (rows[2].cells == std::vector<std::wstring> ({ L"ELVE", L"Bishop" }));
        Assert::IsTrue   (rows[3].cells == std::vector<std::wstring> ({ L"      MACE", L"equipped" }), L"The record's detail is under it, its tab a column");
        Assert::IsTrue   (outParagraph == std::vector<int> ({ 0, 1, 1, 1, 1, 1 }), L"Each row's paragraph");
    }


    TEST_METHOD (Narrow_IsEachRecordsCard)
    {
        std::vector<DxuiTextView::Row>  rows = PreviewText::BuildDocumentRows (MakeDocument(), {}, false);



        Assert::AreEqual ((size_t) 6, rows.size(), L"The title, then a card and a detail for each record, a blank line between");
        Assert::AreEqual (std::wstring (L"ELVE   Bishop"), rows[1].cells[0]);
        Assert::AreEqual (std::wstring (L""),             rows[3].cells[0]);
        Assert::AreEqual (std::wstring (L"SHADE   Bishop"), rows[4].cells[0]);
    }
};
