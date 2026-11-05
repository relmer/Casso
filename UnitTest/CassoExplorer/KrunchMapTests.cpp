#include "Pch.h"
#include "../EhmTestHelper.h"

#include "CassoExplorer/KrunchMapDialog.h"
#include "CassoExplorer/Model/KrunchMap.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "../Dxui/MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





TEST_CLASS (KrunchMapTests)
{
public:

    using Cell = KrunchMap::Cell;

    //  A file at 6 that stays, one at 20 that moves to 10, and one at 40 that
    //  moves to 20.
    static std::vector<PascalKrunchMove>  MakeFiles()
    {
        return { { 1, 6, 6, 4, "A.DATA" }, { 2, 20, 10, 10, "B.DATA" }, { 3, 40, 20, 8, "C.DATA" } };
    }

    static void  AssertCells (const std::vector<KrunchMap::Block> & blocks, int first, int count, Cell cell)
    {
        for (int block = first; block < first + count; block++)
        {
            Assert::IsTrue (blocks[(size_t) block].cell == cell, std::format (L"block {}", block).c_str());
        }
    }



    TEST_METHOD (Build_BeforeTheAppleStarts_ShowsEveryFileAtItsOldPlace)
    {
        std::vector<KrunchMap::Block>  cells = KrunchMap::Build (MakeFiles(), false, 0, 0);



        Assert::AreEqual ((size_t) KrunchMap::kBlocks, cells.size());
        Assert::AreEqual (1, cells[20].file);
        Assert::AreEqual (-1, cells[10].file);
        AssertCells (cells, 0,  6,   Cell::System);
        AssertCells (cells, 6,  4,   Cell::InPlace);
        AssertCells (cells, 10, 10,  Cell::Free);
        AssertCells (cells, 20, 10,  Cell::ToMove);
        AssertCells (cells, 30, 10,  Cell::Free);
        AssertCells (cells, 40, 8,   Cell::ToMove);
        AssertCells (cells, 48, 232, Cell::Free);
    }


    TEST_METHOD (Build_PartWayThroughAFile_ShowsItsOldPlaceAndThePartWritten)
    {
        //  Half of the 18 blocks to move are moved: 9 of the first file's 10.
        std::vector<KrunchMap::Block>  cells = KrunchMap::Build (MakeFiles(), true, 50, 0);



        AssertCells (cells, 10, 9,  Cell::Writing);
        AssertCells (cells, 19, 1,  Cell::Free);
        AssertCells (cells, 20, 10, Cell::Reading);
        AssertCells (cells, 40, 8,  Cell::ToMove);
    }


    TEST_METHOD (Build_AfterAFileMoves_FreesItsOldPlace)
    {
        //  14 of the 18 blocks are moved: the first file's 10, and 4 of the next.
        std::vector<KrunchMap::Block>  cells = KrunchMap::Build (MakeFiles(), true, 78, 1);



        AssertCells (cells, 10, 10, Cell::InPlace);
        AssertCells (cells, 20, 4,  Cell::Writing);
        AssertCells (cells, 24, 16, Cell::Free);
        AssertCells (cells, 40, 8,  Cell::Reading);
    }


    TEST_METHOD (Build_WhenTheKrunchEnds_ShowsEveryFilePackedFromTheStart)
    {
        std::vector<KrunchMap::Block>  cells = KrunchMap::Build (MakeFiles(), true, 100, 2);



        AssertCells (cells, 0,  6,   Cell::System);
        AssertCells (cells, 6,  22,  Cell::InPlace);
        AssertCells (cells, 28, 252, Cell::Free);
    }


    TEST_METHOD (Build_AMoveThatOverlapsItsOldPlace_ShowsTheWrittenPartOverIt)
    {
        //  A file at 12 for 10 blocks moves to 6, over its first 4 old blocks.
        std::vector<PascalKrunchMove>  files = { { 1, 12, 6, 10 } };
        std::vector<KrunchMap::Block>  cells = KrunchMap::Build (files, true, 80, 0);



        AssertCells (cells, 6,  8, Cell::Writing);
        AssertCells (cells, 14, 8, Cell::Reading);
    }


    TEST_METHOD (Counts_LeaveOutTheFilesThatStay)
    {
        Assert::AreEqual (18, KrunchMap::CountBlocksToMove (MakeFiles()));
        Assert::AreEqual (2,  KrunchMap::CountFilesToMove  (MakeFiles()));
    }

    TEST_METHOD (BuildView_WhileAFileMoves_ShowsWhichFileOfHowMany)
    {
        LinkOperation  operation;



        operation.krunchFiles   = MakeFiles();
        operation.krunchPlanned = true;
        operation.krunchTaken   = true;
        operation.krunchMoved   = 1;
        operation.progress      = 55;
        operation.state         = LinkOperationState::Running;

        KrunchMapPanel::View  view = KrunchMapDialog::BuildView (operation, L"Slot 6, Drive 2", false);

        Assert::AreEqual (std::wstring (L"Moving C.DATA, file 2 of 2."), view.status);
        AssertCells (view.blocks, 40, 8, Cell::Reading);
        Assert::AreEqual (std::wstring (L"Stopping after the file being moved."), KrunchMapDialog::DescribeStatus (operation, true));
    }


    TEST_METHOD (BuildView_WhenTheKrunchFails_ShowsNoFileBeingMoved)
    {
        LinkOperation  operation;



        operation.krunchFiles   = MakeFiles();
        operation.krunchPlanned = true;
        operation.krunchTaken   = true;
        operation.progress      = 50;
        operation.state         = LinkOperationState::Failed;
        operation.message       = "The disk could not be written.";

        KrunchMapPanel::View  view = KrunchMapDialog::BuildView (operation, L"Slot 6, Drive 2", false);

        Assert::AreEqual (std::wstring (L"The disk could not be written."), view.status);
        AssertCells (view.blocks, 20, 10, Cell::ToMove);
        AssertCells (view.blocks, 10, 10, Cell::Free);
    }


    TEST_METHOD (BuildView_WhenTheKrunchEnds_ShowsEveryFileInPlace)
    {
        LinkOperation  operation;



        operation.krunchFiles   = MakeFiles();
        operation.krunchPlanned = true;
        operation.state         = LinkOperationState::Done;

        KrunchMapPanel::View  view = KrunchMapDialog::BuildView (operation, L"Slot 6, Drive 2", false);

        Assert::AreEqual (100, view.percent);
        Assert::AreEqual (std::wstring (L"Krunch complete: 2 files moved."), view.status);
        AssertCells (view.blocks, 6, 22, Cell::InPlace);
    }


    TEST_METHOD (BuildView_BeforeTheDirectoryIsRead_HasNoCells)
    {
        LinkOperation  operation;



        operation.state = LinkOperationState::Queued;

        Assert::IsTrue (KrunchMapDialog::BuildView (operation, L"Slot 6, Drive 2", false).blocks.empty());
        Assert::AreEqual (std::wstring (L"Reading the directory."), KrunchMapDialog::DescribeStatus (operation, false));
    }

    TEST_METHOD (DescribeFile_GivesTheFilesBlocksAndWhereItMoves)
    {
        std::vector<PascalKrunchMove>  files = MakeFiles();



        Assert::AreEqual (std::wstring (L"A.DATA: 4 blocks at 6 to 9, which stay in place"),     KrunchMapPanel::DescribeFile (files[0], Cell::InPlace));
        Assert::AreEqual (std::wstring (L"B.DATA: 10 blocks at 20 to 29, moving to 10 to 19"),   KrunchMapPanel::DescribeFile (files[1], Cell::ToMove));
        Assert::AreEqual (std::wstring (L"B.DATA: 10 blocks at 10 to 19, moved from 20 to 29"),  KrunchMapPanel::DescribeFile (files[1], Cell::InPlace));
        Assert::AreEqual (2, KrunchMap::FindMovingFile (files, 1));
        Assert::AreEqual (-1, KrunchMap::FindMovingFile (files, 2));
    }

    //  At 96 DPI on a panel 640 wide, a cell is 14 pixels with a gap of 2,
    //  and the map starts 38 pixels down, below the heading.
    static int  CountFileOutlines (const MockDxuiPainter & painter)
    {
        return (int) std::count_if (painter.Calls().begin(), painter.Calls().end(), [] (const RecordedPaintCall & call)
        {
            return call.kind == RecordedPaintKind::OutlineRect && call.thickness == KrunchMapPanel::kOutlineDip;
        });
    }

    static bool  HasText (const MockDxuiTextRenderer & text, const std::wstring & line)
    {
        return std::any_of (text.Calls().begin(), text.Calls().end(), [&] (const RecordedTextCall & call) { return call.text == line; });
    }


    TEST_METHOD (Panel_PointingAtAFile_OutlinesItAndDescribesIt)
    {
        KrunchMapPanel        panel;
        KrunchMapPanel::View  view;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;
        DxuiDpiScaler         scaler;
        DxuiMouseEvent        move;
        DxuiMouseEvent        exit;



        view.files  = MakeFiles();
        view.blocks = KrunchMap::Build (view.files, false, 0, 0);

        panel.Layout  (RECT { 0, 0, 640, 400 }, scaler);
        panel.SetView (view);

        move.kind        = DxuiMouseEventKind::Move;
        move.positionDip = { 25 * 16 + 5, 38 + 5 };
        panel.OnMouse (move);
        panel.Paint   (painter, text, theme);

        Assert::AreEqual (1, CountFileOutlines (painter));
        Assert::IsTrue   (HasText (text, L"B.DATA: 10 blocks at 20 to 29, moving to 10 to 19"));

        painter.Reset();
        text.Reset();
        exit.kind = DxuiMouseEventKind::Leave;
        panel.OnMouse (exit);
        panel.Paint   (painter, text, theme);

        Assert::AreEqual (0, CountFileOutlines (painter));
        Assert::IsTrue   (HasText (text, L"Point at a block to see its file."));
    }


    TEST_METHOD (PlanKrunch_ListsEveryFile_WhenAskedFor)
    {
        std::vector<Byte>              image (NibblizationLayer::kImageByteSize, 0);
        std::vector<Byte>              directory;
        std::vector<PascalKrunchMove>  moves;
        std::vector<PascalKrunchMove>  files;
        std::vector<Byte>              next;
        FilePayload                    payload;



        PascalVolume::WriteBlank ("MAP", 0, image);
        payload.bytes.resize (PascalVolume::kBlockBytes * 3);

        for (const char * name : { "A.DATA", "B.DATA", "C.DATA" })
        {
            AssertSucceeded (PascalVolume (image).Write (FilePath::Parse (name), payload, next));
            image = next;
        }

        AssertSucceeded (PascalVolume (image).Delete (FilePath::Parse ("B.DATA"), next));
        image = next;

        AssertSucceeded (PascalVolume (image).PlanKrunch (moves, directory, &files));

        Assert::AreEqual ((size_t) 1, moves.size());
        Assert::AreEqual ((size_t) 2, files.size());
        Assert::AreEqual (files[0].from, files[0].to);
        Assert::AreEqual (std::string ("C.DATA"), files[1].name);
        Assert::AreEqual (moves[0].from, files[1].from);
        Assert::AreEqual (moves[0].to,   files[1].to);
    }
};
