#include "Pch.h"

#include "Ui/Settings/ControllersPage.h"
#include "Ui/Settings/ControllersPageState.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ControllersPageLayoutTests
//
//  The Controllers page's layout: its reported content height, which is what
//  lets the Settings sheet scroll it, the Joyport switch and its labels, and
//  the two players' rows. The Joyport and the players sit above the rest, so
//  the page runs taller than the sheet, and a height that stops short of
//  Reset profile leaves the bottom rows under the button row with no way to
//  reach them.
//
//  Each page is built with its mode and its players already set before its
//  first layout, so nothing here reads the system's animation setting.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ControllersPageLayoutTests)
{
public:

    static constexpr UINT    kDpi              = 96;
    static constexpr int     kPagePadPx        = 16;     // the page's own padding at 96 DPI
    static constexpr int     kLeftPx           = 20;
    static constexpr int     kTopPx            = 70;
    static constexpr int     kRightPx          = 780;
    static constexpr int     kBottomPx         = 600;
    static constexpr size_t  kPaddleAxes       = 4;
    static constexpr float   kLevelTolerancePx = 1.0f;   // a rect's middle rounds to whole pixels
    static constexpr float   kHalf             = 0.5f;


    static ControllerDeviceInfo MakeStick()
    {
        ControllerDeviceInfo  info;



        info.unit.model  = { ControllerKind::DirectInput, 0x231d, 0x0121 };
        info.unit.unitId = "{STICK}";
        info.unit.source = ControllerUnitSource::InstanceGuid;
        info.description = L"VKBsim Gladiator";
        info.controls    = { { ControlKind::Axis, 0 }, { ControlKind::Axis, 1 },
                             { ControlKind::Button, 0 }, { ControlKind::Button, 1 }, { ControlKind::Button, 2 } };
        return info;
    }


    //  Lays the page out once in the given mode, on a machine that can take
    //  the Joyport, with the players given, and returns its reported content
    //  height.
    static int LayOutPage (ControllersPage & page, ControllersPageState & state, bool isAtariMode, const PlayerEntries & entries = PlayerEntries())
    {
        DxuiDpiScaler  scaler;



        state.Load ({ MakeStick() }, {}, {}, true);
        state.SetJoyportInEffect (isAtariMode);
        state.SetPlayers (entries, PlayerSlots(), kPaddleAxes);

        page.SetJoyportFns ([isAtariMode] () { return isAtariMode; }, [] () { return true; }, [] (bool) {});
        page.SetState      (&state);

        scaler.SetDpi (kDpi);
        page.Layout   (RECT { kLeftPx, kTopPx, kRightPx, kBottomPx }, scaler);

        return page.GetContentHeightPx();
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


    //  The Joyport's switch, the page's only toggle.
    static const DxuiToggle * FindSwitch (const ControllersPage & page)
    {
        const DxuiToggle  * toggle = nullptr;
        const DxuiToggle  * found  = nullptr;
        size_t              i      = 0;



        for (i = 0; i < page.GetChildCount(); ++i)
        {
            toggle = dynamic_cast<const DxuiToggle *> (page.GetChild (i));

            if (toggle != nullptr)
            {
                found = toggle;
            }
        }

        return found;
    }


    static float GetMiddleX (const RECT & rect)
    {
        return (float) (rect.left + rect.right) * kHalf;
    }


    static float GetMiddleY (const RECT & rect)
    {
        return (float) (rect.top + rect.bottom) * kHalf;
    }


    //  "Apple (rear)" above the switch and "Atari (front)" below it, each
    //  centered on it side to side, in either mode.
    static void AssertPositionLabelsAboveAndBelowTheSwitch (bool isAtariMode, const wchar_t * mode)
    {
        ControllersPage             page;
        ControllersPageState        state;
        const DxuiToggle          * joyportSwitch = nullptr;
        const DxuiLabel           * apple         = nullptr;
        const DxuiLabel           * atari         = nullptr;
        DxuiToggle::TrackAndThumb   pill;
        float                       pillMidX      = 0.0f;



        LayOutPage (page, state, isAtariMode);

        joyportSwitch = FindSwitch (page);
        apple         = FindLabel  (page, L"Apple (rear)");
        atari         = FindLabel  (page, L"Atari (front)");

        Assert::IsNotNull (joyportSwitch, mode);
        Assert::IsNotNull (apple,         L"Apple (rear) is shown in either mode");
        Assert::IsNotNull (atari,         L"Atari (front) is shown in either mode");

        pill     = joyportSwitch->GetTrackAndThumb (false);
        pillMidX = (pill.track.left + pill.track.right) * kHalf;

        Assert::IsTrue   ((float) apple->GetBounds().bottom <= pill.track.top + kLevelTolerancePx,    L"Apple (rear) above the switch");
        Assert::IsTrue   ((float) atari->GetBounds().top    >= pill.track.bottom - kLevelTolerancePx, L"Atari (front) below it");
        Assert::AreEqual (pillMidX, GetMiddleX (apple->GetBounds()), kLevelTolerancePx, L"Apple (rear) centered on the switch");
        Assert::AreEqual (pillMidX, GetMiddleX (atari->GetBounds()), kLevelTolerancePx, L"Atari (front) centered on the switch");
    }


    TEST_METHOD (JoyportPositionLabels_AreAboveAndBelowTheSwitchCenteredOnIt)
    {
        AssertPositionLabelsAboveAndBelowTheSwitch (false, L"Apple mode");
        AssertPositionLabelsAboveAndBelowTheSwitch (true,  L"Atari mode");
    }


    TEST_METHOD (JoyportHeading_SitsLeftOfTheSwitchCenteredOnIt)
    {
        ControllersPage             page;
        ControllersPageState        state;
        const DxuiToggle          * joyportSwitch = nullptr;
        const DxuiLabel           * heading       = nullptr;
        DxuiToggle::TrackAndThumb   pill;



        LayOutPage (page, state, false);

        joyportSwitch = FindSwitch (page);
        heading       = FindLabel  (page, L"Joyport");

        Assert::IsNotNull (joyportSwitch, L"the switch");
        Assert::IsNotNull (heading,       L"the section's label");

        pill = joyportSwitch->GetTrackAndThumb (false);

        Assert::AreEqual (GetMiddleY (heading->GetBounds()), (pill.track.top + pill.track.bottom) * kHalf, kLevelTolerancePx, L"centered on the switch top to bottom");
        Assert::IsTrue   (heading->GetBounds().right <= joyportSwitch->GetBounds().left,                                   L"to the left of the switch");
    }


    //  Both players' rows are always there, each with a Joystick / Paddle
    //  choice and a note of what the player drives. There is no checkbox:
    //  Player 2's entry lists Disabled.
    TEST_METHOD (PlayerRows_AreBothShownWithAModeAndANote)
    {
        ControllersPage                    page;
        ControllersPageState               state;
        PlayerEntries                      entries;
        std::vector<const DxuiComboBox *>  modes;
        const DxuiCheckbox               * checkbox = nullptr;
        size_t                             i        = 0;



        entries[1].mode = PlayerMode::Paddle;
        LayOutPage (page, state, false, entries);
        modes = FindCombos (page, L"Joystick");

        Assert::AreEqual (size_t (2), modes.size(),                   L"a mode for each player");
        Assert::AreEqual (0, modes[0]->GetSelectedIndex(),            L"Player 1 in Joystick mode");
        Assert::AreEqual (1, modes[1]->GetSelectedIndex(),            L"Player 2 in Paddle mode");
        Assert::IsTrue   (modes[0]->IsEnabled() && modes[1]->IsEnabled(), L"both can be chosen in Apple mode");
        Assert::IsNotNull (FindLabel (page, L"joystick 0"),            L"Player 1's note");
        Assert::IsNotNull (FindLabel (page, L"paddle 2"),              L"Player 2's note, beside a joystick");
        Assert::AreEqual (std::wstring (L"Disabled"), FindCombos (page, L"Automatic")[1]->GetItems().back(), L"Player 2's entries end in Disabled");

        for (i = 0; i < page.GetChildCount(); ++i)
        {
            checkbox = dynamic_cast<const DxuiCheckbox *> (page.GetChild (i));

            Assert::IsFalse (checkbox != nullptr && checkbox->IsVisible() && checkbox->GetLabel() == L"Multiplayer", L"and no Multiplayer checkbox");
        }
    }


    //  In Atari mode both players are Atari sticks: the mode shows Joystick
    //  and cannot be chosen, the notes give the jacks, and Player 2's
    //  Disabled reads "same as left".
    TEST_METHOD (PlayerRows_InAtariModeGiveTheJacks)
    {
        ControllersPage                    page;
        ControllersPageState               state;
        PlayerEntries                      entries;
        std::vector<const DxuiComboBox *>  modes;



        entries[1].mode = PlayerMode::Paddle;
        LayOutPage (page, state, true, entries);
        modes = FindCombos (page, L"Joystick");

        Assert::AreEqual (size_t (2), modes.size());
        Assert::IsFalse  (modes[0]->IsEnabled() || modes[1]->IsEnabled(), L"the mode cannot be chosen");
        Assert::AreEqual (0, modes[1]->GetSelectedIndex(),                 L"and shows Joystick");
        Assert::IsNotNull (FindLabel (page, L"left jack"),                 L"Player 1's note");
        Assert::IsNotNull (FindLabel (page, L"right jack"),                L"Player 2's note");
        Assert::AreEqual (std::wstring (L"same as left"), FindCombos (page, L"Automatic")[1]->GetItems().back(), L"after the colon of Player 2:");
    }


    static void AssertHeightReachesLastRow (bool isAtariMode, const wchar_t * mode)
    {
        ControllersPage       page;
        ControllersPageState  state;
        int                   heightPx = LayOutPage (page, state, isAtariMode);
        int                   lowest   = GetLowestVisibleBottom (page);



        Assert::IsTrue   (lowest > kTopPx, mode);
        Assert::AreEqual (lowest + kPagePadPx - kTopPx, heightPx, mode);
    }


    TEST_METHOD (ContentHeight_ReachesResetProfileAndItsPadding_InEveryMode)
    {
        AssertHeightReachesLastRow (false, L"Apple mode");
        AssertHeightReachesLastRow (true,  L"Atari mode");
    }


    TEST_METHOD (ContentHeight_RunsPastTheRectTheSheetGivesAtItsDesignSize)
    {
        ControllersPage       page;
        ControllersPageState  state;
        int                   heightPx = LayOutPage (page, state, false);



        Assert::IsTrue (heightPx > kBottomPx - kTopPx, L"Apple mode does not fit, so the sheet has to scroll it");
    }


    //  Atari mode drops the rows the Joyport does not read. The page is not
    //  shorter for it: it edits the Joyport profile there, whose steering
    //  and fire take more rows than the Default's.
    TEST_METHOD (AtariMode_DropsPb1AndPb2)
    {
        ControllersPage       applePage;
        ControllersPage       atariPage;
        ControllersPageState  appleState;
        ControllersPageState  atariState;



        LayOutPage (applePage, appleState, false);
        LayOutPage (atariPage, atariState, true);

        Assert::IsNotNull (FindLabel (applePage, L"PB1:"), L"Apple mode shows PB1");
        Assert::IsNotNull (FindLabel (applePage, L"PB2:"), L"and PB2");
        Assert::IsNull    (FindLabel (atariPage, L"PB1:"), L"Atari mode drops PB1");
        Assert::IsNull    (FindLabel (atariPage, L"PB2:"), L"and PB2");
        Assert::IsNotNull (FindLabel (atariPage, L"Fire:"), L"and keeps fire");
    }
};