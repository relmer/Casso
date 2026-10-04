#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr float  s_kStripAspect = 560.0f / 384.0f;





////////////////////////////////////////////////////////////////////////////////
//
//  RecordingStripSource
//
//  Keeps what the strip told it and which cells were clicked.
//
////////////////////////////////////////////////////////////////////////////////

class RecordingStripSource : public IDxuiImageStripSource
{
public:
    void   SetCellLayout   (int count, SIZE cellPx) override { cells = count; size = cellPx; layouts++; }
    Image  GetCellImage    (int index) override              { (void) index; return nullptr; }
    Image  GetPreviewImage (int index) override              { (void) index; return nullptr; }
    void   OnCellClicked   (int index) override              { clicked.push_back (index); }

    int               cells   = -1;
    SIZE              size    = {};
    int               layouts = 0;
    std::vector<int>  clicked;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStripTests
//
//  A strip of pictures in a toolbar: cells as thick as the strip at the
//  pictures' aspect ratio, end to end from the leading edge with no gaps and
//  no overlaps, as many as fit whole, standing on end down a side; the
//  pointer finds the cell under it and a click reports it.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiImageStripTests)
{
public:

    TEST_METHOD (CellsKeepThePictureAspect)
    {
        Assert::AreEqual (47, DxuiImageStrip::GetCellLength (32, s_kStripAspect, false), L"lying down: 32 * 560 / 384, rounded");
        Assert::AreEqual (22, DxuiImageStrip::GetCellLength (32, s_kStripAspect, true),  L"standing up: 32 * 384 / 560, rounded");
        Assert::AreEqual (0,  DxuiImageStrip::GetCellLength (0,  s_kStripAspect, false), L"no thickness, no cell");
    }


    TEST_METHOD (OnlyWholeCellsFit)
    {
        Assert::AreEqual (10, DxuiImageStrip::GetCellCount (479, 47), L"479 / 47 is ten whole cells");
        Assert::AreEqual (0,  DxuiImageStrip::GetCellCount (46,  47), L"not even one");
        Assert::AreEqual (0,  DxuiImageStrip::GetCellCount (100, 0),  L"no cell length");
    }


    TEST_METHOD (CellsRunEndToEndWithNoGapsOrOverlaps)
    {
        constexpr RECT  kStrip = { 100, 10, 600, 42 };
        constexpr int   kCell  = 47;
        constexpr int   kCount = 10;

        RECT  previous = {};
        RECT  cell     = {};
        int   i        = 0;



        for (i = 0; i < kCount; i++)
        {
            cell = DxuiImageStrip::GetCellRect (kStrip, i, kCell, false);

            Assert::AreEqual ((int) kStrip.top,    (int) cell.top,    L"as thick as the strip");
            Assert::AreEqual ((int) kStrip.bottom, (int) cell.bottom, L"as thick as the strip");
            Assert::AreEqual (kCell, (int) (cell.right - cell.left), L"every cell the same length");

            if (i == 0)
            {
                Assert::AreEqual ((int) kStrip.left, (int) cell.left, L"the first starts at the leading edge");
            }
            else
            {
                Assert::AreEqual ((int) previous.right, (int) cell.left, L"each starts where the one before ends");
            }

            previous = cell;
        }

        Assert::IsTrue (previous.right <= kStrip.right, L"the last fits inside the strip");

        cell = DxuiImageStrip::GetCellRect (kStrip, 2, 22, true);
        Assert::AreEqual ((int) kStrip.top + 44, (int) cell.top, L"standing up, cells run down");
        Assert::AreEqual ((int) kStrip.left, (int) cell.left, L"across the whole thickness");
    }


    TEST_METHOD (ThePointerFindsTheCellUnderIt)
    {
        constexpr RECT  kStrip = { 100, 10, 600, 42 };



        Assert::AreEqual (0,  DxuiImageStrip::HitTestCell (kStrip, 10, 47, false, 100, 20), L"the leading edge is the first cell");
        Assert::AreEqual (1,  DxuiImageStrip::HitTestCell (kStrip, 10, 47, false, 147, 20), L"the next cell starts at 147");
        Assert::AreEqual (9,  DxuiImageStrip::HitTestCell (kStrip, 10, 47, false, 569, 20), L"the last whole cell");
        Assert::AreEqual (-1, DxuiImageStrip::HitTestCell (kStrip, 10, 47, false, 575, 20), L"past the last whole cell");
        Assert::AreEqual (-1, DxuiImageStrip::HitTestCell (kStrip, 10, 47, false, 200, 50), L"off the strip");
    }


    TEST_METHOD (LayoutTellsTheSourceAndAClickReportsTheCell)
    {
        DxuiImageStrip        strip;
        RecordingStripSource  source;
        DxuiDpiScaler         scaler;
        bool                  taken = false;



        strip.SetSource (&source);
        strip.SetAspect (s_kStripAspect);
        strip.Layout    (RECT { 0, 0, 479, 32 }, false, scaler);

        Assert::AreEqual (10, source.cells,   L"ten cells fit");
        Assert::AreEqual (47, (int) source.size.cx, L"each as wide as the aspect makes it");
        Assert::AreEqual (32, (int) source.size.cy, L"and as tall as the strip");

        Assert::IsTrue  (strip.OnLButtonDown (100, 5), L"a press on a cell arms the entry");
        Assert::IsFalse (strip.OnLButtonDown (475, 5), L"a press past the cells does not");

        taken = strip.OnClick (100, 5);

        Assert::IsTrue (taken, L"a click on a cell is taken");
        Assert::AreEqual<size_t> (1, source.clicked.size(), L"one click reported");
        Assert::AreEqual (2, source.clicked[0], L"x 100 is the third cell");

        taken = strip.OnClick (475, 5);
        Assert::IsFalse (taken, L"the space past the last whole cell is not a cell");

        strip.Layout (RECT { 0, 0, 32, 100 }, false, scaler);

        Assert::AreEqual (4,  source.cells,         L"standing up: 100 / 22 is four");
        Assert::AreEqual (32, (int) source.size.cx, L"as wide as the strip");
        Assert::AreEqual (22, (int) source.size.cy, L"as tall as the aspect makes it");
        Assert::AreEqual (22, strip.GetMinWidthPx (scaler), L"it can shrink to one cell");
    }
};
