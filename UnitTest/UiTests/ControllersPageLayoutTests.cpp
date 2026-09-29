#include "Pch.h"

#include "Ui/Settings/ControllersPage.h"
#include "Ui/Settings/ControllersPageState.h"
#include "../Dxui/MockDxuiTextRenderer.h"

// A ControllersPage holds every control on the page, about 16 KB, and most
// tests here build one or more in the test frame, which trips C6262. The page
// is the system under test -- suppress for this file.
#pragma warning (disable: 6262)

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


    static ControllerDeviceInfo MakeStick (const char * pszUnit = "{STICK}", ControllerFormFactor formFactor = ControllerFormFactor::Gamepad)
    {
        ControllerDeviceInfo  info;



        info.formFactor  = formFactor;
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
    //  `edited`, in a page whose right edge is `right`, and returns its
    //  reported content height. Both sticks are of the form factor `form`.
    static int LayOutPage (ControllersPage       & page,
                           ControllersPageState  & state,
                           const PlayerEntries   & entries = PlayerEntries(),
                           const PlayerSlots     & slots   = MakeTwoPlaying(),
                           size_t                  edited  = 0,
                           int                     right   = kRightPx,
                           ControllerFormFactor    form    = ControllerFormFactor::Gamepad)
    {
        DxuiDpiScaler  scaler;



        state.Load ({ MakeStick ("{A}", form), MakeStick ("{B}", form) }, {}, {}, true);
        state.SetJoyportAvailable (true);
        state.SetPlayers          (entries, slots, kPaddleAxes);
        state.SelectController    (edited);

        page.SetState (&state);

        scaler.SetDpi (kDpi);
        page.Layout   (RECT { kLeftPx, kTopPx, right, kBottomPx }, scaler);

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


    //  The shown drop-downs that offer the given first item, in page order,
    //  those in a target's table included, whether or not scrolled into view.
    static std::vector<const DxuiComboBox *> FindCombos (const IDxuiControl & parent, const std::wstring & firstItem)
    {
        const IDxuiControl                 * child = nullptr;
        const DxuiComboBox                 * combo = nullptr;
        std::vector<const DxuiComboBox *>    found;
        size_t                               i     = 0;



        for (i = 0; i < parent.GetChildCount(); ++i)
        {
            child = parent.GetChild (i);
            combo = dynamic_cast<const DxuiComboBox *> (child);

            if (child == nullptr || !child->IsVisible())
            {
                continue;
            }

            if (combo != nullptr && !combo->GetItems().empty() && combo->GetItems()[0] == firstItem)
            {
                found.push_back (combo);
            }

            for (const DxuiComboBox * inner : FindCombos (*child, firstItem))
            {
                found.push_back (inner);
            }
        }

        return found;
    }


    //  The shown targets' tables, in page order.
    static std::vector<const DxuiScrollPanel *> FindTables (const ControllersPage & page)
    {
        const DxuiScrollPanel                 * table = nullptr;
        std::vector<const DxuiScrollPanel *>    found;
        size_t                                  i     = 0;



        for (i = 0; i < page.GetChildCount(); ++i)
        {
            table = dynamic_cast<const DxuiScrollPanel *> (page.GetChild (i));

            if (table != nullptr && table->IsVisible())
            {
                found.push_back (table);
            }
        }

        return found;
    }


    //  The shown "+" buttons, in page order.
    static std::vector<const DxuiButton *> FindAddButtons (const ControllersPage & page)
    {
        const DxuiButton                 * button = nullptr;
        std::vector<const DxuiButton *>    found;
        size_t                             i      = 0;



        for (i = 0; i < page.GetChildCount(); ++i)
        {
            button = dynamic_cast<const DxuiButton *> (page.GetChild (i));

            if (button != nullptr && button->IsVisible() && button->GetAccessibleName() == L"+")
            {
                found.push_back (button);
            }
        }

        return found;
    }


    //  Lays out a page editing one stick, with PB0 given `count` bindings.
    static void LayOutWithPb0Bindings (ControllersPage & page, ControllersPageState & state, size_t count)
    {
        constexpr int  kFirstSpareButton = 10;     // on no other target, so nothing is shared
        size_t         i                 = 0;



        LayOutPage (page, state, PlayerEntries(), PlayerSlots());

        while (state.GetMapping().pb0.size() > count)
        {
            state.RemoveBinding (PaddleTarget::Pb0, 0);
        }

        for (i = state.GetMapping().pb0.size(); i < count; i++)
        {
            state.AddButtonBinding (PaddleTarget::Pb0, { { ControlKind::Button, kFirstSpareButton + (int) i } });
        }

        page.Relayout();
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


    //  The shown button with the given label, or null.
    static const DxuiButton * FindButton (const ControllersPage & page, const std::wstring & label)
    {
        const DxuiButton  * button = nullptr;
        const DxuiButton  * found  = nullptr;
        size_t              i      = 0;



        for (i = 0; i < page.GetChildCount(); ++i)
        {
            button = dynamic_cast<const DxuiButton *> (page.GetChild (i));

            if (button != nullptr && button->IsVisible() && button->GetAccessibleName() == label)
            {
                found = button;
            }
        }

        return found;
    }


    //  Both players' rows are always there, each with its entry and its mode
    //  and no note beside them. Player 1's modes start at Joystick and Player
    //  2's at Automatic, showing Player 1's mode. There is no checkbox:
    //  Player 2's entry lists Disabled.
    TEST_METHOD (PlayerRows_AreBothShownWithAMode)
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
        Assert::AreEqual (size_t (1), FindCombos (page, L"Automatic (joystick)").size(), L"Player 2's modes");

        one = FindCombos (page, L"Joystick")[0];
        two = FindCombos (page, L"Automatic (joystick)")[0];

        Assert::IsTrue   (one->GetItems() == std::vector<std::wstring> { L"Joystick", L"Joyport left (Atari)", L"Joyport right (Atari)", L"Paddle", L"Two paddles" });
        Assert::AreEqual (0, one->GetSelectedIndex(),                  L"Player 1 in Joystick mode");
        Assert::AreEqual (std::wstring (L"Paddle"), two->GetItems()[(size_t) two->GetSelectedIndex()], L"Player 2 in Paddle mode");
        Assert::IsNotNull (FindLabel (page, L"Player 1:"),             L"the row shows the player, not a jack");
        Assert::IsNull    (FindLabel (page, L"joystick 0"),            L"no note beside Player 1's row");
        Assert::IsNull    (FindLabel (page, L"paddle 2"),              L"nor beside Player 2's");
        Assert::IsTrue    (FindWarnings (page).empty(),                L"no Joyport, no warning");
        Assert::AreEqual (size_t (2), FindCombos (page, L"Automatic").size(), L"each player's entries");
        Assert::AreEqual (std::wstring (L"Disabled"), FindCombos (page, L"Automatic")[1]->GetItems().back(), L"Player 2's entries end in Disabled");

        for (i = 0; i < page.GetChildCount(); ++i)
        {
            checkbox = dynamic_cast<const DxuiCheckbox *> (page.GetChild (i));

            Assert::IsFalse (checkbox != nullptr && checkbox->IsVisible() && checkbox->GetLabel() == L"Multiplayer", L"and no Multiplayer checkbox");
        }
    }


    //  The two drop-downs of each player's row reach the right edge of the
    //  Profile row's Delete... at the design width, and stretch with a page
    //  wider than that by as much, the entry keeping the larger share,
    //  without raising the content width the sheet may grow to. Each
    //  shortens a label too long for it in the middle, so its end stays
    //  visible.
    TEST_METHOD (PlayerRows_StretchToTheProfileRowAndWithTheSheet)
    {
        constexpr int          kDesignRightPx = kRightPx;
        constexpr int          kWideRightPx   = kRightPx + 240;
        ControllersPage        narrow;
        ControllersPage        wide;
        ControllersPageState   narrowState;
        ControllersPageState   wideState;
        const DxuiComboBox   * mode           = nullptr;
        const DxuiComboBox   * entry          = nullptr;
        const DxuiButton     * deleteButton   = nullptr;
        int                    narrowEntry    = 0;



        narrow.SetDesignWidthPx (kDesignRightPx - kLeftPx);
        wide.SetDesignWidthPx   (kDesignRightPx - kLeftPx);

        LayOutPage (narrow, narrowState, PlayerEntries(), PlayerSlots(), 0, kDesignRightPx);
        LayOutPage (wide,   wideState,   PlayerEntries(), PlayerSlots(), 0, kWideRightPx);

        mode         = FindCombos (narrow, L"Joystick")[0];
        entry        = FindCombos (narrow, L"Automatic")[0];
        deleteButton = FindButton (narrow, L"Delete...");
        narrowEntry  = entry->GetBounds().right - entry->GetBounds().left;

        Assert::IsNotNull (deleteButton);
        Assert::AreEqual ((int) deleteButton->GetBounds().right, (int) mode->GetBounds().right, L"the mode reaches Delete...'s right edge");
        Assert::IsTrue    (entry->GetBounds().right < mode->GetBounds().left, L"beside the entry");
        Assert::IsTrue    (narrowEntry > mode->GetBounds().right - mode->GetBounds().left, L"the entry takes the larger share");
        Assert::IsTrue    (entry->GetElide() == DxuiElide::Middle && mode->GetElide() == DxuiElide::Middle, L"both shorten in the middle");

        mode  = FindCombos (wide, L"Joystick")[0];
        entry = FindCombos (wide, L"Automatic")[0];

        Assert::AreEqual ((int) deleteButton->GetBounds().right + (kWideRightPx - kDesignRightPx), (int) mode->GetBounds().right,
                          L"a wider page stretches the row by as much");
        Assert::IsTrue   (entry->GetBounds().right - entry->GetBounds().left > narrowEntry, L"and the entry with it");
        Assert::AreEqual (narrow.GetContentWidthPx(), wide.GetContentWidthPx(), L"and the content width stays the design one");
    }


    //  The shown child of type T, or null.
    template <typename T>
    static const T * FindShown (const ControllersPage & page)
    {
        const T  * found = nullptr;
        size_t     i     = 0;



        for (i = 0; i < page.GetChildCount() && found == nullptr; ++i)
        {
            found = dynamic_cast<const T *> (page.GetChild (i));
            found = (found != nullptr && found->IsVisible()) ? found : nullptr;
        }

        return found;
    }


    //  Paddle and Two paddles show a bar per paddle where the stick's circle
    //  was, each named for the paddle the guest reads; Joystick keeps the
    //  circle.
    TEST_METHOD (PaddleModes_ShowABarPerPaddleInPlaceOfTheStick)
    {
        ControllersPage          joystick;
        ControllersPage          paddle;
        ControllersPage          pairs;
        ControllersPageState     joystickState;
        ControllersPageState     paddleState;
        ControllersPageState     pairsState;
        PlayerEntries            entries;
        PlayerSlots              slots    = MakeTwoPlaying();
        const PaddleBarsView   * bars     = nullptr;



        LayOutPage (joystick, joystickState, entries, slots, 0);

        Assert::IsNotNull (FindShown<StickPositionView> (joystick), L"Joystick shows the circle");
        Assert::IsNull    (FindShown<PaddleBarsView> (joystick),    L"and no bars");

        entries[0].mode = PlayerMode::Paddle;
        entries[1].mode = PlayerMode::Paddle;
        slots[0].target = PlayerAxisTarget::Paddle0;
        slots[1].target = PlayerAxisTarget::Paddle1;
        LayOutPage (paddle, paddleState, entries, slots, 1);

        bars = FindShown<PaddleBarsView> (paddle);

        Assert::IsNull    (FindShown<StickPositionView> (paddle), L"Paddle shows no circle");
        Assert::IsNotNull (bars,                                   L"but a bar");
        Assert::IsTrue    (bars->GetBars() == std::vector<PaddleBar> { { L"PDL1", PaddleBar::kCenter } }, L"one, for paddle 1, at rest");

        entries[0].mode = PlayerMode::TwoPaddles;
        entries[1].mode = PlayerMode::TwoPaddles;
        slots[0].target = PlayerAxisTarget::Paddles01;
        slots[1].target = PlayerAxisTarget::Paddles23;
        LayOutPage (pairs, pairsState, entries, slots, 1);

        bars = FindShown<PaddleBarsView> (pairs);

        Assert::IsNotNull (bars);
        Assert::IsTrue    (bars->GetBars() == std::vector<PaddleBar> { { L"PDL2", PaddleBar::kCenter }, { L"PDL3", PaddleBar::kCenter } },
                           L"Two paddles shows a bar for each of its paddles");
        Assert::AreEqual  (std::wstring (L"PDL1  108"), PaddleBarsView::FormatLabel ({ L"PDL1", 108 }), L"each labeled with its value");
    }


    //  A bar's mark sits where the paddle reads along its track: 0 at the
    //  left end, 255 at the right.
    TEST_METHOD (PaddleBars_TheMarkFallsWhereThePaddleReads)
    {
        constexpr float  kLeft  = 10.0f;
        constexpr float  kWidth = 255.0f;
        constexpr Byte   kMid   = 127;

        Assert::AreEqual (kLeft,               PaddleBarsView::GetMarkX (kLeft, kWidth, 0),    0.001f, L"0 at the left end");
        Assert::AreEqual (kLeft + kWidth,      PaddleBarsView::GetMarkX (kLeft, kWidth, 255),  0.001f, L"255 at the right");
        Assert::AreEqual (kLeft + (float) kMid, PaddleBarsView::GetMarkX (kLeft, kWidth, kMid), 0.001f, L"and in between in proportion");
    }


    //  Player 1 on a Paddle, Player 2 Disabled.
    static PlayerEntries MakeOnePaddle()
    {
        PlayerEntries  entries;



        entries[0].mode = PlayerMode::Paddle;
        entries[1].kind = PlayerEntryKind::Disabled;
        return entries;
    }


    //  Player 1 alone, on paddle 0.
    static PlayerSlots MakePaddleSlots()
    {
        PlayerSlots  slots = MakeTwoPlaying();



        slots[0].target = PlayerAxisTarget::Paddle0;
        slots[1]        = PlayerSlot();
        return slots;
    }


    //  The shown sliders.
    static size_t CountShownSliders (const ControllersPage & page)
    {
        const DxuiSlider  * slider = nullptr;
        size_t              count  = 0;
        size_t              i      = 0;



        for (i = 0; i < page.GetChildCount(); ++i)
        {
            slider = dynamic_cast<const DxuiSlider *> (page.GetChild (i));

            if (slider != nullptr && slider->IsVisible())
            {
                count++;
            }
        }

        return count;
    }


    //  A Joystick profile's axes always give position, so neither the
    //  Position / Paddle speed drop-down nor the speed slider is on the page:
    //  the dead zone slider is the only one.
    TEST_METHOD (JoystickProfile_ShowsNoPaddleSpeed)
    {
        ControllersPage       page;
        ControllersPageState  state;



        LayOutPage (page, state, PlayerEntries(), PlayerSlots(), 0, kRightPx, ControllerFormFactor::Joystick);

        Assert::IsTrue   (FindCombos (page, L"Position").empty(), L"no Position / Paddle speed drop-down");
        Assert::AreEqual ((size_t) 1, CountShownSliders (page),   L"and no speed slider");
    }


    //  A Paddle profile shows the speed slider, and offers Position beside
    //  Paddle speed only for a knob: a DirectInput axis on a controller that
    //  is not a gamepad.
    TEST_METHOD (PaddleProfile_OffersPositionOnlyOnAKnob)
    {
        ControllersPage       gamepad;
        ControllersPage       stick;
        ControllersPageState  gamepadState;
        ControllersPageState  stickState;



        LayOutPage (gamepad, gamepadState, MakeOnePaddle(), MakePaddleSlots(), 0, kRightPx, ControllerFormFactor::Gamepad);
        LayOutPage (stick,   stickState,   MakeOnePaddle(), MakePaddleSlots(), 0, kRightPx, ControllerFormFactor::Joystick);

        Assert::IsTrue   (FindCombos (gamepad, L"Position").empty(),      L"a gamepad's stick is offered no Position");
        Assert::AreEqual ((size_t) 2, CountShownSliders (gamepad),        L"but has its speed slider");
        Assert::AreEqual ((size_t) 1, FindCombos (stick, L"Position").size(), L"a joystick's axis is offered Position");
        Assert::AreEqual ((size_t) 2, CountShownSliders (stick),          L"beside its speed slider");
    }


    //  The Position / Paddle speed drop-down beside Invert is wide enough
    //  for "Paddle speed" and its arrow, and still ends where the mapping
    //  drop-down above it ends; Invert takes what is left.
    TEST_METHOD (ResponseDropDown_FitsPaddleSpeedBesideItsArrow)
    {
        ControllersPage          page;
        ControllersPageState     state;
        MockDxuiTextRenderer     text;
        const DxuiComboBox     * response = nullptr;
        const DxuiComboBox     * mapping  = nullptr;
        float                    fit      = 0.0f;



        page.SetTextRenderer (&text);
        LayOutPage (page, state, MakeOnePaddle(), MakePaddleSlots(), 0, kRightPx, ControllerFormFactor::Joystick);

        response = FindCombos (page, L"Position")[0];
        mapping  = FindCombos (page, kpszPressToAssign)[0];
        fit      = response->GetFitWidthPx (text);

        Assert::IsTrue   ((float) (response->GetBounds().right - response->GetBounds().left) >= fit, L"Paddle speed fits beside the arrow");
        Assert::AreEqual ((int) mapping->GetBounds().right, (int) response->GetBounds().right, L"right-aligned with the mapping drop-down");
    }


    //  The heading above the input picture gives what the controller in
    //  Editing drives.
    TEST_METHOD (Heading_GivesWhatTheEditedControllerDrives)
    {
        ControllersPage       first;
        ControllersPage       second;
        ControllersPageState  firstState;
        ControllersPageState  secondState;
        PlayerSlots           slots       = MakeTwoPlaying();



        slots[1].target = PlayerAxisTarget::Joystick1;

        LayOutPage (first,  firstState,  PlayerEntries(), slots, 0);
        LayOutPage (second, secondState, PlayerEntries(), slots, 1);

        Assert::IsNotNull (FindLabel (first,  L"Joystick 0"), L"Player 1's stick");
        Assert::IsNotNull (FindLabel (second, L"Joystick 1"), L"Player 2's stick");
    }


    //  Player 1 in the left jack: Player 2's list shows the left jack and
    //  cannot choose it, and Player 2, on Paddle beside the Joyport, is
    //  headed by the paddle it plays as though alone, has a warning under its
    //  row and has its button rows disabled.
    TEST_METHOD (PlayerRows_BesideTheJoyportWarnAndDisableTheButtons)
    {
        ControllersPage                        page;
        ControllersPageState                   state;
        const DxuiComboBox                   * two      = nullptr;
        std::vector<const DxuiInfoBanner *>    warnings;
        std::pair<size_t, size_t>              rows;



        LayOutPage (page, state, MakeBesideTheJoyport (PlayerMode::Paddle), MakeTwoPlaying(), 1);

        two      = FindCombos (page, L"Automatic (Joyport right)")[0];
        warnings = FindWarnings (page);
        rows     = CountBindingRows (page);

        Assert::IsNotNull (FindLabel (page, L"Paddle 0"),   L"Player 2 plays its paddle as though alone");
        Assert::IsFalse   (two->IsItemEnabled (2),          L"the left jack is Player 1's");
        Assert::IsTrue    (two->IsItemEnabled (3),          L"the right jack is free");

        Assert::AreEqual (size_t (1), warnings.size(), L"one warning, under Player 2");
        Assert::AreEqual (std::wstring (kpszCutWarning), warnings[0]->GetText());
        Assert::IsTrue   (warnings[0]->GetSeverity() == DxuiInfoBanner::Severity::Info, L"as an info notice");
        Assert::IsNull   (warnings[0]->GetIconGlyph(), L"with the banner's own drawn info icon, like every other info banner");
        Assert::IsTrue   (warnings[0]->GetBounds().top >= FindCombos (page, L"Automatic (Joyport right)")[0]->GetBounds().bottom, L"under Player 2's row");

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


    //  Given the sheet's renderer, the warning is as tall as its measured
    //  line plus the compact padding, not the banner's generous estimate, and
    //  the page's height still reaches its last row.
    TEST_METHOD (PlayerWarning_WithARenderer_HugsItsMeasuredLine)
    {
        constexpr int                          kLineHeightPx = 18;
        constexpr int                          kPadYPx       = 4;
        ControllersPage                        page;
        ControllersPageState                   state;
        MockDxuiTextRenderer                   text;
        std::vector<const DxuiInfoBanner *>    warnings;
        int                                    heightPx      = 0;
        RECT                                   bounds        = {};



        text.SetCannedMetrics (kpszCutWarning, SIZE { 500, kLineHeightPx });
        page.SetTextRenderer  (&text);

        heightPx = LayOutPage (page, state, MakeBesideTheJoyport (PlayerMode::Paddle), MakeTwoPlaying(), 1);
        warnings = FindWarnings (page);

        Assert::AreEqual (size_t (1), warnings.size());

        bounds = warnings[0]->GetBounds();

        Assert::AreEqual (kLineHeightPx + 2 * kPadYPx, (int) (bounds.bottom - bounds.top), L"one line and the compact padding");
        Assert::AreEqual (GetLowestVisibleBottom (page) + kPagePadPx - kTopPx, heightPx, L"the content height follows it");
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


    //  No control on two targets, no warning.
    TEST_METHOD (SharedWarning_IsHiddenWhileNothingIsShared)
    {
        ControllersPage       page;
        ControllersPageState  state;



        LayOutPage (page, state, PlayerEntries(), PlayerSlots());

        Assert::IsTrue (FindWarnings (page).empty());
    }


    //  A button already on PB0 added to PB1 gets a warning under PB1's rows,
    //  giving PB0, and the page's height takes it in. The sheet is asked to
    //  bring the new warning into view.
    TEST_METHOD (SharedWarning_GoesUnderTheEditedTargetGivingTheOthers)
    {
        ControllersPage                        page;
        ControllersPageState                   state;
        std::vector<const DxuiInfoBanner *>    warnings;
        std::optional<RECT>                    revealed;
        int                                    heightPx = 0;



        LayOutPage (page, state, PlayerEntries(), PlayerSlots());
        page.SetOnRevealRequested ([&revealed] (const RECT & rectPx) { revealed = rectPx; });

        state.AddButtonBinding (PaddleTarget::Pb1, { { ControlKind::Button, 0 } });   // button 0 is already PB0
        page.Refresh();

        warnings = FindWarnings (page);
        heightPx = page.GetContentHeightPx();

        Assert::AreEqual (size_t (1), warnings.size());
        Assert::AreEqual (std::wstring (L"Button 1 is also assigned to PB0."), warnings[0]->GetText());
        Assert::IsTrue   (warnings[0]->GetSeverity() == DxuiInfoBanner::Severity::Warning, L"as a warning");
        Assert::IsTrue   (IsBetweenLabels (*warnings[0], page, L"PB1:", L"PB2:"), L"under PB1's rows");
        Assert::AreEqual (GetLowestVisibleBottom (page) + kPagePadPx - kTopPx, heightPx, L"the content height follows it");
        Assert::IsTrue   (revealed.has_value(), L"and it is scrolled to");
        Assert::AreEqual (warnings[0]->GetBounds().top,    revealed->top);
        Assert::AreEqual (warnings[0]->GetBounds().bottom, revealed->bottom);
    }


    //  Added to the earlier target, the warning goes under that one.
    TEST_METHOD (SharedWarning_GoesUnderAnEarlierTargetWhenThatWasEdited)
    {
        ControllersPage                        page;
        ControllersPageState                   state;
        std::vector<const DxuiInfoBanner *>    warnings;



        LayOutPage (page, state, PlayerEntries(), PlayerSlots());
        state.AddButtonBinding (PaddleTarget::Pb0, { { ControlKind::Button, 1 } });   // button 1 is already PB1
        page.Refresh();

        warnings = FindWarnings (page);

        Assert::AreEqual (size_t (1), warnings.size());
        Assert::AreEqual (std::wstring (L"Button 2 is also assigned to PB1."), warnings[0]->GetText());
        Assert::IsTrue   (IsBetweenLabels (*warnings[0], page, L"PB0:", L"PB1:"), L"under PB0's rows");
    }


    //  A mapping switched to with a control already shared has no edit to
    //  follow, so the warning goes under the last of its targets, giving the
    //  earlier ones.
    TEST_METHOD (SharedWarning_OnASwitchGoesUnderTheLastTarget)
    {
        ControllersPage                        page;
        ControllersPageState                   state;
        std::vector<const DxuiInfoBanner *>    warnings;



        LayOutPage (page, state, PlayerEntries(), PlayerSlots());
        state.AddButtonBinding (PaddleTarget::Pb0, { { ControlKind::Button, 1 } });
        state.SelectController (1);
        state.SelectController (0);
        page.Refresh();

        warnings = FindWarnings (page);

        Assert::AreEqual (size_t (1), warnings.size());
        Assert::AreEqual (std::wstring (L"Button 2 is also assigned to PB0."), warnings[0]->GetText());
        Assert::IsTrue   (IsBetweenLabels (*warnings[0], page, L"PB1:", L"PB2:"), L"under PB1's rows");
    }


    //  Two other targets are joined with "and", and each shared control has
    //  a sentence of its own, one to a line, in the warning of the target it
    //  was last added to.
    TEST_METHOD (SharedWarning_GivesEachSharedControlASentence)
    {
        ControllersPage                        page;
        ControllersPageState                   state;
        std::vector<const DxuiInfoBanner *>    warnings;



        LayOutPage (page, state, PlayerEntries(), PlayerSlots());
        state.AddButtonBinding (PaddleTarget::Pb1, { { ControlKind::Button, 0 } });
        state.AddButtonBinding (PaddleTarget::Pb2, { { ControlKind::Button, 0 } });
        state.AddButtonBinding (PaddleTarget::Pb2, { { ControlKind::Button, 1 } });   // button 1 is already PB1
        page.Refresh();

        warnings = FindWarnings (page);

        Assert::AreEqual (size_t (1), warnings.size(), L"one banner");
        Assert::AreEqual (std::wstring (L"Button 1 is also assigned to PB0 and PB1.\nButton 2 is also assigned to PB1."), warnings[0]->GetText());
    }


    //  An axis added to PDL1 while it drives PDL0 gets its warning under
    //  PDL1's rows and options, above the buttons.
    TEST_METHOD (SharedWarning_GoesUnderTheEditedAxis)
    {
        ControllersPage                        page;
        ControllersPageState                   state;
        std::vector<const DxuiInfoBanner *>    warnings;
        AxisBinding                            binding;
        std::wstring                           suffix   = L" is also assigned to PDL0 (X).";



        LayOutPage (page, state, PlayerEntries(), PlayerSlots());
        binding.analog = state.GetMapping().pdl0[0].analog;
        state.AddAxisBinding (PaddleTarget::Pdl1, binding);
        page.Refresh();

        warnings = FindWarnings (page);

        Assert::AreEqual (size_t (1), warnings.size());
        Assert::IsTrue   (warnings[0]->GetText().ends_with (suffix), warnings[0]->GetText().c_str());
        Assert::IsTrue   (IsBetweenLabels (*warnings[0], page, L"PDL1 (Y):", L"Buttons"), L"under PDL1's rows");
    }

    //  Controls shared from different targets get a warning under each.
    TEST_METHOD (SharedWarning_HasABannerForEachTargetWithWarnings)
    {
        ControllersPage                        page;
        ControllersPageState                   state;
        std::vector<const DxuiInfoBanner *>    warnings;



        LayOutPage (page, state, PlayerEntries(), PlayerSlots());
        state.AddButtonBinding (PaddleTarget::Pb1, { { ControlKind::Button, 0 } });
        state.AddButtonBinding (PaddleTarget::Pb2, { { ControlKind::Button, 2 } });
        state.AddButtonBinding (PaddleTarget::Pb0, { { ControlKind::Button, 2 } });
        page.Refresh();

        warnings = FindWarnings (page);

        Assert::AreEqual (size_t (2), warnings.size(), L"two banners");
        Assert::AreEqual (std::wstring (L"Button 3 is also assigned to PB2."), warnings[0]->GetText());
        Assert::IsTrue   (IsBetweenLabels (*warnings[0], page, L"PB0:", L"PB1:"), L"the first under PB0");
        Assert::AreEqual (std::wstring (L"Button 1 is also assigned to PB0."), warnings[1]->GetText());
        Assert::IsTrue   (IsBetweenLabels (*warnings[1], page, L"PB1:", L"PB2:"), L"the second under PB1");
    }


    //  Fire on the Joyport profile takes eight controls. Past four, a
    //  target's rows scroll in a table four rows tall: every row is there,
    //  "+" stays offered beside the first, and the page is no taller than
    //  with four.
    TEST_METHOD (Rows_PastFourScrollInATableFourRowsTall)
    {
        constexpr size_t                        kRows      = 8;
        constexpr int                           kTableHPx  = 4 * (28 + 6) - 6 + 6;   // four rows and gaps, half a gap above and below
        ControllersPage                         page;
        ControllersPage                         fourPage;
        ControllersPageState                    state;
        ControllersPageState                    fourState;
        std::vector<const DxuiScrollPanel *>    tables;
        std::vector<const DxuiButton *>         adds;



        LayOutWithPb0Bindings (page,     state,     kRows);
        LayOutWithPb0Bindings (fourPage, fourState, ControllersPage::kTableRows);

        tables = FindTables (page);
        adds   = FindAddButtons (page);

        Assert::AreEqual ((size_t) 5, tables.size(), L"a table per target");
        Assert::AreEqual ((size_t) 5, adds.size());
        Assert::AreEqual (kRows, FindCombos (*tables[2], kpszPressToAssign).size(), L"every one of PB0's rows is in its table");
        Assert::IsTrue   (tables[2]->IsScrollable(), L"and it scrolls");
        Assert::AreEqual ((LONG) kTableHPx, tables[2]->GetBounds().bottom - tables[2]->GetBounds().top, L"four rows tall");
        Assert::IsFalse  (tables[3]->IsScrollable(), L"a target with one row does not");
        Assert::IsTrue   (adds[2]->IsEnabled(), L"+ is still offered");
        Assert::AreEqual (FindCombos (*tables[2], kpszPressToAssign)[0]->GetBounds().top, adds[2]->GetBounds().top, L"beside the first row");
        Assert::AreEqual (fourPage.GetContentHeightPx(), page.GetContentHeightPx(), L"the page counts the table's four rows");
    }


    //  "+" past four waits on a new row, which is scrolled into view.
    TEST_METHOD (AddRow_PastFourIsScrolledTo)
    {
        ControllersPage                         page;
        ControllersPageState                    state;
        std::vector<const DxuiScrollPanel *>    tables;
        std::vector<const DxuiComboBox *>       rows;
        std::vector<const DxuiComboBox *>       waiting;
        RECT                                    add    = {};
        POINT                                   center = {};
        DxuiMouseEvent                          ev;



        LayOutWithPb0Bindings (page, state, 8);

        add    = FindAddButtons (page)[2]->GetBounds();
        center = { (add.left + add.right) / 2, (add.top + add.bottom) / 2 };

        ev.positionDip = center;
        ev.button      = DxuiMouseButton::Left;
        ev.kind        = DxuiMouseEventKind::Move;
        page.OnMouse (ev);
        ev.kind        = DxuiMouseEventKind::Down;
        page.OnMouse (ev);
        ev.kind        = DxuiMouseEventKind::Up;
        page.OnMouse (ev);

        tables  = FindTables (page);
        rows    = FindCombos (*tables[2], kpszPressToAssign);
        waiting = FindCombos (*tables[2], L"Press a control...");

        Assert::IsTrue   (page.IsCapturing(), L"+ waits on a control");
        Assert::AreEqual ((size_t) 8, rows.size());
        Assert::AreEqual ((size_t) 1, waiting.size(), L"on a ninth row");
        Assert::IsTrue   (waiting[0]->GetBounds().top > rows[7]->GetBounds().top, L"after the other eight");
        Assert::IsTrue   (waiting[0]->GetBounds().bottom <= tables[2]->GetBounds().bottom, L"and in view");
        Assert::IsTrue   (waiting[0]->GetBounds().top    >= tables[2]->GetBounds().top);
    }

    //  The page's width reaches its rightmost control and its padding, which
    //  is what the Settings sheet sizes its largest window to.
    TEST_METHOD (ContentWidth_ReachesTheRightmostControl)
    {
        ControllersPage        page;
        ControllersPageState   state;
        const IDxuiControl   * child = nullptr;
        int                    right = 0;
        size_t                 i     = 0;



        LayOutPage (page, state);

        for (i = 0; i < page.GetChildCount(); ++i)
        {
            child = page.GetChild (i);

            if (child != nullptr && child->IsVisible())
            {
                right = std::max (right, (int) child->GetBounds().right);
            }
        }

        Assert::IsTrue   (right > kLeftPx, L"the page shows controls");
        Assert::AreEqual (right + kPagePadPx - kLeftPx, page.GetContentWidthPx());
    }

    //  Whether a banner sits below one row label and above the next.
    static bool IsBetweenLabels (const DxuiInfoBanner  & banner,
                                 const ControllersPage & page,
                                 const std::wstring    & above,
                                 const std::wstring    & below)
    {
        const DxuiLabel  * upper = FindLabel (page, above);
        const DxuiLabel  * lower = FindLabel (page, below);



        return upper != nullptr && lower != nullptr
            && banner.GetBounds().top    >= upper->GetBounds().bottom
            && banner.GetBounds().bottom <= lower->GetBounds().top;
    }


    //  One of the modes the page lays out differently: the players' entries
    //  and slots that produce it, and the controller being edited.
    struct ModeCase
    {
        const wchar_t  * pszName = nullptr;
        PlayerEntries    entries;
        PlayerSlots      slots;
        size_t           edited  = 0;
    };


    //  Joystick, Paddle, Two paddles and both of the Joyport's jacks.
    static std::vector<ModeCase> MakeModeCases()
    {
        std::vector<ModeCase>  cases (5);



        cases[0].pszName = L"Joystick";
        cases[0].slots   = MakeTwoPlaying();

        cases[1].pszName          = L"Paddle";
        cases[1].entries[0].mode  = PlayerMode::Paddle;
        cases[1].entries[1].mode  = PlayerMode::Paddle;
        cases[1].slots            = MakeTwoPlaying();
        cases[1].slots[0].target  = PlayerAxisTarget::Paddle0;
        cases[1].slots[1].target  = PlayerAxisTarget::Paddle1;
        cases[1].edited           = 1;

        cases[2].pszName          = L"Two paddles";
        cases[2].entries[0].mode  = PlayerMode::TwoPaddles;
        cases[2].entries[1].mode  = PlayerMode::TwoPaddles;
        cases[2].slots            = MakeTwoPlaying();
        cases[2].slots[0].target  = PlayerAxisTarget::Paddles01;
        cases[2].slots[1].target  = PlayerAxisTarget::Paddles23;
        cases[2].edited           = 1;

        cases[3].pszName = L"Joyport left";
        cases[3].entries = MakeBesideTheJoyport (PlayerMode::SameAsPlayer1);
        cases[3].slots   = MakeTwoPlaying();

        cases[4].pszName         = L"Joyport right";
        cases[4].entries[0].mode = PlayerMode::JoyportRight;
        cases[4].entries[1].mode = PlayerMode::SameAsPlayer1;
        cases[4].slots           = MakeTwoPlaying();
        return cases;
    }


    //  The shown label on the same row as `row` and left of it, nearest it,
    //  or null.
    static const DxuiLabel * FindLabelBeside (const ControllersPage & page, const RECT & row)
    {
        const DxuiLabel  * label = nullptr;
        const DxuiLabel  * found = nullptr;
        size_t             i     = 0;



        for (i = 0; i < page.GetChildCount(); ++i)
        {
            label = dynamic_cast<const DxuiLabel *> (page.GetChild (i));

            if (label == nullptr || !label->IsVisible() || label->GetBounds().top != row.top || label->GetBounds().right > row.left)
            {
                continue;
            }

            if (found == nullptr || label->GetBounds().right > found->GetBounds().right)
            {
                found = label;
            }
        }

        return found;
    }


    //  Each target's label sits just left of its first mapping drop-down,
    //  right-aligned, the page's gap from it, in every mode.
    TEST_METHOD (TargetLabels_SitJustLeftOfTheirFirstRow_InEveryMode)
    {
        constexpr int  kGapPx = 6;



        for (const ModeCase & mode : MakeModeCases())
        {
            ControllersPage                         page;
            ControllersPageState                    state;
            std::vector<const DxuiScrollPanel *>    tables;
            std::vector<const DxuiComboBox *>       rows;
            const DxuiLabel                       * label = nullptr;



            LayOutPage (page, state, mode.entries, mode.slots, mode.edited);

            tables = FindTables (page);
            Assert::IsFalse (tables.empty(), mode.pszName);

            for (const DxuiScrollPanel * table : tables)
            {
                rows = FindCombos (*table, kpszPressToAssign);
                Assert::IsFalse (rows.empty(), mode.pszName);

                label = FindLabelBeside (page, rows[0]->GetBounds());

                Assert::IsNotNull (label, mode.pszName);
                Assert::AreEqual  (rows[0]->GetBounds().left - kGapPx, label->GetBounds().right, (std::wstring (mode.pszName) + L": " + label->GetText() + L" ends a gap left of its row").c_str());
                Assert::IsTrue    (label->GetHAlign() == DxuiTextHAlign::Right,                   (std::wstring (mode.pszName) + L": " + label->GetText() + L" is right-aligned").c_str());
            }
        }
    }


    //  The dead zone slider sits the page's section gap below the last
    //  mapping row and the same gap above Calibration, and its track starts
    //  at the drop-down column's left edge, in every mode.
    TEST_METHOD (DeadZone_IsCenteredAndAlignedWithTheRowsAbove_InEveryMode)
    {
        constexpr int  kSectionGapPx = 14;
        constexpr int  kHalfGapPx    = 3;     // a table reaches half a gap below its last row



        for (const ModeCase & mode : MakeModeCases())
        {
            ControllersPage                         page;
            ControllersPageState                    state;
            std::vector<const DxuiScrollPanel *>    tables;
            const DxuiLabel                       * deadZone    = nullptr;
            const DxuiLabel                       * calibration = nullptr;
            const DxuiSlider                      * slider      = nullptr;
            const IDxuiControl                    * child       = nullptr;
            RECT                                    lastTable   = {};
            size_t                                  i           = 0;



            LayOutPage (page, state, mode.entries, mode.slots, mode.edited);

            tables      = FindTables (page);
            deadZone    = FindLabel (page, L"Dead zone:");
            calibration = FindLabel (page, L"Calibration:");

            Assert::IsFalse   (tables.empty(),  mode.pszName);
            Assert::IsNotNull (deadZone,        mode.pszName);
            Assert::IsNotNull (calibration,     mode.pszName);

            lastTable = tables.back()->GetBounds();

            for (i = 0; i < page.GetChildCount() && slider == nullptr; ++i)
            {
                child  = page.GetChild (i);
                slider = dynamic_cast<const DxuiSlider *> (child);
                slider = (slider != nullptr && slider->GetBounds().top == deadZone->GetBounds().top) ? slider : nullptr;
            }

            Assert::IsNotNull (slider, mode.pszName);
            Assert::AreEqual  (kSectionGapPx, (int) (slider->GetBounds().top - (lastTable.bottom - kHalfGapPx)), (std::wstring (mode.pszName) + L": the gap above").c_str());
            Assert::AreEqual  (kSectionGapPx, (int) (calibration->GetBounds().top - slider->GetBounds().bottom), (std::wstring (mode.pszName) + L": the gap below").c_str());
            Assert::AreEqual  (lastTable.left, slider->GetBounds().left + DxuiSlider::kTrackInsetDip,              (std::wstring (mode.pszName) + L": the track starts at the drop-downs' edge").c_str());
        }
    }


    static constexpr int  kIndentPx      = 18;     // the page's child indent at 96 DPI
    static constexpr int  kLabelWidthPx  = 90;
    static constexpr int  kMinLightPx    = 20;     // a button's light, readable beside its row
    static constexpr int  kMinPageWidth  = 688;    // the page area of the sheet at its 720 DIP minimum


    //  Every shown child of type T, in page order.
    template <typename T>
    static std::vector<const T *> FindAllShown (const ControllersPage & page)
    {
        const T                * child = nullptr;
        std::vector<const T *>   found;
        size_t                   i     = 0;



        for (i = 0; i < page.GetChildCount(); ++i)
        {
            child = dynamic_cast<const T *> (page.GetChild (i));

            if (child != nullptr && child->IsVisible())
            {
                found.push_back (child);
            }
        }

        return found;
    }


    //  The picture of the controller the page shows in this mode: the stick,
    //  the Joyport's switches or the paddle bars.
    static const IDxuiControl * FindPicture (const ControllersPage & page)
    {
        const IDxuiControl  * picture = FindShown<StickPositionView> (page);



        if (picture == nullptr)
        {
            picture = FindShown<JoyportSwitchView> (page);
        }

        if (picture == nullptr)
        {
            picture = FindShown<PaddleBarsView> (page);
        }

        return picture;
    }


    //  The mapping controls sit in an indented column on the left, under the
    //  heading, which stays at the margin; the picture sits to their right,
    //  and each button's light is at the picture's left edge, beside its
    //  row, large enough to read. In every mode.
    TEST_METHOD (Columns_PutTheMappingsLeftAndThePictureRight_InEveryMode)
    {
        constexpr int  kMarginPx = kLeftPx + kPagePadPx;



        for (const ModeCase & mode : MakeModeCases())
        {
            ControllersPage                         page;
            ControllersPageState                    state;
            std::vector<const DxuiScrollPanel *>    tables;
            const IDxuiControl                    * picture = nullptr;
            const DxuiLabel                       * buttons = nullptr;
            const DxuiLabel                       * label   = nullptr;
            std::wstring                            name    = mode.pszName;



            LayOutPage (page, state, mode.entries, mode.slots, mode.edited);

            tables  = FindTables (page);
            picture = FindPicture (page);
            buttons = FindLabel (page, L"Buttons");

            Assert::IsFalse   (tables.empty(), mode.pszName);
            Assert::IsNotNull (picture,        mode.pszName);
            Assert::IsNotNull (buttons,        mode.pszName);
            Assert::AreEqual  (kMarginPx, (int) buttons->GetBounds().left, (name + L": the Buttons heading stays at the margin").c_str());

            for (const DxuiScrollPanel * table : tables)
            {
                label = FindLabelBeside (page, FindCombos (*table, kpszPressToAssign)[0]->GetBounds());

                Assert::IsNotNull (label, mode.pszName);
                Assert::AreEqual  (kMarginPx + kIndentPx, (int) label->GetBounds().left, (name + L": " + label->GetText() + L" is indented").c_str());
                Assert::IsTrue    (table->GetBounds().right < picture->GetBounds().left, (name + L": the rows are left of the picture").c_str());
            }

            for (const DxuiButton * add : FindAddButtons (page))
            {
                Assert::IsTrue (add->GetBounds().right < picture->GetBounds().left, (name + L": + is left of the picture").c_str());
            }

            for (const ButtonLightView * light : FindAllShown<ButtonLightView> (page))
            {
                Assert::AreEqual (picture->GetBounds().left, light->GetBounds().left, (name + L": a light is at the picture's left edge").c_str());
                Assert::IsTrue   (light->GetBounds().right - light->GetBounds().left >= kMinLightPx, (name + L": and large enough to read").c_str());
            }
        }
    }


    //  Each button's light is no taller than the mapping drop-down beside it,
    //  and sits within that row.
    TEST_METHOD (ButtonLights_AreNoTallerThanTheRowBesideThem)
    {
        ControllersPage                       page;
        ControllersPageState                  state;
        std::vector<const ButtonLightView *>  lights;
        const DxuiComboBox                  * beside = nullptr;



        LayOutPage (page, state, PlayerEntries(), PlayerSlots());

        lights = FindAllShown<ButtonLightView> (page);
        Assert::AreEqual ((size_t) ControllersPage::kButtonCount, lights.size(), L"a light per button");

        for (const ButtonLightView * light : lights)
        {
            beside = nullptr;

            for (const DxuiComboBox * row : FindCombos (page, kpszPressToAssign))
            {
                if (row->GetBounds().top <= light->GetBounds().top && row->GetBounds().bottom >= light->GetBounds().bottom)
                {
                    beside = row;
                }
            }

            Assert::IsNotNull (beside, L"the light sits within its row");
            Assert::IsTrue    (light->GetBounds().bottom - light->GetBounds().top <= beside->GetBounds().bottom - beside->GetBounds().top, L"and is no taller than it");
        }
    }


    //  The mapping drop-downs, Invert's box, the paddle speed slider's track
    //  and the dead zone slider's track share one left edge, in every mode.
    TEST_METHOD (ConfigurationColumn_SharesOneLeftEdge_InEveryMode)
    {
        constexpr int  kColumnPx = kLeftPx + kPagePadPx + kIndentPx + kLabelWidthPx;
        size_t         speeds    = 0;



        for (const ModeCase & mode : MakeModeCases())
        {
            ControllersPage       page;
            ControllersPageState  state;
            std::wstring          name  = mode.pszName;



            LayOutPage (page, state, mode.entries, mode.slots, mode.edited);

            Assert::IsFalse (FindCombos (page, kpszPressToAssign).empty(), mode.pszName);

            for (const DxuiComboBox * row : FindCombos (page, kpszPressToAssign))
            {
                Assert::AreEqual (kColumnPx, (int) row->GetBounds().left, (name + L": a mapping drop-down").c_str());
            }

            for (const DxuiCheckbox * invert : FindAllShown<DxuiCheckbox> (page))
            {
                Assert::AreEqual (kColumnPx, (int) invert->GetBounds().left, (name + L": Invert's box").c_str());
            }

            for (const DxuiSlider * slider : FindAllShown<DxuiSlider> (page))
            {
                Assert::AreEqual (kColumnPx, (int) slider->GetBounds().left + DxuiSlider::kTrackInsetDip, (name + L": a slider's track").c_str());
                Assert::IsTrue   (FindShown<StickPositionView> (page) == nullptr
                                  || slider->GetBounds().right < FindShown<StickPositionView> (page)->GetBounds().left
                                  || slider->GetBounds().top  >= FindShown<StickPositionView> (page)->GetBounds().bottom,
                                  (name + L": clear of the picture").c_str());
            }

            speeds += FindAllShown<DxuiSlider> (page).size() - 1;
        }

        Assert::IsTrue (speeds > 0, L"a paddle mode showed its speed slider");
    }


    //  Whether two rectangles share any area.
    static bool DoOverlap (const RECT & a, const RECT & b)
    {
        return a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom;
    }


    //  At the sheet's minimum width no two shown controls overlap and none
    //  reaches past the page's padding, in every mode, with the Position /
    //  Paddle speed drop-down offered where a mode has one.
    TEST_METHOD (MinimumWidth_NothingOverlapsOrClips_InEveryMode)
    {
        constexpr int  kRightEdgePx = kLeftPx + kMinPageWidth;
        constexpr int  kHalfGapPx   = 3;



        for (const ModeCase & mode : MakeModeCases())
        {
            ControllersPage              page;
            ControllersPageState         state;
            MockDxuiTextRenderer         text;
            std::vector<RECT>            shown;
            std::vector<std::wstring>    kinds;
            RECT                         bounds = {};
            const IDxuiControl         * child  = nullptr;
            std::wstring                 name   = mode.pszName;
            size_t                       i      = 0;
            size_t                       j      = 0;



            page.SetTextRenderer   (&text);
            page.SetDesignWidthPx  (kMinPageWidth);
            LayOutPage (page, state, mode.entries, mode.slots, mode.edited, kRightEdgePx, ControllerFormFactor::Joystick);

            for (i = 0; i < page.GetChildCount(); ++i)
            {
                child = page.GetChild (i);

                if (child != nullptr && child->IsVisible())
                {
                    bounds = child->GetBounds();

                    // A table reaches half a gap above and below its rows, room
                    // for a row's focus rectangle and nothing drawn; its rows
                    // are what must not overlap.
                    if (dynamic_cast<const DxuiScrollPanel *> (child) != nullptr)
                    {
                        bounds.top    += kHalfGapPx;
                        bounds.bottom -= kHalfGapPx;
                    }

                    shown.push_back (bounds);
                    kinds.push_back (std::wstring (typeid (*child).name(), typeid (*child).name() + strlen (typeid (*child).name())));
                }
            }

            Assert::IsTrue (shown.size() > 10, mode.pszName);

            for (i = 0; i < shown.size(); ++i)
            {
                Assert::IsTrue (shown[i].right <= kRightEdgePx - kPagePadPx, (name + L": " + kinds[i] + L" is not clipped").c_str());

                for (j = i + 1; j < shown.size(); ++j)
                {
                    Assert::IsFalse (DoOverlap (shown[i], shown[j]), (name + L": " + kinds[i] + L" and " + kinds[j] + L" overlap").c_str());
                }
            }
        }
    }
};
