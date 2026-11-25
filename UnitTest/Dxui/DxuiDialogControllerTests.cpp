#include "Pch.h"

#include "MockDxuiControl.h"
#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogControllerTests
//
//  The dialog controller over a fake host: button wiring and default-button
//  emphasis, Enter, Escape and Tab routing, and the modal and modeless end
//  states. A DxuiPanel is the root, and a real DxuiFocusManager walks it.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiDialogControllerTestTypes
{
    //  Records what the controller calls on its host.
    class FakeDialogHost : public IDxuiDialogHost
    {
    public:
        void  EndDialog (int result) override
        {
            endCalls++;
            lastResult = result;
        }

        void  Invalidate() override { invalidates++; }

        bool  OnDialogTabSwitch (bool backward) override
        {
            tabSwitchCalls++;
            lastBackward = backward;
            return tabSwitchResult;
        }

        void  OnDialogKeyHandled (IDxuiControl * focused) override
        {
            keyHandledCalls++;
            lastFocused = focused;
        }

        int             endCalls         = 0;
        int             lastResult       = 0;
        int             invalidates      = 0;
        int             tabSwitchCalls   = 0;
        bool            lastBackward     = false;
        bool            tabSwitchResult  = false;
        int             keyHandledCalls  = 0;
        IDxuiControl  * lastFocused      = nullptr;
    };


    //  A root panel, a real focus manager, and a controller over a fake host.
    class DialogFixture
    {
    public:
        DialogFixture()
        {
            focus.SetRowEpsilonDip (32.0f);
        }

        //  A command button in the root.
        DxuiButton &  AddButton (int commandId)
        {
            DxuiButton  & button = root.Add<DxuiButton> (L"Button");

            button.SetCommandId (commandId);

            return button;
        }

        //  A focusable control in the root, left to right in the order added.
        MockDxuiControl &  AddControl()
        {
            MockDxuiControl  & control = root.Add<MockDxuiControl>();
            LONG               left    = (LONG) (nextLeft);

            control.SetBounds (RECT { left, 0, left + 50, 20 });
            nextLeft += 60;

            return control;
        }

        DxuiPanel             root;
        DxuiFocusManager      focus;
        FakeDialogHost        host;
        DxuiDialogController  controller { host, focus, root };
        int                   nextLeft   = 0;
    };
}

using namespace DxuiDialogControllerTestTypes;





