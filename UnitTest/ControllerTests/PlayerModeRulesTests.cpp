#include "Pch.h"

#include "Controllers/ControllerTokens.h"
#include "Controllers/PlayerModeRules.h"

#include "Core/JsonParser.h"
#include "Machines/Apple2/Common/SiriusJoyport.h"
#include "Shell/EmulatorShell.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PlayerModeRulesTests
//
//  Each player's mode is Joystick, Joyport left, Joyport right or Paddle,
//  and Player 2's may be Same as Player 1, which follows Player 1 into the
//  other jack. The machine's Joyport is on exactly while a playing player is
//  in one of its jacks; a jack one player holds is not offered to the
//  other; and a player beside the Joyport keeps its paddles but not its
//  buttons, which the Joyport owns.
//
//  THE OLD GLOBAL SETTING IS READ ONCE. A saved Sirius Joyport puts Player 1
//  in the left jack and Player 2 on Same as Player 1, and the old key is then
//  marked so that a machine's own older value is never read again.
//
////////////////////////////////////////////////////////////////////////////////

namespace ControllerTests
{
    TEST_CLASS (PlayerModeRulesTests)
    {
    public:

        static PlayerEntries MakeEntries (PlayerMode first, PlayerMode second)
        {
            PlayerEntries  entries;



            entries[0].mode = first;
            entries[1].mode = second;
            return entries;
        }


        static JsonValue ParseOrFail (const char * text)
        {
            JsonValue       v;
            JsonParseError  err;
            HRESULT         hr = JsonParser::Parse (text, v, err);



            Assert::IsTrue (SUCCEEDED (hr), L"fixture JSON did not parse");
            return v;
        }


        static bool IsModeListed (const std::vector<InputModeRules::PlayerModeChoice> & choices, PlayerMode mode)
        {
            return std::any_of (choices.begin(), choices.end(), [mode] (const InputModeRules::PlayerModeChoice & c) { return c.mode == mode; });
        }


        static InputModeRules::PlayerModeChoice FindChoice (const std::vector<InputModeRules::PlayerModeChoice> & choices, PlayerMode mode)
        {
            auto  found = std::find_if (choices.begin(), choices.end(), [mode] (const InputModeRules::PlayerModeChoice & c) { return c.mode == mode; });



            Assert::IsTrue (found != choices.end(), L"the mode is not listed");
            return *found;
        }


        TEST_METHOD (ResolveMode_SameAsPlayerOneFollowsPlayerOneIntoTheOtherJack)
        {
            Assert::IsTrue (PlayerModeRules::ResolveMode (MakeEntries (PlayerMode::Joystick,     PlayerMode::SameAsPlayer1), 1, true) == PlayerMode::Joystick);
            Assert::IsTrue (PlayerModeRules::ResolveMode (MakeEntries (PlayerMode::Paddle,       PlayerMode::SameAsPlayer1), 1, true) == PlayerMode::Paddle);
            Assert::IsTrue (PlayerModeRules::ResolveMode (MakeEntries (PlayerMode::JoyportLeft,  PlayerMode::SameAsPlayer1), 1, true) == PlayerMode::JoyportRight, L"the other jack");
            Assert::IsTrue (PlayerModeRules::ResolveMode (MakeEntries (PlayerMode::JoyportRight, PlayerMode::SameAsPlayer1), 1, true) == PlayerMode::JoyportLeft,  L"the other jack");
            Assert::IsTrue (PlayerModeRules::ResolveMode (MakeEntries (PlayerMode::JoyportLeft,  PlayerMode::Paddle),        1, true) == PlayerMode::Paddle,       L"an explicit mode stands");
        }


        TEST_METHOD (ResolveMode_WithoutAJoyportTheJacksPlayAsJoysticks)
        {
            PlayerEntries  entries = MakeEntries (PlayerMode::JoyportLeft, PlayerMode::SameAsPlayer1);



            Assert::IsTrue (PlayerModeRules::ResolveMode (entries, 0, false) == PlayerMode::Joystick, L"the //c has no Joyport");
            Assert::IsTrue (PlayerModeRules::ResolveMode (entries, 1, false) == PlayerMode::Joystick, L"and Same as Player 1 follows that");
            Assert::IsTrue (PlayerModeRules::ResolveMode (MakeEntries (PlayerMode::SameAsPlayer1, PlayerMode::Joystick), 0, true) == PlayerMode::Joystick,
                            L"Player 1 has no one to follow");
        }


        TEST_METHOD (IsJoyportOn_WhileAPlayingPlayerIsInAJack)
        {
            PlayerEntries  entries = MakeEntries (PlayerMode::Joystick, PlayerMode::JoyportRight);



            Assert::IsTrue  (PlayerModeRules::IsJoyportOn (entries, true));
            Assert::IsFalse (PlayerModeRules::IsJoyportOn (entries, false), L"not on a machine without one");
            Assert::IsFalse (PlayerModeRules::IsJoyportOn (MakeEntries (PlayerMode::Joystick, PlayerMode::Paddle), true));
            Assert::IsFalse (PlayerModeRules::IsJoyportOn (MakeEntries (PlayerMode::Paddle, PlayerMode::SameAsPlayer1), true));
            Assert::IsTrue  (PlayerModeRules::IsJoyportOn (MakeEntries (PlayerMode::JoyportLeft, PlayerMode::Paddle), true));

            entries[1].kind = PlayerEntryKind::Disabled;
            Assert::IsFalse (PlayerModeRules::IsJoyportOn (entries, true), L"a Disabled Player 2 holds no jack");
        }


        TEST_METHOD (GetJack_TheJacksModesOnly)
        {
            Assert::IsTrue  (PlayerModeRules::GetJack (PlayerMode::JoyportLeft)  == std::optional<size_t> (JoyportJacks::kLeftJack));
            Assert::IsTrue  (PlayerModeRules::GetJack (PlayerMode::JoyportRight) == std::optional<size_t> (JoyportJacks::kRightJack));
            Assert::IsFalse (PlayerModeRules::GetJack (PlayerMode::Joystick).has_value());
            Assert::IsFalse (PlayerModeRules::GetJack (PlayerMode::Paddle).has_value());
        }


        TEST_METHOD (AreButtonsCut_ForAPlayerBesideTheJoyport)
        {
            PlayerEntries  entries = MakeEntries (PlayerMode::JoyportLeft, PlayerMode::Paddle);



            Assert::IsFalse (PlayerModeRules::AreButtonsCut (entries, 0, true), L"the Joyport player fires through its jack");
            Assert::IsTrue  (PlayerModeRules::AreButtonsCut (entries, 1, true), L"the Joyport owns all three button lines");
            Assert::IsFalse (PlayerModeRules::AreButtonsCut (entries, 1, false), L"no Joyport, nothing cut");
            Assert::IsFalse (PlayerModeRules::AreButtonsCut (MakeEntries (PlayerMode::Joystick, PlayerMode::Paddle), 0, true));
        }


        TEST_METHOD (ArePaddlesConnected_UnlessEveryPlayingPlayerIsInAJack)
        {
            PlayerEntries  both = MakeEntries (PlayerMode::JoyportLeft, PlayerMode::SameAsPlayer1);



            Assert::IsFalse (PlayerModeRules::ArePaddlesConnected (both, true), L"two Atari sticks and no paddle");
            Assert::IsTrue  (PlayerModeRules::ArePaddlesConnected (MakeEntries (PlayerMode::JoyportLeft, PlayerMode::Paddle),   true));
            Assert::IsTrue  (PlayerModeRules::ArePaddlesConnected (MakeEntries (PlayerMode::Joystick,    PlayerMode::JoyportRight), true));

            both[1].mode = PlayerMode::Joystick;
            both[1].kind = PlayerEntryKind::Disabled;
            Assert::IsFalse (PlayerModeRules::ArePaddlesConnected (both, true), L"a Disabled player stands in for nothing");
        }


        TEST_METHOD (IsModeTaken_AJackTheOtherPlayerHolds)
        {
            PlayerEntries  entries = MakeEntries (PlayerMode::JoyportLeft, PlayerMode::SameAsPlayer1);



            Assert::IsTrue  (PlayerModeRules::IsModeTaken (entries, 1, PlayerMode::JoyportLeft,  true), L"Player 1 holds the left jack");
            Assert::IsFalse (PlayerModeRules::IsModeTaken (entries, 1, PlayerMode::JoyportRight, true));
            Assert::IsFalse (PlayerModeRules::IsModeTaken (entries, 0, PlayerMode::JoyportRight, true), L"Same as Player 1 follows rather than holds");

            entries[1].mode = PlayerMode::JoyportRight;
            Assert::IsTrue  (PlayerModeRules::IsModeTaken (entries, 0, PlayerMode::JoyportRight, true), L"an explicit jack is held");
            Assert::IsFalse (PlayerModeRules::IsModeTaken (entries, 0, PlayerMode::Joystick,     true), L"only the jacks are held");

            entries[1].kind = PlayerEntryKind::Disabled;
            Assert::IsFalse (PlayerModeRules::IsModeTaken (entries, 0, PlayerMode::JoyportRight, true), L"a Disabled player holds no jack");
        }


        TEST_METHOD (AreKeysAndMouseOffered_ByPlayerOnesMode)
        {
            Assert::IsTrue  (PlayerModeRules::AreKeysOffered (MakeEntries (PlayerMode::Joystick,    PlayerMode::SameAsPlayer1), true));
            Assert::IsTrue  (PlayerModeRules::AreKeysOffered (MakeEntries (PlayerMode::JoyportLeft, PlayerMode::SameAsPlayer1), true));
            Assert::IsFalse (PlayerModeRules::AreKeysOffered (MakeEntries (PlayerMode::Paddle,      PlayerMode::SameAsPlayer1), true));

            Assert::IsTrue  (PlayerModeRules::IsMouseOffered (MakeEntries (PlayerMode::Paddle,      PlayerMode::SameAsPlayer1), true));
            Assert::IsFalse (PlayerModeRules::IsMouseOffered (MakeEntries (PlayerMode::Joystick,    PlayerMode::SameAsPlayer1), true));
            Assert::IsFalse (PlayerModeRules::IsMouseOffered (MakeEntries (PlayerMode::JoyportLeft, PlayerMode::SameAsPlayer1), true));
        }


        TEST_METHOD (BuildModeChoices_InOrderWithSameAsPlayerOneFirstForPlayerTwo)
        {
            PlayerEntries                                  entries = MakeEntries (PlayerMode::JoyportLeft, PlayerMode::SameAsPlayer1);
            std::vector<InputModeRules::PlayerModeChoice>  one     = PlayerModeRules::BuildModeChoices (entries, 0, true);
            std::vector<InputModeRules::PlayerModeChoice>  two     = PlayerModeRules::BuildModeChoices (entries, 1, true);
            std::vector<std::wstring>                      labels;



            for (const InputModeRules::PlayerModeChoice & choice : one)
            {
                labels.push_back (choice.label);
            }

            Assert::IsTrue (labels == std::vector<std::wstring> { L"Joystick", L"Joyport left (Atari)", L"Joyport right (Atari)", L"Paddle" });
            Assert::IsTrue (FindChoice (one, PlayerMode::JoyportLeft).isChecked);

            Assert::AreEqual (size_t (5), two.size());
            Assert::AreEqual (std::wstring (L"Same as Player 1"), two[0].label);
            Assert::IsTrue   (two[0].isChecked, L"its own mode is checked, not what it resolves to");
            Assert::IsFalse  (FindChoice (two, PlayerMode::JoyportLeft).isEnabled,  L"the jack Player 1 holds is shown disabled");
            Assert::IsTrue   (FindChoice (two, PlayerMode::JoyportRight).isEnabled);
            Assert::IsFalse  (FindChoice (two, PlayerMode::JoyportRight).isChecked);
        }


        TEST_METHOD (BuildModeChoices_WithoutAJoyportLeavesTheJacksOut)
        {
            PlayerEntries                                  entries = MakeEntries (PlayerMode::JoyportLeft, PlayerMode::SameAsPlayer1);
            std::vector<InputModeRules::PlayerModeChoice>  one     = PlayerModeRules::BuildModeChoices (entries, 0, false);
            std::vector<InputModeRules::PlayerModeChoice>  two     = PlayerModeRules::BuildModeChoices (entries, 1, false);



            Assert::AreEqual (size_t (2), one.size());
            Assert::IsFalse  (IsModeListed (one, PlayerMode::JoyportLeft));
            Assert::IsFalse  (IsModeListed (one, PlayerMode::JoyportRight));
            Assert::IsTrue   (FindChoice (one, PlayerMode::Joystick).isChecked, L"a saved jack plays as Joystick on the //c");
            Assert::AreEqual (size_t (3), two.size());
            Assert::IsTrue   (FindChoice (two, PlayerMode::SameAsPlayer1).isChecked);
        }


        TEST_METHOD (DescribeAssignment_GivesTheJacksOfAJoyportPlayer)
        {
            PlayerModeRules::JackSet  none;
            PlayerModeRules::JackSet  left;
            PlayerModeRules::JackSet  both;



            left.set (JoyportJacks::kLeftJack);
            both.set();

            Assert::AreEqual (std::wstring (L"Player 1: Pad"),                          PlayerModeRules::DescribeAssignment (0, L"Pad", none));
            Assert::AreEqual (std::wstring (L"Player 2 (Joyport left): Pad"),           PlayerModeRules::DescribeAssignment (1, L"Pad", left));
            Assert::AreEqual (std::wstring (L"Player 1 (Joyport left and right): Pad"), PlayerModeRules::DescribeAssignment (0, L"Pad", both));
        }


        TEST_METHOD (MigrateAdapter_ASavedJoyportIsReadOnceAndTheKeyRemoved)
        {
            JsonValue         joyport = ParseOrFail (R"({"gamePortAdapter":"siriusJoyport"})");
            JoyportMigration  migration;



            migration = PlayerModeRules::MigrateAdapter (false, "siriusJoyport", nullptr, true);
            Assert::IsTrue (migration.isJoyport);
            Assert::IsTrue (migration.shouldRemoveKey, L"the global key is removed once read");

            migration = PlayerModeRules::MigrateAdapter (false, "", &joyport, true);
            Assert::IsTrue  (migration.isJoyport,       L"never set globally: the launched machine's own value");
            Assert::IsFalse (migration.shouldRemoveKey, L"and there is no global key to remove");

            migration = PlayerModeRules::MigrateAdapter (false, "", &joyport, false);
            Assert::IsFalse (migration.isJoyport, L"the //c cannot have saved one");

            migration = PlayerModeRules::MigrateAdapter (false, ControllerTokens::kpszAdapterNone, &joyport, true);
            Assert::IsFalse (migration.isJoyport,       L"Apple mode set globally outranks the machine's value");
            Assert::IsTrue  (migration.shouldRemoveKey);

            migration = PlayerModeRules::MigrateAdapter (true, "siriusJoyport", &joyport, true);
            Assert::IsFalse (migration.isJoyport,       L"saved modes: neither old value is read again");
            Assert::IsTrue  (migration.shouldRemoveKey, L"but a global key left behind is still removed");

            migration = PlayerModeRules::MigrateAdapter (true, "", &joyport, true);
            Assert::IsFalse (migration.isJoyport);
            Assert::IsFalse (migration.shouldRemoveKey, L"nothing to do");
        }

        TEST_METHOD (ApplyMigration_PlayerOneLeftAndPlayerTwoSameAsPlayerOne)
        {
            PlayerEntries  entries = PlayerModeRules::ApplyMigration (MakeEntries (PlayerMode::Paddle, PlayerMode::Joystick));



            Assert::IsTrue (entries[0].mode == PlayerMode::JoyportLeft);
            Assert::IsTrue (entries[1].mode == PlayerMode::SameAsPlayer1);
        }


        //  The jacks are offered by the running machine: a machine builds a
        //  Joyport exactly when it can take one, and the //c builds none. The
        //  shell is driven without Initialize, as ShellKeyWiringTests drives
        //  it.
        TEST_METHOD (IsJoyportOffered_FollowsTheRunningMachine)
        {
            std::unique_ptr<EmulatorShell>  shell = std::make_unique<EmulatorShell>();



            Assert::IsFalse (shell->IsJoyportOffered(), L"a running machine with no Joyport, the //c, is not offered one");

            shell->GetMachine().SetJoyport (std::make_unique<SiriusJoyport>());
            Assert::IsTrue (shell->IsJoyportOffered(), L"a running machine that built one is offered it");
        }
    };
}
