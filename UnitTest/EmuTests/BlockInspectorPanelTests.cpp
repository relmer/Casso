#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "FakeDiskFileIo.h"
#include "Devices/Disk/FilePath.h"
#include "Devices/Disk/Inspector/Blocks/BlockInspectorModel.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Ui/DiskInspector/Blocks/BlockFieldsView.h"
#include "Ui/DiskInspector/Blocks/BlockHexSource.h"
#include "Ui/DiskInspector/Blocks/BlockHexView.h"
#include "Ui/DiskInspector/Blocks/BlockInspectorPanel.h"
#include "Ui/DiskInspector/Blocks/BlockMapGrid.h"
#include "Ui/DiskInspector/Blocks/BlockMapView.h"

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
        POINT         at       = floppy.GetCellOrigin (17);



        Assert::AreEqual ((uint32_t) 1, floppy.GetBlocksPerCell());
        Assert::IsTrue (floppy.GetCellPx() > BlockMapGrid::kMinCellPx);
        Assert::IsTrue ((uint64_t) floppy.GetColumns() * floppy.GetRows() >= 280);

        Assert::IsTrue (hardDisk.GetBlocksPerCell() > 1);
        Assert::IsTrue (hardDisk.GetRows() * (hardDisk.GetCellPx() + BlockMapGrid::kGapPx) <= 300 + BlockMapGrid::kGapPx);
        Assert::AreEqual (65534u, hardDisk.GetLastBlock (hardDisk.GetCellCount() - 1));

        Assert::AreEqual ((int64_t) 17, floppy.FindCellAt (at.x + 1, at.y + 1));
        Assert::AreEqual ((int64_t) -1, floppy.FindCellAt (-1, 0));
    }


    TEST_METHOD (Grid_PicksTheMoreNotableRole)
    {
        Assert::IsTrue (BlockMapGrid::PickRole (SectorRole::FileData, SectorRole::CrossLinked) == SectorRole::CrossLinked);
        Assert::IsTrue (BlockMapGrid::PickRole (SectorRole::IndexBlock, SectorRole::Free) == SectorRole::IndexBlock);
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

        panel.Init   (&model, &theme, nullptr, &text);
        panel.Layout (RECT { 0, 0, 1180, 700 }, scaler);
        panel.SelectBlock (2);
        panel.Paint  (painter, text, theme);

        Assert::IsTrue (HasText (text, L"Block 2: Volume header"));
        Assert::IsTrue (HasText (text, L"of 1600"));

        panel.SelectBlock (300);
        panel.GetHexView()->GoToOffset (4);

        Assert::IsTrue (panel.GetHexView()->OnChar (L'C'));
        Assert::IsTrue (panel.GetHexView()->OnChar (L'3'));
        Assert::IsTrue (model.GetEdits().IsChanged (300, 4));

        text.Reset();
        panel.Paint (painter, text, theme);

        Assert::IsTrue (HasText (text, L"1 byte changed, not written"));
    }


    TEST_METHOD (Panel_ParsesDecimalAndHexBlockNumbers)
    {
        uint32_t  block = 0;



        Assert::IsTrue  (BlockInspectorPanel::TryParseBlock (L" 123 ", block));
        Assert::AreEqual (123u, block);
        Assert::IsTrue  (BlockInspectorPanel::TryParseBlock (L"$1F", block));
        Assert::AreEqual (31u, block);
        Assert::IsFalse (BlockInspectorPanel::TryParseBlock (L"12x", block));
        Assert::IsFalse (BlockInspectorPanel::TryParseBlock (L"", block));
        Assert::IsFalse (BlockInspectorPanel::TryParseBlock (L"-4", block));
    }
};
