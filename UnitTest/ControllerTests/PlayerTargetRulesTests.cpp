#include "Pch.h"

#include "Controllers/PlayerTargetRules.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PlayerTargetRulesTests
//
//  What each player's controller reaches: the target a profile implies, and
//  the lines each target is wired to on the hardware. The button table is
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


        static ControlMapping MakeJoystickMapping()
        {
            ControlMapping  mapping;

            mapping.pdl0.push_back ({ AxisBindingKind::Analog, { ControlKind::Axis, 0 } });
            mapping.pdl1.push_back ({ AxisBindingKind::Analog, { ControlKind::Axis, 1 } });
            return mapping;
        }


        static ControlMapping MakePaddleMapping()
        {
            ControlMapping  mapping;

            mapping.pdl0.push_back ({ AxisBindingKind::Analog, { ControlKind::Axis, 0 } });
            return mapping;
        }


        TEST_METHOD (AutomaticTarget_PlayerOneIsJoystickZeroOrPaddleZero)
        {
            ControlMapping  yOnly;

            yOnly.pdl1.push_back ({ AxisBindingKind::Analog, { ControlKind::Axis, 1 } });

            Assert::AreEqual ((int) PlayerAxisTarget::Joystick0,
                              (int) PlayerTargetRules::GetAutomaticTarget (0, MakeJoystickMapping(), PlayerAxisTarget::Joystick1),
                              L"a profile binding both axes plays joystick 0");
            Assert::AreEqual ((int) PlayerAxisTarget::Paddle0,
                              (int) PlayerTargetRules::GetAutomaticTarget (0, MakePaddleMapping(), PlayerAxisTarget::Joystick1),
                              L"one binding PDL0 and not PDL1 plays paddle 0");
            Assert::AreEqual ((int) PlayerAxisTarget::Joystick0,
                              (int) PlayerTargetRules::GetAutomaticTarget (0, ControlMapping(), PlayerAxisTarget::Joystick1),
                              L"one binding neither is not a paddle");
            Assert::AreEqual ((int) PlayerAxisTarget::Joystick0,
                              (int) PlayerTargetRules::GetAutomaticTarget (0, yOnly, PlayerAxisTarget::Joystick1),
                              L"and nor is one binding PDL1 alone");
        }


        TEST_METHOD (AutomaticTarget_PlayerTwoComesToRestBesidePlayerOne)
        {
            Assert::AreEqual ((int) PlayerAxisTarget::Joystick1,
                              (int) PlayerTargetRules::GetAutomaticTarget (1, MakeJoystickMapping(), PlayerAxisTarget::Joystick0),
                              L"a joystick profile plays joystick 1");
            Assert::AreEqual ((int) PlayerAxisTarget::Paddle1,
                              (int) PlayerTargetRules::GetAutomaticTarget (1, MakePaddleMapping(), PlayerAxisTarget::Paddle0),
                              L"a paddle beside player one's paddle 0 is paddle 1");
            Assert::AreEqual ((int) PlayerAxisTarget::Paddle2,
                              (int) PlayerTargetRules::GetAutomaticTarget (1, MakePaddleMapping(), PlayerAxisTarget::Joystick0),
                              L"and beside player one's joystick 0 it is paddle 2");
        }


        //  Every target, with the lines the hardware wires it to: joystick 0
        //  to PB0 and PB1, joystick 1 to PB2 from its first button, a single
        //  paddle to its own line from its first button, and paddle 3 to none.
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

            for (target = (int) PlayerAxisTarget::Joystick0; target <= (int) PlayerAxisTarget::Paddle3; target++)
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

                    default:
                        Assert::Fail (L"a target the sweep does not know");
                }

                Assert::IsTrue (PlayerTargetRules::GetButtonRoute ((PlayerAxisTarget) target) == expected,
                                std::format (L"target {} reaches the lines it is wired to", target).c_str());
                swept++;
            }

            Assert::AreEqual ((size_t) 6, swept, L"all six targets were checked");
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