TEST_CLASS (DxuiDialogControllerTests)
{
public:

    //  The outline a button paints, which differs for an emphasized one.
    static RecordedPaintCall  PaintOutline (DxuiButton & button)
    {
        MockDxuiPainter        painter;
        MockDxuiTextRenderer   text;
        MockDxuiTheme          theme;
        RecordedPaintCall      found;

        button.Layout (RECT { 0, 0, 80, 24 });
        button.Paint  (painter, text, theme);

        for (const RecordedPaintCall & call : painter.Calls())
        {
            if (call.kind == RecordedPaintKind::OutlineRoundedRect)
            {
                found = call;
            }
        }

        return found;
    }




    TEST_METHOD (Begin_EmphasizesTheDefaultButton_AndNoOther)
    {
        DialogFixture      fixture;
        DxuiButton       & ok     = fixture.AddButton (IDOK);
        DxuiButton       & cancel = fixture.AddButton (IDCANCEL);
        RecordedPaintCall  before = PaintOutline (cancel);
        RecordedPaintCall  okRing;
        RecordedPaintCall  cancelRing;

        fixture.controller.Begin (nullptr, IDOK, true, nullptr);

        okRing     = PaintOutline (ok);
        cancelRing = PaintOutline (cancel);

        Assert::IsTrue (okRing.thickness != cancelRing.thickness || okRing.argb != cancelRing.argb, L"the default button has the emphasis outline");
        Assert::IsTrue (cancelRing.thickness == before.thickness && cancelRing.argb == before.argb, L"the other button is as it was");
    }




    TEST_METHOD (Begin_AButtonWithACommandIdAndNoHandler_EndsTheDialogWithThatId)
    {
        DialogFixture   fixture;
        DxuiButton    & yes = fixture.AddButton (IDYES);

        fixture.controller.Begin (nullptr, IDYES, true, nullptr);
        yes.Click();

        Assert::AreEqual (1, fixture.host.endCalls);
        Assert::AreEqual (IDYES, fixture.host.lastResult);
    }




    TEST_METHOD (Begin_AButtonWithAClickHandler_IsLeftAlone)
    {
        DialogFixture   fixture;
        DxuiButton    & apply   = fixture.AddButton (IDRETRY);
        int             clicked = 0;

        apply.SetOnClick ([&clicked] () { clicked++; });

        fixture.controller.Begin (nullptr, IDOK, true, nullptr);
        apply.Click();

        Assert::AreEqual (1, clicked);
        Assert::AreEqual (0, fixture.host.endCalls);
    }




    TEST_METHOD (Begin_AButtonWithNoCommandId_IsNotWired)
    {
        DialogFixture   fixture;
        DxuiButton    & plain = fixture.AddButton (0);

        fixture.controller.Begin (nullptr, IDOK, true, nullptr);

        Assert::IsFalse (plain.HasClickHandler());
    }




    TEST_METHOD (Begin_FocusesTheInitialControl)
    {
        DialogFixture      fixture;
        MockDxuiControl  & first  = fixture.AddControl();
        MockDxuiControl  & second = fixture.AddControl();

        fixture.controller.Begin (nullptr, IDOK, true, &second);

        Assert::AreEqual (static_cast<void *> (&second), static_cast<void *> (fixture.focus.GetFocusedControl()));
        Assert::IsFalse  (first.lastFocused);
    }




    TEST_METHOD (Enter_WithNothingFocused_ClicksTheDefaultButton)
    {
        DialogFixture   fixture;
        bool            isHandled = false;

        fixture.AddButton (IDOK);

        fixture.controller.Begin (nullptr, IDOK, true, nullptr);
        Assert::IsTrue (fixture.focus.GetFocusedControl() == nullptr, L"nothing is focused");

        isHandled = fixture.controller.HandleKey (VK_RETURN, false, false);

        Assert::IsTrue   (isHandled);
        Assert::AreEqual (1, fixture.host.endCalls);
        Assert::AreEqual (IDOK, fixture.host.lastResult);
    }




    TEST_METHOD (Enter_ConsumedByTheFocusedControl_DoesNotClickTheDefaultButton)
    {
        DialogFixture      fixture;
        MockDxuiControl  & field     = fixture.AddControl();
        bool               isHandled = false;

        fixture.AddButton (IDOK);
        field.consumeKey = true;

        fixture.controller.Begin (nullptr, IDOK, true, &field);
        isHandled = fixture.controller.HandleKey (VK_RETURN, false, false);

        Assert::IsTrue   (isHandled);
        Assert::AreEqual (1, field.keyCount);
        Assert::AreEqual (0, fixture.host.endCalls);
    }




    TEST_METHOD (Escape_ClicksTheEnabledCancelButton)
    {
        DialogFixture   fixture;
        bool            isHandled = false;

        fixture.AddButton (IDOK);
        fixture.AddButton (IDCANCEL);

        fixture.controller.Begin (nullptr, IDOK, true, nullptr);
        isHandled = fixture.controller.HandleKey (VK_ESCAPE, false, false);

        Assert::IsTrue   (isHandled);
        Assert::AreEqual (1, fixture.host.endCalls);
        Assert::AreEqual (IDCANCEL, fixture.host.lastResult);
    }




    TEST_METHOD (Escape_WithNoCancelButton_EndsTheDialogWithIdcancel)
    {
        DialogFixture   fixture;
        bool            isHandled = false;

        fixture.AddButton (IDOK);

        fixture.controller.Begin (nullptr, IDOK, true, nullptr);
        isHandled = fixture.controller.HandleKey (VK_ESCAPE, false, false);

        Assert::IsTrue   (isHandled);
        Assert::AreEqual (1, fixture.host.endCalls);
        Assert::AreEqual (IDCANCEL, fixture.host.lastResult);
    }




    TEST_METHOD (Escape_ConsumedByTheFocusedControl_DoesNotEndTheDialog)
    {
        DialogFixture      fixture;
        MockDxuiControl  & dropDown  = fixture.AddControl();
        bool               isHandled = false;

        fixture.AddButton (IDCANCEL);
        dropDown.consumeKey = true;

        fixture.controller.Begin (nullptr, IDOK, true, &dropDown);
        isHandled = fixture.controller.HandleKey (VK_ESCAPE, false, false);

        Assert::IsTrue   (isHandled);
        Assert::AreEqual (0, fixture.host.endCalls);
        Assert::IsTrue   (fixture.host.invalidates >= 1, L"the host repaints");
    }




    TEST_METHOD (Tab_MovesFocusToTheNextControl)
    {
        DialogFixture      fixture;
        MockDxuiControl  & first     = fixture.AddControl();
        MockDxuiControl  & second    = fixture.AddControl();
        bool               isHandled = false;

        fixture.controller.Begin (nullptr, IDOK, true, &first);
        isHandled = fixture.controller.HandleKey (VK_TAB, false, false);

        Assert::IsTrue  (isHandled);
        Assert::AreEqual (static_cast<void *> (&second), static_cast<void *> (fixture.focus.GetFocusedControl()));
    }




    TEST_METHOD (Tab_ConsumedByTheFocusedControl_DoesNotMoveFocus)
    {
        DialogFixture      fixture;
        MockDxuiControl  & first     = fixture.AddControl();
        bool               isHandled = false;

        fixture.AddControl();
        first.consumeKey = true;

        fixture.controller.Begin (nullptr, IDOK, true, &first);
        isHandled = fixture.controller.HandleKey (VK_TAB, false, false);

        Assert::IsTrue   (isHandled);
        Assert::AreEqual (static_cast<void *> (&first), static_cast<void *> (fixture.focus.GetFocusedControl()));
    }




    TEST_METHOD (CtrlTab_GoesToTheHostTabSwitch_ShiftMeansBackward)
    {
        DialogFixture      fixture;
        MockDxuiControl  & first     = fixture.AddControl();
        bool               isHandled = false;

        fixture.AddControl();
        fixture.host.tabSwitchResult = true;

        fixture.controller.Begin (nullptr, IDOK, true, &first);

        isHandled = fixture.controller.HandleKey (VK_TAB, false, true);
        Assert::IsTrue  (isHandled);
        Assert::AreEqual (1, fixture.host.tabSwitchCalls);
        Assert::IsFalse (fixture.host.lastBackward);

        isHandled = fixture.controller.HandleKey (VK_TAB, true, true);
        Assert::IsTrue  (isHandled);
        Assert::AreEqual (2, fixture.host.tabSwitchCalls);
        Assert::IsTrue  (fixture.host.lastBackward);
        Assert::AreEqual (static_cast<void *> (&first), static_cast<void *> (fixture.focus.GetFocusedControl()), L"the host took the key, so focus did not move");
    }




    TEST_METHOD (CtrlTab_NotTakenByTheHost_MovesFocusAsTabDoes)
    {
        DialogFixture      fixture;
        MockDxuiControl  & first  = fixture.AddControl();
        MockDxuiControl  & second = fixture.AddControl();

        fixture.host.tabSwitchResult = false;

        fixture.controller.Begin (nullptr, IDOK, true, &first);
        fixture.controller.HandleKey (VK_TAB, false, true);

        Assert::AreEqual (1, fixture.host.tabSwitchCalls);
        Assert::AreEqual (static_cast<void *> (&second), static_cast<void *> (fixture.focus.GetFocusedControl()));
    }




    TEST_METHOD (ArrowKey_ShowsTheFocusCueOfTheFocusedControl)
    {
        DialogFixture      fixture;
        MockDxuiControl  & slider = fixture.AddControl();

        fixture.controller.Begin (nullptr, IDOK, true, &slider);
        slider.SetFocusCueVisible (false);

        fixture.controller.HandleKey (VK_RIGHT, false, false);

        Assert::IsTrue   (slider.IsFocusCueVisible());
        Assert::AreEqual (1, slider.keyCount, L"and the key went to the control");
    }




    TEST_METHOD (AConsumedKey_NotifiesTheHostWithTheFocusedControl_AndRepaints)
    {
        DialogFixture      fixture;
        MockDxuiControl  & slider = fixture.AddControl();

        slider.consumeKey = true;

        fixture.controller.Begin (nullptr, IDOK, true, &slider);
        fixture.controller.HandleKey (VK_LEFT, false, false);

        Assert::AreEqual (1, fixture.host.keyHandledCalls);
        Assert::AreEqual (static_cast<void *> (&slider), static_cast<void *> (fixture.host.lastFocused));
        Assert::IsTrue   (fixture.host.invalidates >= 1);
    }




    TEST_METHOD (AKeyNoControlTakes_IsNotHandled_AndTheHostIsNotNotified)
    {
        DialogFixture   fixture;
        bool            isHandled = true;

        fixture.controller.Begin (nullptr, IDOK, true, nullptr);
        isHandled = fixture.controller.HandleKey (VK_LEFT, false, false);

        Assert::IsFalse  (isHandled);
        Assert::AreEqual (0, fixture.host.keyHandledCalls);
        Assert::AreEqual (0, fixture.host.invalidates);
    }




    TEST_METHOD (TryClickButton_SkipsADisabledOrHiddenButton)
    {
        DialogFixture   fixture;
        DxuiButton    & disabled = fixture.AddButton (IDYES);
        DxuiButton    & hidden   = fixture.AddButton (IDNO);

        fixture.AddButton (IDOK);
        fixture.controller.Begin (nullptr, IDOK, true, nullptr);
        disabled.SetEnabled (false);
        hidden.SetVisible   (false);

        Assert::IsFalse (fixture.controller.TryClickButton (IDYES));
        Assert::IsFalse (fixture.controller.TryClickButton (IDNO));
        Assert::IsFalse (fixture.controller.TryClickButton (0), L"a zero id is not a command button");
        Assert::IsFalse (fixture.controller.TryClickButton (IDABORT), L"no such button");
        Assert::AreEqual (0, fixture.host.endCalls);

        Assert::IsTrue   (fixture.controller.TryClickButton (IDOK));
        Assert::AreEqual (1, fixture.host.endCalls);
    }




    TEST_METHOD (End_Modal_ExitsTheLoopOnce_AndKeepsTheFirstResult)
    {
        DialogFixture  fixture;

        fixture.controller.Begin (nullptr, IDOK, true, nullptr);

        Assert::IsTrue (fixture.controller.End (IDYES) == DxuiDialogEndAction::ExitModalLoop);
        Assert::IsTrue (fixture.controller.End (IDNO)  == DxuiDialogEndAction::None);

        Assert::IsTrue   (fixture.controller.IsModalDone());
        Assert::IsTrue   (fixture.controller.IsActive());
        Assert::AreEqual (IDYES, fixture.controller.GetModalResult());
    }




    TEST_METHOD (End_Modeless_ClosesTheWindow_AndDeactivates)
    {
        DialogFixture  fixture;

        fixture.controller.Begin (nullptr, IDOK, false, nullptr);

        Assert::IsTrue (fixture.controller.End (IDOK) == DxuiDialogEndAction::CloseModeless);

        Assert::IsFalse (fixture.controller.IsActive());
        Assert::IsFalse (fixture.controller.IsModalDone());
        Assert::IsTrue  (fixture.controller.End (IDOK) == DxuiDialogEndAction::None, L"a second end finds no dialog");
    }




    TEST_METHOD (End_WhenNoDialogIsOpen_DoesNothing)
    {
        DialogFixture  fixture;

        Assert::IsTrue  (fixture.controller.End (IDOK) == DxuiDialogEndAction::None);
        Assert::IsFalse (fixture.controller.IsActive());
        Assert::IsFalse (fixture.controller.IsModalDone());
    }




    TEST_METHOD (Begin_ClearsTheResultOfAnEarlierDialog)
    {
        DialogFixture  fixture;

        fixture.controller.Begin (nullptr, IDOK, true, nullptr);
        fixture.controller.End (IDYES);
        fixture.controller.Deactivate();

        Assert::IsFalse (fixture.controller.IsActive());

        fixture.controller.Begin (nullptr, IDOK, true, nullptr);

        Assert::IsFalse  (fixture.controller.IsModalDone());
        Assert::AreEqual (IDCANCEL, fixture.controller.GetModalResult());
        Assert::IsTrue   (fixture.controller.IsActive());
    }
};
