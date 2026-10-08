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
            site.Paint              (painter, text, theme);
            site.PaintAfterSiblings (painter, text, theme);
            return painter.Calls().size();
        }
    };



    //  A drag of the console held over the middle of the code's group, so
    //  the drag shows the edge guides, that group's cross, and the shade of
    //  the cross's center button.
    static void HoverTheCodesCross (Rig & rig)
    {
        DxuiMouseEvent  ev;



        for (size_t i = 0; i < rig.site.GetGroupCount(); i++)
        {
            if (rig.site.GetGroup (i)->IndexOf (&rig.code) >= 0)
            {
                ev.positionDip = POINT { (rig.site.GetGroup (i)->GetBounds().left + rig.site.GetGroup (i)->GetBounds().right) / 2,
                                         (rig.site.GetGroup (i)->GetBounds().top + rig.site.GetGroup (i)->GetBounds().bottom) / 2 };
            }
        }

        ev.kind   = DxuiMouseEventKind::Move;
        ev.button = DxuiMouseButton::Left;

        rig.site.BeginDrag (L"console");
        rig.site.OnMouse   (ev);
    }



    //  How many painter calls the marks take, and how many pictures.
    static void CountMarks (Rig & rig, size_t & fills, size_t & images)
    {
        fills  = 0;
        images = 0;

        for (const DxuiDockDragMark & mark : rig.site.GetDragMarks (rig.theme))
        {
            images += (mark.image != nullptr) ? 1 : 0;
            fills  += (mark.image != nullptr) ? 0 : mark.dotted ? DxuiDockSite::GetOutlineStrips (mark).size() : 1;
        }
    }



    TEST_CLASS (DxuiDockSiteDragMarksTests)
    {
    public:

        TEST_METHOD (MarksListEveryTargetOnlyDuringADrag)
        {
            Rig                            rig;
            std::vector<DxuiDockDragMark>  marks;
            size_t                         guides = 0;



            Assert::IsTrue (rig.site.GetDragMarks (rig.theme).empty(), L"no drag, no marks");

            HoverTheCodesCross (rig);
            marks = rig.site.GetDragMarks (rig.theme);

            for (const DxuiDockDragMark & mark : marks)
            {
                if (mark.image == nullptr)
                {
                    continue;
                }

                guides++;
                Assert::AreEqual ((long) mark.image->width,  mark.rect.right - mark.rect.left, L"a guide's mark bounds its picture");
                Assert::AreEqual ((long) mark.image->height, mark.rect.bottom - mark.rect.top);
            }

            Assert::IsNotNull (rig.site.GetHoveredZone(), L"the middle of the group is its cross's center button");
            Assert::AreEqual  ((size_t) 5, guides, L"a guide at each edge and the cross of the group under the pointer");
            Assert::AreEqual  (guides + 1, marks.size(), L"and the shade of the hovered target, under them");
            Assert::IsTrue    (marks.front().image == nullptr);

            rig.site.CancelDrag();
            Assert::IsTrue (rig.site.GetDragMarks (rig.theme).empty(), L"the marks go when the drag ends");
        }


        //  A window drawing the marks in an overlay keeps the site from
        //  painting them a second time, under its floating windows.
        TEST_METHOD (PaintLeavesOutMarksDrawnElsewhere)
        {
            Rig     rig;
            size_t  resting = rig.CountPaintCalls();
            size_t  fills   = 0;
            size_t  images  = 0;



            HoverTheCodesCross (rig);
            CountMarks (rig, fills, images);

            Assert::IsTrue (fills > 0 && images > 0, L"a drag shows a shade and guides");

            rig.text.Reset();
            Assert::AreEqual (resting + fills, rig.CountPaintCalls(),        L"the site paints the marks itself");
            Assert::AreEqual (images,          rig.text.IconCalls().size(), L"the guides as pictures");

            rig.site.SetDragMarksDrawnElsewhere (true);
            rig.text.Reset();
            Assert::AreEqual (resting, rig.CountPaintCalls(), L"the overlay paints them instead");
            Assert::IsTrue   (rig.text.IconCalls().empty());
        }


        //  The marks lie over every pane control, whichever order the site and
        //  the panes paint in, so the site draws them in its after pass, after
        //  the groups' frames, and its own Paint leaves them out. The guides
        //  go through the text pass, after every fill, so they lie over the
        //  shade too.
        TEST_METHOD (MarksArePaintedAfterTheSiblings)
        {
            Rig     rig;
            size_t  resting = rig.CountPaintCalls();
            size_t  frames  = 0;
            size_t  fills   = 0;
            size_t  images  = 0;



            rig.painter.Reset();
            rig.site.PaintAfterSiblings (rig.painter, rig.text, rig.theme);
            frames = rig.painter.Calls().size();

            HoverTheCodesCross (rig);
            CountMarks (rig, fills, images);

            Assert::IsTrue (fills > 0 && images > 0, L"a drag shows marks");

            Assert::IsTrue (frames > 0, L"at rest the after pass draws the frames");

            rig.painter.Reset();
            rig.text.Reset();
            rig.site.Paint (rig.painter, rig.text, rig.theme);
            Assert::AreEqual (resting - frames, rig.painter.Calls().size(), L"Paint draws the site as it rests");
            Assert::IsTrue   (rig.text.IconCalls().empty());

            rig.painter.Reset();
            rig.text.Reset();
            rig.site.PaintAfterSiblings (rig.painter, rig.text, rig.theme);
            Assert::AreEqual (frames + fills, rig.painter.Calls().size(),   L"the after pass draws the frames and the marks");
            Assert::AreEqual (images,         rig.text.IconCalls().size(), L"and the guides");
        }


        TEST_METHOD (RenderMarksBlendsPicturesInPremultipliedPixels)
        {
            std::vector<uint32_t>           pixels (4 * 4, 0xDEADBEEFu);
            std::vector<DxuiDockDragMark>   marks;
            std::shared_ptr<DxuiIconImage>  image = std::make_shared<DxuiIconImage>();
            DxuiDockDragMark                guide;



            image->width      = 2;
            image->height     = 2;
            image->bgraPremul = { 0xFF0000FFu, 0x80800000u, 0u, 0x40404040u };

            marks.push_back ({ RECT { 0, 0, 4, 4 }, 0xFF00FF00u, 0 });

            guide.rect  = RECT { 1, 1, 3, 3 };
            guide.image = image;
            marks.push_back (guide);

            guide.rect = RECT { 3, 3, 5, 5 };
            marks.push_back (guide);

            DxuiDragOverlay::RenderMarks (marks, 4, 4, pixels.data());

            Assert::AreEqual (0xFF00FF00u, pixels[0],          L"outside the picture");
            Assert::AreEqual (0xFF0000FFu, pixels[1 * 4 + 1], L"an opaque picture pixel covers");
            Assert::AreEqual (0xFF807F00u, pixels[1 * 4 + 2], L"half red over green");
            Assert::AreEqual (0xFF00FF00u, pixels[2 * 4 + 1], L"a clear picture pixel leaves what is there");
            Assert::AreEqual (0xFF0000FFu, pixels[3 * 4 + 3], L"a picture past the edge is clipped to it");
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
