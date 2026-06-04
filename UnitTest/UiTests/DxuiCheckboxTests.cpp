#include "Pch.h"

#include "Widgets/DxuiCheckbox.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CheckboxTests
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    RECT MakeRect (int l, int t, int r, int b)
    {
        RECT  rc = { l, t, r, b };
        return rc;
    }
}


TEST_CLASS (CheckboxTests)
{
public:

    TEST_METHOD (HitTest_InsideRect_True)
    {
        DxuiCheckbox  cb;
        cb.SetRect (MakeRect (10, 10, 110, 30));
        Assert::IsTrue (cb.HitTest (50, 20));
    }

    TEST_METHOD (HitTest_OutsideRect_False)
    {
        DxuiCheckbox  cb;
        cb.SetRect (MakeRect (10, 10, 110, 30));
        Assert::IsFalse (cb.HitTest (5,  5));
        Assert::IsFalse (cb.HitTest (50, 31));
    }

    TEST_METHOD (HitTest_DisabledNeverHits)
    {
        DxuiCheckbox  cb;
        cb.SetRect (MakeRect (10, 10, 110, 30));
        cb.SetEnabled (false);
        Assert::IsFalse (cb.HitTest (50, 20));
    }

    TEST_METHOD (MouseDownThenUp_Toggles)
    {
        DxuiCheckbox  cb;
        bool      observed = false;
        cb.SetRect (MakeRect (0, 0, 100, 20));
        cb.SetOnChange ([&] (bool v) { observed = v; });

        Assert::IsTrue (cb.OnLButtonDown (10, 10));
        Assert::IsTrue (cb.Pressed());
        Assert::IsTrue (cb.OnLButtonUp   (10, 10));
        Assert::IsTrue (cb.Checked());
        Assert::IsTrue (observed);
        Assert::IsFalse (cb.Pressed());
    }

    TEST_METHOD (MouseDownInsideThenUpOutside_CancelsToggle)
    {
        DxuiCheckbox  cb;
        cb.SetRect (MakeRect (0, 0, 100, 20));

        Assert::IsTrue (cb.OnLButtonDown (10, 10));
        Assert::IsFalse (cb.OnLButtonUp (200, 10));
        Assert::IsFalse (cb.Checked());
    }

    TEST_METHOD (Disabled_RejectsMouseInput)
    {
        DxuiCheckbox  cb;
        cb.SetRect (MakeRect (0, 0, 100, 20));
        cb.SetEnabled (false);

        Assert::IsFalse (cb.OnLButtonDown (10, 10));
        Assert::IsFalse (cb.OnLButtonUp   (10, 10));
        Assert::IsFalse (cb.Checked());
    }

    TEST_METHOD (KeySpace_FocusedAndEnabled_Toggles)
    {
        DxuiCheckbox  cb;
        cb.SetRect (MakeRect (0, 0, 100, 20));
        cb.SetFocused (true);

        Assert::IsTrue (cb.OnKey (VK_SPACE));
        Assert::IsTrue (cb.Checked());
        Assert::IsTrue (cb.OnKey (VK_RETURN));
        Assert::IsFalse (cb.Checked());
    }

    TEST_METHOD (Key_WithoutFocus_NoOp)
    {
        DxuiCheckbox  cb;
        cb.SetRect (MakeRect (0, 0, 100, 20));
        Assert::IsFalse (cb.OnKey (VK_SPACE));
        Assert::IsFalse (cb.Checked());
    }

    TEST_METHOD (HoverStateFollowsMouseMove)
    {
        DxuiCheckbox  cb;
        cb.SetRect (MakeRect (0, 0, 100, 20));

        cb.SetMouseHover (10, 10);
        Assert::IsTrue (cb.Hover());

        cb.SetMouseHover (200, 200);
        Assert::IsFalse (cb.Hover());
    }

    TEST_METHOD (DisableClearsTransientState)
    {
        DxuiCheckbox  cb;
        cb.SetRect (MakeRect (0, 0, 100, 20));

        Assert::IsTrue (cb.OnLButtonDown (10, 10));
        cb.SetMouseHover (10, 10);
        Assert::IsTrue (cb.Pressed());
        Assert::IsTrue (cb.Hover());

        cb.SetEnabled (false);
        Assert::IsFalse (cb.Pressed());
        Assert::IsFalse (cb.Hover());
    }
};
