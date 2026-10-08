#include "Pch.h"

#include "Controllers/JoyportJackRules.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  JoyportJackRulesTests
//
//  Which player's switches each Joyport jack carries. Each player's mode
//  puts it in a jack, or in none; a player drives its own jack, and one
//  driving alone also drives the other jack while that jack is free -- no
//  player in it, or its player idle -- so a one-player game works whichever
//  jack it reads. A player who left keeps their jack, which reads open, and
//  the one who stayed never takes it.
//
////////////////////////////////////////////////////////////////////////////////

namespace ControllerTests
{
    TEST_CLASS (JoyportJackRulesTests)
    {
    public:

        using Jacks = std::array<JoyportJackSource, JoyportJacks::kJackCount>;

        static constexpr JoyportPlayerState  kIdle    = JoyportPlayerState::Idle;
        static constexpr JoyportPlayerState  kDriving = JoyportPlayerState::Driving;
        static constexpr JoyportPlayerState  kHeld    = JoyportPlayerState::Held;
        static constexpr JoyportJackSource   kNone    = JoyportJackSource::None;
        static constexpr JoyportJackSource   kOne     = JoyportJackSource::Player1;
        static constexpr JoyportJackSource   kTwo     = JoyportJackSource::Player2;


        static constexpr size_t  kLeft  = JoyportJacks::kLeftJack;
        static constexpr size_t  kRight = JoyportJacks::kRightJack;

        using Jack = std::optional<size_t>;


        static Jacks Assign (JoyportPlayerState first, JoyportPlayerState second, Jack firstJack = kLeft, Jack secondJack = kRight)
        {
            JoyportPlayers  players;

            players.players = { first, second };
            players.jacks   = { firstJack, secondJack };
            return JoyportJackRules::AssignJacks (players);
        }


        //  Every row of the table in contracts/switch-evaluation.md, with
        //  Player 1 in the left jack and Player 2 in the right.
        TEST_METHOD (AssignJacks_EveryRowOfTheTable)
        {
            Assert::IsTrue (Assign (kDriving, kIdle)    == Jacks { kOne,  kOne  }, L"Player 1 alone is on both jacks");
            Assert::IsTrue (Assign (kIdle,    kDriving) == Jacks { kTwo,  kTwo  }, L"Player 2 alone is on both jacks");
            Assert::IsTrue (Assign (kDriving, kDriving) == Jacks { kOne,  kTwo  }, L"two driving split the jacks");
            Assert::IsTrue (Assign (kHeld,    kDriving) == Jacks { kNone, kTwo  }, L"Player 1 left: the left jack is held open");
            Assert::IsTrue (Assign (kDriving, kHeld)    == Jacks { kOne,  kNone }, L"Player 2 left: the right jack is held open");
            Assert::IsTrue (Assign (kIdle,    kIdle)    == Jacks { kNone, kNone }, L"nobody driving");
            Assert::IsTrue (Assign (kHeld,    kIdle)    == Jacks { kNone, kNone });
            Assert::IsTrue (Assign (kIdle,    kHeld)    == Jacks { kNone, kNone });
            Assert::IsTrue (Assign (kHeld,    kHeld)    == Jacks { kNone, kNone });
        }


        //  The jacks follow the modes, not the player numbers: Player 1 in
        //  the right jack and Player 2 in the left drive the jacks they are
        //  in, and a player in no jack leaves the other jack free.
        TEST_METHOD (AssignJacks_EachPlayerDrivesTheJackItsModeGives)
        {
            Assert::IsTrue (Assign (kDriving, kDriving, kRight, kLeft)        == Jacks { kTwo, kOne }, L"the modes, not the numbers");
            Assert::IsTrue (Assign (kDriving, kDriving, kLeft,  std::nullopt) == Jacks { kOne, kOne }, L"Player 2 on Joystick or Paddle leaves the right jack free");
            Assert::IsTrue (Assign (kDriving, kDriving, std::nullopt, kRight) == Jacks { kTwo, kTwo }, L"and Player 1 beside the Joyport the left");
            Assert::IsTrue (Assign (kDriving, kDriving, std::nullopt, std::nullopt) == Jacks { kNone, kNone }, L"nobody in a jack");
            Assert::IsTrue (Assign (kHeld,    kDriving, std::nullopt, kRight) == Jacks { kTwo, kTwo }, L"a held player in no jack holds none");
        }


        //  Over every pair of states and jacks: a jack's source is always a
        //  driving player in a jack, and a held player's jack is never
        //  handed to the other.
        TEST_METHOD (AssignJacks_AJackIsOnlyEverADrivingPlayersAndAHeldJackIsNeverHandedOver)
        {
            const JoyportPlayerState  states[] = { kIdle, kDriving, kHeld };
            const Jack                places[] = { std::nullopt, kLeft, kRight };
            size_t                    checked  = 0;



            for (JoyportPlayerState first : states)
            {
                for (JoyportPlayerState second : states)
                {
                    for (Jack firstJack : places)
                    {
                        for (Jack secondJack : places)
                        {
                            Jacks  jacks = {};

                            if (firstJack.has_value() && firstJack == secondJack)
                            {
                                continue;
                            }

                            jacks = Assign (first, second, firstJack, secondJack);

                            for (JoyportJackSource source : jacks)
                            {
                                Assert::IsTrue (source != kOne || (first  == kDriving && firstJack.has_value()),  L"Player 1's switches only while it drives in a jack");
                                Assert::IsTrue (source != kTwo || (second == kDriving && secondJack.has_value()), L"Player 2's only while it drives in a jack");
                            }

                            Assert::IsTrue (first  != kHeld || !firstJack.has_value()  || jacks[firstJack.value()]  == kNone, L"a held jack reads open");
                            Assert::IsTrue (second != kHeld || !secondJack.has_value() || jacks[secondJack.value()] == kNone, L"a held jack reads open");
                            checked++;
                        }
                    }
                }
            }

            Assert::AreEqual (size_t (63), checked);
        }


        //  The jacks come from each player's mode as it resolves: Same as
        //  Player 1 is the other jack, a Disabled Player 2 is in none, and a
        //  machine without a Joyport has none.
        TEST_METHOD (ReducePlayers_TheJacksFromTheModes)
        {
            PlayerEntries   entries;
            PlayerSlots     slots;
            JoyportPlayers  players;



            entries[0].mode = PlayerMode::JoyportLeft;
            entries[1].mode = PlayerMode::SameAsPlayer1;
            players         = JoyportJackRules::ReducePlayers (slots, entries, true);
            Assert::IsTrue (players.jacks[0] == Jack (kLeft));
            Assert::IsTrue (players.jacks[1] == Jack (kRight), L"Same as Player 1 is the other jack");

            entries[1].mode = PlayerMode::Paddle;
            players         = JoyportJackRules::ReducePlayers (slots, entries, true);
            Assert::IsFalse (players.jacks[1].has_value());

            entries[1].mode = PlayerMode::SameAsPlayer1;
            entries[1].kind = PlayerEntryKind::Disabled;
            players         = JoyportJackRules::ReducePlayers (slots, entries, true);
            Assert::IsFalse (players.jacks[1].has_value(), L"a Disabled player is in no jack");

            players = JoyportJackRules::ReducePlayers (slots, entries, false);
            Assert::IsFalse (players.jacks[0].has_value(), L"no Joyport, no jacks");
        }


        //  What a player's slot and entry come to: a controller in play or the
        //  arrow keys drive; a held slot is held; anything else is idle, the
        //  mouse as paddle included, since it closes no switch.
        TEST_METHOD (ReducePlayerState_FromTheSlotAndTheEntry)
        {
            PlayerSlot   slot;
            PlayerEntry  entry;



            slot.holder = ControllerUnitKey();

            slot.state = PlayerSlotState::Playing;
            Assert::IsTrue (JoyportJackRules::ReducePlayerState (0, slot, entry) == kDriving);

            slot.state = PlayerSlotState::Provisional;
            Assert::IsTrue (JoyportJackRules::ReducePlayerState (0, slot, entry) == kDriving);

            slot.state = PlayerSlotState::Held;
            Assert::IsTrue (JoyportJackRules::ReducePlayerState (1, slot, entry) == kHeld);

            slot.state = PlayerSlotState::Waiting;
            Assert::IsTrue (JoyportJackRules::ReducePlayerState (1, slot, entry) == kIdle, L"a holder that has not given input");

            slot       = PlayerSlot();
            entry.kind = PlayerEntryKind::ArrowKeys;
            Assert::IsTrue (JoyportJackRules::ReducePlayerState (0, slot, entry) == kDriving, L"the arrow keys drive Player 1");

            entry.kind = PlayerEntryKind::MousePaddle;
            Assert::IsTrue (JoyportJackRules::ReducePlayerState (0, slot, entry) == kIdle, L"the mouse closes no switch");
        }
    };
}
