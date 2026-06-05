#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;



////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHostWindowClientTests
//
//  Verifies the IDxuiHostClient dispatch wiring on DxuiHostWindow.
//  Uses adopt-mode hosts (which carry a null HWND) plus direct
//  WndProc invocation; the tests only exercise paths where the
//  client claims the message (returns DxuiMessageResult::Handled,
//  or a non-default LRESULT for rich-return hooks) so the host
//  returns without ever reaching DefWindowProc against the null
//  HWND.
//
////////////////////////////////////////////////////////////////////////////////


namespace
{
    DxuiHostWindow::CreateParams  MakeAdoptParams()
    {
        DxuiHostWindow::CreateParams  cp;

        cp.title           = L"ClientDispatchTest";
        cp.backdrop        = DxuiHostWindowBackdrop::None;
        return cp;
    }



    //
    //  Recording client — every dispatched method bumps a counter
    //  and (for the calls that carry payload) stashes the most
    //  recent argument values so the test can assert them.
    //
    class RecordingClient : public IDxuiHostClient
    {
    public:
        DxuiMessageResult  claim                = DxuiMessageResult::Handled;
        LRESULT            onCreateReturn       = 0;
        LRESULT            onNcHitTestReturn    = HTCAPTION;
        LRESULT            onCtlColorReturn     = 0;
        LRESULT            onDrawItemReturn     = TRUE;

        int      onCreateCount         = 0;
        int      onNcHitTestCount      = 0;
        int      onCtlColorCount       = 0;
        HDC      lastCtlColorHdc       = nullptr;
        HWND     lastCtlColorHwnd      = nullptr;

        int      onDrawItemCount       = 0;
        WPARAM   lastDrawItemWParam    = 0;
        LPARAM   lastDrawItemLParam    = 0;

        int      onCharCount           = 0;
        WPARAM   lastCharCh            = 0;

        int      onCommandCount        = 0;
        WORD     lastCommandId         = 0;
        WORD     lastNotifyCode        = 0;

        int      onKeyDownCount        = 0;
        WPARAM   lastKeyDownVk         = 0;

        int      onKeyUpCount          = 0;
        int      onMouseMoveCount      = 0;
        int      onMouseLeaveCount     = 0;
        int      onLButtonDownCount    = 0;
        int      onLButtonUpCount      = 0;
        int      onMoveCount           = 0;
        int      onSizeCount           = 0;
        UINT     lastSizeWidth         = 0;
        UINT     lastSizeHeight        = 0;

        int      onTimerCount          = 0;
        UINT_PTR lastTimerId           = 0;

        int      onNotifyCount         = 0;
        int      onInitMenuPopupCount  = 0;
        int      onPaintCount          = 0;
        int      onCloseCount          = 0;
        int      onDestroyCount        = 0;
        int      onNcLButtonUpCount    = 0;
        LRESULT  lastNcHitTest         = HTNOWHERE;

        LRESULT            OnCreate         (HWND, UINT, WPARAM, LPARAM)        override { ++onCreateCount;                                                            return onCreateReturn;    }
        LRESULT            OnNcHitTest      (HWND, UINT, WPARAM, LPARAM)        override { ++onNcHitTestCount;                                                         return onNcHitTestReturn; }
        LRESULT            OnCtlColorStatic (HDC hdc, HWND hCtl)                override { ++onCtlColorCount; lastCtlColorHdc = hdc; lastCtlColorHwnd = hCtl;          return onCtlColorReturn;  }
        LRESULT            OnDrawItem       (HWND, UINT, WPARAM wp, LPARAM lp)  override { ++onDrawItemCount; lastDrawItemWParam = wp; lastDrawItemLParam = lp;        return onDrawItemReturn;  }

