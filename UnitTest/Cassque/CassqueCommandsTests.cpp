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

    TEST_METHOD (Menu_HasFourTitlesWithTheirRows)
    {
        CassqueCommands               commands ({});
        std::vector<DxuiMenuBarItem>  items = commands.BuildMenuItems();

        Assert::AreEqual ((size_t) 4, items.size());
        Assert::AreEqual (std::wstring (L"&File"), items[0].label);
        Assert::AreEqual (std::wstring (L"&Help"), items[3].label);

        Assert::IsTrue (items[1].submenu[1].kind == DxuiPopupMenuItem::Kind::Separator);
        Assert::AreEqual ((int) CassqueCommands::kTogglePreview, items[1].submenu[2].command->id);
        Assert::AreEqual (std::wstring (L"Alt+P"), items[1].submenu[2].command->accelerator);
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

        Assert::IsTrue (entries[2].group != entries[3].group);
    }
};
