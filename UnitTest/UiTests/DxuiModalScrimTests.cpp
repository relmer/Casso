#include "Pch.h"

#include "Widgets/DxuiModalScrim.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ModalScrimTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ModalScrimTests)
{
public:

    TEST_METHOD (Show_FlipsVisible)
    {
        DxuiModalScrim  scrim;

        scrim.Show (nullptr, nullptr);

        Assert::IsTrue (scrim.IsVisible());
    }

    TEST_METHOD (Confirm_InvokesConfirmCallback_AndHides)
    {
        DxuiModalScrim  scrim;
        int         confirmHits = 0;
        int         cancelHits  = 0;
        scrim.Show ([&] { confirmHits++; }, [&] { cancelHits++; });

        scrim.Confirm();

        Assert::AreEqual (1, confirmHits);
        Assert::AreEqual (0, cancelHits);
        Assert::IsFalse (scrim.IsVisible());
    }

    TEST_METHOD (Cancel_InvokesCancelCallback_AndHides)
    {
        DxuiModalScrim  scrim;
        int         cancelHits = 0;
        scrim.Show (nullptr, [&] { cancelHits++; });

        scrim.Cancel();

        Assert::AreEqual (1, cancelHits);
        Assert::IsFalse (scrim.IsVisible());
    }

    TEST_METHOD (KeyEscape_FiresCancelOnly)
    {
        DxuiModalScrim  scrim;
        int         confirmHits = 0;
        int         cancelHits  = 0;
        scrim.Show ([&] { confirmHits++; }, [&] { cancelHits++; });

        Assert::IsTrue (scrim.OnKey (VK_ESCAPE));
        Assert::AreEqual (0, confirmHits);
        Assert::AreEqual (1, cancelHits);
    }

    TEST_METHOD (KeyEnter_FiresConfirmOnly)
    {
        DxuiModalScrim  scrim;
        int         confirmHits = 0;
        int         cancelHits  = 0;
        scrim.Show ([&] { confirmHits++; }, [&] { cancelHits++; });

        Assert::IsTrue (scrim.OnKey (VK_RETURN));
        Assert::AreEqual (1, confirmHits);
        Assert::AreEqual (0, cancelHits);
    }

    TEST_METHOD (KeyOther_NotConsumed)
    {
        DxuiModalScrim  scrim;
        scrim.Show (nullptr, nullptr);
        Assert::IsFalse (scrim.OnKey (VK_TAB));
        Assert::IsTrue  (scrim.IsVisible());
    }

    TEST_METHOD (ConfirmWhileHidden_NoOp)
    {
        DxuiModalScrim  scrim;
        int         hits = 0;
        scrim.Show ([&] { hits++; }, nullptr);
        scrim.Hide();

        scrim.Confirm();
        Assert::AreEqual (0, hits);
    }
};
