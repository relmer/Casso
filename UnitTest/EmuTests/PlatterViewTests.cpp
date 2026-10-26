#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "Devices/Disk/Inspector/DiskAnalyzer.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Ui/DiskInspector/PlatterView.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterViewTests
//
//  What the platter view paints over the rings, through the mock painter:
//  the "Alignment" overlay (FR-031) marks each whole track's sector 0 and
//  longest sync run, and only while it is turned on.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (PlatterViewTests)
{
public:

    static int CountCircles (const MockDxuiPainter & painter, uint32_t argb)
    {
        return static_cast<int> (std::count_if (painter.Calls().begin(), painter.Calls().end(), [argb] (const RecordedPaintCall & c)
        {
            return c.kind == RecordedPaintKind::FillCircle && c.argb == argb;
        }));
    }



    TEST_METHOD (TheAlignmentOverlayMarksEachWholeTrack)
    {
        vector<Byte>          sectors (NibblizationLayer::kImageByteSize, 0);
        DiskImage             image;
        DiskAnalysis          analysis;
        InspectorViewModel    model;
        InspectorViewContext  context;
        DxuiTheme             theme    = DxuiTheme::Dark();
        DxuiDpiScaler         scaler;
        MockDxuiPainter       hidden;
        MockDxuiPainter       shown;
        MockDxuiTextRenderer  text;



        AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, image));
        DiskAnalyzer::Analyze (DiskCopy::MakeFromImage (image, 1, "test", 0, false), DecodeSettings::MakeStandard(), analysis);
        model.SetDisk (1);
        model.SetAnalysis (&analysis);

        context.analysis = &analysis;
        context.model    = &model;
        context.hasDisk  = true;

        PlatterView  view (context);

        view.Layout ({ 0, 0, 600, 600 }, scaler);

        view.Paint (hidden, text, theme);
        Assert::AreEqual (0, CountCircles (hidden, theme.Accent()), L"no marks while the overlay is off");

        view.SetAlignmentShown (true);
        view.Paint (shown, text, theme);
        Assert::AreEqual (35, CountCircles (shown, theme.Accent()),          L"a sector 0 mark on each of the 35 whole tracks");
        Assert::AreEqual (35, CountCircles (shown, theme.ForegroundMuted()), L"a longest-sync mark on each");
    }


    //  The head marker is drawn over the platter but takes no input: a click
    //  on its bullseye selects the head's track and the sector under the
    //  head, with the platter turned as the drive turns it (FR-064).
    TEST_METHOD (AClickOnTheHeadsBullseyeSelectsTheCellUnderIt)
    {
        static constexpr int     kHeadTrack = 20 * 4;
        static constexpr double  kHeadTurn  = 0.3;
        static constexpr int     kSide      = 6000;

        vector<Byte>          sectors (NibblizationLayer::kImageByteSize, 0);
        DiskImage             image;
        DiskAnalysis          analysis;
        InspectorViewModel    model;
        InspectorViewContext  context;
        DxuiDpiScaler         scaler;
        InspectorHead         head;
        POINT                 eye      = {};
        PlatterHit            hit;
        DxuiMouseEvent        ev;
        int                   sector   = -1;



        AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, image));
        DiskAnalyzer::Analyze (DiskCopy::MakeFromImage (image, 1, "test", 0, false), DecodeSettings::MakeStandard(), analysis);
        model.SetDisk (1);
        model.SetAnalysis (&analysis);
        model.SetRotation (-kHeadTurn);

        context.analysis = &analysis;
        context.model    = &model;
        context.hasDisk  = true;

        PlatterView  view (context);

        //  Large enough that each quarter track's ring is several pixels wide.
        view.Layout ({ 0, 0, kSide, kSide }, scaler);

        head.isShown      = true;
        head.quarterTrack = kHeadTrack;
        head.turn         = kHeadTurn;
        head.activity     = HeadActivity::Reading;
        context.head = head;

        Assert::IsTrue (view.GetHeadPoint (eye));
        Assert::AreEqual (static_cast<LONG> (kSide / 2), eye.x, L"the head is at 12 o'clock");
        Assert::IsTrue   (eye.y < kSide / 2);

        hit = view.HitTest (eye);
        Assert::IsTrue   (hit.isOnDisk);
        Assert::AreEqual (kHeadTrack, hit.quarterTrack, L"the bullseye is on the head's quarter track");
        Assert::AreEqual (kHeadTurn, hit.turn, 0.005, L"and at the head's turn");

        const TrackAnalysis &  track = *analysis.tracks[analysis.entries[kHeadTrack].slot];

        sector = PlatterGeometry::GetSectorAt (track, PlatterGeometry::GetCellAtTurn (track, hit.turn));
        Assert::IsTrue (sector >= 0, L"a sector lies under the head");

        ev.button      = DxuiMouseButton::Left;
        ev.positionDip = eye;
        ev.kind        = DxuiMouseEventKind::Down;
        Assert::IsTrue (view.OnMouse (ev));
        ev.kind        = DxuiMouseEventKind::Up;
        Assert::IsTrue (view.OnMouse (ev));

        Assert::AreEqual (kHeadTrack, model.GetQuarterTrack());
        Assert::AreEqual (sector, model.GetSectorIndex());
    }
};
