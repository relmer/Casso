#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiEditableGridTests
//
//  The editable grid: a cell edited where it lies by a click, committed by
//  Enter and canceled by Escape; a failed edit open with its reason;
//  the hint row that adds a row once its required columns have text; Tab
//  along a row; read-only rows; Delete; and a row dragged by its gripper.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiEditableGridTests)
{
public:

    struct Commit
    {
        int           row    = -1;
        int           column = -1;
        std::wstring  text;
    };


    //  A grid of Name, Start (required) and End over the host's rows, each
    //  callback logged and the rows set again as a host sets them.
    struct Host
    {
        DxuiEditableGrid                        grid;
        std::vector<std::vector<std::wstring>>  rows;
        std::vector<bool>                       readOnly;
        std::vector<Commit>                     commits;
        std::vector<std::vector<std::wstring>>  adds;
        std::vector<std::pair<int, int>>        moves;
        std::vector<int>                        deletes;
        std::wstring                            error;
        bool                                    isRefusing = false;

        Host()
        {
            DxuiDpiScaler  scaler;

            scaler.SetDpi (96);

            grid.SetGridColumns ({ { { L"Name", 120 } }, { { L"Start", 80 }, true, true }, { { L"End", 80 } } });
            grid.SetAddRowText  (L"Add new range");

            grid.SetOnCommitCell ([this] (int row, int column, const std::wstring & text, std::wstring & outError)
            {
                commits.push_back ({ row, column, text });
                outError = isRefusing ? L"Not a range." : L"";

                if (!isRefusing)
                {
                    rows[(size_t) row][(size_t) column] = text;
                    Refresh();
                }

                return !isRefusing;
            });

            grid.SetOnAddRow ([this] (const std::vector<std::wstring> & texts, std::wstring & outError)
            {
                adds.push_back (texts);
                outError = isRefusing ? L"No end." : L"";

                if (!isRefusing)
                {
                    rows.push_back (texts);
                    readOnly.push_back (false);
                    Refresh();
                }

                return !isRefusing;
            });

            grid.SetOnMoveRow   ([this] (int from, int to) { moves.push_back ({ from, to }); });
            grid.SetOnDeleteRow ([this] (int row)          { deletes.push_back (row); });
            grid.SetOnError     ([this] (const std::wstring & text) { error = text; });

            rows     = { { L"Zero page", L"$0", L"$FF" }, { L"Stack", L"$100", L"$1FF" }, { L"ROM", L"$D000", L"$FFFF" } };
            readOnly = { false, false, true };
            Refresh();

            grid.Layout (RECT { 0, 0, 400, 300 }, scaler);
        }

        void Refresh()
        {
            std::vector<DxuiEditableGrid::GridRow>  gridRows;

            for (size_t r = 0; r < rows.size(); r++)
            {
                DxuiEditableGrid::GridRow  row;

                for (const std::wstring & text : rows[r])
                {
                    row.cells.push_back ({ text });
                }

                row.isReadOnly = readOnly[r];
                gridRows.push_back (std::move (row));
            }

            grid.SetGridRows (std::move (gridRows));
        }

        POINT GetCellCenter (int row, int column) const
        {
            RECT  cell = {};

            Assert::IsTrue (grid.GetCellTextRectPx (row, (size_t) column + 1, cell), L"the cell is in view");
            return POINT { (cell.left + cell.right) / 2, (cell.top + cell.bottom) / 2 };
        }

        void Mouse (DxuiMouseEventKind kind, POINT at)
        {
            DxuiMouseEvent  ev;

            ev.kind        = kind;
            ev.button      = DxuiMouseButton::Left;
            ev.positionDip = at;
            grid.OnMouse (ev);
        }

        void Click (int row, int column)
        {
            POINT  at = GetCellCenter (row, column);

            Mouse (DxuiMouseEventKind::Down, at);
            Mouse (DxuiMouseEventKind::Up,   at);
        }

        void Key (WPARAM vk, bool shift = false)
        {
            DxuiKeyEvent  ev;

            ev.kind  = DxuiKeyEventKind::Down;
            ev.vk    = vk;
            ev.shift = shift;
            grid.OnKey (ev);
        }
    };



    TEST_METHOD (TheLastRowIsAHintToAddARow)
    {
        Host  host;



        Assert::AreEqual (4, host.grid.GetRowCount(), L"the rows and the hint row");
        Assert::IsTrue   (host.grid.IsAddRow (3));

        const std::vector<DxuiListView::Cell> & hint = host.grid.GetCellsOfRow (3);

        Assert::AreEqual (std::wstring (L"Add new range"), hint[1].text, L"in the first editable column");
        Assert::IsTrue   (hint[1].dim);
        Assert::IsTrue   (hint[1].weight == DxuiFontWeight::Italic);
    }



    TEST_METHOD (AClickEditsACellAndEnterCommitsIt)
    {
        Host  host;



        host.Click (1, 1);
        Assert::IsTrue   (host.grid.IsEditing(), L"one click");
        Assert::AreEqual (1, host.grid.GetEditRow());
        Assert::AreEqual (1, host.grid.GetEditColumn());
        Assert::AreEqual (std::wstring (L"$100"), host.grid.GetEditor().GetText());

        host.grid.GetEditor().SetText (L"$180");
        host.Key (VK_RETURN);

        Assert::IsFalse  (host.grid.IsEditing());
        Assert::AreEqual ((size_t) 1, host.commits.size());
        Assert::AreEqual (std::wstring (L"$180"), host.commits[0].text);
        Assert::AreEqual (std::wstring (L"$180"), host.grid.GetCellsOfRow (1)[2].text);
    }



    TEST_METHOD (AnUnchangedCellIsNotCommitted)
    {
        Host  host;



        host.Click (0, 0);
        host.Key   (VK_RETURN);

        Assert::IsFalse (host.grid.IsEditing());
        Assert::IsTrue  (host.commits.empty());
    }



    TEST_METHOD (ARefusedEditStaysOpenWithItsReasonAndEscapeCancelsIt)
    {
        Host  host;



        host.isRefusing = true;
        host.grid.BeginEdit (0, 2);
        host.grid.GetEditor().SetText (L"$-1");

        Assert::IsFalse  (host.grid.EndEdit (true));
        Assert::IsTrue   (host.grid.IsEditing());
        Assert::AreEqual (std::wstring (L"Not a range."), host.error);

        host.Key (VK_ESCAPE);
        Assert::IsFalse  (host.grid.IsEditing());
        Assert::AreEqual (std::wstring (L"$FF"), host.grid.GetCellsOfRow (0)[3].text, L"as it was");
        Assert::IsTrue   (host.error.empty(), L"the reason goes with the edit");
    }



    TEST_METHOD (TheHintRowAddsARowOnceItsRequiredColumnsHaveText)
    {
        Host  host;



        host.Click (3, 0);
        Assert::IsTrue (host.grid.IsEditing(), L"a click on the hint");

        host.grid.GetEditor().SetText (L"Game");
        host.Key (VK_RETURN);

        Assert::IsTrue   (host.adds.empty(), L"the start is required");
        Assert::AreEqual (1, host.grid.GetEditColumn(), L"Enter goes on to the start");
        Assert::AreEqual (std::wstring (L"Game"), host.grid.GetCellsOfRow (3)[1].text, L"the typed text is on the row");

        host.grid.GetEditor().SetText (L"$6000");
        host.Key (VK_RETURN);

        Assert::AreEqual ((size_t) 1, host.adds.size());
        Assert::AreEqual (std::wstring (L"Game"),  host.adds[0][0]);
        Assert::AreEqual (std::wstring (L"$6000"), host.adds[0][1]);
        Assert::IsFalse  (host.grid.IsEditing(), L"the edit ends");
        Assert::AreEqual (5, host.grid.GetRowCount(), L"and a new hint row follows it");
        Assert::AreEqual (std::wstring (L"Add new range"), host.grid.GetCellsOfRow (4)[1].text);
    }



    TEST_METHOD (ARowTheHostRefusesGoesOnToItsNextEmptyColumn)
    {
        Host  host;



        host.isRefusing = true;
        host.grid.BeginEdit (3, 1);
        host.grid.GetEditor().SetText (L"$6000");
        host.Key (VK_RETURN);

        Assert::AreEqual ((size_t) 1, host.adds.size());
        Assert::IsTrue   (host.grid.IsEditing());
        Assert::AreEqual (2, host.grid.GetEditColumn(), L"the end, the next empty column");
        Assert::AreEqual (std::wstring (L"No end."), host.error);
        Assert::AreEqual (std::wstring (L"$6000"), host.grid.GetPendingTexts()[1], L"the typed text is in place");
    }



    TEST_METHOD (EscapeDropsANewRow)
    {
        Host  host;



        host.grid.BeginEdit (3, 0);
        host.grid.GetEditor().SetText (L"Game");
        host.Key (VK_TAB);
        Assert::AreEqual (1, host.grid.GetEditColumn(), L"Tab moves along the row");

        host.Key (VK_ESCAPE);
        Assert::IsFalse  (host.grid.IsEditing());
        Assert::IsTrue   (host.grid.GetPendingTexts()[0].empty());
        Assert::AreEqual (std::wstring (L"Add new range"), host.grid.GetCellsOfRow (3)[1].text);
    }



    TEST_METHOD (TabAndShiftTabMoveAlongARow)
    {
        Host  host;



        host.grid.BeginEdit (0, 0);
        host.Key (VK_TAB);
        Assert::AreEqual (1, host.grid.GetEditColumn());

        host.Key (VK_TAB, true);
        Assert::AreEqual (0, host.grid.GetEditColumn());
        Assert::AreEqual (0, host.grid.GetEditRow());
    }



    TEST_METHOD (AReadOnlyRowIsNotEditedOrDeleted)
    {
        Host  host;



        host.Click (2, 0);
        Assert::IsFalse (host.grid.IsEditing());
        Assert::IsFalse (host.grid.CanDeleteRow (2));

        host.grid.SetSelectedRow (2);
        host.Key (VK_DELETE);
        Assert::IsTrue  (host.deletes.empty());
    }



    TEST_METHOD (DeleteDeletesTheSelectedRow)
    {
        Host  host;



        host.grid.SetSelectedRow (1);
        host.Key (VK_DELETE);

        Assert::AreEqual ((size_t) 1, host.deletes.size());
        Assert::AreEqual (1, host.deletes[0]);
    }



    TEST_METHOD (ARowIsDraggedByItsGripper)
    {
        Host   host;
        POINT  from = host.GetCellCenter (0, 0);
        POINT  to   = host.GetCellCenter (2, 0);



        from.x = host.grid.GetPaneLeadPx() + 2;
        to.x   = from.x;

        Assert::AreEqual (-1, host.grid.GetGridColumnAt (from.x), L"the gripper's column");

        host.Mouse (DxuiMouseEventKind::Down, from);
        Assert::IsTrue  (host.grid.IsDraggingRow());
        Assert::IsFalse (host.grid.IsEditing(), L"a gripper is not a cell");

        host.Mouse (DxuiMouseEventKind::Move, to);
        host.Mouse (DxuiMouseEventKind::Up,   to);

        Assert::IsFalse  (host.grid.IsDraggingRow());
        Assert::AreEqual ((size_t) 1, host.moves.size());
        Assert::AreEqual (0, host.moves[0].first);
        Assert::AreEqual (2, host.moves[0].second);
    }



    TEST_METHOD (AColumnsValidationKeepsTheBoxOpen)
    {
        Host  host;



        host.grid.SetOnValidate ([] (int column, const std::wstring & text, std::wstring & outError)
        {
            outError = L"Digits only.";
            return column != 2 || text.find_first_not_of (L"$0123456789ABCDEF") == std::wstring::npos;
        });

        host.grid.BeginEdit (0, 2);
        host.grid.GetEditor().SetText (L"later");
        host.Key (VK_RETURN);

        Assert::IsTrue   (host.grid.IsEditing());
        Assert::IsTrue   (host.commits.empty());
        Assert::AreEqual (std::wstring (L"Digits only."), host.error);
    }
};
