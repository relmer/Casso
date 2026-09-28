#include "Pch.h"

#include "Ui/Settings/ControllersPage.h"
#include "Ui/Settings/ControllersPageState.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ControllersPageLayoutTests
//
//  The Controllers page's layout: its reported content height, which is what
//  lets the Settings sheet scroll it, and the two players' rows, with the
//  warning under a player whose buttons the Joyport has taken. The players
//  sit above the rest, so the page runs taller than the sheet, and a height
//  that stops short of Reset profile leaves the bottom rows under the button
//  row with no way to reach them.
//
//  Each page is built with its players already set before its first layout,
//  so nothing here reads the system's animation setting.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ControllersPageLayoutTests)
{
public:

    static constexpr UINT    kDpi        = 96;
    static constexpr int     kPagePadPx  = 16;     // the page's own padding at 96 DPI
    static constexpr int     kLeftPx     = 20;
    static constexpr int     kTopPx      = 70;
    static constexpr int     kRightPx    = 780;
    static constexpr int     kBottomPx   = 600;
    static constexpr size_t  kPaddleAxes = 4;

    static constexpr const wchar_t *  kpszPressToAssign = L"Press to assign...";
    static constexpr const wchar_t *  kpszCutWarning    = L"This controller's buttons are disabled because Player 1 is using the Joyport.";


    static ControllerDeviceInfo MakeStick (const char * pszUnit = "{STICK}")
    {
        ControllerDeviceInfo  info;



        info.unit.model  = { ControllerKind::DirectInput, 0x231d, 0x0121 };
        info.unit.unitId = pszUnit;
        info.unit.source = ControllerUnitSource::InstanceGuid;
        info.description = L"VKBsim Gladiator";
        info.controls    = { { ControlKind::Axis, 0 }, { ControlKind::Axis, 1 },
                             { ControlKind::Button, 0 }, { ControlKind::Button, 1 }, { ControlKind::Button, 2 } };
        return info;
    }


    //  Player 1 on the first stick and Player 2 on the second, both playing.
    static PlayerSlots MakeTwoPlaying()
    {
        PlayerSlots  slots;



        slots[0].state  = PlayerSlotState::Playing;
        slots[0].holder = MakeStick ("{A}").unit;
        slots[1].state  = PlayerSlotState::Playing;
        slots[1].holder = MakeStick ("{B}").unit;
        return slots;
    }


    //  Lays the page out once on a machine that can take the Joyport, with
    //  two sticks attached and the players given, editing the controller at
    //  `edited`, and returns its reported content height.
    static int LayOutPage (ControllersPage       & page,
                           ControllersPageState  & state,
                           const PlayerEntries   & entries = PlayerEntries(),
                           const PlayerSlots     & slots   = MakeTwoPlaying(),
                           size_t                  edited  = 0)
    {
        DxuiDpiScaler  scaler;



        state.Load ({ MakeStick ("{A}"), MakeStick ("{B}") }, {}, {}, true);
        state.SetJoyportAvailable (true);
        state.SetPlayers          (entries, slots, kPaddleAxes);
        state.SelectController    (edited);

        page.SetState (&state);

        scaler.SetDpi (kDpi);
        page.Layout   (RECT { kLeftPx, kTopPx, kRightPx, kBottomPx }, scaler);

        return page.GetContentHeightPx();
    }


    //  Player 1 in the Joyport's left jack and Player 2 beside it in the
    //  given mode.
    static PlayerEntries MakeBesideTheJoyport (PlayerMode secondMode)
    {
        PlayerEntries  entries;



        entries[0].mode = PlayerMode::JoyportLeft;
        entries[1].mode = secondMode;
        return entries;
    }


    //  The bottom edge of the lowest control the page shows.
    static int GetLowestVisibleBottom (const ControllersPage & page)
    {
        const IDxuiControl  * child  = nullptr;
        int                   lowest = 0;
        size_t                i      = 0;



        for (i = 0; i < page.GetChildCount(); ++i)
        {
            child = page.GetChild (i);

            if (child != nullptr && child->IsVisible())
            {
                lowest = std::max (lowest, (int) child->GetBounds().bottom);
            }
        }

        return lowest;
    }


    //  The shown label with the given text, or null.
    static const DxuiLabel * FindLabel (const ControllersPage & page, const std::wstring & text)
    {
        const DxuiLabel  * label = nullptr;
        const DxuiLabel  * found = nullptr;
        size_t             i     = 0;



        for (i = 0; i < page.GetChildCount(); ++i)
        {
            label = dynamic_cast<const DxuiLabel *> (page.GetChild (i));

            if (label != nullptr && label->IsVisible() && label->GetText() == text)
            {
                found = label;
            }
        }

        return found;
    }


    //  The shown warnings, in page order.
    static std::vector<const DxuiInfoBanner *> FindWarnings (const ControllersPage & page)
    {
        const DxuiInfoBanner                 * banner = nullptr;
        std::vector<const DxuiInfoBanner *>    found;
        size_t                                 i      = 0;



        for (i = 0; i < page.GetChildCount(); ++i)
        {
            banner = dynamic_cast<const DxuiInfoBanner *> (page.GetChild (i));

            if (banner != nullptr && banner->IsVisible())
            {
                found.push_back (banner);
            }
        }

        return found;
    }


    //  The shown drop-downs that offer the given first item, in page order.
    static std::vector<const DxuiComboBox *> FindCombos (const ControllersPage & page, const std::wstring & firstItem)
    {
        const DxuiComboBox                 * combo = nullptr;
        std::vector<const DxuiComboBox *>    found;
        size_t                               i     = 0;



        for (i = 0; i < page.GetChildCount(); ++i)
        {
            combo = dynamic_cast<const DxuiComboBox *> (page.GetChild (i));

            if (combo != nullptr && combo->IsVisible() && !combo->GetItems().empty() && combo->GetItems()[0] == firstItem)
            {
                found.push_back (combo);
            }
        }

        return found;
    }


    //  How many shown binding rows can and cannot be edited.
    static std::pair<size_t, size_t> CountBindingRows (const ControllersPage & page)
    {
        std::pair<size_t, size_t>  counts;



        for (const DxuiComboBox * row : FindCombos (page, kpszPressToAssign))
        {
            (row->IsEnabled() ? counts.first : counts.second)++;
        }

        return counts;
    }


    //  Both players' rows are always there, each with its mode and a note of
    //  what the player drives. Player 1's modes start at Joystick and Player
    //  2's at Same as Player 1. There is no checkbox: Player 2's entry lists
    //  Disabled.
    TEST_METHOD (PlayerRows_AreBothShownWithAModeAndANote)
    {
        ControllersPage          page;
        ControllersPageState     state;
        PlayerEntries            entries;
        const DxuiComboBox     * one      = nullptr;
        const DxuiComboBox     * two      = nullptr;
        const DxuiCheckbox     * checkbox = nullptr;
        size_t                   i        = 0;



        entries[1].mode = PlayerMode::Paddle;
        LayOutPage (page, state, entries, PlayerSlots());

        Assert::AreEqual (size_t (1), FindCombos (page, L"Joystick").size(),         L"Player 1's modes");
        Assert::AreEqual (size_t (1), FindCombos (page, L"Same as Player 1").size(), L"Player 2's modes");

        one = FindCombos (page, L"Joystick")[0];
        two = FindCombos (page, L"Same as Player 1")[0];

        Assert::IsTrue   (one->GetItems() == std::vector<std::wstring> { L"Joystick", L"Joyport left (Atari)", L"Joyport right (Atari)", L"Paddle" });
        Assert::AreEqual (0, one->GetSelectedIndex(),                  L"Player 1 in Joystick mode");
        Assert::AreEqual (std::wstring (L"Paddle"), two->GetItems()[(size_t) two->GetSelectedIndex()], L"Player 2 in Paddle mode");
        Assert::IsNotNull (FindLabel (page, L"Player 1:"),             L"the row shows the player, not a jack");
        Assert::IsNotNull (FindLabel (page, L"joystick 0"),            L"Player 1's note");
        Assert::IsNotNull (FindLabel (page, L"paddle 2"),              L"Player 2's note, beside a joystick");
        Assert::IsTrue    (FindWarnings (page).empty(),                L"no Joyport, no warning");
        Assert::AreEqual (size_t (2), FindCombos (page, L"Automatic").size(), L"each player's entries");
        Assert::AreEqual (std::wstring (L"Disabled"), FindCombos (page, L"Automatic")[1]->GetItems().back(), L"Player 2's entries end in Disabled");

        for (i = 0; i < page.GetChildCount(); ++i)
        {
            checkbox = dynamic_cast<const DxuiCheckbox *> (page.GetChild (i));

            Assert::IsFalse (checkbox != nullptr && checkbox->IsVisible() && checkbox->GetLabel() == L"Multiplayer", L"and no Multiplayer checkbox");
        }
    }


    //  Player 1 in the left jack: its note gives the jacks it drives, Player
    //  2's list shows the left jack and cannot choose it, and Player 2, on
    //  Paddle beside the Joyport, has a warning under its row and its
    //  button rows disabled.
    TEST_METHOD (PlayerRows_BesideTheJoyportWarnAndDisableTheButtons)
    {
        ControllersPage                        page;
        ControllersPageState                   state;
        const DxuiComboBox                   * two      = nullptr;
        std::vector<const DxuiInfoBanner *>    warnings;
        std::pair<size_t, size_t>              rows;



        LayOutPage (page, state, MakeBesideTheJoyport (PlayerMode::Paddle), MakeTwoPlaying(), 1);

        two      = FindCombos (page, L"Same as Player 1")[0];
        warnings = FindWarnings (page);
        rows     = CountBindingRows (page);

        Assert::IsNotNull (FindLabel (page, L"both jacks"), L"Player 1 alone in the Joyport drives both jacks");
        Assert::IsNotNull (FindLabel (page, L"paddle 0"),   L"and Player 2 plays its paddle as though alone");
        Assert::IsFalse   (two->IsItemEnabled (2),          L"the left jack is Player 1's");
        Assert::IsTrue    (two->IsItemEnabled (3),          L"the right jack is free");

        Assert::AreEqual (size_t (1), warnings.size(), L"one warning, under Player 2");
        Assert::AreEqual (std::wstring (kpszCutWarning), warnings[0]->GetText());
        Assert::IsTrue   (warnings[0]->GetSeverity() == DxuiInfoBanner::Severity::Warning, L"with the warning triangle");
        Assert::IsTrue   (warnings[0]->GetBounds().top >= FindCombos (page, L"Same as Player 1")[0]->GetBounds().bottom, L"under Player 2's row");

        Assert::IsTrue (rows.first  > 0, L"its paddle row can be edited");
        Assert::IsTrue (rows.second > 0, L"and its button row cannot");
    }


    //  The same player beside a Joystick Player 1 keeps its buttons.
    TEST_METHOD (PlayerRows_WithoutTheJoyportKeepTheButtons)
    {
        ControllersPage            page;
        ControllersPageState       state;
        PlayerEntries              entries;
        std::pair<size_t, size_t>  rows;



        entries[1].mode = PlayerMode::Paddle;
        LayOutPage (page, state, entries, MakeTwoPlaying(), 1);
        rows = CountBindingRows (page);

        Assert::IsTrue   (rows.first > 0);
        Assert::AreEqual (size_t (0), rows.second, L"every row it shows can be edited");
    }


    static void AssertHeightReachesLastRow (const PlayerEntries & entries, size_t edited, const wchar_t * mode)
    {
        ControllersPage       page;
        ControllersPageState  state;
        int                   heightPx = LayOutPage (page, state, entries, MakeTwoPlaying(), edited);
        int                   lowest   = GetLowestVisibleBottom (page);



        Assert::IsTrue   (lowest > kTopPx, mode);
        Assert::AreEqual (lowest + kPagePadPx - kTopPx, heightPx, mode);
    }


    TEST_METHOD (ContentHeight_ReachesResetProfileAndItsPadding_InEveryMode)
    {
        AssertHeightReachesLastRow (PlayerEntries(),                                   0, L"both on Joystick");
        AssertHeightReachesLastRow (MakeBesideTheJoyport (PlayerMode::SameAsPlayer1), 0, L"both in the Joyport");
        AssertHeightReachesLastRow (MakeBesideTheJoyport (PlayerMode::Paddle),        1, L"a Paddle beside the Joyport, with its warning");
    }


    TEST_METHOD (ContentHeight_RunsPastTheRectTheSheetGivesAtItsDesignSize)
    {
        ControllersPage       page;
        ControllersPageState  state;
        int                   heightPx = LayOutPage (page, state);



        Assert::IsTrue (heightPx > kBottomPx - kTopPx, L"both on Joystick does not fit, so the sheet has to scroll it");
    }


    //  A controller in a jack drops the rows the Joyport does not read. The
    //  page is not shorter for it: it edits the Joyport profile there, whose
    //  steering and fire take more rows than the Default's.
    TEST_METHOD (InAJack_DropsPb1AndPb2)
    {
        ControllersPage       joystickPage;
        ControllersPage       jackPage;
        ControllersPageState  joystickState;
        ControllersPageState  jackState;



        LayOutPage (joystickPage, joystickState, PlayerEntries(), PlayerSlots());
        LayOutPage (jackPage,     jackState,     MakeBesideTheJoyport (PlayerMode::SameAsPlayer1));

        Assert::IsNotNull (FindLabel (joystickPage, L"PB1:"),  L"a joystick shows PB1");
        Assert::IsNotNull (FindLabel (joystickPage, L"PB2:"),  L"and PB2");
        Assert::IsNull    (FindLabel (jackPage,     L"PB1:"),  L"a jack drops PB1");
        Assert::IsNull    (FindLabel (jackPage,     L"PB2:"),  L"and PB2");
        Assert::IsNotNull (FindLabel (jackPage,     L"Fire:"), L"and keeps fire");
    }
};
