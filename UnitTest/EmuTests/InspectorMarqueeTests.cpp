#include "Pch.h"

#include "../Dxui/MockDxuiPainter.h"
#include "Ui/DiskInspector/InspectorMarquee.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorMarqueeTests
//
//  The selected file's marquee (FR-090): dashes alternate between its two
//  colors, a polyline's next segment continues the pattern, the dashes march
//  with time, and a dimmed color lies halfway to the background.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (InspectorMarqueeTests)
{
public:

    TEST_METHOD (DashesAlternateBetweenTheTwoColors)
    {
        DiskInspectorPalette  palette = DiskInspectorPalette::MakeFallback (true);
        InspectorMarquee      marquee (palette, 4.0f, 2.0f, 0);
        MockDxuiPainter       painter;
        float                 end     = marquee.DrawLine (painter, 0.0f, 10.0f, 20.0f, 10.0f, 0.0f);



        Assert::AreEqual (20.0f, end);
        Assert::AreEqual (static_cast<size_t> (5), painter.Calls().size());

        for (size_t i = 0; i < painter.Calls().size(); i++)
        {
            Assert::AreEqual (4.0f * i, painter.Calls()[i].x);
            Assert::AreEqual (4.0f,     painter.Calls()[i].width);
            Assert::AreEqual ((i % 2 == 0) ? palette.colors.selection : DiskInspectorPalette::GetTextColorOn (palette.colors.selection), painter.Calls()[i].argb);
        }

        Assert::AreNotEqual (painter.Calls()[0].argb, painter.Calls()[1].argb);
    }



    TEST_METHOD (TheNextSegmentContinuesThePattern)
    {
        DiskInspectorPalette  palette = DiskInspectorPalette::MakeFallback (true);
        InspectorMarquee      marquee (palette, 4.0f, 2.0f, 0);
        MockDxuiPainter       painter;
        float                 s       = marquee.DrawLine (painter, 0.0f, 0.0f, 6.0f, 0.0f, 0.0f);



        marquee.DrawLine (painter, 6.0f, 0.0f, 6.0f, 6.0f, s);

        //  The dash from 4 to 8 runs around the corner in one color.
        Assert::AreEqual (static_cast<size_t> (4), painter.Calls().size());
        Assert::AreEqual (2.0f, painter.Calls()[1].width);
        Assert::AreEqual (2.0f, painter.Calls()[2].height);
        Assert::AreEqual (painter.Calls()[1].argb, painter.Calls()[2].argb);
        Assert::AreNotEqual (painter.Calls()[2].argb, painter.Calls()[3].argb);
    }



    TEST_METHOD (TheDashesMarchWithTime)
    {
        DiskInspectorPalette  palette = DiskInspectorPalette::MakeFallback (true);
        InspectorMarquee      later (palette, 4.0f, 2.0f, InspectorMarquee::kStepMs);
        MockDxuiPainter       painter;



        later.DrawLine (painter, 0.0f, 0.0f, 20.0f, 0.0f, 0.0f);

        Assert::AreEqual (3.0f, painter.Calls()[0].width, L"a step later, the first dash has moved on a pixel");
    }



    TEST_METHOD (ADimmedColorLiesHalfwayToTheBackground)
    {
        DiskInspectorPalette  palette = DiskInspectorPalette::MakeFallback (true);
        uint32_t              red     = 0;



        palette.background = 0xFF000000;
        red                = (InspectorMarquee::Dim (palette, 0xFFFFFFFF) >> 16) & 0xFF;

        Assert::IsTrue (red == 0x7F || red == 0x80, std::to_wstring (red).c_str());
    }
};
