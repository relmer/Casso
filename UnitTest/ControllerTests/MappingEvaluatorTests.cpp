#include "Pch.h"

#include "Controllers/MappingEvaluator.h"

#include "Controllers/DirectInputSampleDecoder.h"
#include "Controllers/XInputSampleDecoder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MappingEvaluatorTests
//
//  What a controller reading becomes at the game port. Two rules carry most
//  of it: a button is pressed if ANY of its controls is held, and an axis
//  follows whichever of its controls is furthest from center, so a stick at
//  rest cannot dilute a D-pad press mapped to the same axis.
//
//  The default mapping is tested against a device that reports axes with no
//  hardware behind them, which is not hypothetical: a flight stick here
//  reported its rudder-pedal axes while the pedals were unplugged, pinned at
//  one end of their travel. A default mapping that took the first axes it
//  found would hand the guest a control jammed hard over.
//
////////////////////////////////////////////////////////////////////////////////

namespace ControllerTests
{
    TEST_CLASS (MappingEvaluatorTests)
    {
    public:

        static constexpr float  kNoDeadzone = 0.0f;
        static constexpr Byte   kCenter     = 127;


        static ControllerSample MakeSample()
        {
            ControllerSample  sample;

            sample.connected = true;
            return sample;
        }


        // A flight stick: primary axes, a hat, buttons, and two axes whose
        // hardware is not attached and so sit pinned at one end.
        static std::vector<ControlId> MakeFlightStickControls()
        {
            return
            {
                { ControlKind::Axis, 0 }, { ControlKind::Axis, 1 }, { ControlKind::Axis, 3 },
                { ControlKind::Axis, 4 }, { ControlKind::Axis, 5 },
                { ControlKind::Button, 0 }, { ControlKind::Button, 1 }, { ControlKind::Button, 2 },
                { ControlKind::DpadUp, 0 }, { ControlKind::DpadDown, 0 },
                { ControlKind::DpadLeft, 0 }, { ControlKind::DpadRight, 0 },
            };
        }


        TEST_METHOD (Default_UsesThePrimaryAxesNotTheFirstOnesReported)
        {
            ControllerModelKey    model    = { ControllerKind::DirectInput, 0x231d, 0x0121 };
            ControlMapping        mapping  = DefaultMapping::For (model, MakeFlightStickControls());
            ControllerSample      sample   = MakeSample();
            MappingEvaluator      evaluator;
            GamePortContribution  result;

            // The unplugged pedal axes read pinned at an end; the stick is centered.
            sample.axes[4] = -1.0f;
            sample.axes[5] = -1.0f;

            result = evaluator.Evaluate (sample, mapping, kNoDeadzone);

            Assert::AreEqual (static_cast<size_t> (1), mapping.pdl0.size(), L"one binding on PDL0");
            Assert::IsTrue   (mapping.pdl0[0].analog == ControlId { ControlKind::Axis, 0 }, L"PDL0 is the primary X axis");
            Assert::IsTrue   (mapping.pdl1[0].analog == ControlId { ControlKind::Axis, 1 }, L"PDL1 is the primary Y axis");
            Assert::AreEqual (kCenter, result.paddle[0].value(), L"a centered stick reads center");
            Assert::AreEqual (kCenter, result.paddle[1].value(), L"and an axis with no hardware behind it must not reach the game port");
        }


        TEST_METHOD (Default_XboxLeftStickAndFirstTwoButtons)
        {
            ControllerModelKey    model     = { ControllerKind::XInput, 0, 0 };
            ControlMapping        mapping   = DefaultMapping::For (model, XInputSampleDecoder::ListControls());
            ControllerSample      sample    = MakeSample();
            MappingEvaluator      evaluator;
            GamePortContribution  result;

            sample.axes[XInputSampleDecoder::kLeftStickX] = 1.0f;
            sample.axes[XInputSampleDecoder::kLeftStickY] = -1.0f;
            sample.buttons.set (0);

            result = evaluator.Evaluate (sample, mapping, kNoDeadzone);

            Assert::AreEqual (static_cast<Byte> (255), result.paddle[0].value(), L"full right is 255");
            Assert::AreEqual (static_cast<Byte> (0),   result.paddle[1].value(), L"full up is 0");
            Assert::IsTrue   (result.buttons.test (0), L"A is PB0");
            Assert::IsFalse  (result.buttons.test (1), L"B is not held");
        }


        TEST_METHOD (Default_DeviceWithoutASecondButtonLeavesPb1Empty)
        {
            ControllerModelKey  model    = { ControllerKind::DirectInput, 1, 2 };
            ControlMapping      mapping  = DefaultMapping::For (model, { { ControlKind::Axis, 0 }, { ControlKind::Axis, 1 }, { ControlKind::Button, 0 } });

            Assert::AreEqual (static_cast<size_t> (1), mapping.pb0.size(), L"the one button it has drives PB0");
            Assert::IsTrue   (mapping.pb1.empty(),                        L"and PB1 stays unassigned rather than borrowing another control");
        }


        TEST_METHOD (Paddles_OnePlayersPaddleAndButton)
        {
            ControllerModelKey  model   = { ControllerKind::XInput, 0, 0 };
            ControlMapping      mapping = DefaultMapping::MakePaddles (model, XInputSampleDecoder::ListControls());

            Assert::AreEqual (static_cast<size_t> (1), mapping.pdl0.size());
            Assert::IsTrue   (mapping.pdl0[0].analog == ControlId { ControlKind::Axis, XInputSampleDecoder::kLeftStickX }, L"PDL0 is left stick X");
            Assert::IsTrue   (mapping.pdl0[0].response == AxisResponse::Rate, L"so a released stick leaves its paddle where it was");
            Assert::AreEqual (AxisBinding::kDefaultMaxSpeed, mapping.pdl0[0].maxSpeed, 0.0001f);
            Assert::AreEqual (static_cast<size_t> (1), mapping.pb0.size());
            Assert::IsTrue   (mapping.pb0[0].control == ControlId { ControlKind::Button, 0 }, L"A is the paddle's button");
            Assert::IsTrue   (mapping.pdl1.empty(), L"one controller is one player, so the second paddle is left for another controller");
            Assert::IsTrue   (mapping.pb1.empty());
            Assert::IsTrue   (mapping.pb2.empty());
        }


        TEST_METHOD (Paddles_BindsOnlyControlsTheDeviceReports)
        {
            ControllerModelKey  model = { ControllerKind::DirectInput, 1, 2 };
            ControlMapping      stick = DefaultMapping::MakePaddles (model, MakeFlightStickControls());
            ControlMapping      bare  = DefaultMapping::MakePaddles (model, { { ControlKind::Axis, 1 } });

            Assert::IsTrue (stick.pdl0[0].analog == ControlId { ControlKind::Axis, 0 }, L"a DirectInput device's X axis drives the paddle");
            Assert::IsTrue (stick.pdl0[0].response == AxisResponse::Rate);
            Assert::IsTrue (stick.pdl1.empty());
            Assert::IsTrue (bare.pdl0.empty(), L"a device with no X axis leaves the paddle unassigned");
            Assert::IsTrue (bare.pb0.empty(),  L"and binds no button it does not report");
        }


        TEST_METHOD (Default_ClaimsPdl0AndPdl1AndNoMore)
        {
            ControllerModelKey  model   = { ControllerKind::XInput, 0, 0 };
            ControlMapping      mapping = DefaultMapping::For (model, XInputSampleDecoder::ListControls());

            // A controller with a second stick must not take the axes a second
            // player would use (FR-038).
            Assert::IsFalse (mapping.pdl0.empty());
            Assert::IsFalse (mapping.pdl1.empty());
            Assert::IsTrue  (mapping.pdl2.empty(), L"the right stick is not put on PDL2 by default");
            Assert::IsTrue  (mapping.pdl3.empty(), L"nor on PDL3");
        }


        TEST_METHOD (FourAxes_BothSticksDriveAllFour)
        {
            ControllerSample      sample = MakeSample();
            ControlMapping        mapping;
            MappingEvaluator      evaluator;
            GamePortContribution  result;

            mapping.pdl0.push_back ({ AxisBindingKind::Analog, { ControlKind::Axis, XInputSampleDecoder::kLeftStickX } });
            mapping.pdl1.push_back ({ AxisBindingKind::Analog, { ControlKind::Axis, XInputSampleDecoder::kLeftStickY } });
            mapping.pdl2.push_back ({ AxisBindingKind::Analog, { ControlKind::Axis, XInputSampleDecoder::kRightStickX } });
            mapping.pdl3.push_back ({ AxisBindingKind::Analog, { ControlKind::Axis, XInputSampleDecoder::kRightStickY } });

            sample.axes[XInputSampleDecoder::kLeftStickX]  =  1.0f;
            sample.axes[XInputSampleDecoder::kLeftStickY]  = -1.0f;
            sample.axes[XInputSampleDecoder::kRightStickX] = -1.0f;
            sample.axes[XInputSampleDecoder::kRightStickY] =  1.0f;

            result = evaluator.Evaluate (sample, mapping, kNoDeadzone);

            Assert::AreEqual (static_cast<Byte> (255), result.paddle[0].value(), L"left stick X is PDL0");
            Assert::AreEqual (static_cast<Byte> (0),   result.paddle[1].value(), L"left stick Y is PDL1");
            Assert::AreEqual (static_cast<Byte> (0),   result.paddle[2].value(), L"right stick X is PDL2");
            Assert::AreEqual (static_cast<Byte> (255), result.paddle[3].value(), L"right stick Y is PDL3");
        }


        TEST_METHOD (TwoAxisMachine_BindingsOnPdl2AndPdl3AreIgnored)
        {
            ControllerSample      sample = MakeSample();
            ControlMapping        mapping;
            AxisBinding           rate;
            MappingEvaluator      evaluator;
            GamePortContribution  result;

            rate.analog   = { ControlKind::Axis, XInputSampleDecoder::kRightStickX };
            rate.response = AxisResponse::Rate;

            mapping.pdl0.push_back ({ AxisBindingKind::Analog, { ControlKind::Axis, XInputSampleDecoder::kLeftStickX } });
            mapping.pdl2.push_back (rate);
            mapping.pdl3.push_back ({ AxisBindingKind::Analog, { ControlKind::Axis, XInputSampleDecoder::kRightStickY } });

            sample.axes[XInputSampleDecoder::kLeftStickX]  = 1.0f;
            sample.axes[XInputSampleDecoder::kRightStickX] = 1.0f;
            sample.axes[XInputSampleDecoder::kRightStickY] = 1.0f;

            result = evaluator.Evaluate (sample, mapping, kNoDeadzone, MappingEvaluator::kMaxRateStep, 2);

            Assert::AreEqual (static_cast<Byte> (255), result.paddle[0].value(), L"PDL0 still plays");
            Assert::IsTrue   (result.paddle[1].has_value(),  L"and PDL1 rests at center, since the machine has it");
            Assert::IsFalse  (result.paddle[2].has_value(),  L"PDL2 is left absent on a machine with two axes");
            Assert::IsFalse  (result.paddle[3].has_value(),  L"and so is PDL3");
            Assert::IsFalse  (evaluator.IsRateMoving(),      L"a rate binding on an axis the machine lacks does not keep the thread polling");

            result = evaluator.Evaluate (sample, mapping, kNoDeadzone, MappingEvaluator::kMaxRateStep);

            Assert::IsTrue (result.paddle[2].value() > kCenter,
                L"the bindings were kept, not discarded: four axes play them again, from center");
        }


        TEST_METHOD (Empty_TargetsRestAtCenterAndReleased)
        {
            ControllerSample      sample = MakeSample();
            MappingEvaluator      evaluator;
            GamePortContribution  result = evaluator.Evaluate (sample, ControlMapping(), kNoDeadzone);

            Assert::AreEqual (kCenter, result.paddle[0].value(), L"no binding is center");
            Assert::AreEqual (kCenter, result.paddle[1].value(), L"on both axes");
            Assert::IsTrue   (result.buttons.none(),             L"and no button is pressed");
        }


        TEST_METHOD (Buttons_PressedIfAnyControlIsHeld)
        {
            ControlMapping        mapping;
            ControllerSample      sample = MakeSample();
            MappingEvaluator      evaluator;
            GamePortContribution  result;

            mapping.pb0.push_back ({ { ControlKind::Button, 0 } });
            mapping.pb0.push_back ({ { ControlKind::Button, 3 } });

            sample.buttons.set (3);
            result = evaluator.Evaluate (sample, mapping, kNoDeadzone);

            Assert::IsTrue (result.buttons.test (0), L"the second control holds PB0 on its own");
        }


        TEST_METHOD (Buttons_HatDirectionDrivesAButton)
        {
            ControlMapping        mapping;
            ControllerSample      sample = MakeSample();
            MappingEvaluator      evaluator;
            GamePortContribution  result;

            mapping.pb1.push_back ({ { ControlKind::DpadLeft, 0 } });
            sample.hats[0] = ControllerSample::kHatLeft | ControllerSample::kHatUp;

            result = evaluator.Evaluate (sample, mapping, kNoDeadzone);

            Assert::IsTrue (result.buttons.test (1), L"a diagonal hat still holds its left direction");
        }


        TEST_METHOD (Buttons_TriggerPressesPastItsThreshold)
        {
            ControlMapping        mapping;
            ControllerSample      sample = MakeSample();
            MappingEvaluator      evaluator;
            GamePortContribution  result;

            mapping.pb0.push_back ({ { ControlKind::Trigger, 1 }, ButtonBinding::kTriggerThreshold });

            sample.triggers[1] = 0.05f;
            result = evaluator.Evaluate (sample, mapping, kNoDeadzone);
            Assert::IsFalse (result.buttons.test (0), L"a barely touched trigger is not a press");

            sample.triggers[1] = 0.5f;
            result = evaluator.Evaluate (sample, mapping, kNoDeadzone);
            Assert::IsTrue (result.buttons.test (0), L"a pulled trigger is");
        }


        TEST_METHOD (Buttons_AxisDirectionDecidesThePress)
        {
            ControlMapping        mapping;
            ControllerSample      sample = MakeSample();
            MappingEvaluator      evaluator;
            GamePortContribution  result;

            mapping.pb0.push_back ({ { ControlKind::Axis, 2 }, 0.5f, true });

            sample.axes[2] = 0.9f;
            result = evaluator.Evaluate (sample, mapping, kNoDeadzone);
            Assert::IsFalse (result.buttons.test (0), L"pushed the other way is not a press");

            sample.axes[2] = -0.9f;
            result = evaluator.Evaluate (sample, mapping, kNoDeadzone);
            Assert::IsTrue (result.buttons.test (0), L"pushed the bound way is");
        }


        TEST_METHOD (Axis_FurthestFromCenterWins)
        {
            ControlMapping        mapping;
            ControllerSample      sample = MakeSample();
            MappingEvaluator      evaluator;
            GamePortContribution  result;
            AxisBinding           stick;
            AxisBinding           dpad;

            stick.analog   = { ControlKind::Axis, 0 };
            dpad.kind      = AxisBindingKind::DigitalPair;
            dpad.negative  = { ControlKind::DpadLeft, 0 };
            dpad.positive  = { ControlKind::DpadRight, 0 };

            mapping.pdl0.push_back (stick);
            mapping.pdl0.push_back (dpad);

            sample.axes[0] = 0.1f;                            // stick nearly at rest
            sample.hats[0] = ControllerSample::kHatLeft;      // D-pad pushed

            result = evaluator.Evaluate (sample, mapping, kNoDeadzone);

            Assert::AreEqual (static_cast<Byte> (0), result.paddle[0].value(),
                L"the D-pad wins: a stick resting near center must not dilute it");
        }


        TEST_METHOD (Axis_DigitalPairDrivesBothEndsAndBothHeldIsCenter)
        {
            ControlMapping        mapping;
            ControllerSample      sample = MakeSample();
            MappingEvaluator      evaluator;
            GamePortContribution  result;
            AxisBinding           dpad;

            dpad.kind     = AxisBindingKind::DigitalPair;
            dpad.negative = { ControlKind::DpadUp, 0 };
            dpad.positive = { ControlKind::DpadDown, 0 };
            mapping.pdl1.push_back (dpad);

            sample.hats[0] = ControllerSample::kHatDown;
            result         = evaluator.Evaluate (sample, mapping, kNoDeadzone);
            Assert::AreEqual (static_cast<Byte> (255), result.paddle[1].value(), L"down drives the far end");

            sample.hats[0] = ControllerSample::kHatUp;
            result         = evaluator.Evaluate (sample, mapping, kNoDeadzone);
            Assert::AreEqual (static_cast<Byte> (0), result.paddle[1].value(), L"up drives the near end");

            sample.hats[0] = ControllerSample::kHatUp | ControllerSample::kHatDown;
            result         = evaluator.Evaluate (sample, mapping, kNoDeadzone);
            Assert::AreEqual (kCenter, result.paddle[1].value(), L"both at once is no direction, as with opposing arrow keys");
        }


        TEST_METHOD (Axis_InvertedFlipsTheEnds)
        {
            ControlMapping        mapping;
            ControllerSample      sample = MakeSample();
            MappingEvaluator      evaluator;
            GamePortContribution  result;
            AxisBinding           binding;

            binding.analog   = { ControlKind::Axis, 1 };
            binding.inverted = true;
            mapping.pdl1.push_back (binding);

            sample.axes[1] = -1.0f;
            result         = evaluator.Evaluate (sample, mapping, kNoDeadzone);

            Assert::AreEqual (static_cast<Byte> (255), result.paddle[1].value(), L"inverted turns full up into full down");
        }


        static ControlMapping MakeRateMapping (float maxSpeed)
        {
            ControlMapping  mapping;
            AxisBinding     binding;

            binding.analog   = { ControlKind::Axis, 0 };
            binding.response = AxisResponse::Rate;
            binding.maxSpeed = maxSpeed;
            mapping.pdl0.push_back (binding);
            return mapping;
        }


        TEST_METHOD (Rate_MovesByDeflectionTimesSpeedTimesTime)
        {
            MappingEvaluator  evaluator;
            ControlMapping    mapping = MakeRateMapping (100.0f);
            ControllerSample  sample  = MakeSample();

            sample.axes[0] = 0.5f;

            // 0.5 deflection at 100 units a second for 0.04 s is 2 units a step.
            Assert::AreEqual ((int) 129, (int) evaluator.Evaluate (sample, mapping, kNoDeadzone, 0.04f).paddle[0].value());
            Assert::AreEqual ((int) 131, (int) evaluator.Evaluate (sample, mapping, kNoDeadzone, 0.04f).paddle[0].value());
            Assert::IsTrue   (evaluator.IsRateMoving(), L"a deflected rate binding is moving");
        }


        TEST_METHOD (Rate_HoldsItsPositionWhenTheStickIsLetGo)
        {
            MappingEvaluator  evaluator;
            ControlMapping    mapping = MakeRateMapping (1000.0f);
            ControllerSample  pushed  = MakeSample();
            Byte              moved   = 0;

            pushed.axes[0] = 1.0f;
            moved = evaluator.Evaluate (pushed, mapping, kNoDeadzone, 0.05f).paddle[0].value();

            Assert::AreEqual ((int) moved, (int) evaluator.Evaluate (MakeSample(), mapping, kNoDeadzone, 0.05f).paddle[0].value(),
                L"released, the paddle stays where it was turned rather than springing back to center");
            Assert::IsFalse  (evaluator.IsRateMoving());
        }


        TEST_METHOD (Rate_StopsAtBothEnds)
        {
            MappingEvaluator  evaluator;
            ControlMapping    mapping = MakeRateMapping (1024.0f);
            ControllerSample  sample  = MakeSample();
            int               i       = 0;

            sample.axes[0] = 1.0f;

            for (i = 0; i < 20; i++)
            {
                evaluator.Evaluate (sample, mapping, kNoDeadzone, 0.05f);
            }

            Assert::AreEqual ((int) 255, (int) evaluator.Evaluate (sample, mapping, kNoDeadzone, 0.05f).paddle[0].value());

            sample.axes[0] = -1.0f;

            for (i = 0; i < 20; i++)
            {
                evaluator.Evaluate (sample, mapping, kNoDeadzone, 0.05f);
            }

            Assert::AreEqual ((int) 0, (int) evaluator.Evaluate (sample, mapping, kNoDeadzone, 0.05f).paddle[0].value());
        }


        TEST_METHOD (Rate_ALongGapMovesNoFurtherThanOneStep)
        {
            MappingEvaluator  capped;
            MappingEvaluator  stepped;
            ControlMapping    mapping = MakeRateMapping (256.0f);
            ControllerSample  sample  = MakeSample();

            sample.axes[0] = 1.0f;

            Assert::AreEqual ((int) stepped.Evaluate (sample, mapping, kNoDeadzone, MappingEvaluator::kMaxRateStep).paddle[0].value(),
                              (int) capped.Evaluate  (sample, mapping, kNoDeadzone, 10.0f).paddle[0].value(),
                L"a reading after ten seconds idle does not throw the paddle across the screen");
        }


        TEST_METHOD (Rate_ResetReturnsToCenter)
        {
            MappingEvaluator  evaluator;
            ControlMapping    mapping = MakeRateMapping (1000.0f);
            ControllerSample  pushed  = MakeSample();

            pushed.axes[0] = 1.0f;
            evaluator.Evaluate (pushed, mapping, kNoDeadzone, 0.05f);
            evaluator.ResetRate();

            Assert::AreEqual ((int) kCenter, (int) evaluator.Evaluate (MakeSample(), mapping, kNoDeadzone, 0.05f).paddle[0].value());
        }


        TEST_METHOD (Pb2_BindingsDrivePb2)
        {
            MappingEvaluator  evaluator;
            ControlMapping    mapping;
            ControllerSample  sample = MakeSample();

            mapping.pb2.push_back (ButtonBinding { { ControlKind::Button, 4 } });
            sample.buttons.set (4);

            Assert::IsTrue  (evaluator.Evaluate (sample, mapping, kNoDeadzone).buttons.test (2), L"PB2 reads pressed");
            Assert::IsFalse (evaluator.Evaluate (sample, mapping, kNoDeadzone).buttons.test (0), L"and PB0 does not");
        }


        //  A stick on PDL0/PDL1 and a button on PB0, the shape a joystick
        //  profile has, for the Joyport switch tests below.
        static ControlMapping MakeStickMapping (AxisResponse response = AxisResponse::Absolute)
        {
            ControlMapping  mapping;
            AxisBinding     x;
            AxisBinding     y;

            x.analog   = { ControlKind::Axis, 0 };
            x.response = response;
            y.analog   = { ControlKind::Axis, 1 };
            y.response = response;
            mapping.pdl0.push_back (x);
            mapping.pdl1.push_back (y);
            mapping.pb0.push_back (ButtonBinding { { ControlKind::Button, 0 } });
            return mapping;
        }


        static bool IsClosed (const GamePortContribution & result, JoystickSwitch sw)
        {
            return result.switches.test (static_cast<size_t> (sw));
        }


        TEST_METHOD (Switches_EachDirectionClosesOnlyPastTheThreshold)
        {
            struct Case { int axis; float sign; JoystickSwitch sw; const wchar_t * name; };

            const Case  cases[] =
            {
                { 0, -1.0f, JoystickSwitch::Left,  L"left"  },
                { 0,  1.0f, JoystickSwitch::Right, L"right" },
                { 1, -1.0f, JoystickSwitch::Up,    L"up"    },
                { 1,  1.0f, JoystickSwitch::Down,  L"down"  },
            };

            MappingEvaluator  evaluator;
            ControlMapping    mapping = MakeStickMapping();

            for (const Case & c : cases)
            {
                ControllerSample  sample = MakeSample();

                sample.axes[c.axis] = c.sign * (MappingEvaluator::kSwitchThreshold - 0.01f);
                Assert::IsTrue (evaluator.Evaluate (sample, mapping, kNoDeadzone).switches.none(),
                    std::format (L"{} short of the threshold closes nothing", c.name).c_str());

                sample.axes[c.axis] = c.sign * (MappingEvaluator::kSwitchThreshold + 0.01f);
                GamePortContribution  result = evaluator.Evaluate (sample, mapping, kNoDeadzone);

                Assert::IsTrue (IsClosed (result, c.sw), std::format (L"{} past the threshold closes", c.name).c_str());
                Assert::AreEqual (static_cast<size_t> (1), result.switches.count(),
                    std::format (L"{} closes only itself", c.name).c_str());
            }
        }


        TEST_METHOD (Switches_TheThresholdIsMeasuredAfterTheDeadzone)
        {
            constexpr float   kDeadzone = 0.5f;
            MappingEvaluator  evaluator;
            ControlMapping    mapping   = MakeStickMapping();
            ControllerSample  sample    = MakeSample();

            //  0.7 raw is 0.4 of the travel beyond a 0.5 deadzone: short of it.
            sample.axes[0] = 0.7f;
            Assert::IsFalse (IsClosed (evaluator.Evaluate (sample, mapping, kDeadzone), JoystickSwitch::Right));

            //  0.8 raw is 0.6 of the travel beyond it: past.
            sample.axes[0] = 0.8f;
            Assert::IsTrue (IsClosed (evaluator.Evaluate (sample, mapping, kDeadzone), JoystickSwitch::Right));
        }


        TEST_METHOD (Switches_ADiagonalClosesOneOfEachPair)
        {
            MappingEvaluator      evaluator;
            ControlMapping        mapping = MakeStickMapping();
            ControllerSample      sample  = MakeSample();
            GamePortContribution  result;

            sample.axes[0] = -0.7f;
            sample.axes[1] = -0.7f;
            result = evaluator.Evaluate (sample, mapping, kNoDeadzone);

            Assert::IsTrue (IsClosed (result, JoystickSwitch::Left), L"up-left closes left");
            Assert::IsTrue (IsClosed (result, JoystickSwitch::Up),   L"and up");
            Assert::AreEqual (static_cast<size_t> (2), result.switches.count(), L"and nothing else");
        }


        TEST_METHOD (Switches_ADigitalPairClosesAtOnceAndBothHeldClosesNeither)
        {
            ControlMapping        mapping;
            ControllerSample      sample = MakeSample();
            MappingEvaluator      evaluator;
            GamePortContribution  result;
            AxisBinding           dpad;

            dpad.kind     = AxisBindingKind::DigitalPair;
            dpad.negative = { ControlKind::DpadLeft, 0 };
            dpad.positive = { ControlKind::DpadRight, 0 };
            mapping.pdl0.push_back (dpad);

            sample.hats[0] = ControllerSample::kHatLeft;
            result = evaluator.Evaluate (sample, mapping, 0.9f);
            Assert::IsTrue (IsClosed (result, JoystickSwitch::Left), L"a D-pad press needs no threshold, even under a large deadzone");

            sample.hats[0] = ControllerSample::kHatLeft | ControllerSample::kHatRight;
            result = evaluator.Evaluate (sample, mapping, kNoDeadzone);
            Assert::IsTrue (result.switches.none(), L"a stick cannot close both, so both held closes neither");
        }


        TEST_METHOD (Switches_ARateBindingOpensWhenTheStickIsReleased)
        {
            MappingEvaluator  evaluator;
            ControlMapping    mapping = MakeStickMapping (AxisResponse::Rate);
            ControllerSample  pushed  = MakeSample();
            Byte              held    = 0;

            pushed.axes[0] = 1.0f;
            Assert::IsTrue (IsClosed (evaluator.Evaluate (pushed, mapping, kNoDeadzone, 0.05f), JoystickSwitch::Right),
                L"deflected closes");

            for (int i = 0; i < 20; i++)
            {
                evaluator.Evaluate (pushed, mapping, kNoDeadzone, 0.05f);
            }

            GamePortContribution  released = evaluator.Evaluate (MakeSample(), mapping, kNoDeadzone, 0.05f);

            held = released.paddle[0].value();
            Assert::AreEqual ((int) 255, (int) held, L"the rate paddle stays at the end it was turned to");
            Assert::IsTrue (released.switches.none(), L"but the switch follows the stick, not the paddle, and opens");
        }


        TEST_METHOD (Switches_InvertedSwapsTheDirection)
        {
            MappingEvaluator  evaluator;
            ControlMapping    mapping = MakeStickMapping();
            ControllerSample  sample  = MakeSample();

            mapping.pdl1[0].inverted = true;
            sample.axes[1] = -1.0f;

            Assert::IsTrue (IsClosed (evaluator.Evaluate (sample, mapping, kNoDeadzone), JoystickSwitch::Down),
                L"inverted full up is down");
        }


        TEST_METHOD (Switches_FireIsPb0AndPb1Pb2DriveNothing)
        {
            MappingEvaluator  evaluator;
            ControlMapping    mapping = MakeStickMapping();
            ControllerSample  sample  = MakeSample();

            mapping.pb1.push_back (ButtonBinding { { ControlKind::Button, 1 } });
            mapping.pb2.push_back (ButtonBinding { { ControlKind::Button, 2 } });

            sample.buttons.set (1);
            sample.buttons.set (2);
            Assert::IsTrue (evaluator.Evaluate (sample, mapping, kNoDeadzone).switches.none(),
                L"PB1 and PB2 have no Joyport switch to drive");

            sample.buttons.set (0);
            Assert::IsTrue (IsClosed (evaluator.Evaluate (sample, mapping, kNoDeadzone), JoystickSwitch::Fire), L"PB0 is fire");
        }
    };
}
