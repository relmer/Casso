#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;



namespace
{
    constexpr UINT  s_kSyntheticDpi    = 192;     // 200% scale
    constexpr UINT  s_kDefaultDpi      = 96;
    constexpr LONG  s_kFakeHwndValue   = 0x12345678;


    DxuiHwndSource::CreateParams  MakeAdoptParams ()
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
}





TEST_CLASS (DxuiHwndSourceAdoptModeTests)
{
public:

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
        Assert::IsNull    (host->Hwnd());
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
                          (unsigned int) host->Scaler().Dpi());
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
    //  Hwnd() returning the supplied value before drop is sufficient
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
        Assert::AreEqual ((intptr_t) s_kFakeHwndValue, (intptr_t) host->Hwnd());

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
        Assert::AreEqual ((unsigned int) s_kDefaultDpi, (unsigned int) host->Scaler().Dpi());
    }
};
