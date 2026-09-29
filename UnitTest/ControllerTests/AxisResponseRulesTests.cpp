#include "Pch.h"

#include "Controllers/AxisResponseRules.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AxisResponseRulesTests
//
//  Which responses a binding may have in a profile of each kind: Position
//  only in a Joystick profile, Paddle speed in a Paddle profile with Position
//  offered beside it only for a DirectInput axis on a controller that is not
//  a gamepad, and a Joyport profile left as it is.
//
////////////////////////////////////////////////////////////////////////////////

namespace ControllerTests
{
    TEST_CLASS (AxisResponseRulesTests)
    {
    public:

        static AxisBinding MakeAnalog (ControlKind control, AxisResponse response)
        {
            AxisBinding  binding;



            binding.analog   = { control, 0 };
            binding.response = response;
            return binding;
        }


        //  A Paddle profile offers Position for a DirectInput axis on a
        //  joystick or a wheel, and on nothing else.
        TEST_METHOD (Position_IsOfferedOnlyForAKnobInAPaddleProfile)
        {
            AxisBinding  axis    = MakeAnalog (ControlKind::Axis,    AxisResponse::Rate);
            AxisBinding  trigger = MakeAnalog (ControlKind::Trigger, AxisResponse::Rate);
            AxisBinding  pair;



            pair.kind     = AxisBindingKind::DigitalPair;
            pair.negative = { ControlKind::DpadLeft,  0 };
            pair.positive = { ControlKind::DpadRight, 0 };

            Assert::IsTrue  (AxisResponseRules::IsPositionOffered (ProfileMode::Paddle,   axis,    ControllerKind::DirectInput, ControllerFormFactor::Joystick), L"a DirectInput joystick's axis");
            Assert::IsTrue  (AxisResponseRules::IsPositionOffered (ProfileMode::Paddle,   axis,    ControllerKind::DirectInput, ControllerFormFactor::Wheel),    L"a wheel");
            Assert::IsFalse (AxisResponseRules::IsPositionOffered (ProfileMode::Paddle,   axis,    ControllerKind::DirectInput, ControllerFormFactor::Gamepad),  L"not a DirectInput gamepad's stick");
            Assert::IsFalse (AxisResponseRules::IsPositionOffered (ProfileMode::Paddle,   axis,    ControllerKind::XInput,      ControllerFormFactor::Gamepad),  L"not an Xbox controller's stick");
            Assert::IsFalse (AxisResponseRules::IsPositionOffered (ProfileMode::Paddle,   axis,    ControllerKind::XInput,      ControllerFormFactor::Wheel),    L"nor an XInput wheel's");
            Assert::IsFalse (AxisResponseRules::IsPositionOffered (ProfileMode::Paddle,   trigger, ControllerKind::DirectInput, ControllerFormFactor::Joystick), L"not a trigger");
            Assert::IsFalse (AxisResponseRules::IsPositionOffered (ProfileMode::Paddle,   pair,    ControllerKind::DirectInput, ControllerFormFactor::Joystick), L"not a digital pair");
            Assert::IsFalse (AxisResponseRules::IsPositionOffered (ProfileMode::Joystick, axis,    ControllerKind::DirectInput, ControllerFormFactor::Joystick), L"a Joystick profile offers no choice");
            Assert::IsFalse (AxisResponseRules::IsPositionOffered (ProfileMode::Joyport,  axis,    ControllerKind::DirectInput, ControllerFormFactor::Joystick), L"nor does a Joyport profile");
        }


        //  A Joystick profile's axes always give position.
        TEST_METHOD (JoystickProfile_AlwaysGivesPosition)
        {
            AxisBinding  rate = MakeAnalog (ControlKind::Axis, AxisResponse::Rate);



            Assert::IsTrue (AxisResponseRules::GetAllowedResponse (ProfileMode::Joystick, rate, ControllerKind::XInput,      ControllerFormFactor::Gamepad)  == AxisResponse::Absolute, L"an Xbox stick");
            Assert::IsTrue (AxisResponseRules::GetAllowedResponse (ProfileMode::Joystick, rate, ControllerKind::DirectInput, ControllerFormFactor::Joystick) == AxisResponse::Absolute, L"a DirectInput stick");
            Assert::IsTrue (AxisResponseRules::GetNewResponse     (ProfileMode::Joystick, rate, ControllerKind::DirectInput, ControllerFormFactor::Joystick, AxisRole::Throttle) == AxisResponse::Absolute, L"and a new binding");
        }


        //  A Paddle profile plays Position only where it is offered, and a new
        //  binding starts at Paddle speed.
        TEST_METHOD (PaddleProfile_PlaysPositionOnlyOnAKnob)
        {
            AxisBinding  position = MakeAnalog (ControlKind::Axis, AxisResponse::Absolute);
            AxisBinding  rate     = MakeAnalog (ControlKind::Axis, AxisResponse::Rate);



            Assert::IsTrue (AxisResponseRules::GetAllowedResponse (ProfileMode::Paddle, position, ControllerKind::XInput,      ControllerFormFactor::Gamepad)  == AxisResponse::Rate,     L"Position on an Xbox stick plays as Paddle speed");
            Assert::IsTrue (AxisResponseRules::GetAllowedResponse (ProfileMode::Paddle, position, ControllerKind::DirectInput, ControllerFormFactor::Gamepad)  == AxisResponse::Rate,     L"and on a DirectInput gamepad's");
            Assert::IsTrue (AxisResponseRules::GetAllowedResponse (ProfileMode::Paddle, position, ControllerKind::DirectInput, ControllerFormFactor::Wheel)    == AxisResponse::Absolute, L"a wheel keeps Position");
            Assert::IsTrue (AxisResponseRules::GetAllowedResponse (ProfileMode::Paddle, rate,     ControllerKind::DirectInput, ControllerFormFactor::Wheel)    == AxisResponse::Rate,     L"or Paddle speed");
            Assert::IsTrue (AxisResponseRules::GetNewResponse     (ProfileMode::Paddle, position, ControllerKind::DirectInput, ControllerFormFactor::Wheel, AxisRole::Centering) == AxisResponse::Rate, L"a new binding on a centering axis starts at Paddle speed");
        }


        //  A Joyport profile's responses are left as they are.
        TEST_METHOD (JoyportProfile_IsUnchanged)
        {
            AxisBinding  rate = MakeAnalog (ControlKind::Axis, AxisResponse::Rate);



            Assert::IsTrue (AxisResponseRules::GetAllowedResponse (ProfileMode::Joyport, rate, ControllerKind::XInput, ControllerFormFactor::Gamepad) == AxisResponse::Rate);
            Assert::IsTrue (AxisResponseRules::GetNewResponse     (ProfileMode::Joyport, rate,                                  ControllerKind::XInput, ControllerFormFactor::Gamepad, AxisRole::Centering) == AxisResponse::Rate);
            Assert::IsTrue (AxisResponseRules::GetNewResponse     (ProfileMode::Joyport, MakeAnalog (ControlKind::Axis, AxisResponse::Absolute), ControllerKind::XInput, ControllerFormFactor::Gamepad, AxisRole::Centering) == AxisResponse::Absolute);
        }


        //  In a Paddle profile a new binding on an axis that stays where it is
        //  left starts at Position where Position is offered, and at Paddle
        //  speed elsewhere: a gamepad's slider and an Xbox stick have no
        //  Position to start at.
        TEST_METHOD (PaddleProfile_NewBindingOnANonCenteringAxisStartsAtPosition)
        {
            AxisBinding  rate = MakeAnalog (ControlKind::Axis, AxisResponse::Rate);



            Assert::IsTrue (AxisResponseRules::GetNewResponse (ProfileMode::Paddle, rate, ControllerKind::DirectInput, ControllerFormFactor::Joystick, AxisRole::JoystickZ) == AxisResponse::Absolute, L"a joystick's throttle");
            Assert::IsTrue (AxisResponseRules::GetNewResponse (ProfileMode::Paddle, rate, ControllerKind::DirectInput, ControllerFormFactor::Joystick, AxisRole::Slider)    == AxisResponse::Absolute, L"a slider");
            Assert::IsTrue (AxisResponseRules::GetNewResponse (ProfileMode::Paddle, rate, ControllerKind::DirectInput, ControllerFormFactor::Joystick, AxisRole::Centering) == AxisResponse::Rate,     L"a joystick's stick");
            Assert::IsTrue (AxisResponseRules::GetNewResponse (ProfileMode::Paddle, rate, ControllerKind::DirectInput, ControllerFormFactor::Gamepad,  AxisRole::Slider)    == AxisResponse::Rate,     L"a gamepad's slider");
        }


        //  A saved Paddle speed on a throttle stays Paddle speed: only a new
        //  binding's starting response depends on the axis's role.
        TEST_METHOD (PaddleProfile_KeepsASavedPaddleSpeedOnAThrottle)
        {
            ControlMapping  mapping;
            AxisBinding     throttle = MakeAnalog (ControlKind::Axis, AxisResponse::Rate);



            throttle.analog = { ControlKind::Axis, 2 };
            mapping.pdl0    = { throttle };

            AxisResponseRules::Normalize (mapping, ProfileMode::Paddle, ControllerKind::DirectInput, ControllerFormFactor::Joystick);

            Assert::IsTrue (mapping.pdl0[0].response == AxisResponse::Rate);
        }


        //  A saved profile is loaded with each analog binding at its allowed
        //  response, on every axis target, keeping its paddle speed.
        TEST_METHOD (Normalize_SetsEveryAnalogBindingOnEveryAxis)
        {
            constexpr float  kSpeed    = 300.0f;
            ControlMapping   paddle;
            ControlMapping   joystick;
            AxisBinding      position  = MakeAnalog (ControlKind::Axis, AxisResponse::Absolute);
            AxisBinding      rate      = MakeAnalog (ControlKind::Axis, AxisResponse::Rate);



            position.maxSpeed = kSpeed;
            paddle.pdl0  = { position };
            paddle.pdl1  = { rate, position };
            paddle.pdl2  = { position };
            paddle.pdl3  = { position };
            joystick     = paddle;

            AxisResponseRules::Normalize (paddle,   ProfileMode::Paddle,   ControllerKind::XInput, ControllerFormFactor::Gamepad);
            AxisResponseRules::Normalize (joystick, ProfileMode::Joystick, ControllerKind::XInput, ControllerFormFactor::Gamepad);

            Assert::IsTrue   (paddle.pdl0[0].response == AxisResponse::Rate, L"PDL0's Position loads as Paddle speed");
            Assert::AreEqual (kSpeed, paddle.pdl0[0].maxSpeed, L"at its own speed");
            Assert::IsTrue   (paddle.pdl1[1].response == AxisResponse::Rate, L"every binding, not only the first");
            Assert::IsTrue   (paddle.pdl2[0].response == AxisResponse::Rate, L"PDL2");
            Assert::IsTrue   (paddle.pdl3[0].response == AxisResponse::Rate, L"PDL3");
            Assert::IsTrue   (joystick.pdl1[0].response == AxisResponse::Absolute, L"a Joystick profile's Paddle speed loads as Position");
        }
    };
}