        DxuiMessageResult  OnChar           (WPARAM ch, LPARAM)                 override { ++onCharCount;          lastCharCh = ch;                                    return claim; }
        DxuiMessageResult  OnCommandEx      (WORD id, WORD notify, HWND)        override { ++onCommandCount;       lastCommandId = id; lastNotifyCode = notify;        return claim; }
        DxuiMessageResult  OnKeyDown        (WPARAM vk, LPARAM)                 override { ++onKeyDownCount;       lastKeyDownVk = vk;                                 return claim; }
        DxuiMessageResult  OnKeyUp          (WPARAM, LPARAM)                    override { ++onKeyUpCount;         return claim; }
        DxuiMessageResult  OnMouseMove      (WPARAM, LPARAM)                    override { ++onMouseMoveCount;     return claim; }
        DxuiMessageResult  OnMouseLeave     ()                                  override { ++onMouseLeaveCount;    return claim; }
        DxuiMessageResult  OnLButtonDown    (WPARAM, LPARAM)                    override { ++onLButtonDownCount;   return claim; }
        DxuiMessageResult  OnLButtonUp      (WPARAM, LPARAM)                    override { ++onLButtonUpCount;     return claim; }
        DxuiMessageResult  OnMove           (int, int)                          override { ++onMoveCount;          return claim; }
        DxuiMessageResult  OnSize           (UINT w, UINT h)                    override { ++onSizeCount; lastSizeWidth = w; lastSizeHeight = h;                       return claim; }
        DxuiMessageResult  OnTimer          (UINT_PTR id)                       override { ++onTimerCount; lastTimerId = id;                                           return claim; }
        DxuiMessageResult  OnNotify         (WPARAM, LPARAM)                    override { ++onNotifyCount;        return claim; }
        DxuiMessageResult  OnInitMenuPopup  (HMENU, UINT, bool)                 override { ++onInitMenuPopupCount; return claim; }
        DxuiMessageResult  OnPaint          ()                                  override { ++onPaintCount;         return claim; }
        DxuiMessageResult  OnClose          ()                                  override { ++onCloseCount;         return claim; }
        void               OnDestroy        ()                                  override { ++onDestroyCount; }
        DxuiMessageResult  OnNcLButtonUp    (LRESULT ht, int, int)              override { ++onNcLButtonUpCount; lastNcHitTest = ht;                                   return claim; }
    };



    //
    //  Build a synthetic host (no HWND, no device) suitable for
    //  driving WndProc against client-claimed messages. Synthetic
    //  hosts skip CreateDeviceAndSwapChain entirely, so they're
    //  the cheapest seam for dispatch testing.
    //
    std::unique_ptr<DxuiHostWindow>  BuildSyntheticHost()
    {
        RECT                            bounds = { 0, 0, 800, 600 };
        std::unique_ptr<DxuiPanel>      root   = std::make_unique<DxuiPanel>();

        return std::make_unique<DxuiHostWindow> (bounds, 6.0f, std::move (root));
    }
}





