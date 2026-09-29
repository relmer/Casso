#include "Pch.h"

#include "Controllers/AxisRoleRules.h"
#include "Controllers/ControlLabels.h"
#include "Controllers/ControlMapping.h"
#include "Controllers/DirectInputSampleDecoder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AxisRoleRulesTests
//
//  Which DirectInput axes stay where they are left. Only a Slider, Dial or
//  Throttle usage, DirectInput's slider type, and a joystick's Z count; a
//  wheel, rudder pedals, and accelerator and brake pedals may spring back and
//  stay centering, whatever slot they land in.
//
////////////////////////////////////////////////////////////////////////////////

namespace ControllerTests
{
    TEST_CLASS (AxisRoleRulesTests)
    {
    public:

        static constexpr int  kAxisX       = 0;
        static constexpr int  kAxisZ       = DefaultMapping::kAxisZ;
        static constexpr int  kAxisRz      = DefaultMapping::kAxisRz;
        static constexpr int  kFirstSlider = 6;


        //  The VKBsim Gladiator as enumeration reports it: X and Y, and its
        //  throttle on the Z axis, which it reports with the Generic Desktop
        //  Z usage on a joystick-class device.
        static std::vector<ControlId> MakeGladiatorControls()
        {
            AxisRole  z = AxisRoleRules::Classify (AxisRoleRules::kUsagePageGeneric, AxisRoleRules::kUsageZ, false, kAxisZ, ControllerFormFactor::Joystick);



            return { { ControlKind::Axis, 0 }, { ControlKind::Axis, 1 }, { ControlKind::Axis, kAxisZ, z },
                     { ControlKind::Button, 0 }, { ControlKind::Button, 1 } };
        }


        //  A Slider, Dial or Throttle usage stays where it is left on any
        //  device, and so does an axis DirectInput typed a slider.
        TEST_METHOD (Classify_SliderDialAndThrottle_AreNonCentering)
        {
            constexpr Word  kGeneric    = AxisRoleRules::kUsagePageGeneric;
            constexpr Word  kSimulation = AxisRoleRules::kUsagePageSimulation;

            Assert::IsTrue (AxisRoleRules::Classify (kSimulation, AxisRoleRules::kUsageThrottle, false, kFirstSlider, ControllerFormFactor::Joystick) == AxisRole::Throttle, L"a Throttle usage");
            Assert::IsTrue (AxisRoleRules::Classify (kGeneric,    AxisRoleRules::kUsageSlider,   false, kFirstSlider, ControllerFormFactor::Gamepad)  == AxisRole::Slider,   L"a Slider usage, even on a gamepad");
            Assert::IsTrue (AxisRoleRules::Classify (kGeneric,    AxisRoleRules::kUsageDial,     false, kAxisRz,      ControllerFormFactor::Wheel)    == AxisRole::Dial,     L"a Dial usage");
            Assert::IsTrue (AxisRoleRules::Classify (0,           0,                             true,  kFirstSlider, ControllerFormFactor::Joystick) == AxisRole::Slider,   L"DirectInput's slider type with no usage");
        }


        //  A wheel, rudder pedals, and accelerator and brake pedals stay
        //  centering, even in the Z slot of a joystick, and even typed a
        //  slider: they often spring back, to center or to one end.
        TEST_METHOD (Classify_WheelRudderAcceleratorAndBrake_AreCentering)
        {
            constexpr Word  kGeneric    = AxisRoleRules::kUsagePageGeneric;
            constexpr Word  kSimulation = AxisRoleRules::kUsagePageSimulation;

            Assert::IsTrue (AxisRoleRules::Classify (kGeneric,    AxisRoleRules::kUsageWheel,       false, kAxisX,       ControllerFormFactor::Wheel)    == AxisRole::Centering, L"a Wheel usage");
            Assert::IsTrue (AxisRoleRules::Classify (kSimulation, AxisRoleRules::kUsageRudder,      false, kAxisZ,       ControllerFormFactor::Joystick) == AxisRole::Centering, L"a Rudder usage in a joystick's Z slot");
            Assert::IsTrue (AxisRoleRules::Classify (kSimulation, AxisRoleRules::kUsageRudder,      true,  kFirstSlider, ControllerFormFactor::Joystick) == AxisRole::Centering, L"a Rudder usage typed a slider");
            Assert::IsTrue (AxisRoleRules::Classify (kSimulation, AxisRoleRules::kUsageAccelerator, false, kAxisZ,       ControllerFormFactor::Wheel)    == AxisRole::Centering, L"an Accelerator usage");
            Assert::IsTrue (AxisRoleRules::Classify (kSimulation, AxisRoleRules::kUsageBrake,       false, kAxisRz,      ControllerFormFactor::Wheel)    == AxisRole::Centering, L"a Brake usage");
        }


        //  A Z axis stays where it is left only on a joystick-class device,
        //  where it is the throttle by convention; a gamepad's or a wheel's
        //  Z, and a joystick's X, are centering.
        TEST_METHOD (Classify_ZIsNonCenteringOnlyOnAJoystick)
        {
            constexpr Word  kGeneric = AxisRoleRules::kUsagePageGeneric;
            constexpr Word  kZ       = AxisRoleRules::kUsageZ;
            constexpr Word  kX       = 0x30;     // HID Generic Desktop X

            Assert::IsTrue (AxisRoleRules::Classify (kGeneric, kZ, false, kAxisZ, ControllerFormFactor::Joystick) == AxisRole::JoystickZ, L"a joystick's Z");
            Assert::IsTrue (AxisRoleRules::Classify (0,        0,  false, kAxisZ, ControllerFormFactor::Joystick) == AxisRole::JoystickZ, L"and its Z slot with no usage reported");
            Assert::IsTrue (AxisRoleRules::Classify (kGeneric, kZ, false, kAxisZ, ControllerFormFactor::Gamepad)  == AxisRole::Centering, L"not a gamepad's Z");
            Assert::IsTrue (AxisRoleRules::Classify (kGeneric, kZ, false, kAxisZ, ControllerFormFactor::Wheel)    == AxisRole::Centering, L"nor a wheel's");
            Assert::IsTrue (AxisRoleRules::Classify (kGeneric, kX, false, kAxisX, ControllerFormFactor::Joystick) == AxisRole::Centering, L"nor a joystick's X");
        }


        //  The Gladiator's throttle, its Z axis, stays where it is left; its
        //  stick does not.
        TEST_METHOD (Gladiator_ThrottleIsItsOnlyNonCenteringAxis)
        {
            std::vector<ControlId>                     controls = MakeGladiatorControls();
            std::bitset<ControllerSample::kAxisCount>  freeAxes = AxisRoleRules::GetNonCenteringAxes (controls);



            Assert::IsTrue   (AxisRoleRules::IsNonCentering (AxisRoleRules::GetRole (controls, { ControlKind::Axis, kAxisZ })), L"Z, looked up without its role");
            Assert::IsFalse  (AxisRoleRules::IsNonCentering (AxisRoleRules::GetRole (controls, { ControlKind::Axis, 0 })),      L"not X");
            Assert::AreEqual ((size_t) 1, freeAxes.count(), L"only Z");
            Assert::IsTrue   (freeAxes.test (kAxisZ));
            Assert::IsTrue   (AxisRoleRules::FindNonCenteringAxis (controls) == ControlId { ControlKind::Axis, kAxisZ }, L"found first");
        }


        //  A control is the same control whatever role a copy of it carries,
        //  so a binding read from saved prefs still finds its axis.
        TEST_METHOD (ControlId_RoleIsNotPartOfItsIdentity)
        {
            ControlId  saved   = { ControlKind::Axis, kAxisZ };
            ControlId  listed  = { ControlKind::Axis, kAxisZ, AxisRole::JoystickZ };
            ControlId  another = { ControlKind::Axis, kAxisRz, AxisRole::JoystickZ };

            Assert::IsTrue  (saved == listed);
            Assert::IsFalse (listed == another);
        }


        //  The decoder's list of a device's controls carries each axis's role.
        TEST_METHOD (ListControls_CarriesEachAxissRole)
        {
            DirectInputObjectLayout  layout;
            std::vector<ControlId>   controls;



            layout.presentAxes.set (0);
            layout.presentAxes.set ((size_t) kAxisZ);
            layout.axisRoles[(size_t) kAxisZ] = AxisRole::Throttle;

            controls = DirectInputSampleDecoder::ListControls (layout);

            Assert::AreEqual ((size_t) 2, controls.size());
            Assert::IsTrue   (controls[0].role == AxisRole::Centering, L"X keeps centering");
            Assert::IsTrue   (controls[1].role == AxisRole::Throttle,  L"Z carries its role");
        }


        //  The picker shows an axis by its HID role where the device reports
        //  one, and a joystick's Z, a throttle only by convention, by its slot.
        TEST_METHOD (Labels_GiveAnAxisItsHidRole)
        {
            constexpr ControllerKind  kDi = ControllerKind::DirectInput;

            Assert::AreEqual (std::wstring (L"Throttle"), ControlLabels::For (kDi, { ControlKind::Axis, kFirstSlider, AxisRole::Throttle }));
            Assert::AreEqual (std::wstring (L"Dial"),     ControlLabels::For (kDi, { ControlKind::Axis, kAxisRz,      AxisRole::Dial }));
            Assert::AreEqual (std::wstring (L"Slider"),   ControlLabels::For (kDi, { ControlKind::Axis, kAxisZ,       AxisRole::Slider }));
            Assert::AreEqual (std::wstring (L"Slider 1"), ControlLabels::For (kDi, { ControlKind::Axis, kFirstSlider, AxisRole::Slider }), L"a slider slot keeps its number");
            Assert::AreEqual (std::wstring (L"Z axis"),   ControlLabels::For (kDi, { ControlKind::Axis, kAxisZ,       AxisRole::JoystickZ }));
        }


        //  A joystick with a throttle plays the Paddles starting point's PDL0
        //  on the stick's X at Paddle speed and PDL1 on the throttle at
        //  Position, with PB0 alone; a gamepad with a slider, and a joystick
        //  without one, keep the stick's X at Paddle speed and leave PDL1 off.
        TEST_METHOD (Paddles_BindPdl1ToANonCenteringAxisAtPosition)
        {
            ControllerModelKey      model  = { ControllerKind::DirectInput, 0x231d, 0x0121 };
            std::vector<ControlId>  plain  = { { ControlKind::Axis, 0 }, { ControlKind::Axis, 1 }, { ControlKind::Button, 0 } };
            std::vector<ControlId>  slider = plain;
            ControlMapping          mapping;



            slider.push_back ({ ControlKind::Axis, kFirstSlider, AxisRole::Slider });

            mapping = DefaultMapping::MakePaddles (model, ControllerFormFactor::Joystick, MakeGladiatorControls());
            Assert::AreEqual ((size_t) 1, mapping.pdl0.size());
            Assert::IsTrue   (mapping.pdl0[0].analog == ControlId { ControlKind::Axis, 0 },       L"the Gladiator's stick X");
            Assert::IsTrue   (mapping.pdl0[0].response == AxisResponse::Rate,                     L"at Paddle speed");
            Assert::AreEqual ((size_t) 1, mapping.pdl1.size());
            Assert::IsTrue   (mapping.pdl1[0].analog == ControlId { ControlKind::Axis, kAxisZ }, L"the Gladiator's throttle");
            Assert::IsTrue   (mapping.pdl1[0].response == AxisResponse::Absolute,                 L"at Position");
            Assert::AreEqual ((size_t) 1, mapping.pb0.size(),                                     L"PB0 on the first button");
            Assert::IsTrue   (mapping.pb1.empty(),                                                L"PB1 unassigned");

            mapping = DefaultMapping::MakePaddles (model, ControllerFormFactor::Gamepad, slider);
            Assert::IsTrue   (mapping.pdl0[0].analog == ControlId { ControlKind::Axis, 0 },       L"a gamepad keeps its stick");
            Assert::IsTrue   (mapping.pdl0[0].response == AxisResponse::Rate,                     L"at Paddle speed");

            mapping = DefaultMapping::MakePaddles (model, ControllerFormFactor::Joystick, plain);
            Assert::IsTrue   (mapping.pdl0[0].analog == ControlId { ControlKind::Axis, 0 },       L"a joystick with nothing that stays put keeps X");
            Assert::IsTrue   (mapping.pdl0[0].response == AxisResponse::Rate,                     L"at Paddle speed");
            Assert::IsTrue   (mapping.pdl1.empty(),                                               L"no knob, no PDL1");
        }
    };
}
