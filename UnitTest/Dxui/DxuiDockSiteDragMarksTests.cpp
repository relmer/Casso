#include "Pch.h"

#include "MockDxuiControl.h"
#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockSiteDragMarksTests
//
//  The drop targets of a drag as a list of marks, so a window can draw them
//  in an overlay above its floating windows, and the overlay's rendering of
//  them.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiDockSiteDragMarksTests
{
    struct Rig
    {
        DxuiDockSite          site;
        MockDxuiControl       code;
        MockDxuiControl       regs;
        MockDxuiControl       console;
        DxuiDpiScaler         scaler;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;

        Rig()
        {
            DxuiPaneLayout  layout = DxuiPaneLayout::MakeSingle (L"code");

            layout.Add        (L"regs",    L"");
            layout.Add        (L"console", L"");
            layout.DockToSide (L"console", L"code", DxuiDockSide::Bottom);

            site.AddPane       (L"code",    L"Disassembly", &code);
            site.AddPane       (L"regs",    L"Registers",   &regs);
            site.AddPane       (L"console", L"Console",     &console);
            site.SetPaneLayout (layout);
            site.Layout        (RECT { 0, 0, 1000, 600 }, scaler);
        }

        size_t CountPaintCalls()
        {
            painter.Reset();
            site.Paint (painter, text, theme);
            return painter.Calls().size();
        }
    };



    TEST_CLASS (DxuiDockSiteDragMarksTests)
    {
    public:

        TEST_METHOD (MarksListEveryTargetOnlyDuringADrag)
        {
            Rig  rig;



            Assert::IsTrue (rig.site.GetDragMarks (rig.theme).empty(), L"no drag, no marks");

            rig.site.BeginDrag (L"console");

            std::vector<DxuiDockDragMark>  marks    = rig.site.GetDragMarks (rig.theme);
            size_t                         outlines = 0;
            size_t                         dotted   = 0;

            for (const DxuiDockDragMark & mark : marks)
            {
                outlines += (mark.outlinePx > 0 && !mark.dotted) ? 1 : 0;
                dotted   += mark.dotted ? 1 : 0;
            }

            Assert::IsTrue   (outlines > 0, L"each target square is outlined");
            Assert::AreEqual (outlines * 2 + dotted, marks.size(), L"each target is a fill and an outline, and a split target adds its dotted picture");

            rig.site.CancelDrag();
            Assert::IsTrue (rig.site.GetDragMarks (rig.theme).empty(), L"the marks go when the drag ends");
        }


        //  A window drawing the marks in an overlay keeps the site from
        //  painting them a second time, under its floating windows.
        TEST_METHOD (PaintLeavesOutMarksDrawnElsewhere)
        {
            Rig     rig;
            size_t  resting = rig.CountPaintCalls();
            size_t  marks   = 0;



            rig.site.BeginDrag (L"console");
            for (const DxuiDockDragMark & mark : rig.site.GetDragMarks (rig.theme))
            {
                marks += mark.dotted ? DxuiDockSite::GetOutlineStrips (mark).size() : 1;
            }

            Assert::AreEqual (resting + marks, rig.CountPaintCalls(), L"the site paints the marks itself");

            rig.site.SetDragMarksDrawnElsewhere (true);
            Assert::AreEqual (resting, rig.CountPaintCalls(), L"the overlay paints them instead");
        }


        TEST_METHOD (RenderMarksBlendsFillsAndOutlinesInPremultipliedPixels)
        {
            std::vector<uint32_t>          pixels (10 * 10, 0xDEADBEEFu);
            std::vector<DxuiDockDragMark>  marks;



            marks.push_back ({ RECT { 0, 0, 4, 4 },  0x80FF0000u, 0 });
            marks.push_back ({ RECT { 4, 4, 10, 10 }, 0xFF00FF00u, 1 });

            DxuiDragOverlay::RenderMarks (marks, 10, 10, pixels.data());

            Assert::AreEqual (0x80800000u, pixels[0],          L"half-transparent red, premultiplied");
            Assert::AreEqual (0u,          pixels[5],          L"the rest is cleared to transparent");
            Assert::AreEqual (0xFF00FF00u, pixels[4 * 10 + 4], L"the outline's corner");
            Assert::AreEqual (0xFF00FF00u, pixels[9 * 10 + 9], L"the far corner");
            Assert::AreEqual (0u,          pixels[6 * 10 + 6], L"an outline leaves its inside clear");
        }
    };
}
