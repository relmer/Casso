#include "Pch.h"
#include "CassoExplorer/CassoExplorerCommands.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCommandsTests
//
//  The menu the table builds, the keys that reach commands, and that every
//  command's state and action come from the handlers with its own id.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoExplorerCommandsTests)
{
public:

    TEST_METHOD (Menu_HasFiveTitlesWithTheirRows)
    {
        CassoExplorerCommands               commands ({});
        std::vector<DxuiMenuBarItem>  items = commands.BuildMenuItems();

        Assert::AreEqual ((size_t) 5, items.size());
        Assert::AreEqual (std::wstring (L"&File"), items[0].label);
        Assert::AreEqual (std::wstring (L"&Edit"), items[1].label);
        Assert::AreEqual (std::wstring (L"&Help"), items[4].label);

        Assert::IsTrue (items[2].submenu[1].kind == DxuiPopupMenuItem::Kind::Separator);
        Assert::AreEqual ((int) CassoExplorerCommands::kTogglePreview, items[2].submenu[2].command->id);
        Assert::AreEqual (std::wstring (L"Alt+P"), items[2].submenu[2].command->accelerator);
    }


    //  Copy and select all display their keystrokes in the menu but are not in
    //  the key table: DxuiCommandRouter handles them for the focused pane.
    TEST_METHOD (Keys_CopyAndSelectAllAreLeftToTheFocusedPane)
    {
        Assert::AreEqual (0, CassoExplorerCommands::TranslateKey (WPARAM ('C'), true, false, false));
        Assert::AreEqual (0, CassoExplorerCommands::TranslateKey (WPARAM ('A'), true, false, false));
        Assert::AreEqual ((int) CassoExplorerCommands::kGoToOffset,
                          CassoExplorerCommands::TranslateKey (WPARAM ('G'), true, false, false));
    }


    TEST_METHOD (Handlers_ReceiveTheCommandsOwnId)
    {
        std::vector<int>  dispatched;
        CassoExplorerCommands   commands ({
            [&dispatched] (int id) { dispatched.push_back (id); },
            []            (int id) { return id != CassoExplorerCommands::kBack; },
            []            (int id) { return id == CassoExplorerCommands::kThemeDark; } });

        commands.Find (CassoExplorerCommands::kRefresh)->dispatch();

        Assert::AreEqual ((size_t) 1, dispatched.size());
        Assert::AreEqual ((int) CassoExplorerCommands::kRefresh, dispatched[0]);

        Assert::IsFalse (commands.Find (CassoExplorerCommands::kBack)->IsEnabled());
        Assert::IsTrue  (commands.Find (CassoExplorerCommands::kUp)->IsEnabled());
        Assert::IsTrue  (commands.Find (CassoExplorerCommands::kThemeDark)->IsChecked());
        Assert::IsFalse (commands.Find (CassoExplorerCommands::kThemeLight)->IsChecked());
    }


    TEST_METHOD (MissingHandlers_LeaveRowsEnabledAndInert)
    {
        CassoExplorerCommands  commands ({});

        commands.Find (CassoExplorerCommands::kExit)->dispatch();

        Assert::IsTrue  (commands.Find (CassoExplorerCommands::kExit)->IsEnabled());
        Assert::IsFalse (commands.Find (CassoExplorerCommands::kTogglePreview)->IsChecked());
        Assert::IsTrue  (commands.Find (12345) == nullptr);
    }


    TEST_METHOD (TranslateKey_MatchesModifiersExactly)
    {
        Assert::AreEqual ((int) CassoExplorerCommands::kRefresh,       CassoExplorerCommands::TranslateKey (VK_F5,   false, false, false));
        Assert::AreEqual ((int) CassoExplorerCommands::kTogglePreview, CassoExplorerCommands::TranslateKey ('P',     false, true,  false));
        Assert::AreEqual ((int) CassoExplorerCommands::kBack,          CassoExplorerCommands::TranslateKey (VK_LEFT, false, true,  false));
        Assert::AreEqual (0, CassoExplorerCommands::TranslateKey ('P',     false, false, false));
        Assert::AreEqual (0, CassoExplorerCommands::TranslateKey (VK_LEFT, false, false, false));
        Assert::AreEqual (0, CassoExplorerCommands::TranslateKey (VK_F5,   true,  false, false));
    }


    TEST_METHOD (Toolbar_ShowsNavigationAndCommandBarShowsExplorersCommands)
    {
        CassoExplorerCommands                  commands ({});
        std::vector<DxuiToolbar::Entry>  nav      = commands.BuildToolbarEntries();
        std::vector<DxuiToolbar::Entry>  bar      = commands.BuildCommandBarEntries();
        std::vector<int>                 ids;

        //  The address row: Back, Forward, Up and Refresh, one group of icons.
        Assert::AreEqual ((size_t) 4, nav.size());
        Assert::AreEqual ((int) CassoExplorerCommands::kBack, nav[0].command->id);

        for (const DxuiToolbar::Entry & entry : nav)
        {
            Assert::IsTrue   (entry.iconOnly, L"The navigation buttons are icons alone");
            Assert::AreEqual (nav[0].group, entry.group, L"in one evenly spaced group");
        }

        for (const DxuiToolbar::Entry & entry : bar)
        {
            ids.push_back (entry.command->id);

            Assert::IsNotNull (entry.command->glyph);
            Assert::AreNotEqual ((int) CassoExplorerCommands::kNewTab, entry.command->id, L"A new tab opens from the tab strip");
            Assert::IsFalse   (entry.command->tip.empty());
        }

        //  The command bar, in Explorer's order, then the preview toggle and
        //  the theme at the far end.
        Assert::IsTrue (ids == std::vector<int> { CassoExplorerCommands::kNew,
                                                  CassoExplorerCommands::kCutItems, CassoExplorerCommands::kCopyItems, CassoExplorerCommands::kPasteItems,
                                                  CassoExplorerCommands::kRenameItem, CassoExplorerCommands::kDeleteItems,
                                                  CassoExplorerCommands::kSort, CassoExplorerCommands::kView, CassoExplorerCommands::kAbout,
                                                  CassoExplorerCommands::kTogglePreview, CassoExplorerCommands::kTheme });

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
