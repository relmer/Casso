#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHwndSourceSysCharTests
//
//  The WM_SYSCHAR that follows a claimed Alt+key must not reach
//  DefWindowProc, which asks for a Win32 menu the window lacks and beeps.
//  One that follows an unclaimed Alt+key still does, so Alt+Space keeps
//  opening the window menu.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiHwndSourceSysCharTests)
{
public:

    class KeyClient : public IDxuiHostClient
    {
    public:
        DxuiMessageResult  claim = DxuiMessageResult::Handled;

        DxuiMessageResult  OnKeyDown (WPARAM, LPARAM) override { return claim; }
    };



    TEST_METHOD_INITIALIZE (Setup)
    {
        DxuiResetUiThreadIdForTest();
    }



    int  SendAltKeyAndCountDefault (DxuiMessageResult claim)
    {
        RECT                             bounds           = { 0, 0, 800, 600 };
        std::unique_ptr<DxuiHwndSource>  host             = std::make_unique<DxuiHwndSource> (bounds, 6.0f, std::make_unique<DxuiPanel>());
        KeyClient                        client;
        int                              sysCharDefaults  = 0;



        client.claim = claim;
        host->SetClient (&client);
        host->SetDefaultProcForTest ([&sysCharDefaults] (HWND, UINT msg, WPARAM, LPARAM) -> LRESULT
        {
            sysCharDefaults += (msg == WM_SYSCHAR) ? 1 : 0;
            return 0;
        });

        (void) host->WndProc (WM_SYSKEYDOWN, 'F', 0);
        (void) host->WndProc (WM_SYSCHAR,    'f', 0);

        host->SetClient (nullptr);
        return sysCharDefaults;
    }



    TEST_METHOD (ClaimedAltKey_SysCharDoesNotReachDefaultProc)
    {
        Assert::AreEqual (0, SendAltKeyAndCountDefault (DxuiMessageResult::Handled));
    }



    TEST_METHOD (UnclaimedAltKey_SysCharReachesDefaultProc)
    {
        Assert::AreEqual (1, SendAltKeyAndCountDefault (DxuiMessageResult::NotHandled));
    }
};
