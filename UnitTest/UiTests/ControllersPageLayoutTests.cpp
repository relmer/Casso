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

    static constexpr UINT    kDpi        = 96;
    static constexpr int     kPagePadPx  = 16;     // the page's own padding at 96 DPI
    static constexpr int     kLeftPx     = 20;
    static constexpr int     kTopPx      = 70;
    static constexpr int     kRightPx    = 780;
    static constexpr int     kBottomPx   = 600;
    static constexpr size_t  kPaddleAxes = 4;


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
