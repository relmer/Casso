#include "Pch.h"

#include "Ui/Settings/ControllersPage.h"
#include "Ui/Settings/ControllersPageState.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ControllersPageLayoutTests
//
//  The Controllers page's reported content height, which is what lets the
//  Settings sheet scroll it. The Joyport and the players sit above the rest,
//  so the page runs taller than the sheet, and a height that stops short of
//  Reset profile leaves the bottom rows under the button row with no way to
//  reach them.
//
//  Each page is built with its mode and its Multiplayer box already set
//  before its first layout, which lays the rows out in place with no slide,
//  so nothing here reads the system's animation setting.
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
    //  the Joyport, and returns its reported content height.
    static int LayOutPage (ControllersPage & page, ControllersPageState & state, bool isAtariMode, bool isMultiplayer)
    {
        DxuiDpiScaler  scaler;



        state.Load ({ MakeStick() }, {}, {}, true);
        state.SetPlayers (PlayerEntries(), PlayerSlots(), kPaddleAxes);
        state.SetMultiplayerChecked (isMultiplayer);

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


    static float GetMiddleY (const RECT & rect)
    {
        return (float) (rect.top + rect.bottom) * kHalf;
    }


    static void AssertPositionLabelsLevelWithTheKnob (bool isAtariMode, const wchar_t * mode)
    {
        ControllersPage             page;
        ControllersPageState        state;
        const DxuiToggle          * joyportSwitch = nullptr;
        const DxuiLabel           * apple         = nullptr;
        const DxuiLabel           * atari         = nullptr;
        DxuiToggle::TrackAndThumb   up;
        DxuiToggle::TrackAndThumb   down;



        LayOutPage (page, state, isAtariMode, false);

        joyportSwitch = FindSwitch (page);
        apple         = FindLabel  (page, L"Apple (rear)");
        atari         = FindLabel  (page, L"Atari (front)");

        Assert::IsNotNull (joyportSwitch, mode);
        Assert::IsNotNull (apple,         L"Apple (rear) is shown in either mode");
        Assert::IsNotNull (atari,         L"Atari (front) is shown in either mode");

        up   = joyportSwitch->GetTrackAndThumb (false);
        down = joyportSwitch->GetTrackAndThumb (true);

        Assert::AreEqual (up.thumbCenter.y,   GetMiddleY (apple->GetBounds()), kLevelTolerancePx, L"Apple (rear) level with the knob's up position");
        Assert::AreEqual (down.thumbCenter.y, GetMiddleY (atari->GetBounds()), kLevelTolerancePx, L"Atari (front) level with the knob's down position");
        Assert::IsTrue    (up.thumbCenter.y < down.thumbCenter.y,                                  L"the knob's up position is above its down position");
        Assert::IsTrue    ((float) apple->GetBounds().left >= up.track.right,                      L"Apple (rear) to the right of the switch");
        Assert::IsTrue    ((float) atari->GetBounds().left >= up.track.right,                      L"Atari (front) to the right of the switch");
        Assert::IsTrue    (joyportSwitch->GetBounds().right <= apple->GetBounds().left,           L"the switch takes clicks on the pill, not on its labels");
    }


    TEST_METHOD (JoyportPositionLabels_AreLevelWithTheKnobsTwoPositions)
    {
        AssertPositionLabelsLevelWithTheKnob (false, L"Apple mode");
        AssertPositionLabelsLevelWithTheKnob (true,  L"Atari mode");
    }


    TEST_METHOD (JoyportHeading_SitsLeftOfTheSwitchCenteredOnIt)
    {
        ControllersPage             page;
        ControllersPageState        state;
        const DxuiToggle          * joyportSwitch = nullptr;
        const DxuiLabel           * heading       = nullptr;
        DxuiToggle::TrackAndThumb   pill;



        LayOutPage (page, state, false, false);

        joyportSwitch = FindSwitch (page);
        heading       = FindLabel  (page, L"Joyport");

        Assert::IsNotNull (joyportSwitch, L"the switch");
        Assert::IsNotNull (heading,       L"the section's label");

        pill = joyportSwitch->GetTrackAndThumb (false);

        Assert::AreEqual (GetMiddleY (heading->GetBounds()), (pill.track.top + pill.track.bottom) * kHalf, kLevelTolerancePx, L"centered on the switch top to bottom");
        Assert::IsTrue   (heading->GetBounds().right <= joyportSwitch->GetBounds().left,                                   L"to the left of the switch");
    }

    static void AssertHeightReachesLastRow (bool isAtariMode, bool isMultiplayer, const wchar_t * mode)
    {
        ControllersPage       page;
        ControllersPageState  state;
        int                   heightPx = LayOutPage (page, state, isAtariMode, isMultiplayer);
        int                   lowest   = GetLowestVisibleBottom (page);



        Assert::IsTrue   (lowest > kTopPx, mode);
        Assert::AreEqual (lowest + kPagePadPx - kTopPx, heightPx, mode);
    }


    TEST_METHOD (ContentHeight_ReachesResetProfileAndItsPadding_InEveryMode)
    {
        AssertHeightReachesLastRow (false, false, L"Apple mode, one player");
        AssertHeightReachesLastRow (false, true,  L"Apple mode, Multiplayer ticked");
        AssertHeightReachesLastRow (true,  false, L"Atari mode, one player");
        AssertHeightReachesLastRow (true,  true,  L"Atari mode, Multiplayer ticked");
    }


    TEST_METHOD (ContentHeight_RunsPastTheRectTheSheetGivesAtItsDesignSize)
    {
        ControllersPage       page;
        ControllersPageState  state;
        int                   heightPx = LayOutPage (page, state, false, true);



        Assert::IsTrue (heightPx > kBottomPx - kTopPx, L"Apple mode with two players does not fit, so the sheet has to scroll it");
    }


    TEST_METHOD (ContentHeight_GrowsWithPlayerTwosRow)
    {
        ControllersPage       apartPage;
        ControllersPage       tickedPage;
        ControllersPageState  apartState;
        ControllersPageState  tickedState;
        int                   apartPx  = LayOutPage (apartPage,  apartState,  false, false);
        int                   tickedPx = LayOutPage (tickedPage, tickedState, false, true);



        Assert::IsTrue (tickedPx > apartPx, L"ticking Multiplayer adds Player 2's row");
    }


    TEST_METHOD (ContentHeight_ShorterInAtariMode)
    {
        ControllersPage       applePage;
        ControllersPage       atariPage;
        ControllersPageState  appleState;
        ControllersPageState  atariState;
        int                   applePx = LayOutPage (applePage, appleState, false, true);
        int                   atariPx = LayOutPage (atariPage, atariState, true,  true);



        Assert::IsTrue (atariPx < applePx, L"Atari mode drops PB1, PB2 and the paddle options");
    }
};
