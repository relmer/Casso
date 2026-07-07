#include "Pch.h"

#include "Window/DxuiPropertyPage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;



namespace
{
    //
    //  Minimal DxuiPropertyPage subclass that records OnApply() calls and
    //  returns a configurable result, so the dirty / apply contract can be
    //  exercised without a window.
    //
    class TestPage : public DxuiPropertyPage
    {
    public:
        TestPage () : DxuiPropertyPage (L"Test") {}

        int   applyCalls  = 0;
        bool  applyResult = true;

        bool  OnApply () override { ++applyCalls; return applyResult; }
    };
}




TEST_CLASS (DxuiPropertyPageTests)
{
public:
    TEST_METHOD (Title_ReturnsCtorValue)
    {
        TestPage  page;

        Assert::AreEqual (L"Test", page.Title().c_str());
    }


    TEST_METHOD (MarkDirty_TogglesFlagAndFiresOnChangeOnce)
    {
        TestPage  page;
        int       changes = 0;


        page.SetOnDirtyChanged ([&changes] () { ++changes; });

        Assert::IsFalse (page.IsDirty());

        page.MarkDirty (true);
        Assert::IsTrue  (page.IsDirty());
        Assert::AreEqual (1, changes);

        // Re-marking dirty with no state change fires no callback.
        page.MarkDirty (true);
        Assert::AreEqual (1, changes);

        page.MarkDirty (false);
        Assert::IsFalse (page.IsDirty());
        Assert::AreEqual (2, changes);
    }


    TEST_METHOD (OnApply_DefaultsAndOverrideResult)
    {
        TestPage  page;


        page.applyResult = false;
        Assert::IsFalse (page.OnApply());
        Assert::AreEqual (1, page.applyCalls);

        page.applyResult = true;
        Assert::IsTrue (page.OnApply());
        Assert::AreEqual (2, page.applyCalls);
    }
};
