#include "Pch.h"

#include "Theme/DxuiDarkTheme.h"
#include "Theme/DxuiLightTheme.h"
#include "Widgets/DxuiDockSite.h"
#include "Window/DxuiDockedWindow.h"
#include "MockDxuiControl.h"
#include "MockDxuiPainter.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDragMarkVisibilityTests
//
//  What a drag shows has to be seen: every drop target square carries a
//  picture in the accent color that stands out from the page under it, and
//  the fade over a dragged pane's header row lowers that row's alpha rather
//  than darkening it.
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
        }
    };



    static bool Inside (const RECT & inner, const RECT & outer)
    {
        return inner.left >= outer.left && inner.top >= outer.top && inner.right <= outer.right && inner.bottom <= outer.bottom &&
               inner.right > inner.left && inner.bottom > inner.top;
    }



    static void CheckEverySquareHasAnAccentPicture (const IDxuiTheme & theme)
    {
        Rig                            rig;
        std::vector<DxuiDockDragMark>  marks   = rig.site.GetDragMarks (theme);
        size_t                         squares = 0;



        for (const DxuiDockDragMark & square : marks)
        {
            bool  pictured = false;

            if (square.outlinePx != 0 || square.argb != theme.BackgroundElevated())
            {
                continue;
            }

            squares++;

            for (const DxuiDockDragMark & mark : marks)
            {
                pictured = pictured || (mark.argb == theme.Accent() && Inside (mark.rect, square.rect));
            }

            Assert::IsTrue (pictured, L"a drop target square shows an accent picture");
        }

        Assert::IsTrue (squares >= 9, L"the drag shows the edge squares and at least one compass");
    }



    TEST_CLASS (DxuiDragMarkVisibilityTests)
    {
    public:

        TEST_METHOD (EveryDropTargetSquareShowsAnAccentPictureInTheDarkTheme)
        {
            DxuiDarkTheme  theme;



            CheckEverySquareHasAnAccentPicture (theme);
        }


        TEST_METHOD (EveryDropTargetSquareShowsAnAccentPictureInTheLightTheme)
        {
            DxuiLightTheme  theme;



            CheckEverySquareHasAnAccentPicture (theme);
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
