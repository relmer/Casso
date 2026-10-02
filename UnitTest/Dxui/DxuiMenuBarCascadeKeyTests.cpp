#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBarCascadeKeyTests
//
//  The keys in a cascade a menu bar's menu opens: Left closes the cascade,
//  Escape closes one level, and Right on a row that opens nothing moves on
//  to the next menu, so the keyboard can always leave a cascade. And where
//  the access-key underline sits under its letter.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiMenuBarCascadeKeyTests)
{
public:

    std::vector<std::shared_ptr<DxuiCommand>>  m_commands;

    std::shared_ptr<const DxuiCommand>  MakeCommand (const wchar_t * label)
    {
        std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();



        command->label    = label;
        command->dispatch = [] {};

        m_commands.push_back (command);
        return command;
    }


    //  File, then View whose first row is a cascade, then Debug.
    std::vector<DxuiMenuBarItem>  MakeItems()
    {
        std::vector<DxuiMenuBarItem>    items;
        std::vector<DxuiPopupMenuItem>  cascade;



        cascade.push_back (DxuiPopupMenuItem::ForCommand (MakeCommand (L"Disassembly 1")));
        cascade.push_back (DxuiPopupMenuItem::ForCommand (MakeCommand (L"Disassembly 2")));

        items.push_back ({ L"&File",  0, { DxuiPopupMenuItem::ForCommand (MakeCommand (L"Close")) } });
        items.push_back ({ L"&View",  0, { DxuiPopupMenuItem::ForSubmenu (MakeCommand (L"Disassembly"), std::move (cascade)),
                                           DxuiPopupMenuItem::ForCommand (MakeCommand (L"Registers")) } });
        items.push_back ({ L"&Debug", 0, { DxuiPopupMenuItem::ForCommand (MakeCommand (L"Run")) } });

        return items;
    }


    TEST_METHOD (LeftClosesTheCascadeAndThenMovesToThePreviousMenu)
    {
        DxuiMenuBar  bar;



        bar.SetItems (MakeItems());
        Assert::IsTrue (bar.HandleAltKey (L'v'));

        Assert::IsTrue   (bar.HandleKey (VK_RIGHT));
        Assert::AreEqual (1, bar.OpenIndex(), L"Right on the cascade's row opens the cascade");

        Assert::IsTrue   (bar.HandleKey (VK_LEFT));
        Assert::AreEqual (1, bar.OpenIndex(), L"Left closes the cascade and leaves View open");

        Assert::IsTrue   (bar.HandleKey (VK_LEFT));
        Assert::AreEqual (0, bar.OpenIndex(), L"Left again goes to File");
    }


    TEST_METHOD (EscapeClosesOneLevelAtATime)
    {
        DxuiMenuBar  bar;



        bar.SetItems (MakeItems());
        Assert::IsTrue (bar.HandleAltKey (L'v'));
        Assert::IsTrue (bar.HandleKey (VK_RIGHT));

        Assert::IsTrue   (bar.HandleKey (VK_ESCAPE));
        Assert::IsTrue   (bar.IsOpen(), L"the first Escape closes only the cascade");
        Assert::AreEqual (1, bar.OpenIndex());

        Assert::IsTrue   (bar.HandleKey (VK_ESCAPE));
        Assert::IsFalse  (bar.IsOpen());
    }


    TEST_METHOD (RightOnTheLastLevelMovesToTheNextMenu)
    {
        DxuiMenuBar  bar;



        bar.SetItems (MakeItems());
        Assert::IsTrue (bar.HandleAltKey (L'v'));
        Assert::IsTrue (bar.HandleKey (VK_RIGHT));

        Assert::IsTrue   (bar.HandleKey (VK_RIGHT));
        Assert::AreEqual (2, bar.OpenIndex(), L"Right on a row that opens nothing goes to Debug");
        Assert::IsTrue   (bar.IsOpen());
    }


    TEST_METHOD (TheUnderlineSitsMidwayBetweenTheBaselineAndTheLineBottom)
    {
        //  A 24-pixel line: the underline sat at 20 under the baseline, and
        //  at 24 at the line's bottom before that.
        Assert::AreEqual (22.0f, DxuiMenuBar::GetMnemonicUnderlineTop (0.0f, 24.0f));
        Assert::AreEqual (122.0f, DxuiMenuBar::GetMnemonicUnderlineTop (100.0f, 24.0f));
    }
};
