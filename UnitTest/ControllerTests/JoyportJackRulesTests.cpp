#include "Pch.h"

#include "Controllers/JoyportJackRules.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  JoyportJackRulesTests
//
//  Which player's switches each Joyport jack carries. One player driving
//  alone is on both jacks; two driving split them, Player 1 left and Player 2
//  right; a player who left keeps their jack, which reads open, and the one
//  who stayed never takes it; Player 2 set to Same as left leaves Player 1 on
//  both whatever Player 2's controller does.
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


        static Jacks Assign (JoyportPlayerState first, JoyportPlayerState second, bool isPlayer2Disabled = false)
        {
            JoyportPlayers  players;

            players.players           = { first, second };
            players.isPlayer2Disabled = isPlayer2Disabled;
            return JoyportJackRules::AssignJacks (players);
        }


        //  Every row of the table in contracts/switch-evaluation.md.
        TEST_METHOD (AssignJacks_EveryRowOfTheTable)
        {
            Assert::IsTrue (Assign (kDriving, kIdle)          == Jacks { kOne,  kOne  }, L"Player 1 alone is on both jacks");
            Assert::IsTrue (Assign (kIdle,    kDriving)       == Jacks { kTwo,  kTwo  }, L"Player 2 alone is on both jacks");
            Assert::IsTrue (Assign (kDriving, kIdle,    true) == Jacks { kOne,  kOne  }, L"Same as left: Player 1 on both");
            Assert::IsTrue (Assign (kDriving, kDriving)       == Jacks { kOne,  kTwo  }, L"two driving split the jacks");
            Assert::IsTrue (Assign (kHeld,    kDriving)       == Jacks { kNone, kTwo  }, L"Player 1 left: the left jack is held open");
            Assert::IsTrue (Assign (kDriving, kHeld)          == Jacks { kOne,  kNone }, L"Player 2 left: the right jack is held open");
            Assert::IsTrue (Assign (kIdle,    kIdle)          == Jacks { kNone, kNone }, L"nobody driving");
            Assert::IsTrue (Assign (kHeld,    kIdle)          == Jacks { kNone, kNone });
            Assert::IsTrue (Assign (kIdle,    kHeld)          == Jacks { kNone, kNone });
            Assert::IsTrue (Assign (kHeld,    kHeld)          == Jacks { kNone, kNone });
        }


        //  Same as left keeps Player 1 on both jacks even while a second
        //  controller drives for Player 2.
        TEST_METHOD (AssignJacks_SameAsLeftKeepsPlayerOneOnBoth)
        {
            Assert::IsTrue (Assign (kDriving, kDriving, true) == Jacks { kOne,  kOne  });
            Assert::IsTrue (Assign (kIdle,    kDriving, true) == Jacks { kNone, kNone }, L"Player 2 drives nothing while set to Same as left");
        }


        //  Over every pair of states, both ways of Player 2's entry: a jack's
        //  source is always a driving player, and a held player's jack is
        //  never handed to the other.
        TEST_METHOD (AssignJacks_AJackIsOnlyEverADrivingPlayersAndAHeldJackIsNeverHandedOver)
        {
            const JoyportPlayerState  states[] = { kIdle, kDriving, kHeld };
            size_t                    checked  = 0;



            for (JoyportPlayerState first : states)
            {
                for (JoyportPlayerState second : states)
                {
                    for (bool isDisabled : { false, true })
                    {
                        Jacks  jacks = Assign (first, second, isDisabled);

                        for (JoyportJackSource source : jacks)
                        {
                            Assert::IsTrue (source != kOne || first == kDriving,                   L"Player 1's switches only while Player 1 drives");
                            Assert::IsTrue (source != kTwo || (second == kDriving && !isDisabled), L"Player 2's only while Player 2 drives and is not Same as left");
                        }

                        Assert::IsTrue (first  != kHeld || jacks[JoyportJacks::kLeftJack]  == kNone, L"a held left jack reads open");
                        Assert::IsTrue (second != kHeld || isDisabled || jacks[JoyportJacks::kRightJack] == kNone, L"a held right jack reads open");
                        checked++;
                    }
                }
            }

            Assert::AreEqual (size_t (18), checked);
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
