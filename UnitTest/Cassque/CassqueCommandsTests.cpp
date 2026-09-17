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


    //  Copy and select all display their keystrokes in the menu but are not in
    //  the key table: DxuiCommandRouter handles them for the focused pane.
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
        Assert::IsTrue  (commands.Find (12345) == nullptr);
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


    TEST_METHOD (Toolbar_ShowsNavigationAndCommandBarShowsExplorersCommands)
    {
        CassqueCommands                  commands ({});
        std::vector<DxuiToolbar::Entry>  nav      = commands.BuildToolbarEntries();
        std::vector<DxuiToolbar::Entry>  bar      = commands.BuildCommandBarEntries();
        std::vector<int>                 ids;

        //  The address row: Back, Forward, Up and Refresh, one group of icons.
        Assert::AreEqual ((size_t) 4, nav.size());
        Assert::AreEqual ((int) CassqueCommands::kBack, nav[0].command->id);

        for (const DxuiToolbar::Entry & entry : nav)
        {
            Assert::IsTrue   (entry.iconOnly, L"The navigation buttons are icons alone");
            Assert::AreEqual (nav[0].group, entry.group, L"in one evenly spaced group");
        }

        for (const DxuiToolbar::Entry & entry : bar)
        {
            ids.push_back (entry.command->id);

            Assert::IsNotNull (entry.command->glyph);
            Assert::AreNotEqual ((int) CassqueCommands::kNewTab, entry.command->id, L"A new tab opens from the tab strip");
            Assert::IsFalse   (entry.command->tip.empty());
        }

        //  The command bar, in Explorer's order, then the preview toggle and
        //  the theme at the far end.
        Assert::IsTrue (ids == std::vector<int> { CassqueCommands::kNew,
                                                  CassqueCommands::kCutItems, CassqueCommands::kCopyItems, CassqueCommands::kPasteItems,
                                                  CassqueCommands::kRenameItem, CassqueCommands::kDeleteItems,
                                                  CassqueCommands::kSort, CassqueCommands::kView, CassqueCommands::kAbout,
                                                  CassqueCommands::kTogglePreview, CassqueCommands::kTheme });

        for (size_t i = 1; i < 6; i++)
        {
            Assert::IsTrue   (bar[i].iconOnly, L"The clipboard, Rename and Delete are icons alone");
            Assert::AreEqual (bar[1].group, bar[i].group);
        }

        Assert::IsTrue  (bar[0].kind == DxuiToolbar::Kind::DropDown, L"New opens its choices");
        Assert::IsTrue  (bar[6].kind == DxuiToolbar::Kind::DropDown && bar[7].kind == DxuiToolbar::Kind::DropDown);
        Assert::IsTrue  (bar[8].seeMoreOnly, L"About lives in See more");
        Assert::IsTrue  (bar[9].trailing && bar[10].trailing, L"The preview toggle and the theme sit at the far end");
        Assert::IsFalse (bar[7].trailing);
    }};
