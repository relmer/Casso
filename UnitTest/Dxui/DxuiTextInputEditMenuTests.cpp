#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextInputEditMenuTests
//
//  A text box's right-click menu: Undo, Cut, Copy, Paste, Delete and Select
//  all, dimmed as the box's state allows, each run through the box's
//  standard commands; and the box under a point found among a window's
//  controls.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiTextInputEditMenuTests)
{
public:

    static const DxuiCommand & FindRow (const std::vector<DxuiPopupMenuItem> & items, const std::wstring & label)
    {
        for (const DxuiPopupMenuItem & item : items)
        {
            if (item.kind == DxuiPopupMenuItem::Kind::Command && item.command->label == label)
            {
                return *item.command;
            }
        }

        Assert::Fail ((L"no row " + label).c_str());
        return *items.front().command;
    }


    TEST_METHOD (TheMenuHasAnEditControlsRowsInItsOrder)
    {
        DxuiTextInput                   box;
        std::vector<DxuiPopupMenuItem>  items = DxuiContextMenu::MakeEditItems (box);
        std::vector<std::wstring>       labels;



        for (const DxuiPopupMenuItem & item : items)
        {
            labels.push_back (item.kind == DxuiPopupMenuItem::Kind::Separator ? std::wstring (L"-") : item.command->label);
        }

        Assert::IsTrue (labels == std::vector<std::wstring> { L"Undo", L"-", L"Cut", L"Copy", L"Paste", L"Delete", L"-", L"Select all" });
        Assert::AreEqual (std::wstring (L"Ctrl+A"), FindRow (items, L"Select all").accelerator);
    }


    TEST_METHOD (TheRowsAreDimmedAsTheBoxsStateAllows)
    {
        DxuiTextInput                   box;
        std::vector<DxuiPopupMenuItem>  items;



        box.SetText    (L"LDA $0400");
        box.SetFocused (true);
        items = DxuiContextMenu::MakeEditItems (box);

        Assert::IsFalse (FindRow (items, L"Cut").isEnabled(),    L"no selection to cut");
        Assert::IsFalse (FindRow (items, L"Delete").isEnabled(), L"or delete");
        Assert::IsTrue  (FindRow (items, L"Paste").isEnabled());
        Assert::IsTrue  (FindRow (items, L"Select all").isEnabled());

        box.SetSelection (4, 9);

        Assert::IsTrue (FindRow (items, L"Copy").isEnabled(),   L"a selection to copy");
        Assert::IsTrue (FindRow (items, L"Delete").isEnabled(), L"and delete");
    }


    TEST_METHOD (DeleteAndSelectAllActOnTheBox)
    {
        DxuiTextInput                   box;
        std::vector<DxuiPopupMenuItem>  items;



        box.SetText      (L"LDA $0400");
        box.SetFocused   (true);
        box.SetSelection (3, 9);
        items = DxuiContextMenu::MakeEditItems (box);

        FindRow (items, L"Delete").dispatch();
        Assert::AreEqual (std::wstring (L"LDA"), box.GetText(), L"the selection is deleted");

        FindRow (items, L"Select all").dispatch();
        Assert::AreEqual ((size_t) 0, box.GetSelectionStart());
        Assert::AreEqual ((size_t) 3, box.GetSelectionEnd(), L"the whole text is selected");

        FindRow (items, L"Undo").dispatch();
        Assert::AreEqual (std::wstring (L"LDA $0400"), box.GetText(), L"Undo puts the deleted text back");
    }


    TEST_METHOD (TheBoxUnderAPointIsFoundAmongTheControls)
    {
        DxuiPanel        root;
        DxuiPanel      * group = root.CreateChild<DxuiPanel>();
        DxuiTextInput  * box   = group->CreateChild<DxuiTextInput>();
        DxuiTextInput  * other = root.CreateChild<DxuiTextInput>();



        box->SetRect   (RECT { 10, 10, 110, 30 });
        other->SetRect (RECT { 10, 50, 110, 70 });

        Assert::IsTrue (DxuiWindow::FindTextBoxAt (root, POINT { 20, 20 }) == box,   L"a box inside a group");
        Assert::IsTrue (DxuiWindow::FindTextBoxAt (root, POINT { 20, 60 }) == other, L"a box beside it");
        Assert::IsNull (DxuiWindow::FindTextBoxAt (root, POINT { 200, 20 }),         L"no box");

        other->SetVisible (false);
        Assert::IsNull (DxuiWindow::FindTextBoxAt (root, POINT { 20, 60 }), L"a hidden box is passed over");
    }


    //  A dialog's keys are built by MakeKeyEvent, so Ctrl+A with the Ctrl key
    //  down selects the box's text, as it does in a pane.
    TEST_METHOD (CtrlAInADialogSelectsTheBoxsText)
    {
        DxuiTextInput  box;
        DxuiKeyEvent   ev;



        DxuiInputState::SetKeyReader ([] (int vk) { return (vk == VK_CONTROL) ? (SHORT) 0x8000 : SHORT (0); });
        ev = DxuiWindow::MakeKeyEvent (DxuiKeyEventKind::Down, 'A');
        DxuiInputState::SetKeyReader ([] (int) { return SHORT (0); });

        box.SetText      (L"$0300");
        box.SetFocused   (true);
        box.SetSelection (5, 5);

        Assert::IsTrue   (ev.ctrl, L"the Ctrl key is in the event");
        Assert::IsTrue   (box.OnKey (ev));
        Assert::AreEqual ((size_t) 0, box.GetSelectionStart());
        Assert::AreEqual ((size_t) 5, box.GetSelectionEnd());
    }
};