TEST_CLASS (DxuiHostWindowClientTests)
{
public:

    TEST_METHOD_INITIALIZE (Setup)
    {
        DxuiResetUiThreadIdForTest();
    }



    //
    //  With no client installed, the host's WndProc still routes
    //  messages without crashing — the per-message client checks
    //  are all null-guarded. (Smoke test: WM_TIMER falls through
    //  to DefWindowProc with a null HWND, which is benign.)
    //
    TEST_METHOD (NoClient_WndProc_DoesNotCrash)
    {
        std::unique_ptr<DxuiHostWindow>  host = BuildSyntheticHost();

        (void) host->WndProc (WM_TIMER, 0, 0);
    }



    //
    //  Installed client receives WM_COMMAND with the right id and
    //  notification code. A claiming client (returns Handled)
    //  makes the host short-circuit DefWindowProc and return 0.
    //
    TEST_METHOD (Client_WmCommand_DispatchesAndClaims)
    {
        std::unique_ptr<DxuiHostWindow>  host    = BuildSyntheticHost();
        RecordingClient                  client;
        LRESULT                          result  = 0;

        host->SetClient (&client);

        result = host->WndProc (WM_COMMAND, MAKEWPARAM (0x1234, 0x5678), 0);

        Assert::AreEqual (1, client.onCommandCount);
        Assert::AreEqual ((int) 0x1234, (int) client.lastCommandId);
        Assert::AreEqual ((int) 0x5678, (int) client.lastNotifyCode);
        Assert::AreEqual ((LRESULT) 0, result);
    }



    //
    //  WM_KEYDOWN dispatches through OnKeyDown. WM_SYSKEYDOWN is
    //  bundled into the same branch so menu-mnemonic Alt+letter
    //  presses reach the client identically.
    //
    TEST_METHOD (Client_WmKeyDown_DispatchesForBothKeyAndSysKey)
    {
        std::unique_ptr<DxuiHostWindow>  host    = BuildSyntheticHost();
        RecordingClient                  client;

        host->SetClient (&client);

        (void) host->WndProc (WM_KEYDOWN,    VK_F10, 0);
        (void) host->WndProc (WM_SYSKEYDOWN, VK_F4,  0);

        Assert::AreEqual (2, client.onKeyDownCount);
        Assert::AreEqual ((WPARAM) VK_F4, client.lastKeyDownVk);
    }



    //
    //  WM_SIZE dispatches the unpacked client width/height to
    //  OnSize (in addition to the host's own panel-tree relayout).
    //
    TEST_METHOD (Client_WmSize_DispatchesUnpackedDimensions)
    {
        std::unique_ptr<DxuiHostWindow>  host    = BuildSyntheticHost();
        RecordingClient                  client;

        host->SetClient (&client);

        (void) host->WndProc (WM_SIZE, 0, MAKELPARAM (640, 480));

        Assert::AreEqual (1,        client.onSizeCount);
        Assert::AreEqual ((UINT) 640, client.lastSizeWidth);
        Assert::AreEqual ((UINT) 480, client.lastSizeHeight);
    }



    //
    //  WM_DESTROY fires OnDestroy (notification only — no claim
    //  semantics, so the message still falls through to whatever
    //  the OS does next).
    //
    TEST_METHOD (Client_WmDestroy_NotifiesClient)
    {
        std::unique_ptr<DxuiHostWindow>  host    = BuildSyntheticHost();
        RecordingClient                  client;

        host->SetClient (&client);

        (void) host->WndProc (WM_DESTROY, 0, 0);

        Assert::AreEqual (1, client.onDestroyCount);
    }



    //
    //  WM_CLOSE dispatches through OnClose; when the client
    //  claims (returns Handled), the host short-circuits so the
    //  default "DestroyWindow on close" behavior does not run.
    //
    TEST_METHOD (Client_WmClose_ClientCanSuppressDefault)
    {
        std::unique_ptr<DxuiHostWindow>  host    = BuildSyntheticHost();
        RecordingClient                  client;
        LRESULT                          result  = 0;

        host->SetClient (&client);
        client.claim = DxuiMessageResult::Handled;

        result = host->WndProc (WM_CLOSE, 0, 0);

        Assert::AreEqual (1, client.onCloseCount);
        Assert::AreEqual ((LRESULT) 0, result);
    }



    //
    //  WM_NCLBUTTONUP dispatches the original hit-test code so a
    //  client can act on system-button releases (Min/Max/Close)
    //  without re-deriving the hit-test from the click position.
    //
    TEST_METHOD (Client_WmNcLButtonUp_ForwardsHitTestCode)
    {
        std::unique_ptr<DxuiHostWindow>  host    = BuildSyntheticHost();
        RecordingClient                  client;
        LRESULT                          result  = 0;

        host->SetClient (&client);

        result = host->WndProc (WM_NCLBUTTONUP, (WPARAM) HTCLOSE, MAKELPARAM (100, 50));

        Assert::AreEqual (1,                    client.onNcLButtonUpCount);
        Assert::AreEqual ((LRESULT) HTCLOSE,    client.lastNcHitTest);
        Assert::AreEqual ((LRESULT) 0,          result);
    }



    //
    //  Setting the client to nullptr after installing one
    //  detaches it cleanly; subsequent messages do not dispatch.
    //
    TEST_METHOD (Client_NullSetClient_DetachesPriorClient)
    {
        std::unique_ptr<DxuiHostWindow>  host    = BuildSyntheticHost();
        RecordingClient                  client;

        host->SetClient (&client);
        (void) host->WndProc (WM_COMMAND, MAKEWPARAM (1, 0), 0);
        Assert::AreEqual (1, client.onCommandCount);

        host->SetClient (nullptr);
        (void) host->WndProc (WM_COMMAND, MAKEWPARAM (2, 0), 0);

        Assert::AreEqual (1, client.onCommandCount);
    }



    //
    //  WM_CREATE dispatches through OnCreate and the LRESULT the
    //  client returns is propagated verbatim from WndProc. A -1
    //  return aborts window creation; CreateWindowEx would return
    //  NULL in real use. The synthetic host just verifies the
    //  return-value passthrough.
    //
    TEST_METHOD (Client_WmCreate_PropagatesNegativeOneToAbort)
    {
        std::unique_ptr<DxuiHostWindow>  host    = BuildSyntheticHost();
        RecordingClient                  client;
        LRESULT                          result  = 0;

        host->SetClient (&client);
        client.onCreateReturn = (LRESULT) -1;

        result = host->WndProc (WM_CREATE, 0, 0);

        Assert::AreEqual (1,            client.onCreateCount);
        Assert::AreEqual ((LRESULT) -1, result);
    }



    //
    //  WM_NCHITTEST falls through to OnNcHitTest when the
    //  framework's classification yields HTCLIENT / HTNOWHERE.
    //  The synthetic host has no HWND so HandleNcHitTest returns
    //  HTNOWHERE, which trips the client fallback. The LRESULT
    //  the client returns is propagated verbatim.
    //
    TEST_METHOD (Client_WmNcHitTest_FallsThroughOnHostClientArea)
    {
        std::unique_ptr<DxuiHostWindow>  host    = BuildSyntheticHost();
        RecordingClient                  client;
        LRESULT                          result  = 0;

        host->SetClient (&client);
        client.onNcHitTestReturn = HTCAPTION;

        result = host->WndProc (WM_NCHITTEST, 0, MAKELPARAM (100, 10));

        Assert::AreEqual (1,                    client.onNcHitTestCount);
        Assert::AreEqual ((LRESULT) HTCAPTION,  result);
    }



    //
    //  WM_CTLCOLORSTATIC forwards the HDC/HWND payload and
    //  propagates the LRESULT (an HBRUSH cast to LRESULT) when
    //  the client returns non-zero. A zero return means "use
    //  DefWindowProc's default brush" and the host falls through.
    //
    TEST_METHOD (Client_WmCtlColorStatic_ForwardsHdcAndHwnd)
    {
        std::unique_ptr<DxuiHostWindow>  host    = BuildSyntheticHost();
        RecordingClient                  client;
        LRESULT                          result  = 0;
        HDC                              fakeHdc = (HDC) 0x1234;
        HWND                             fakeHwnd = (HWND) 0x5678;

        host->SetClient (&client);
        client.onCtlColorReturn = (LRESULT) 0xABCD;

        result = host->WndProc (WM_CTLCOLORSTATIC, (WPARAM) fakeHdc, (LPARAM) fakeHwnd);

        Assert::AreEqual (1,                    client.onCtlColorCount);
        Assert::IsTrue   (client.lastCtlColorHdc  == fakeHdc);
        Assert::IsTrue   (client.lastCtlColorHwnd == fakeHwnd);
        Assert::AreEqual ((LRESULT) 0xABCD,     result);
    }



    //
    //  WM_DRAWITEM forwards the full WndProc payload so the
    //  client can reinterpret_cast lParam back to DRAWITEMSTRUCT
    //  on its end. The client's LRESULT (typically TRUE) is
    //  propagated verbatim.
    //
    TEST_METHOD (Client_WmDrawItem_ForwardsFullWndProcPayload)
    {
        std::unique_ptr<DxuiHostWindow>  host    = BuildSyntheticHost();
        RecordingClient                  client;
        LRESULT                          result  = 0;

        host->SetClient (&client);

        result = host->WndProc (WM_DRAWITEM, 42, (LPARAM) 0xDEADBEEF);

        Assert::AreEqual (1,                    client.onDrawItemCount);
        Assert::AreEqual ((WPARAM) 42,          client.lastDrawItemWParam);
        Assert::AreEqual ((LPARAM) 0xDEADBEEF,  client.lastDrawItemLParam);
        Assert::AreEqual ((LRESULT) TRUE,       result);
    }
};
