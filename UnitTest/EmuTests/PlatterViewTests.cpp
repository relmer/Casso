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
};
