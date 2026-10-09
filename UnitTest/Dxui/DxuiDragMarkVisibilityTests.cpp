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



    //  A drag of the console with the pointer in a corner of the documents'
    //  group, clear of every button, so the guides show with no button lit:
    //  the four edge guides and the documents' cross.
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



    //  The window edge a guide sits against, in the rig's site of 1000 by
    //  600.
    static DxuiDockSide GetEdge (const RECT & rect)
    {
        POINT  center = { (rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2 };



        return (center.x < 100) ? DxuiDockSide::Left
             : (center.x > 900) ? DxuiDockSide::Right
             : (center.y < 100) ? DxuiDockSide::Top
                                : DxuiDockSide::Bottom;
    }



    //  Each guide is the picture DxuiDockGuide draws from the theme's
    //  DockGuide colors, with no button lit, so every button shows at 70%
    //  and each edge guide fades whole; the cross's own border, which never
    //  fades, is the theme's. No part of any guide is in the accent.
    static void CheckEveryGuideUsesTheDockGuideColors (const IDxuiTheme & theme)
    {
        Rig                            rig;
        std::vector<DxuiDockDragMark>  marks  = rig.site.GetDragMarks (theme);
        size_t                         guides = 0;
        uint32_t                       accent = theme.Accent() | 0xFF000000u;
        DxuiDockGuideColors            colors;



        colors.border       = theme.DockGuideBorder();
        colors.fill         = theme.DockGuideFill();
        colors.buttonBorder = theme.DockGuideButtonBorder();
        colors.buttonFill   = theme.DockGuideButtonFill();
        colors.glyph        = theme.DockGuideGlyph();
        colors.arrow        = theme.DockGuideArrow();

        for (const DxuiDockDragMark & mark : marks)
        {
            long               width = mark.rect.right - mark.rect.left;
            DxuiDockGuideKind  kind  = DxuiDockGuideKind::Edge;
            DxuiDockSide       edge  = DxuiDockSide::Left;
            DxuiIconImage      drawn;

            if (mark.image == nullptr)
            {
                continue;
            }

            guides++;
            kind  = (width == DxuiDockGuide::GetSizePx (DxuiDockGuideKind::LargeCross, rig.scaler).cx) ? DxuiDockGuideKind::LargeCross
                  : (width == DxuiDockGuide::GetSizePx (DxuiDockGuideKind::SmallCross, rig.scaler).cx) ? DxuiDockGuideKind::SmallCross
                                                                                                        : DxuiDockGuideKind::Edge;
            edge  = (kind == DxuiDockGuideKind::Edge) ? GetEdge (mark.rect) : DxuiDockSide::Left;
            drawn = DxuiDockGuide::Render (kind, edge, -1, colors, rig.scaler);

            Assert::IsTrue (drawn.bgraPremul == mark.image->bgraPremul, L"the guide is drawn from the theme's DockGuide colors");

            if (kind != DxuiDockGuideKind::Edge)
            {
                Assert::AreEqual (theme.DockGuideBorder(), mark.image->bgraPremul[(size_t) (width / 2)], L"the cross's border");
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
