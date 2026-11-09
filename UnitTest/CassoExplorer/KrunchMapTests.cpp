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


    //  B.DATA moves in parts of 4, 2 and 4 blocks, as tracks end at 24 and
    //  16; C.DATA in two of 4, as a track ends at 24.
    TEST_METHOD (SplitIntoParts_EndsEachPartAtATrackEnd)
    {
        std::vector<KrunchMap::Part>  parts = KrunchMap::SplitIntoParts (MakeFiles());



        Assert::AreEqual ((size_t) 5, parts.size());
        Assert::AreEqual (4, parts[0].blocks);
        Assert::AreEqual (2, parts[1].blocks);
        Assert::AreEqual (24, parts[1].from);
        Assert::AreEqual (14, parts[1].to);
        Assert::AreEqual (4, parts[2].blocks);
        Assert::AreEqual (2, parts[3].file);
        Assert::AreEqual (44, parts[4].from);
    }


    TEST_METHOD (FindLabelRow_PicksThePlacesLongestRow)
    {
        Assert::IsTrue (KrunchMapPanel::FindLabelRow (30, 47) == std::pair<int, int> (30, 39));
        Assert::IsTrue (KrunchMapPanel::FindLabelRow (36, 52) == std::pair<int, int> (40, 52));
        Assert::IsTrue (KrunchMapPanel::FindLabelRow (12, 12) == std::pair<int, int> (12, 12));
    }


    TEST_METHOD (Panel_LabelsEachPlace_AroundTheBlocksInFlight)
    {
        KrunchMapPanel        panel;
        KrunchMapPanel::View  view;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;
        DxuiDpiScaler         scaler;



        //  B.DATA's second part is in flight: its written place has W at 14
        //  and 15, and its old place R at 24 and 25, so its name is drawn in
        //  10 to 13, 20 to 23 and 26 to 29.
        view.files  = MakeFiles();
        view.blocks = KrunchMap::Build (view.files, true, 1, 0);

        panel.Layout  (RECT { 0, 0, 640, 400 }, scaler);
        panel.SetView (view);
        panel.Paint   (painter, text, theme);

        Assert::AreEqual (1, CountText (text, L"A.DATA"));
        Assert::AreEqual (3, CountText (text, L"B.DATA"));
        Assert::AreEqual (1, CountText (text, L"C.DATA"));
        Assert::AreEqual (2, CountText (text, L"R"));
        Assert::AreEqual (2, CountText (text, L"W"));
    }

    TEST_METHOD (Build_PartWayThroughAFile_MarksThePartInFlight)
    {
        //  B.DATA's first part is written, and its second is in flight.
        std::vector<KrunchMap::Block>  cells = KrunchMap::Build (MakeFiles(), true, 1, 0);



        AssertCells (cells, 10, 6,  Cell::Writing);
        AssertCells (cells, 16, 4,  Cell::Free);
        AssertCells (cells, 20, 10, Cell::Reading);
        AssertCells (cells, 40, 8,  Cell::ToMove);

        Assert::AreEqual ((wchar_t) 0, cells[10].activity);
        Assert::AreEqual (L'W',        cells[14].activity);
        Assert::AreEqual (L'W',        cells[15].activity);
        Assert::AreEqual (L'R',        cells[24].activity);
        Assert::AreEqual (L'R',        cells[25].activity);
        Assert::AreEqual ((wchar_t) 0, cells[26].activity);
    }


    TEST_METHOD (Build_AfterAPartIsWritten_ShowsItsLettersAgain)
    {
        std::vector<KrunchMap::Block>  cells = KrunchMap::Build (MakeFiles(), true, 2, 0);



        Assert::AreEqual ((wchar_t) 0, cells[14].activity);
        Assert::AreEqual (L'R',        cells[26].activity);
    }


    TEST_METHOD (Build_AfterAFileMoves_FreesItsOldPlace)
    {
        //  B.DATA's three parts and C.DATA's first are written.
        std::vector<KrunchMap::Block>  cells = KrunchMap::Build (MakeFiles(), true, 4, 1);



        AssertCells (cells, 10, 10, Cell::InPlace);
        AssertCells (cells, 20, 8,  Cell::Writing);
        AssertCells (cells, 28, 12, Cell::Free);
        AssertCells (cells, 40, 8,  Cell::Reading);

        Assert::AreEqual (L'W', cells[24].activity);
        Assert::AreEqual (L'R', cells[44].activity);
    }


    TEST_METHOD (Build_WhenTheKrunchEnds_ShowsEveryFilePackedFromTheStart)
    {
        std::vector<KrunchMap::Block>  cells = KrunchMap::Build (MakeFiles(), true, 5, 2);



        AssertCells (cells, 0,  6,   Cell::System);
        AssertCells (cells, 6,  22,  Cell::InPlace);
        AssertCells (cells, 28, 252, Cell::Free);
    }


    TEST_METHOD (Build_AMoveThatOverlapsItsOldPlace_ShowsTheWrittenPartOverIt)
    {
        //  A file at 12 for 10 blocks moves to 6 in parts of 2, 2 and 6, over
        //  its first 4 old blocks; the third part is in flight.
        std::vector<PascalKrunchMove>  files = { { 1, 12, 6, 10, "X" } };
        std::vector<KrunchMap::Block>  cells = KrunchMap::Build (files, true, 2, 0);



        AssertCells (cells, 6,  10, Cell::Writing);
        AssertCells (cells, 16, 6,  Cell::Reading);
        Assert::AreEqual (L'W', cells[12].activity);
        Assert::AreEqual (L'R', cells[16].activity);
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
        operation.krunchParts   = 4;
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
        Assert::AreEqual (1, KrunchMap::FindMovingFile (files, 0));
        Assert::AreEqual (2, KrunchMap::FindMovingFile (files, 3));
        Assert::AreEqual (2, KrunchMap::FindMovingFile (files, 5));
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
    static int  CountText (const MockDxuiTextRenderer & text, const std::wstring & line)
    {
        return (int) std::count_if (text.Calls().begin(), text.Calls().end(), [&] (const RecordedTextCall & call) { return call.text == line; });
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
