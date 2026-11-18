#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextRendererTypographyTests
//
//  A window's typography is found from the window or the nearest window that
//  owns it, so a popup or a floating window draws in the typography of the
//  window that owns it, and every other window in the process default.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiTextRendererTypographyTests)
{
public:

    //  A hidden window, for its handle and its owner.
    static HWND  CreateHiddenWindow (HWND owner)
    {
        return CreateWindowExW (0, L"STATIC", L"", WS_POPUP, 0, 0, 1, 1, owner, nullptr, GetModuleHandleW (nullptr), nullptr);
    }



    TEST_METHOD (AWindowAndTheWindowsItOwnsUseItsTypography)
    {
        DxuiTextRenderer::Typography  typography;
        HWND                          owner    = nullptr;
        HWND                          owned    = nullptr;
        HWND                          stranger = nullptr;



        DxuiResetUiThreadIdForTest();

        owner    = CreateHiddenWindow (nullptr);
        owned    = CreateHiddenWindow (owner);
        stranger = CreateHiddenWindow (nullptr);

        Assert::IsNotNull (owner);
        Assert::IsNotNull (owned);
        Assert::IsNotNull (stranger);

        DxuiTextRenderer::SetTextGlow      (typography, 0x20, 0.1f);
        DxuiTextRenderer::AttachTypography (owner, &typography);

        Assert::IsTrue (&DxuiTextRenderer::FindTypography (owner)    == &typography,                              L"the window's");
        Assert::IsTrue (&DxuiTextRenderer::FindTypography (owned)    == &typography,                              L"a window it owns finds its owner's");
        Assert::IsTrue (&DxuiTextRenderer::FindTypography (stranger) == &DxuiTextRenderer::GetDefaultTypography(), L"any other window, the process default");
        Assert::IsTrue (&DxuiTextRenderer::FindTypography (nullptr)  == &DxuiTextRenderer::GetDefaultTypography(), L"no window, the process default");

        DxuiTextRenderer::AttachTypography (owner, nullptr);

        Assert::IsTrue (&DxuiTextRenderer::FindTypography (owned) == &DxuiTextRenderer::GetDefaultTypography(), L"detached, the process default again");

        DestroyWindow (owned);
        DestroyWindow (owner);
        DestroyWindow (stranger);
    }
};
