#include "Pch.h"

#include "MockDxuiControl.h"
#include "MockDxuiPainter.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDragMarkVisibilityTests
//
//  What a drag shows has to be seen, in Visual Studio's colors: every guide
//  draws its border, its buttons and their pictures in the theme's
//  DockGuide colors rather than in the accent, and the fade over a dragged
//  pane's header row lowers that row's alpha rather than darkening it.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiDragMarkVisibilityTests
{
    class EraseProbe : public MockDxuiPainter
    {
    public:
        void  SetErase (bool erase) override { m_erase = erase; }
        bool  IsErasing() const              { return m_erase; }

    private:
        bool  m_erase = false;
    };



    //  A drag of the console from outside the site, so the guides show with
    //  no button lit: the four edge guides, and the documents' cross once the
    //  pointer is over their group.
    struct Rig
    {
        DxuiDockSite     site;
        MockDxuiControl  code;
        MockDxuiControl  regs;
        MockDxuiControl  console;
        DxuiDpiScaler    scaler;

        Rig()
        {
            DxuiPaneLayout  layout = DxuiPaneLayout::MakeSingle (L"code");
            DxuiMouseEvent  ev;

            layout.Add        (L"regs",    L"");
            layout.Add        (L"console", L"");
            layout.DockToSide (L"console", L"code", DxuiDockSide::Bottom);

            site.AddPane       (L"code",    L"Disassembly", &code);
            site.AddPane       (L"regs",    L"Registers",   &regs);
            site.AddPane       (L"console", L"Console",     &console);
            site.SetDocumentFn ([] (const std::wstring & pane) { return pane == L"code"; });
            site.SetPaneLayout (layout);
            site.Layout (RECT { 0, 0, 1000, 600 }, scaler);
            site.BeginDrag (L"console");

            ev.kind        = DxuiMouseEventKind::Move;
            ev.positionDip = POINT { 20, 20 };
            site.OnMouse (ev);
        }
    };



    static uint32_t GetPixel (const DxuiDockDragMark & mark, const RECT & rect, int x, int y)
    {
        return mark.image->bgraPremul[(size_t) (y - rect.top) * (size_t) mark.image->width + (size_t) (x - rect.left)];
    }



    //  The dock button of the window edge a guide sits against, in the rig's
    //  site of 1000 by 600.
    static DxuiDockGuideButton GetEdgeButton (const RECT & rect)
    {
        POINT  center = { (rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2 };



        return (center.x < 100) ? DxuiDockGuideButton::DockLeft
             : (center.x > 900) ? DxuiDockGuideButton::DockRight
             : (center.y < 100) ? DxuiDockGuideButton::DockTop
                                : DxuiDockGuideButton::DockBottom;
    }



    //  Each guide's button: inside its border and outside its picture, the
    //  button's fill; on its picture's left edge, the picture's color. The
    //  border at the middle of the guide's top edge is the guide's border.
    static void CheckEveryGuideUsesTheDockGuideColors (const IDxuiTheme & theme)
    {
        Rig                            rig;
        std::vector<DxuiDockDragMark>  marks  = rig.site.GetDragMarks (theme);
        size_t                         guides = 0;
        uint32_t                       accent = theme.Accent() | 0xFF000000u;



        for (const DxuiDockDragMark & mark : marks)
        {
            SIZE                              size    = {};
            DxuiDockGuideKind                 kind    = DxuiDockGuideKind::Edge;
            std::vector<DxuiDockGuideButton>  buttons;
            POINT                             origin  = { mark.rect.left, mark.rect.top };

            if (mark.image == nullptr)
            {
                continue;
            }

            guides++;
            size    = { mark.rect.right - mark.rect.left, mark.rect.bottom - mark.rect.top };
            kind    = (size.cx == DxuiDockGuide::GetSizePx (DxuiDockGuideKind::LargeCross, rig.scaler).cx) ? DxuiDockGuideKind::LargeCross
                    : (size.cx == DxuiDockGuide::GetSizePx (DxuiDockGuideKind::SmallCross, rig.scaler).cx) ? DxuiDockGuideKind::SmallCross
                                                                                                              : DxuiDockGuideKind::Edge;
            buttons = (kind == DxuiDockGuideKind::Edge) ? std::vector<DxuiDockGuideButton> { GetEdgeButton (mark.rect) }
                                                        : DxuiDockGuide::GetButtons (kind, DxuiDockSide::Left);

            Assert::AreEqual (theme.DockGuideBorder(), GetPixel (mark, mark.rect, (mark.rect.left + mark.rect.right) / 2, mark.rect.top), L"the guide's border");

            for (DxuiDockGuideButton button : buttons)
            {
                RECT  rect  = DxuiDockGuide::GetButtonRect (kind, button, origin, rig.scaler);
                RECT  glyph = DxuiDockGuide::GetGlyphRect  (kind, button, rig.scaler);
                int   midY  = (int) (origin.y + (glyph.top + glyph.bottom) / 2);

                Assert::AreEqual (theme.DockGuideButtonFill(), GetPixel (mark, mark.rect, rect.left + 2, (rect.top + rect.bottom) / 2), L"a button's fill");
                Assert::AreEqual (theme.DockGuideGlyph(),      GetPixel (mark, mark.rect, origin.x + glyph.left, midY),                L"its picture");
            }

            for (uint32_t pixel : mark.image->bgraPremul)
            {
                Assert::AreNotEqual (accent, pixel, L"no part of a guide is drawn in the accent");
            }
        }

        Assert::AreEqual ((size_t) 5, guides, L"the four edge guides and the documents' cross");
    }



    TEST_CLASS (DxuiDragMarkVisibilityTests)
    {
    public:

        TEST_METHOD (EveryGuideUsesTheDockGuideColorsInTheDarkTheme)
        {
            DxuiDarkTheme  theme;



            CheckEveryGuideUsesTheDockGuideColors (theme);
        }


        TEST_METHOD (EveryGuideUsesTheDockGuideColorsInTheLightTheme)
        {
            DxuiLightTheme  theme;



            CheckEveryGuideUsesTheDockGuideColors (theme);
        }


        TEST_METHOD (TheHeaderFadeDrawsInEraseMode)
        {
            EraseProbe  painter;



            DxuiDockedWindow::PaintHeaderFade (painter, RECT { 0, 0, 400, 30 });

            Assert::IsTrue  (painter.IsErasing());
            Assert::IsFalse (painter.Calls().empty());
        }
    };
}
