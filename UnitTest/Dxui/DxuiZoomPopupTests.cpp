#include "Pch.h"

#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiZoomPopupTests
//
//  The zoom popup: the button at rest in the corner, the panel above it on
//  hover with the percentage where the button has it, the slider across the
//  host's range, Reset, the wheel, and the label of a deep zoom.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiZoomPopupTests)
{
public:

    static constexpr RECT  kCorner = { 0, 0, 400, 300 };



    static DxuiZoomPopup MakePopup()
    {
        DxuiZoomPopup  popup;
        DxuiDpiScaler  scaler;



        popup.Place (kCorner, kCorner, scaler);
        return popup;
    }



    static void OpenFully (DxuiZoomPopup & popup)
    {
        RECT  button = popup.GetButtonRect();



        popup.OnHover ({ button.left + 1, button.top + 1 });
        popup.Tick (0);
        popup.Tick (DxuiZoomPopup::kAnimateMs * 2);
    }



    static const RecordedTextCall * FindString (const MockDxuiTextRenderer & text, const std::wstring & label)
    {
        const RecordedTextCall  * found = nullptr;



        for (const RecordedTextCall & call : text.Calls())
        {
            found = (found == nullptr && call.kind == RecordedTextKind::DrawString && call.text == label) ? &call : found;
        }

        return found;
    }



    static DxuiMouseEvent MakeEvent (DxuiMouseEventKind kind, POINT point)
    {
        DxuiMouseEvent  ev = {};



        ev.kind        = kind;
        ev.button      = DxuiMouseButton::Left;
        ev.positionDip = point;
        return ev;
    }



    TEST_METHOD (AtRestTheButtonSitsAMarginInFromTheCorner)
    {
        DxuiZoomPopup  popup  = MakePopup();
        RECT           button = popup.GetButtonRect();



        Assert::AreEqual (400L - DxuiZoomPopup::kMarginDip,                                button.right);
        Assert::AreEqual (300L - DxuiZoomPopup::kMarginDip,                                button.bottom);
        Assert::AreEqual ((long) DxuiZoomPopup::kButtonWidthDip,                           button.right - button.left);
        Assert::AreEqual ((long) DxuiZoomPopup::kButtonHeightDip,                          button.bottom - button.top);
        Assert::IsFalse  (popup.IsOpen());
        Assert::IsTrue   (popup.GetPanelRect().right == popup.GetPanelRect().left, L"no panel at rest");
    }



    TEST_METHOD (TheButtonDrawsTheZoomAsAPercentage)
    {
        DxuiZoomPopup         popup = MakePopup();
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;



        popup.SetRange (0.5, 2.0, false);
        popup.SetZoom  (1.0);
        popup.Paint    (text, theme);

        Assert::IsNotNull (FindString (text, L"Zoom 100%"));
        Assert::IsNull    (FindString (text, L"Reset"), L"Reset only on the open panel");
    }



    TEST_METHOD (HoverOpensThePanelAboveWithThePercentageInPlace)
    {
        DxuiZoomPopup             popup  = MakePopup();
        MockDxuiTextRenderer      closed;
        MockDxuiTextRenderer      open;
        MockDxuiTheme             theme;
        RECT                      button = popup.GetButtonRect();
        RECT                      panel  = {};
        const RecordedTextCall  * before = nullptr;
        const RecordedTextCall  * after  = nullptr;



        popup.Paint (closed, theme);
        OpenFully   (popup);
        popup.Paint (open, theme);

        panel  = popup.GetPanelRect();
        before = FindString (closed, L"Zoom 100%");
        after  = FindString (open,   L"Zoom 100%");

        Assert::IsTrue    (popup.IsOpen());
        Assert::IsTrue    (panel.top < button.top, L"the panel grows up");
        Assert::AreEqual  (button.right,  panel.right);
        Assert::AreEqual  (button.bottom, panel.bottom);
        Assert::IsNotNull (before);
        Assert::IsNotNull (after);
        Assert::AreEqual  (before->x, after->x);
        Assert::AreEqual  (before->y, after->y);
        Assert::IsNotNull (FindString (open, L"Reset"));
    }



    TEST_METHOD (MovingAwayClosesThePanelButNearbyDoesNot)
    {
        DxuiZoomPopup  popup = MakePopup();
        RECT           panel = {};



        OpenFully (popup);
        panel = popup.GetPanelRect();

        popup.OnHover ({ panel.left - DxuiZoomPopup::kNearDip / 2, panel.top });
        Assert::IsTrue (popup.IsOpen(), L"within the near margin");

        popup.OnHover ({ 10, 10 });
        Assert::IsFalse (popup.IsOpen(), L"far away");
    }



    TEST_METHOD (TheSliderRunsAcrossTheHostsRange)
    {
        DxuiZoomPopup  popup = MakePopup();
        RECT           track = {};



        popup.SetRange (1.0, 64.0, true);
        OpenFully (popup);
        track = popup.GetTrackRect();

        Assert::AreEqual (1.0,  popup.GetZoomAt ({ track.left,      track.top }), 1e-6);
        Assert::AreEqual (64.0, popup.GetZoomAt ({ track.right + 5, track.top }), 1e-6);
        Assert::AreEqual (8.0,  popup.GetZoomAtPosition (0.5f), 1e-6, L"logarithmic: halfway is the geometric middle");
        Assert::AreEqual (0.5f, popup.GetPosition (8.0), 1e-6f);

        popup.SetRange (0.5, 2.0, false);
        Assert::AreEqual (1.25, popup.GetZoomAtPosition (0.5f), 1e-6, L"linear: halfway is the middle");
    }



    TEST_METHOD (APressOnTheTrackDragsTheZoomAndResetReturnsToTheHostsDefault)
    {
        DxuiZoomPopup  popup  = MakePopup();
        double         zoomed = 0.0;
        RECT           track  = {};
        RECT           reset  = {};



        popup.SetRange     (0.5, 2.0, false);
        popup.SetResetZoom (1.0);
        popup.SetOnZoom    ([&zoomed] (double zoom) { zoomed = zoom; });
        OpenFully (popup);

        track = popup.GetTrackRect();
        reset = popup.GetResetRect();

        Assert::IsTrue   (popup.OnMouse (MakeEvent (DxuiMouseEventKind::Down, { track.right - 1, track.top + 1 })));
        Assert::IsTrue   (popup.IsDragging());
        Assert::AreEqual (2.0, zoomed, 1e-6);

        popup.OnMouse    (MakeEvent (DxuiMouseEventKind::Move, { track.left, track.top + 1 }));
        Assert::AreEqual (0.5, zoomed, 1e-6, L"the drag follows the pointer");

        popup.OnMouse    (MakeEvent (DxuiMouseEventKind::Up, { track.left, track.top + 1 }));
        Assert::IsFalse  (popup.IsDragging());

        popup.OnMouse    (MakeEvent (DxuiMouseEventKind::Down, { reset.left + 1, reset.top + 1 }));
        Assert::AreEqual (1.0, zoomed, 1e-6);
    }



    TEST_METHOD (TheWheelOverTheButtonStepsByTheHostsStep)
    {
        DxuiZoomPopup   popup  = MakePopup();
        double          zoomed = 0.0;
        RECT            button = popup.GetButtonRect();
        DxuiMouseEvent  wheel  = MakeEvent (DxuiMouseEventKind::Wheel, { button.left + 1, button.top + 1 });



        popup.SetRange  (1.0, 64.0, true);
        popup.SetZoom   (2.0);
        popup.SetOnStep ([] (double zoom, int steps) { return zoom * (steps > 0 ? 4.0 : 0.25); });
        popup.SetOnZoom ([&zoomed] (double zoom) { zoomed = zoom; });

        wheel.wheelDelta = 1.0f;

        Assert::IsTrue   (popup.OnMouse (wheel));
        Assert::AreEqual (8.0, zoomed, 1e-6);

        wheel.positionDip = { 10, 10 };
        Assert::IsFalse  (popup.OnMouse (wheel), L"not away from the popup");
    }



    TEST_METHOD (ADeepZoomReadsAsAFactor)
    {
        Assert::AreEqual (std::wstring (L"Zoom 100%"),             DxuiZoomPopup::GetPercentLabel (1.0));
        Assert::AreEqual (std::wstring (L"Zoom 67%"),              DxuiZoomPopup::GetPercentLabel (2.0 / 3.0));
        Assert::AreEqual (std::wstring (L"Zoom 9999%"),            DxuiZoomPopup::GetPercentLabel (99.99));
        Assert::AreEqual (std::wstring (L"Zoom 4096") + L"\x00D7", DxuiZoomPopup::GetPercentLabel (4096.0));
    }
};
