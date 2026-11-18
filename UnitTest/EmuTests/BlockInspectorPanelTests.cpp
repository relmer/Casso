#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "FakeDiskFileIo.h"
#include "Devices/Disk/FilePath.h"
#include "Devices/Disk/Inspector/Blocks/BlockInspectorModel.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Ui/DiskInspector/Blocks/BlockFieldsView.h"
#include "Ui/DiskInspector/Blocks/BlockHexSource.h"
#include "Ui/DiskInspector/Blocks/BlockHexView.h"
#include "Ui/DiskInspector/Blocks/BlockInspectorPanel.h"
#include "Ui/DiskInspector/Blocks/BlockMapGrid.h"
#include "Ui/DiskInspector/Blocks/BlockMapView.h"
#include "Core/NumberFormat.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanelTests
//
//  Block mode's window parts without a window: the map's grid, a click and
//  a point on the map, a click on a linked field, typing in the hex view,
//  and the panel's status line, block number and typing through to the
//  model's edits.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BlockInspectorPanelTests)
{
public:

    static constexpr size_t   kBlockBytes = 512;
    static constexpr size_t   k800KBlocks = 1600;
    static constexpr int64_t  kTime       = 1'700'000'000;
    static constexpr Word     kLoad       = 0x2000;

    static void OpenModel (FakeDiskFileIo & io, BlockInspectorModel & model)
    {
        vector<Byte>                     buffer (k800KBlocks * kBlockBytes, 0);
        vector<Byte>                     next;
        FilePayload                      payload;
        std::unique_ptr<BlockImageFile>  file;
        std::wstring                     error;



        AssertSucceeded (ProDosSkeleton::Write (buffer, "BLOCKS"));

        payload.bytes.assign (300, 0x5A);
        payload.type           = ProDosVolume::kTypeBinary;
        payload.loadAddress    = kLoad;
        payload.hasLoadAddress = true;

        AssertSucceeded (ProDosVolume (buffer).Write (FilePath::Parse ("SEED"), payload, next));

        io.files["vol.po"]  = next;
        io.stamps["vol.po"] = { next.size(), kTime };

        AssertSucceeded (BlockImageFile::Open (io, "vol.po", file, error));
        AssertSucceeded (model.Open (std::move (file)));
    }


    static bool HasText (const MockDxuiTextRenderer & text, const std::wstring & part)
    {
        for (const RecordedTextCall & call : text.Calls())
        {
            if (call.text.find (part) != std::wstring::npos)
            {
                return true;
            }
        }

        return false;
    }


    TEST_METHOD (Grid_FitsEveryBlockAndFindsTheCellAtAPoint)
    {
        BlockMapGrid  floppy   (280, 400, 400);
        BlockMapGrid  hardDisk (65535, 300, 300);
        BlockMapGrid  huge     (2'100'000, 300, 300);
        POINT         at       = floppy.GetCellOrigin (17);



        Assert::AreEqual ((uint32_t) 1, floppy.GetBlocksPerCell());
        Assert::IsTrue (floppy.GetCellPx() > BlockMapGrid::kMinCellPx);
        Assert::IsTrue ((uint64_t) floppy.GetColumns() * floppy.GetRows() >= 280);

        Assert::AreEqual ((uint32_t) 1, hardDisk.GetBlocksPerCell(), L"a block a cell while a block is at least a pixel");
        Assert::AreEqual (0, hardDisk.GetGapPx(), L"cells this small touch");
        Assert::IsTrue   (hardDisk.GetRows() * hardDisk.GetPitch() <= 300.0);
        Assert::AreEqual (65534u, hardDisk.GetLastBlock (hardDisk.GetCellCount() - 1));
        Assert::IsTrue   (huge.GetBlocksPerCell() > 1, L"blocks less than a pixel are joined");
        Assert::IsTrue   (huge.GetPitch() >= 1.0);

        Assert::AreEqual ((int64_t) 17, floppy.FindCellAt (at.x + 1, at.y + 1));
        Assert::AreEqual ((int64_t) -1, floppy.FindCellAt (-1, 0));
    }


    TEST_METHOD (Grid_PicksTheMoreNotableRole)
    {
        Assert::IsTrue (BlockMap::PickRole (SectorRole::FileData, SectorRole::CrossLinked) == SectorRole::CrossLinked);
        Assert::IsTrue (BlockMap::PickRole (SectorRole::IndexBlock, SectorRole::Free) == SectorRole::IndexBlock);
    }


    TEST_METHOD (MapView_AClickSelectsTheBlockAndAPointDescribesIt)
    {
        FakeDiskFileIo       io;
        BlockInspectorModel  model;
        BlockMapView         view;
        DxuiDpiScaler        scaler;
        DxuiMouseEvent       ev;
        uint32_t             selected = UINT32_MAX;
        std::wstring         hover;
        BlockMapGrid         grid (k800KBlocks, 400, 400);
        POINT                at     = grid.GetCellOrigin (2);



        OpenModel (io, model);

        view.SetModel    (&model);
        view.SetOnSelect ([&] (uint32_t block) { selected = block; });
        view.SetOnHover  ([&] (const std::wstring & text) { hover = text; });
        view.Layout      (RECT { 0, 0, 400, 400 }, scaler);

        ev.kind        = DxuiMouseEventKind::Move;
        ev.positionDip = { at.x + 1, at.y + 1 };
        view.OnMouse (ev);

        Assert::AreEqual (std::wstring (L"Block 2: Volume header"), hover);

        ev.kind   = DxuiMouseEventKind::Down;
        ev.button = DxuiMouseButton::Left;
        view.OnMouse (ev);

        Assert::AreEqual (2u, selected);
    }


    TEST_METHOD (Grid_EachZoomDrawsEveryCellLargerThanTheLast)
    {
        BlockMapGrid  fit    (65535, 300, 300);
        double        before = 0.0;
        double        zoom   = 1.0;
        bool          isGap  = false;



        for (zoom = 1.0; zoom < 40.0; zoom *= 1.25)
        {
            BlockMapGrid  grid (65535, fit.GetBlockColumns(), fit.GetBlockPx() * zoom);
            double        size = grid.GetPitch() - grid.GetGapPx();

            Assert::AreEqual ((uint32_t) 1, grid.GetGroup());
            Assert::IsTrue   (size > before, std::format (L"a cell at zoom {} is {} px, after {} px", zoom, size, before).c_str());

            isGap  = isGap || grid.GetGapPx() > 0;
            before = size;
        }

        Assert::IsTrue (isGap, L"the cells are drawn apart once they are large enough");
    }


    TEST_METHOD (MapView_AFlightZoomsOutThenInToTheHighlightedBlocks)
    {
        FakeDiskFileIo       io;
        BlockInspectorModel  model;
        BlockMapView         view;
        DxuiDpiScaler        scaler;
        std::wstring         tip;
        RECT                 anchor = {};
        double               start  = 0.0;
        double               least  = 0.0;
        double               ms     = 0.0;



        OpenModel (io, model);

        view.SetModel    (&model);
        view.SetZoomable (true);
        view.Layout      (RECT { 0, 0, 400, 400 }, scaler);
        view.Refresh();

        //  Zoomed in on the first blocks, then a block in the middle of the
        //  volume is highlighted.
        view.StepZoomAt (100, { 1, 1 });
        start = view.GetZoom();
        least = start;

        model.HighlightBlocks ({ 820 });
        view.Refresh();
        view.StartFlight (0.0);
        Assert::IsTrue (view.IsFlying());

        for (ms = 0.0; view.StepFlight (ms); ms += 20.0)
        {
            least = std::min (least, view.GetZoom());
        }

        Assert::IsFalse (view.IsFlying());
        Assert::IsTrue  (least < start && least < view.GetZoom(), std::format (L"from {} out to {} and in to {}", start, least, view.GetZoom()).c_str());
        Assert::IsTrue  (view.GetTooltip ({ 200, 200 }, tip, anchor));
        Assert::IsTrue  (tip.starts_with (L"Block 820\n"), tip.c_str());

        //  A zoom ends a flight.
        view.StartFlight (ms);
        view.StepZoom (-1);
        Assert::IsFalse (view.IsFlying());
    }


    TEST_METHOD (Grid_EveryGapIsOnePixelAndEachPixelIsInTheCellDrawnThere)
    {
        BlockMapGrid  fit  (65535, 437, 311);
        BlockMapGrid  zoom (65535, fit.GetBlockColumns(), fit.GetBlockPx() * 5.0);



        for (const BlockMapGrid * grid : { &fit, &zoom })
        {
            int  x = 0;
            int  c = 0;

            for (c = 0; c + 1 < grid->GetColumns(); c++)
            {
                RECT  a = grid->GetCellRect ((uint32_t) c);
                RECT  b = grid->GetCellRect ((uint32_t) c + 1);

                Assert::AreEqual ((LONG) grid->GetGapPx(), b.left - a.right, std::format (L"the gap after column {}", c).c_str());
            }

            for (x = 0; x < grid->GetColumns() * (int) grid->GetPitch() - 2; x++)
            {
                int64_t  cell = grid->FindCellAt (x, 0);
                RECT     r    = grid->GetCellRect ((uint32_t) cell);

                Assert::IsTrue (cell >= 0 && x >= r.left && x <= r.right, std::format (L"pixel {} in cell {}", x, cell).c_str());
            }
        }
    }


    TEST_METHOD (MapView_FreeSpaceIsNotDimmedAndRunsAreLabeledWithTheirFile)
    {
        FakeDiskFileIo             io;
        BlockInspectorModel        model;
        BlockMapView               view;
        DxuiDpiScaler              scaler;
        DxuiDarkTheme              theme;
        MockDxuiPainter            painter;
        MockDxuiTextRenderer       text;
        BlockMapGrid               grid (k800KBlocks, 400, 400);
        POINT                      at   = grid.GetCellOrigin (grid.GetCellOf (1500));
        vector<DxuiTooltipSwatch>  plain;
        vector<DxuiTooltipSwatch>  dimmed;
        uint32_t                   seed = 0;



        OpenModel (io, model);

        view.SetModel    (&model);
        view.SetZoomable (true);
        view.Layout      (RECT { 0, 0, 400, 400 }, scaler);
        view.Refresh();

        Assert::IsTrue (view.GetTooltipSwatches ({ at.x + 1, at.y + 1 }, plain));

        model.HighlightBlocks ({ 2 });
        view.Refresh();
        Assert::IsTrue   (view.GetTooltipSwatches ({ at.x + 1, at.y + 1 }, dimmed));
        Assert::AreEqual (plain[0].argb, dimmed[0].argb, L"free space is not dimmed while other blocks are highlighted");

        //  At the closest zoom, the file's block has its name.
        while (seed < k800KBlocks && model.GetOwners (seed).find (L"SEED") == std::wstring::npos)
        {
            seed++;
        }

        Assert::IsTrue (seed < k800KBlocks);

        model.ClearHighlight();
        at = grid.GetCellOrigin (grid.GetCellOf (seed));
        view.StepZoomAt (100, { at.x + 1, at.y + 1 });
        view.Paint (painter, text, theme);

        Assert::IsTrue  (HasText (text, L"SEED"));
        Assert::IsFalse (HasText (text, L"1,500"), L"blocks are not labeled with their numbers");
    }


    TEST_METHOD (MapView_ZoomStepsEvenlyToALargeCellAndTipsAsTheFileMapGrid)
    {
        FakeDiskFileIo             io;
        BlockInspectorModel        model;
        BlockMapView               view;
        DxuiDpiScaler              scaler;
        std::wstring               tip;
        RECT                       anchor = {};
        vector<DxuiTooltipSwatch>  swatches;
        BlockMapGrid               grid (k800KBlocks, 400, 400);
        POINT                      at     = grid.GetCellOrigin (2);
        int                        steps  = 0;



        OpenModel (io, model);

        view.SetModel    (&model);
        view.SetZoomable (true);
        view.Layout      (RECT { 0, 0, 400, 400 }, scaler);
        view.Refresh();

        Assert::IsTrue (view.GetTooltip ({ at.x + 1, at.y + 1 }, tip, anchor));
        Assert::AreEqual (std::wstring (L"Block 2\nVolume header"), tip);
        Assert::IsTrue   (view.GetTooltipSwatches ({ at.x + 1, at.y + 1 }, swatches));
        Assert::AreEqual (1, swatches[0].line);

        //  After a zoom about block 2, block 2 is under the point.
        view.StepZoomAt (3, { at.x + 1, at.y + 1 });
        Assert::IsTrue (view.GetTooltip ({ at.x + 1, at.y + 1 }, tip, anchor));
        Assert::AreEqual (std::wstring (L"Block 2\nVolume header"), tip);
        view.Fit();

        while (!view.IsAtMost() && steps < 100)
        {
            double  before = view.GetZoom();

            view.StepZoom (1);
            Assert::IsTrue (view.GetZoom() / before < 1.3, L"each step zooms by about the same ratio");
            steps++;
        }

        Assert::IsTrue (steps >= 5, std::format (L"{} steps", steps).c_str());
    }


    TEST_METHOD (FieldsView_ACutOffRowHasItsWholeRectForTheTip)
    {
        DxuiDpiScaler         scaler;
        BlockFieldsView       view;
        MockDxuiTextRenderer  text;
        vector<BlockField>    fields (2);
        RECT                  box = {};



        fields[0].label = L"Data, block 173 of the file: X/LAST.GLADIATR.1, a name long enough to be cut off";
        fields[1].label = L"Next";

        view.SetFields (fields);
        view.Layout    (RECT { 0, 0, 160, 200 }, scaler);

        Assert::IsTrue  (view.GetClippedRowRect (0, text, box));
        Assert::IsTrue  (box.right > 160, L"the tip extends past the view's right edge");
        Assert::AreEqual (0L, box.left);
        Assert::IsFalse (view.GetClippedRowRect (1, text, box), L"a row that fits has no tip");
        Assert::AreEqual (0, view.FindRowAt ({ 10, scaler.ToPx ((int) BlockFieldsView::kRowDip) + 2 }));
    }


    TEST_METHOD (FieldsView_TheSameBlocksFieldsKeepTheRowsWhereTheyWere)
    {
        DxuiDpiScaler       scaler;
        BlockFieldsView     view;
        vector<BlockField>  fields (40);
        DxuiMouseEvent      ev;



        view.SetFields (fields);
        view.Layout    (RECT { 0, 0, 300, 200 }, scaler);

        ev.kind        = DxuiMouseEventKind::Wheel;
        ev.wheelDelta  = -2.0f;
        ev.positionDip = { 10, 100 };
        Assert::IsTrue (view.OnMouse (ev));
        Assert::IsTrue (view.GetTopRow() > 0);

        //  As marking the field under the pointer draws the block again.
        view.UpdateFields (fields);
        Assert::IsTrue (view.GetTopRow() > 0, L"the rows stay scrolled");

        view.SetFields (fields);
        Assert::AreEqual (0, view.GetTopRow(), L"a new block's fields start at the top");
    }


    TEST_METHOD (FieldsView_AClickOnALinkedFieldFollowsIt)
    {
        DxuiDpiScaler       scaler;
        BlockFieldsView     view;
        vector<BlockField>  fields (2);
        DxuiMouseEvent      ev;
        int64_t             followed = -1;
        float               row      = scaler.ToPxf (BlockFieldsView::kRowDip);



        fields[0].label = L"Plain";
        fields[1].label = L"Next directory block";
        fields[1].link  = 3;

        view.SetFields   (fields);
        view.SetOnFollow ([&] (const BlockField * field) { followed = field->link; });
        view.Layout      (RECT { 0, 0, 300, 200 }, scaler);

        ev.kind        = DxuiMouseEventKind::Down;
        ev.button      = DxuiMouseButton::Left;
        ev.positionDip = { 10, (LONG) (row * 1.5f) };
        view.OnMouse (ev);

        Assert::AreEqual ((int64_t) -1, followed, L"a field with no link isn't followed");

        ev.positionDip = { 10, (LONG) (row * 2.5f) };
        view.OnMouse (ev);

        Assert::AreEqual ((int64_t) 3, followed);
    }


    TEST_METHOD (HexView_TwoDigitsSetOneByteAndMoveTheCaret)
    {
        BlockHexView                                 hex;
        BlockHexSource                               source;
        std::array<Byte, kBlockBytes>                bytes  = {};
        vector<std::tuple<uint64_t, bool, wchar_t>>  typed;



        source.SetBytes (bytes);
        hex.SetSource   (&source);
        hex.SetOnType   ([&] (uint64_t offset, bool isHex, bool isHigh, wchar_t ch) { (void) isHex; typed.push_back ({ offset, isHigh, ch }); return true; });
        hex.GoToOffset  (10);

        Assert::IsTrue (hex.OnChar (L'A'));
        Assert::IsTrue (hex.OnChar (L'5'));

        Assert::AreEqual ((size_t) 2, typed.size());
        Assert::IsTrue (typed[0] == std::tuple<uint64_t, bool, wchar_t> (10, true, L'A'));
        Assert::IsTrue (typed[1] == std::tuple<uint64_t, bool, wchar_t> (10, false, L'5'));
        Assert::AreEqual ((uint64_t) 11, hex.GetCaret());
    }


    TEST_METHOD (Panel_ShowsTheBlockAndTypingEditsIt)
    {
        FakeDiskFileIo        io;
        BlockInspectorModel   model;
        BlockInspectorPanel   panel;
        DxuiDarkTheme         theme;
        DxuiDpiScaler         scaler;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;



        OpenModel (io, model);

        panel.Init   (&model, nullptr, &theme, nullptr, &text);
        panel.Layout (RECT { 0, 0, 1180, 700 }, scaler);
        panel.SelectBlock (2);
        panel.Paint  (painter, text, theme);

        Assert::IsTrue (HasText (text, L"Block 2"));
        Assert::IsTrue (HasText (text, L"Volume header"));

        panel.SelectBlock (300);
        panel.GetHexView()->GoToOffset (4);

        Assert::IsFalse (panel.GetHexView()->OnChar (L'C'), L"typing doesn't edit until Edit block");
        Assert::IsTrue  (model.GetEdits().IsEmpty());

        panel.SetEditing (true);

        //  As a window delivers a keystroke: the key-down, then the character.
        for (wchar_t ch : { L'C', L'3' })
        {
            (void) panel.GetHexView()->OnKey (DxuiKeyEvent { DxuiKeyEventKind::Down, (WPARAM) ch });
            Assert::IsTrue (panel.GetHexView()->OnKey (DxuiKeyEvent { DxuiKeyEventKind::Char, (WPARAM) ch }));
        }

        Assert::IsTrue (model.GetEdits().IsChanged (300, 4));
        Assert::AreEqual (1, model.GetEdits().CountChangedBytes(), L"both digits went into one byte");
    }


    TEST_METHOD (Panel_ABlockIsDrawnOnlyAfterTheWorkerLoadsIt)
    {
        FakeDiskFileIo        io;
        BlockInspectorModel   model;
        BlockInspectorWorker  worker;
        BlockInspectorPanel   panel;
        DxuiDarkTheme         theme;
        DxuiDpiScaler         scaler;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;



        OpenModel (io, model);

        panel.Init   (&model, &worker, &theme, nullptr, &text);
        panel.Layout (RECT { 0, 0, 1180, 700 }, scaler);

        worker.WaitForWork();
        worker.RunFinished();
        panel.SelectBlock (2);

        Assert::IsTrue  (panel.IsBusy(), L"block 2 is loaded by a job");
        Assert::IsFalse (panel.GetHexView()->OnChar (L'C'), L"typing is ignored while the job runs");

        worker.WaitForWork();
        Assert::IsTrue  (model.IsLoaded (2));

        text.Reset();
        panel.Paint (painter, text, theme);
        Assert::IsFalse (HasText (text, L"Block 2"), L"drawn only once the job's completion runs");

        worker.RunFinished();
        Assert::IsFalse (panel.IsBusy());

        text.Reset();
        panel.Paint (painter, text, theme);
        Assert::IsTrue  (HasText (text, L"Block 2"));
    }


    TEST_METHOD (Panel_TheWheelScrollsTheHexViewUnderThePointer)
    {
        FakeDiskFileIo        io;
        BlockInspectorModel   model;
        BlockInspectorPanel   panel;
        DxuiDarkTheme         theme;
        DxuiDpiScaler         scaler;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        DxuiMouseEvent        ev;
        RECT                  hex = {};



        OpenModel (io, model);

        panel.Init   (&model, nullptr, &theme, nullptr, &text);
        panel.Layout (RECT { 0, 0, 1180, 700 }, scaler);
        panel.SelectBlock (2);
        panel.Paint  (painter, text, theme);

        hex            = panel.GetHexView()->GetBounds();
        ev.kind        = DxuiMouseEventKind::Wheel;
        ev.wheelDelta  = -1.0f;
        ev.positionDip = { (hex.left + hex.right) / 2, (hex.top + hex.bottom) / 2 };

        Assert::IsTrue (panel.OnMouse (ev));
        Assert::IsTrue (panel.GetHexView()->GetTopRow() > 0, L"the hex view scrolled down");
    }


    TEST_METHOD (Panel_ALinkedByteHasATipThatSaysWhereItLeads)
    {
        FakeDiskFileIo             io;
        BlockInspectorModel        model;
        BlockInspectorPanel        panel;
        DxuiDarkTheme              theme;
        DxuiDpiScaler              scaler;
        MockDxuiPainter            painter;
        MockDxuiTextRenderer       text;
        RECT                       hex     = {};
        RECT                       anchor  = {};
        std::wstring               tip;
        vector<DxuiTooltipSwatch>  swatches;
        bool                       isFound = false;



        OpenModel (io, model);

        panel.Init   (&model, nullptr, &theme, nullptr, &text);
        panel.Layout (RECT { 0, 0, 1180, 700 }, scaler);
        panel.SelectBlock (2);
        panel.Paint  (painter, text, theme);

        hex = panel.GetHexView()->GetBounds();

        //  Block 2, the volume directory's key block, starts with links to
        //  the previous and next directory blocks.
        for (int x = hex.left; x < hex.right && !isFound; x += 2)
        {
            isFound = panel.GetHexTooltip ({ x, hex.top + 4 }, tip, anchor, swatches) && tip.find (L"Links to block") != std::wstring::npos;
        }

        Assert::IsTrue (isFound, tip.c_str());
        Assert::IsFalse (swatches.empty());
    }


    TEST_METHOD (Panel_SelectingARowDrawsItsFirstBlockBelowTheList)
    {
        FakeDiskFileIo        io;
        BlockInspectorModel   model;
        BlockInspectorPanel   panel;
        DxuiDarkTheme         theme;
        DxuiDpiScaler         scaler;
        MockDxuiTextRenderer  text;
        int                   row   = 0;



        OpenModel (io, model);

        panel.Init   (&model, nullptr, &theme, nullptr, &text);
        panel.Layout (RECT { 0, 0, 1180, 700 }, scaler);

        BlockNavigator  & nav = panel.GetNavigator();

        for (row = 0; nav.GetRow (row) != nullptr && nav.GetRow (row)->cells[0] != L"SEED"; row++)
        {
        }

        Assert::IsTrue (nav.GetRow (row) != nullptr);

        //  As a click on the row does.
        nav.SelectForBlock (nav.GetRow (row)->first);

        Assert::AreEqual (nav.GetRow (row)->first, model.GetBlock());
        Assert::IsTrue   (panel.GetHexView()->IsVisible() && nav.GetList().IsVisible());
    }


    TEST_METHOD (Panel_ADoubleClickOnTheMapOpensTheBlock)
    {
        FakeDiskFileIo        io;
        BlockInspectorModel   model;
        BlockInspectorPanel   panel;
        DxuiDarkTheme         theme;
        DxuiDpiScaler         scaler;
        MockDxuiTextRenderer  text;
        DxuiMouseEvent        ev;
        RECT                  map    = {};
        int                   click  = 0;



        OpenModel (io, model);

        panel.Init   (&model, nullptr, &theme, nullptr, &text);
        panel.Layout (RECT { 0, 0, 1180, 700 }, scaler);

        map            = panel.GetMap().GetBounds();
        ev.button      = DxuiMouseButton::Left;
        ev.positionDip = { (map.left + map.right) / 2, (map.top + map.bottom) / 2 };

        for (click = 0; click < 2; click++)
        {
            ev.kind = DxuiMouseEventKind::Down;
            panel.OnMouse (ev);
            ev.kind = DxuiMouseEventKind::Up;
            panel.OnMouse (ev);
        }

        Assert::IsFalse (panel.GetNavigator().IsBlockPlace(), L"the list stays at its place");
        Assert::IsTrue  (model.GetBlock() > 100, L"the block at the middle of the map is drawn below the list");
    }


    TEST_METHOD (Panel_ADragOnTheZoomedMapPansIt)
    {
        FakeDiskFileIo        io;
        BlockInspectorModel   model;
        BlockInspectorPanel   panel;
        DxuiDarkTheme         theme;
        DxuiDpiScaler         scaler;
        MockDxuiTextRenderer  text;
        DxuiMouseEvent        ev;
        RECT                  map    = {};
        POINT                 center = {};
        std::wstring          before;
        std::wstring          after;
        RECT                  anchor = {};



        OpenModel (io, model);

        panel.Init   (&model, nullptr, &theme, nullptr, &text);
        panel.Layout (RECT { 0, 0, 1180, 700 }, scaler);

        map    = panel.GetMap().GetBounds();
        center = { (map.left + map.right) / 2, (map.top + map.bottom) / 2 };
        panel.GetMap().StepZoomAt (8, center);
        Assert::IsTrue (panel.GetMap().GetTooltip (center, before, anchor));

        ev.button      = DxuiMouseButton::Left;
        ev.kind        = DxuiMouseEventKind::Down;
        ev.positionDip = center;
        panel.OnMouse (ev);

        ev.kind        = DxuiMouseEventKind::Move;
        ev.positionDip = { center.x - 150, center.y - 150 };
        panel.OnMouse (ev);

        ev.kind = DxuiMouseEventKind::Up;
        panel.OnMouse (ev);

        Assert::IsTrue     (panel.GetMap().GetTooltip (center, after, anchor));
        Assert::AreNotEqual (before, after, L"a drag moves other blocks under the point");
    }


    TEST_METHOD (Panel_PointingAtTheGridReportsTheCellAndTheWheelZoomsIt)
    {
        FakeDiskFileIo        io;
        BlockInspectorModel   model;
        BlockInspectorPanel   panel;
        DxuiDarkTheme         theme;
        DxuiDpiScaler         scaler;
        MockDxuiTextRenderer  text;
        DxuiMouseEvent        ev;
        std::wstring          hint;
        RECT                  map   = {};



        OpenModel (io, model);

        panel.Init      (&model, nullptr, &theme, nullptr, &text);
        panel.SetOnHint ([&] (const std::wstring & value) { hint = value; });
        panel.Layout    (RECT { 0, 0, 1180, 700 }, scaler);

        map            = panel.GetMap().GetBounds();
        ev.kind        = DxuiMouseEventKind::Move;
        ev.positionDip = { (map.left + map.right) / 2, (map.top + map.bottom) / 2 };
        panel.OnMouse (ev);

        Assert::IsTrue (hint.find (L"Block") == 0, hint.c_str());

        ev.kind       = DxuiMouseEventKind::Wheel;
        ev.wheelDelta = WHEEL_DELTA;
        Assert::IsTrue (panel.OnMouse (ev));
        Assert::IsFalse (panel.GetMap().IsAtFit(), L"the wheel over the grid zooms it");
    }



    TEST_METHOD (Panel_ParsesDecimalAndHexBlockNumbers)
    {
        uint32_t  block = 0;



        Assert::IsTrue  (BlockInspectorPanel::TryParseBlock (L" 123 ", block));
        Assert::AreEqual (123u, block);
        Assert::IsTrue  (BlockInspectorPanel::TryParseBlock (NumberFormat::FormatInteger (21820), block));
        Assert::AreEqual (21820u, block);
        Assert::IsTrue  (BlockInspectorPanel::TryParseBlock (L"$1F", block));
        Assert::AreEqual (31u, block);
        Assert::IsFalse (BlockInspectorPanel::TryParseBlock (L"12x", block));
        Assert::IsFalse (BlockInspectorPanel::TryParseBlock (L"", block));
        Assert::IsFalse (BlockInspectorPanel::TryParseBlock (L"-4", block));
    }



    TEST_METHOD (Panel_WithGermanSettings_WritesAndParsesBlockNumbersWithDots)
    {
        NumberSettings  english;
        NumberSettings  german;
        uint32_t        block    = 0;
        std::wstring    written;
        bool            isParsed = false;



        german.thousands = L".";
        german.decimal   = L",";

        NumberFormat::SetSettings (&german);
        written  = NumberFormat::FormatInteger (47790);
        isParsed = BlockInspectorPanel::TryParseBlock (L"47.790", block);
        NumberFormat::SetSettings (&english);

        Assert::AreEqual (std::wstring (L"47.790"), written);
        Assert::IsTrue   (isParsed, L"a Go to of 47.790 is a block number");
        Assert::AreEqual (47790u, block);
    }
};
