#include "Pch.h"

#include "Controllers/PlayerTargetRules.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PlayerTargetRulesTests
//
//  What each player's controller reaches: the target the players' modes give
//  it, and the lines each target is wired to on the hardware. The button table is
//  checked by sweeping every target, so a target the table forgot fails
//  rather than quietly reaching nothing.
//
////////////////////////////////////////////////////////////////////////////////

namespace ControllerTests
{
    TEST_CLASS (PlayerTargetRulesTests)
    {
    public:

        static constexpr size_t  kFourAxes = 4;
        static constexpr size_t  kTwoAxes  = 2;
        static constexpr size_t  kPb0      = 0;
        static constexpr size_t  kPb1      = 1;
        static constexpr size_t  kPb2      = 2;
        static constexpr size_t  kPdl2     = 2;
        static constexpr size_t  kPdl3     = 3;


        //  FR-039's table: Player 1 takes joystick 0, paddle 0 in Paddle mode,
        //  or paddles 0 and 1 in Two paddles mode; Player 2 takes joystick 1,
        //  paddles 2 and 3 in Two paddles mode, or in Paddle mode paddle 1
        //  beside Player 1's single paddle and paddle 2 beside anything else.
        TEST_METHOD (ModeTarget_EveryRowOfTheWiringTable)
        {
            struct Row
            {
                size_t            player;
                bool              isPaddle;
                bool              isTwoPaddles;
                bool              isPlayerOnePaddle;
                PlayerAxisTarget  expected;
            };

            const Row  kRows[] =
            {
                { 0, false, false, false, PlayerAxisTarget::Joystick0 },
                { 0, false, false, true,  PlayerAxisTarget::Joystick0 },
                { 0, true,  false, false, PlayerAxisTarget::Paddle0   },
                { 0, true,  false, true,  PlayerAxisTarget::Paddle0   },
                { 0, false, true,  false, PlayerAxisTarget::Paddles01 },
                { 1, false, false, false, PlayerAxisTarget::Joystick1 },
                { 1, false, false, true,  PlayerAxisTarget::Joystick1 },
                { 1, true,  false, true,  PlayerAxisTarget::Paddle1   },
                { 1, true,  false, false, PlayerAxisTarget::Paddle2   },
                { 1, false, true,  true,  PlayerAxisTarget::Paddles23 },
                { 1, false, true,  false, PlayerAxisTarget::Paddles23 },
            };

            for (const Row & row : kRows)
            {
                Assert::AreEqual ((int) row.expected,
                                  (int) PlayerTargetRules::GetModeTarget (row.player, row.isPaddle, row.isTwoPaddles, row.isPlayerOnePaddle),
                                  std::format (L"player {}, {} mode, beside a {}", row.player + 1,
                                               row.isTwoPaddles ? L"Two paddles" : row.isPaddle ? L"Paddle" : L"Joystick",
                                               row.isPlayerOnePaddle ? L"paddle" : L"joystick").c_str());
            }
        }


        //  One player on its own: a joystick drives what a single controller
        //  always has, and a paddle drives PDL0 and PB0 from whichever slot it
        //  holds.
        TEST_METHOD (LoneRoute_FollowsTheMode)
        {
            PlayerTargetRules::Route  joystick = PlayerTargetRules::GetLoneRoute (PlayerAxisTarget::Joystick1, kFourAxes);
            PlayerTargetRules::Route  paddle   = PlayerTargetRules::GetLoneRoute (PlayerAxisTarget::Paddle2,   kFourAxes);
            PlayerTargetRules::Route  pair     = PlayerTargetRules::GetLoneRoute (PlayerAxisTarget::Paddles23, kFourAxes);

            Assert::IsTrue (joystick == PlayerTargetRules::GetSingleRoute (kFourAxes), L"a lone joystick drives PDL0, PDL1 and PB0-PB2");
            Assert::IsTrue (paddle.paddles[0] == 0 && !paddle.paddles[1].has_value(), L"a lone paddle drives PDL0 alone");
            Assert::IsTrue (paddle.buttons[kPb0] == kPb0 && !paddle.buttons[kPb1].has_value() && !paddle.buttons[kPb2].has_value(),
                            L"and PB0 alone");
            Assert::IsTrue (pair.paddles[0] == 0 && pair.paddles[1] == 1 && !pair.paddles[kPdl2].has_value(),
                            L"a lone pair of paddles drives PDL0 and PDL1");
            Assert::IsTrue (pair.buttons[kPb0] == kPb0 && pair.buttons[kPb1] == kPb1 && !pair.buttons[kPb2].has_value(),
                            L"and PB0 and PB1");
        }

        //  Every target, with the lines the hardware wires it to: joystick 0
        //  to PB0 and PB1, joystick 1 to PB2 from its first button, a single
        //  paddle to its own line from its first button, paddle 3 to none, and
        //  a pair of paddles to the lines of the joystick it stands in for.
        TEST_METHOD (ButtonRoute_EveryTargetReachesTheLinesItIsWiredTo)
        {
            using Route = PlayerTargetRules::ButtonRoute;

            const Route  kNone  = {};
            const Route  kJoy0  = { kPb0, kPb1, std::nullopt };
            const Route  kToPb0 = { kPb0, std::nullopt, std::nullopt };
            const Route  kToPb1 = { kPb1, std::nullopt, std::nullopt };
            const Route  kToPb2 = { kPb2, std::nullopt, std::nullopt };
            int          target = 0;
            size_t       swept  = 0;

            for (target = (int) PlayerAxisTarget::Joystick0; target <= (int) PlayerAxisTarget::Paddles23; target++)
            {
                Route  expected = kNone;

                switch ((PlayerAxisTarget) target)
                {
                    case PlayerAxisTarget::Joystick0:  expected = kJoy0;  break;
                    case PlayerAxisTarget::Joystick1:  expected = kToPb2; break;
                    case PlayerAxisTarget::Paddle0:    expected = kToPb0; break;
                    case PlayerAxisTarget::Paddle1:    expected = kToPb1; break;
                    case PlayerAxisTarget::Paddle2:    expected = kToPb2; break;
                    case PlayerAxisTarget::Paddle3:    expected = kNone;  break;
                    case PlayerAxisTarget::Paddles01:  expected = kJoy0;  break;
                    case PlayerAxisTarget::Paddles23:  expected = kToPb2; break;

                    default:
                        Assert::Fail (L"a target the sweep does not know");
                }

                Assert::IsTrue (PlayerTargetRules::GetButtonRoute ((PlayerAxisTarget) target) == expected,
                                std::format (L"target {} reaches the lines it is wired to", target).c_str());
                swept++;
            }

            Assert::AreEqual ((size_t) 8, swept, L"all eight targets were checked");
        }


        TEST_METHOD (SingleRoute_DrivesPdl0Pdl1AndEveryButton)
        {
            PlayerTargetRules::Route  route = PlayerTargetRules::GetSingleRoute (kFourAxes);

            Assert::IsTrue (route.paddles[0] == 0, L"its pdl0 on PDL0");
            Assert::IsTrue (route.paddles[1] == 1, L"its pdl1 on PDL1");
            Assert::IsFalse (route.paddles[kPdl2].has_value(), L"nothing on PDL2, which a second player would hold");
            Assert::IsFalse (route.paddles[kPdl3].has_value());
            Assert::IsTrue (route.buttons[kPb0] == kPb0 && route.buttons[kPb1] == kPb1 && route.buttons[kPb2] == kPb2,
                L"and pb0-pb2 on PB0-PB2");

            route = PlayerTargetRules::GetSingleRoute (kTwoAxes);

            Assert::AreEqual ((size_t) 2, PlayerTargetRules::CountPaddles (route), L"a //c still gets both of its paddles");
        }


        TEST_METHOD (TargetRoute_PdlBindingsLandOnTheSlotsPaddlesInOrder)
        {
            PlayerTargetRules::Route  joystick1 = PlayerTargetRules::GetTargetRoute (PlayerAxisTarget::Joystick1, kFourAxes);
            PlayerTargetRules::Route  paddle1   = PlayerTargetRules::GetTargetRoute (PlayerAxisTarget::Paddle1,   kFourAxes);

            Assert::IsTrue (joystick1.paddles[0] == kPdl2 && joystick1.paddles[1] == kPdl3,
                L"joystick 1 plays the controller's pdl0 and pdl1 on PDL2 and PDL3");
            Assert::IsTrue (paddle1.paddles[0] == 1 && !paddle1.paddles[1].has_value(),
                L"a single paddle plays only its pdl0 bindings");
            Assert::AreEqual ((size_t) 0, PlayerTargetRules::CountPaddles (PlayerTargetRules::GetTargetRoute (PlayerAxisTarget::Joystick1, kTwoAxes)),
                L"a //c has none of joystick 1's paddles");
        }
    };
}
