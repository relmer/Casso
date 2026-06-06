#include "Pch.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace
{
    DxuiKeyEvent  MakeKeyDown (WPARAM vk)
    {
        DxuiKeyEvent  ev = {};

        ev.kind = DxuiKeyEventKind::Down;
        ev.vk   = vk;
        return ev;
    }
}





TEST_CLASS (DxuiDialogTests)
{
public:

    TEST_METHOD_INITIALIZE (Setup)
    {
        DxuiResetUiThreadIdForTest();
    }


    TEST_METHOD (DefaultConstruction_HasNoButtonsOrTitle)
    {
        DxuiDialog  dlg;


        Assert::AreEqual (std::wstring(), dlg.Title());
        Assert::AreEqual ((size_t) 0, dlg.ButtonCount());
        Assert::AreEqual (-1, dlg.DefaultIndex());
        Assert::AreEqual (-1, dlg.CancelIndex());
        Assert::IsFalse (dlg.IsBuilt());
    }


    TEST_METHOD (Configuration_StoresTitleAndButtons)
    {
        DxuiDialog  dlg;

        dlg.SetTitle (L"Hello");
        dlg.AddButton (L"OK",     1, /*isDefault*/ true);
        dlg.AddButton (L"Cancel", 2, /*isDefault*/ false, /*isCancel*/ true);


        Assert::AreEqual (std::wstring (L"Hello"), dlg.Title());
        Assert::AreEqual ((size_t) 2, dlg.ButtonCount());
        Assert::AreEqual (0, dlg.DefaultIndex());
        Assert::AreEqual (1, dlg.CancelIndex());
        Assert::AreEqual (1, dlg.ButtonAt (0).returnCode);
        Assert::AreEqual (2, dlg.ButtonAt (1).returnCode);
        Assert::AreEqual (std::wstring (L"OK"),     dlg.ButtonAt (0).label);
        Assert::AreEqual (std::wstring (L"Cancel"), dlg.ButtonAt (1).label);
    }


    TEST_METHOD (Build_MaterializesCaptionContentAndButtonRow)
    {
        DxuiDialog                  dlg;
        std::unique_ptr<DxuiPanel>  content = std::make_unique<DxuiPanel>();
        DxuiPanel *                 contentRaw = content.get();

        dlg.SetTitle (L"T");
        dlg.SetContent (std::move (content));
        dlg.AddButton (L"OK", 1, true);
        dlg.AddButton (L"X",  0, false, true);

        dlg.Build();


        Assert::IsTrue (dlg.IsBuilt());
        Assert::IsNotNull ((const void *) dlg.CaptionBar());
        Assert::IsNotNull ((const void *) dlg.ButtonRow());
        Assert::AreEqual ((const void *) contentRaw, (const void *) dlg.ContentPanel());
        Assert::AreEqual ((size_t) 3, dlg.ChildCount());
    }


    TEST_METHOD (Build_SynthesizesEmptyContentWhenNoneProvided)
    {
        DxuiDialog  dlg;

        dlg.SetTitle (L"T");
        dlg.AddButton (L"OK", 0, true);
        dlg.Build();


        Assert::IsTrue (dlg.IsBuilt());
        Assert::IsNotNull ((const void *) dlg.ContentPanel());
    }


    TEST_METHOD (ActivateDefault_ReturnsDefaultButtonReturnCode)
    {
        DxuiDialog  dlg;

        dlg.SetTitle (L"T");
        dlg.AddButton (L"OK",     7, true);
        dlg.AddButton (L"Cancel", 8, false, true);
        dlg.Build();


        std::optional<int>  rc = dlg.ActivateDefault();

        Assert::IsTrue (rc.has_value());
        Assert::AreEqual (7, rc.value());
    }


    TEST_METHOD (ActivateCancel_ReturnsCancelButtonReturnCode)
    {
        DxuiDialog  dlg;

        dlg.SetTitle (L"T");
        dlg.AddButton (L"OK",     7, true);
        dlg.AddButton (L"Cancel", 8, false, true);
        dlg.Build();


        std::optional<int>  rc = dlg.ActivateCancel();

        Assert::IsTrue (rc.has_value());
        Assert::AreEqual (8, rc.value());
    }


    TEST_METHOD (ActivateDefault_NoDefaultButton_ReturnsNullopt)
    {
        DxuiDialog  dlg;

        dlg.AddButton (L"OK", 1);
        dlg.AddButton (L"X",  2, false, true);
        dlg.Build();


        std::optional<int>  rc = dlg.ActivateDefault();

        Assert::IsFalse (rc.has_value());
    }


    TEST_METHOD (ActivateCancel_NoCancelButton_ReturnsNullopt)
    {
        DxuiDialog  dlg;

        dlg.AddButton (L"OK", 1, true);
        dlg.Build();


        std::optional<int>  rc = dlg.ActivateCancel();

        Assert::IsFalse (rc.has_value());
    }


    TEST_METHOD (OnKey_EnterInvokesDefaultAndConsumes)
    {
        DxuiDialog  dlg;
        int         captured = -1;

        dlg.AddButton (L"OK",     5, true);
        dlg.AddButton (L"Cancel", 6, false, true);
        dlg.Build();
        dlg.SetCloseHandler ([&captured] (int rc) { captured = rc; });


        bool  consumed = dlg.OnKey (MakeKeyDown (VK_RETURN));

        Assert::IsTrue  (consumed);
        Assert::AreEqual (5, captured);
    }


    TEST_METHOD (OnKey_EscapeInvokesCancelAndConsumes)
    {
        DxuiDialog  dlg;
        int         captured = -1;

        dlg.AddButton (L"OK",     5, true);
        dlg.AddButton (L"Cancel", 6, false, true);
        dlg.Build();
        dlg.SetCloseHandler ([&captured] (int rc) { captured = rc; });


        bool  consumed = dlg.OnKey (MakeKeyDown (VK_ESCAPE));

        Assert::IsTrue  (consumed);
        Assert::AreEqual (6, captured);
    }


    TEST_METHOD (OnKey_EnterWithoutDefault_DoesNotConsume)
    {
        DxuiDialog  dlg;

        dlg.AddButton (L"OK", 1);
        dlg.Build();


        bool  consumed = dlg.OnKey (MakeKeyDown (VK_RETURN));

        Assert::IsFalse (consumed);
    }


    TEST_METHOD (CloseHandler_FiresOnceWithChosenReturnCode)
    {
        DxuiDialog  dlg;
        int         fireCount = 0;
        int         captured  = -1;

        dlg.AddButton (L"Yes", 11, true);
        dlg.Build();
        dlg.SetCloseHandler ([&fireCount, &captured] (int rc)
                             {
                                 fireCount++;
                                 captured = rc;
                             });


        std::optional<int>  rc = dlg.ActivateDefault();

        Assert::IsTrue   (rc.has_value());
        Assert::AreEqual (11, rc.value());
        Assert::AreEqual (1,  fireCount);
        Assert::AreEqual (11, captured);
    }


    TEST_METHOD (AccessibleRole_IsDialog)
    {
        DxuiDialog  dlg;

        Assert::IsTrue (dlg.AccessibleRole() == DxuiAccessibleRole::Dialog);
    }


    TEST_METHOD (AccessibleName_ReturnsTitle)
    {
        DxuiDialog  dlg;

        dlg.SetTitle (L"Hello");

        Assert::AreEqual (std::wstring (L"Hello"), dlg.AccessibleName());
    }
};
