#include "Pch.h"

#include "MockDxuiControl.h"
#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockDragLayerTests
//
//  A drag's drop preview lies over everything in a pane, its pictures
//  included, as Visual Studio's does. A flush draws all of its fills, then
//  all of its text and pictures, so a shade filled in the page's flush lay
//  under a pane's picture, which kept the heat map's map untinted. The
//  window paints the marks in a drag layer, a flush of their own after the
//  page's: its fills cover every picture the page drew.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiDockDragLayerTests
{
    ////////////////////////////////////////////////////////////////////////////
    //
    //  PictureControl
    //
    //  A pane that is one picture over its whole area, as the heat map's map
    //  is: drawn through the text pass, as every picture is.
    //
    ////////////////////////////////////////////////////////////////////////////

    class PictureControl : public MockDxuiControl
    {
    public:
        void  Paint (IDxuiPainter      & /*painter*/,
                     IDxuiTextRenderer & text,
                     const IDxuiTheme  & /*theme*/) override
        {
            HRESULT  hr     = S_OK;
            RECT     bounds = GetBounds();



            hr = text.DrawFramebuffer (m_pixels.data(), kSide, kSide, (float) bounds.left, (float) bounds.top,
                                       (float) (bounds.right - bounds.left), (float) (bounds.bottom - bounds.top));
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

    private:
        static constexpr int  kSide = 2;

        std::vector<uint32_t>  m_pixels = std::vector<uint32_t> ((size_t) (kSide * kSide), 0xFF00FF00u);
    };





    ////////////////////////////////////////////////////////////////////////////
    //
    //  LayerTestWindow
    //
    //  A floating window whose creation and layers a test can run, as its
    //  host's paint pump would, without an HWND.
    //
    ////////////////////////////////////////////////////////////////////////////

    class LayerTestWindow : public DxuiDockedWindow
    {
    public:
        using DxuiDockedWindow::OnCreate;
        using DxuiDockedWindow::OnKey;
        using DxuiDockedWindow::HasDragLayer;
        using DxuiDockedWindow::PaintDragLayer;
    };





    //  One fill+text flush: its painter's fills land first, then its text
    //  renderer's text and pictures.
    struct Flush
    {
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
    };



    //  The map over the console, both panes of the window, which holds their
    //  controls as an application's floating window does.
    struct Rig
    {
        PictureControl   map;
        MockDxuiControl  console;
        LayerTestWindow  window;
        DxuiDpiScaler    scaler;
        MockDxuiTheme    theme;

        Rig()
        {
            DxuiPaneLayout  layout = DxuiPaneLayout::MakeSingle (L"map");

            layout.Add        (L"console", L"");
            layout.DockToSide (L"console", L"map", DxuiDockSide::Bottom);

            window.OnCreate();
            window.Adopt (map);
            window.Adopt (console);
            window.GetSite().AddPane       (L"map",     L"Heat map", &map);
            window.GetSite().AddPane       (L"console", L"Console",  &console);
            window.GetSite().SetPaneLayout (layout);
            window.GetSite().Layout        (RECT { 0, 0, 1000, 600 }, scaler);
        }

        //  The console dragged over the middle of the map's pane, the center
        //  button of its cross, so the shade covers the map.
        bool TryDragOverTheMap()
        {
            DxuiMouseEvent  ev;
            RECT            area = map.GetBounds();



            ev.kind        = DxuiMouseEventKind::Move;
            ev.button      = DxuiMouseButton::Left;
            ev.positionDip = POINT { (area.left + area.right) / 2, (area.top + area.bottom) / 2 };

            window.GetSite().BeginDrag (L"console");
            (void) window.GetSite().OnMouse (ev);

            return window.GetSite().GetHoveredZone() != nullptr;
        }

        //  The fills of a flush in the theme's drop preview color.
        std::vector<RecordedPaintCall> GetShades (const Flush & flush) const
        {
            std::vector<RecordedPaintCall>  shades;

            for (const RecordedPaintCall & call : flush.painter.Calls())
            {
                if (call.kind == RecordedPaintKind::FillRect && call.argb == theme.DockPreview())
                {
                    shades.push_back (call);
                }
            }

            return shades;
        }
    };



    static bool IsCovered (const RecordedTextCall & picture, const RecordedPaintCall & shade)
    {
        return shade.x <= picture.x && shade.y <= picture.y &&
               shade.x + shade.width >= picture.x + picture.width && shade.y + shade.height >= picture.y + picture.height;
    }



    TEST_CLASS (DxuiDockDragLayerTests)
    {
    public:

        //  The page's flush draws the map's picture and no shade; the drag
        //  layer, flushed after it, fills the shade over the whole picture,
        //  and draws the guides over that.
        TEST_METHOD (TheShadeIsFlushedAfterThePanesPicture)
        {
            Rig                               rig;
            Flush                             page;
            Flush                             layer;
            std::vector<RecordedPaintCall>    shades;
            const RecordedTextCall          * picture = nullptr;
            bool                              covered = false;



            Assert::IsFalse (rig.window.HasDragLayer(),  L"no drag, no drag layer");
            Assert::IsTrue  (rig.TryDragOverTheMap(),    L"the middle of the map's pane is its cross's center button");
            Assert::IsTrue  (rig.window.HasDragLayer(),  L"a drag flushes a drag layer");

            rig.window.Paint (page.painter, page.text, rig.theme);

            for (const RecordedTextCall & call : page.text.Calls())
            {
                picture = (call.kind == RecordedTextKind::DrawFramebuffer) ? &call : picture;
            }

            if (picture == nullptr)
            {
                Assert::Fail (L"the page draws the map's picture");
                return;
            }

            Assert::IsTrue (rig.GetShades (page).empty(),  L"the page fills no shade, which its picture would cover");
            Assert::IsTrue (page.text.IconCalls().empty(), L"nor any guide");

            rig.window.PaintDragLayer (layer.painter, layer.text, rig.theme);
            shades = rig.GetShades (layer);

            for (const RecordedPaintCall & shade : shades)
            {
                covered = covered || IsCovered (*picture, shade);
            }

            Assert::IsTrue   (covered,                                   L"the drag layer's shade covers the whole picture");
            Assert::AreEqual ((size_t) 5, layer.text.IconCalls().size(), L"and its guides, the edges' and the map's cross, lie over the shade");
        }


        //  Escape in the window cancels the drag, and the drag layer goes.
        TEST_METHOD (EscapeInTheWindowCancelsItsSitesDrag)
        {
            Rig             rig;
            DxuiKeyEvent    escape;
            DxuiTabGroup  * group = nullptr;
            RECT            tab   = {};
            DxuiMouseEvent  ev;



            for (size_t i = 0; i < rig.window.GetSite().GetGroupCount(); i++)
            {
                group = (rig.window.GetSite().GetGroup (i)->IndexOf (&rig.console) >= 0) ? rig.window.GetSite().GetGroup (i) : group;
            }

            if (group == nullptr)
            {
                Assert::Fail (L"the rig has a console group");
                return;
            }

            //  The console's tab dragged off its strip: a floating window's
            //  site keeps the drag rather than tearing the pane off.
            tab            = group->GetTabRect (0);
            ev.kind        = DxuiMouseEventKind::Down;
            ev.button      = DxuiMouseButton::Left;
            ev.positionDip = POINT { (tab.left + tab.right) / 2, (tab.top + tab.bottom) / 2 };
            (void) rig.window.GetSite().OnMouse (ev);

            ev.kind        = DxuiMouseEventKind::Move;
            ev.positionDip = POINT { ev.positionDip.x, ev.positionDip.y + 200 };
            (void) rig.window.GetSite().OnMouse (ev);

            Assert::IsTrue (rig.window.GetSite().HasPointerDrag(), L"the tab's drag is the pointer's");

            escape.kind = DxuiKeyEventKind::Down;
            escape.vk   = VK_ESCAPE;

            Assert::IsTrue  (rig.window.OnKey (escape),           L"the window takes Escape");
            Assert::IsFalse (rig.window.GetSite().IsDragging(),   L"and cancels the drag");
            Assert::IsFalse (rig.window.HasDragLayer(),           L"so no drag layer is left");
        }
    };
}
