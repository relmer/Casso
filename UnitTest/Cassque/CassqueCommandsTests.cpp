#include "Pch.h"
#include "Cassque/CassqueCommands.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueCommandsTests
//
//  The menu the table builds, the keys that reach commands, and that every
//  command's state and action come from the handlers with its own id.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassqueCommandsTests)
{
public:

    TEST_METHOD (Menu_HasFiveTitlesWithTheirRows)
    {
        CassqueCommands               commands ({});
        std::vector<DxuiMenuBarItem>  items = commands.BuildMenuItems();

        Assert::AreEqual ((size_t) 5, items.size());
        Assert::AreEqual (std::wstring (L"&File"), items[0].label);
        Assert::AreEqual (std::wstring (L"&Edit"), items[1].label);
        Assert::AreEqual (std::wstring (L"&Help"), items[4].label);

        Assert::IsTrue (items[2].submenu[1].kind == DxuiPopupMenuItem::Kind::Separator);
        Assert::AreEqual ((int) CassqueCommands::kTogglePreview, items[2].submenu[2].command->id);
        Assert::AreEqual (std::wstring (L"Alt+P"), items[2].submenu[2].command->accelerator);
    }


    //  Copy and select all show their keystrokes but claim none: both mean
    //  different things in the hex view's two columns, so the pane with focus
    //  handles them and the window never takes them first.
    TEST_METHOD (Keys_CopyAndSelectAllAreLeftToTheFocusedPane)
    {
        Assert::AreEqual (0, CassqueCommands::TranslateKey (WPARAM ('C'), true, false, false));
        Assert::AreEqual (0, CassqueCommands::TranslateKey (WPARAM ('A'), true, false, false));
        Assert::AreEqual ((int) CassqueCommands::kGoToOffset,
                          CassqueCommands::TranslateKey (WPARAM ('G'), true, false, false));
    }


    TEST_METHOD (Handlers_ReceiveTheCommandsOwnId)
    {
        std::vector<int>  dispatched;
        CassqueCommands   commands ({
            [&dispatched] (int id) { dispatched.push_back (id); },
            []            (int id) { return id != CassqueCommands::kBack; },
            []            (int id) { return id == CassqueCommands::kThemeDark; } });

        commands.Find (CassqueCommands::kRefresh)->dispatch();

        Assert::AreEqual ((size_t) 1, dispatched.size());
        Assert::AreEqual ((int) CassqueCommands::kRefresh, dispatched[0]);

        Assert::IsFalse (commands.Find (CassqueCommands::kBack)->IsEnabled());
        Assert::IsTrue  (commands.Find (CassqueCommands::kUp)->IsEnabled());
        Assert::IsTrue  (commands.Find (CassqueCommands::kThemeDark)->IsChecked());
        Assert::IsFalse (commands.Find (CassqueCommands::kThemeLight)->IsChecked());
    }


    TEST_METHOD (MissingHandlers_LeaveRowsEnabledAndInert)
    {
        CassqueCommands  commands ({});

        commands.Find (CassqueCommands::kExit)->dispatch();

        Assert::IsTrue  (commands.Find (CassqueCommands::kExit)->IsEnabled());
        Assert::IsFalse (commands.Find (CassqueCommands::kTogglePreview)->IsChecked());
        Assert::IsNull  (commands.Find (12345));
    }


    TEST_METHOD (TranslateKey_MatchesModifiersExactly)
    {
        Assert::AreEqual ((int) CassqueCommands::kRefresh,       CassqueCommands::TranslateKey (VK_F5,   false, false, false));
        Assert::AreEqual ((int) CassqueCommands::kTogglePreview, CassqueCommands::TranslateKey ('P',     false, true,  false));
        Assert::AreEqual ((int) CassqueCommands::kBack,          CassqueCommands::TranslateKey (VK_LEFT, false, true,  false));
        Assert::AreEqual (0, CassqueCommands::TranslateKey ('P',     false, false, false));
        Assert::AreEqual (0, CassqueCommands::TranslateKey (VK_LEFT, false, false, false));
        Assert::AreEqual (0, CassqueCommands::TranslateKey (VK_F5,   true,  false, false));
    }


    TEST_METHOD (Toolbar_ShowsNavigationRefreshNewTabAndPreview)
    {
        CassqueCommands                  commands ({});
        std::vector<DxuiToolbar::Entry>  entries = commands.BuildToolbarEntries();

        Assert::AreEqual ((size_t) 6, entries.size());
        Assert::AreEqual ((int) CassqueCommands::kBack, entries[0].command->id);
        Assert::IsTrue   (entries[5].kind == DxuiToolbar::Kind::Toggle);
        Assert::AreEqual ((int) CassqueCommands::kTogglePreview, entries[5].command->id);

        for (const DxuiToolbar::Entry & entry : entries)
        {
            Assert::IsNotNull (entry.command->glyph);
            Assert::IsFalse   (entry.command->tip.empty());
        }

        //  Back, Forward, Up and Refresh are one group of bare icons, as
        //  Explorer draws them; the new tab button keeps its label and starts
        //  a group of its own.
        for (size_t i = 0; i < 4; i++)
        {
            Assert::IsTrue   (entries[i].iconOnly,              L"The navigation buttons are icons alone");
            Assert::AreEqual (entries[0].group, entries[i].group, L"in one evenly spaced group");
        }

        Assert::IsFalse (entries[4].iconOnly, L"The new tab button keeps its label");
        Assert::IsTrue  (entries[3].group != entries[4].group);
    }
};
