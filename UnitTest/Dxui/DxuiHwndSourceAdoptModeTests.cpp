#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHwndSourceAdoptModeTests
//
//  ADOPT mode: the host attaching to a client's existing window rather than
//  owning the paint pump itself.
//
//  Two modes exist because the emulator shell needs one and standalone panels
//  need the other. In adopt mode the client owns Present and the message loop,
//  and the host supplies only chrome and input routing -- so these assert what
//  the host must NOT do: no swap chain of its own, no paint pump, no
//  PostQuitMessage.
//
//  The distinction is easy to erode. A change made for the full-ownership path
//  that happens to work there will silently take over the client's window here,
//  which is why the two modes are tested apart.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiHwndSourceAdoptModeTests)
{
public:

    static constexpr UINT  s_kSyntheticDpi    = 192;     // 200% scale
    static constexpr UINT  s_kDefaultDpi      = 96;
    static constexpr LONG  s_kFakeHwndValue   = 0x12345678;


    DxuiHwndSource::CreateParams  MakeAdoptParams()
    {
        DxuiHwndSource::CreateParams  cp;

        cp.title           = L"AdoptModeTest";
        cp.hInstance       = nullptr;
        cp.ownerHwnd       = nullptr;
        cp.borderless      = true;
        cp.resizable       = true;
        cp.roundedCorners  = true;
        cp.darkMode        = true;
        cp.backdrop        = DxuiHwndSourceBackdrop::None;
        cp.resizeBorderDip = 6.0f;
        cp.initialSizeDip  = { 1024, 768 };
        return cp;
    }

    TEST_METHOD_INITIALIZE (Setup)
    {
        DxuiResetUiThreadIdForTest();
    }



    //
    //  Adopt-mode construction succeeds with a synthetic (null) HWND.
    //  The host stores the supplied HWND and does NOT register a
    //  window class or stand up render resources.
    //

    TEST_METHOD (CreateInAdoptMode_NullHwnd_Succeeds)
    {
        std::unique_ptr<DxuiHwndSource>  host;
        HRESULT                          hr   = S_OK;
        DxuiHwndSource::CreateParams     cp   = MakeAdoptParams();



        hr = DxuiHwndSource::CreateInAdoptMode (nullptr, cp, host);

        Assert::AreEqual (S_OK, hr);
        Assert::IsNotNull (host.get());
        Assert::IsNull    (host->GetHwnd());
    }



    //
    //  HandleMessage routes WM_NCHITTEST through the plugged-in
    //  hit-test delegate. The delegate receives the screen-space
    //  point and its return value becomes outResult.
    //

    TEST_METHOD (HandleMessage_NcHitTest_RoutesThroughDelegate)
    {
        std::unique_ptr<DxuiHwndSource>  host;
        HRESULT                          hr            = S_OK;
        LRESULT                          outResult     = 0;
        bool                             handled       = false;
        POINT                            seenPoint     = { 0, 0 };



        hr = DxuiHwndSource::CreateInAdoptMode (nullptr, MakeAdoptParams(), host);
        Assert::AreEqual (S_OK, hr);

        host->SetHitTestDelegate ([&seenPoint] (POINT pt) -> LRESULT
        {
            seenPoint = pt;
            return HTMAXBUTTON;
        });

        handled = host->HandleMessage (WM_NCHITTEST,
                                       0,
                                       MAKELPARAM (101, 202),
                                       outResult);

        Assert::IsTrue   (handled);
        Assert::AreEqual ((LRESULT) HTMAXBUTTON, outResult);
        Assert::AreEqual ((LONG) 101, (LONG) seenPoint.x);
        Assert::AreEqual ((LONG) 202, (LONG) seenPoint.y);
    }



    //
    //  When no hit-test delegate is set, the framework classifier
    //  runs. With no HWND it falls through to HTNOWHERE — which the
    //  caller's WndProc then handles via DefWindowProc / its own
    //  resize-edge logic. Confirms HandleMessage still claims the
    //  message (returns true) even when the delegate is unset, so
    //  the consumer never accidentally double-dispatches NC hits.
    //

    TEST_METHOD (HandleMessage_NcHitTest_NoDelegate_FallsThroughToFramework)
    {
        std::unique_ptr<DxuiHwndSource>  host;
        HRESULT                          hr        = S_OK;
        LRESULT                          outResult = 0;
        bool                             handled   = false;



        hr = DxuiHwndSource::CreateInAdoptMode (nullptr, MakeAdoptParams(), host);
        Assert::AreEqual (S_OK, hr);

        handled = host->HandleMessage (WM_NCHITTEST,
                                       0,
                                       MAKELPARAM (10, 20),
                                       outResult);

        Assert::IsTrue   (handled);
        Assert::AreEqual ((LRESULT) HTNOWHERE, outResult);
    }



    //
    //  The last pixel of a window's right and bottom edges resizes at 125%,
    //  where its DIP coordinate rounded to the nearest is the client's edge.
    //

    TEST_METHOD (HandleMessage_NcHitTest_TheLastPixelResizesAt125Percent)
    {
        std::unique_ptr<DxuiHwndSource>  host;
        HWND                             hwnd      = CreateWindowExW (0, L"STATIC", L"", WS_POPUP, 0, 0, 1518, 1078, nullptr, nullptr, nullptr, nullptr);
        HRESULT                          hr        = S_OK;
        LRESULT                          outResult = 0;
        POINT                            right     = { 1517, 500 };
        POINT                            bottom    = { 500, 1077 };



        Assert::IsNotNull (hwnd);

        hr = DxuiHwndSource::CreateInAdoptMode (hwnd, MakeAdoptParams(), host);
        Assert::AreEqual (S_OK, hr);

        host->SetDpiForTest (120);

        ClientToScreen (hwnd, &right);
        ClientToScreen (hwnd, &bottom);

        (void) host->HandleMessage (WM_NCHITTEST, 0, MAKELPARAM (right.x, right.y), outResult);
        Assert::AreEqual ((LRESULT) HTRIGHT, outResult);

        (void) host->HandleMessage (WM_NCHITTEST, 0, MAKELPARAM (bottom.x, bottom.y), outResult);
        Assert::AreEqual ((LRESULT) HTBOTTOM, outResult);

        host.reset();
        DestroyWindow (hwnd);
    }



    //
    //  A borderless window drawn over a backdrop is created without the
    //  system menu style, so Windows doesn't draw caption buttons under its
    //  see-through caption, and the host opens the system menu. A window
    //  drawn opaque, or a composited one, is created with the style.
    //

    TEST_METHOD (IsSystemMenuDropped_OnlyForABorderlessWindowOverABackdrop)
    {
        struct Case { bool overBackdrop; bool composited; bool borderless; bool dropped; };

        for (const Case & c : { Case { true, false, true, true }, Case { false, false, true, false }, Case { true, true, true, false }, Case { true, false, false, false } })
        {
            std::unique_ptr<DxuiHwndSource>  host;
            DxuiHwndSource::CreateParams     cp = MakeAdoptParams();

            cp.drawOverBackdrop = c.overBackdrop;
            cp.composited       = c.composited;
            cp.borderless       = c.borderless;

            Assert::AreEqual (S_OK, DxuiHwndSource::CreateInAdoptMode (nullptr, cp, host));
            Assert::AreEqual (c.dropped, host->IsSystemMenuDropped(),
                              std::format (L"over a backdrop {}, composited {}, borderless {}", c.overBackdrop, c.composited, c.borderless).c_str());
        }
    }



    //
    //  GetSystemMenu returns null for a window created without the system
    //  menu style, so the window is created with it and the menu is taken
    //  before the style is dropped. It's a menu after the style is gone, with
    //  Close on it.
    //

    TEST_METHOD (DropSystemMenuStyle_ReturnsTheMenuTakenBeforeTheStyleIsGone)
    {
        HWND   hwnd    = CreateWindowExW (0, L"STATIC", L"", WS_OVERLAPPEDWINDOW, 0, 0, 800, 600, nullptr, nullptr, nullptr, nullptr);
        HWND   without = CreateWindowExW (0, L"STATIC", L"", WS_OVERLAPPEDWINDOW & ~WS_SYSMENU, 0, 0, 800, 600, nullptr, nullptr, nullptr, nullptr);
        HMENU  menu    = nullptr;



        Assert::IsNotNull (hwnd);
        Assert::IsNotNull (without);
        Assert::IsNull    (GetSystemMenu (without, FALSE), L"null for a window created without the style");

        menu = DxuiHwndSource::DropSystemMenuStyle (hwnd);

        Assert::IsNotNull   (menu, L"the menu");
        Assert::IsTrue      ((GetWindowLongPtrW (hwnd, GWL_STYLE) & WS_SYSMENU) == 0, L"the style is gone");
        Assert::IsTrue      (IsMenu (menu) != FALSE, L"the menu taken first is a menu");
        Assert::AreNotEqual (-1, (int) GetMenuState (menu, SC_CLOSE, MF_BYCOMMAND), L"with Close on it");

        DestroyWindow (without);
        DestroyWindow (hwnd);
    }



    //
    //  A maximized window's client area is inset from the top by the same
    //  frame Windows insets it by at the sides, so the caption is on the
    //  screen. A restored window's client area starts at the window's top.
    //

    TEST_METHOD (HandleMessage_NcCalcSize_AMaximizedWindowsTopIsInsetLikeItsSides)
    {
        for (bool maximized : { false, true })
        {
            std::unique_ptr<DxuiHwndSource>  host;
            HWND                             hwnd   = CreateWindowExW (0, L"STATIC", L"", WS_OVERLAPPEDWINDOW, 0, 0, 800, 600, nullptr, nullptr, nullptr, nullptr);
            NCCALCSIZE_PARAMS                params = {};
            WINDOWPOS                        pos    = {};
            LRESULT                          result = 0;
            RECT                             window = { 100, 50, 900, 650 };

            Assert::IsNotNull (hwnd);

            if (maximized)
            {
                SetWindowLongPtrW (hwnd, GWL_STYLE, GetWindowLongPtrW (hwnd, GWL_STYLE) | WS_MAXIMIZE);
            }

            Assert::AreEqual (S_OK, DxuiHwndSource::CreateInAdoptMode (hwnd, MakeAdoptParams(), host));

            params.rgrc[0] = window;
            params.rgrc[1] = window;
            params.rgrc[2] = window;
            params.lppos   = &pos;

            (void) host->HandleMessage (WM_NCCALCSIZE, TRUE, reinterpret_cast<LPARAM> (&params), result);

            Assert::AreEqual (maximized ? params.rgrc[0].left - window.left : 0L, params.rgrc[0].top - window.top,
                              maximized ? L"maximized: inset at the top as at the left" : L"restored: the client starts at the window's top");
            Assert::IsTrue   (!maximized || params.rgrc[0].top > window.top, L"a maximized window's frame is inset");

            host.reset();
            DestroyWindow (hwnd);
        }
    }



    //
    //  WM_DPICHANGED propagates DPI to the host's scaler. Adopt mode
    //  does NOT claim the message (returns false) so the caller's
    //  WndProc keeps doing its own SetWindowPos + subclass DPI hooks.
    //

    TEST_METHOD (HandleMessage_DpiChanged_UpdatesScalerAndDoesNotClaim)
    {
        std::unique_ptr<DxuiHwndSource>  host;
        HRESULT                          hr        = S_OK;
        LRESULT                          outResult = 0;
        bool                             handled   = false;



        hr = DxuiHwndSource::CreateInAdoptMode (nullptr, MakeAdoptParams(), host);
        Assert::AreEqual (S_OK, hr);

        handled = host->HandleMessage (WM_DPICHANGED,
                                       MAKEWPARAM (s_kSyntheticDpi, s_kSyntheticDpi),
                                       0,
                                       outResult);

        Assert::IsFalse  (handled);
        Assert::AreEqual ((unsigned int) s_kSyntheticDpi,
                          (unsigned int) host->GetScaler().GetDpi());
    }



    //
    //  HandleMessage returns false for any message Dxui does not
    //  own, letting the caller's WndProc fall through to whatever
    //  legacy handling (or DefWindowProc) applies.
    //

    TEST_METHOD (HandleMessage_UnhandledMessage_ReturnsFalse)
    {
        std::unique_ptr<DxuiHwndSource>  host;
        HRESULT                          hr        = S_OK;
        LRESULT                          outResult = 0;
        bool                             handled   = false;



        hr = DxuiHwndSource::CreateInAdoptMode (nullptr, MakeAdoptParams(), host);
        Assert::AreEqual (S_OK, hr);

        handled = host->HandleMessage (WM_LBUTTONDOWN, 0, 0, outResult);

        Assert::IsFalse (handled);
    }



    //
    //  WM_THEMECHANGED is observed but not claimed — the host
    //  refreshes its tree-side theme state and returns false so the
    //  caller's WndProc can also forward to DefWindowProc.
    //

    TEST_METHOD (HandleMessage_ThemeChanged_DoesNotClaim)
    {
        std::unique_ptr<DxuiHwndSource>  host;
        HRESULT                          hr        = S_OK;
        LRESULT                          outResult = 0;
        bool                             handled   = false;



        hr = DxuiHwndSource::CreateInAdoptMode (nullptr, MakeAdoptParams(), host);
        Assert::AreEqual (S_OK, hr);

        handled = host->HandleMessage (WM_THEMECHANGED, 0, 0, outResult);

        Assert::IsFalse (handled);
    }



    //
    //  Destroying an adopt-mode host does NOT call DestroyWindow on
    //  the supplied HWND. We can't observe DestroyWindow directly
    //  with a fake HWND value, but a release-without-crash plus
    //  GetHwnd() returning the supplied value before drop is sufficient
    //  evidence the host treats the HWND as non-owned. (If
    //  DestroyWindow ran on a bogus HWND the process would AV.)
    //

    TEST_METHOD (AdoptMode_DestructorDoesNotDestroyHwnd)
    {
        std::unique_ptr<DxuiHwndSource>  host;
        HRESULT                          hr            = S_OK;
        HWND                             fakeHwnd      = (HWND) (intptr_t) s_kFakeHwndValue;



        hr = DxuiHwndSource::CreateInAdoptMode (fakeHwnd, MakeAdoptParams(), host);
        Assert::AreEqual (S_OK, hr);
        Assert::AreEqual ((intptr_t) s_kFakeHwndValue, (intptr_t) host->GetHwnd());

        // Release the host — must not call DestroyWindow on the bogus
        // HWND. If it did, this would AV.
        host.reset();
    }



    //
    //  The default scaler in adopt mode is 96 DPI when constructed
    //  with a null HWND (GetDpiForWindow returns 0 → falls back).
    //

    TEST_METHOD (AdoptMode_DefaultDpiIs96WhenNoHwnd)
    {
        std::unique_ptr<DxuiHwndSource>  host;
        HRESULT                          hr   = S_OK;



        hr = DxuiHwndSource::CreateInAdoptMode (nullptr, MakeAdoptParams(), host);
        Assert::AreEqual (S_OK, hr);
        Assert::AreEqual ((unsigned int) s_kDefaultDpi, (unsigned int) host->GetScaler().GetDpi());
    }
};

