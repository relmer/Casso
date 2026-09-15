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
            Assert::AreEqual (kCenter, result.paddle.value()[0], L"a centered stick reads center");
            Assert::AreEqual (kCenter, result.paddle.value()[1], L"and an axis with no hardware behind it must not reach the game port");
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

            Assert::AreEqual (static_cast<Byte> (255), result.paddle.value()[0], L"full right is 255");
            Assert::AreEqual (static_cast<Byte> (0),   result.paddle.value()[1], L"full up is 0");
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


        TEST_METHOD (Paddles_XboxOneStickPerPlayerWithRateResponse)
        {
            ControllerModelKey  model   = { ControllerKind::XInput, 0, 0 };
            ControlMapping      mapping = DefaultMapping::MakePaddles (model, XInputSampleDecoder::ListControls());

            Assert::AreEqual (static_cast<size_t> (1), mapping.pdl0.size());
            Assert::AreEqual (static_cast<size_t> (1), mapping.pdl1.size());
            Assert::IsTrue   (mapping.pdl0[0].analog == ControlId { ControlKind::Axis, XInputSampleDecoder::kLeftStickX },  L"PDL0 is left stick X");
            Assert::IsTrue   (mapping.pdl1[0].analog == ControlId { ControlKind::Axis, XInputSampleDecoder::kRightStickX }, L"PDL1 is right stick X");
            Assert::IsTrue   (mapping.pdl0[0].response == AxisResponse::Rate, L"so a released stick leaves its paddle where it was");
            Assert::IsTrue   (mapping.pdl1[0].response == AxisResponse::Rate);
            Assert::AreEqual (AxisBinding::kDefaultMaxSpeed, mapping.pdl0[0].maxSpeed, 0.0001f);
            Assert::IsTrue   (mapping.pb0 == DefaultMapping::For (model, XInputSampleDecoder::ListControls()).pb0, L"the buttons are the default mapping's");
            Assert::IsTrue   (mapping.pb1 == DefaultMapping::For (model, XInputSampleDecoder::ListControls()).pb1);
            Assert::IsTrue   (mapping.pb2.empty());
        }


        TEST_METHOD (Paddles_DirectInputUsesRxWhenReportedAndNothingOtherwise)
        {
            ControllerModelKey  model      = { ControllerKind::DirectInput, 1, 2 };
            ControlMapping      withRx     = DefaultMapping::MakePaddles (model, MakeFlightStickControls());
            ControlMapping      withoutRx  = DefaultMapping::MakePaddles (model, { { ControlKind::Axis, 0 }, { ControlKind::Axis, 1 }, { ControlKind::Button, 0 } });

            Assert::IsTrue  (withRx.pdl1[0].analog == ControlId { ControlKind::Axis, 3 }, L"Rx drives PDL1");
            Assert::IsTrue  (withRx.pdl1[0].response == AxisResponse::Rate);
            Assert::IsTrue  (withoutRx.pdl0[0].analog == ControlId { ControlKind::Axis, 0 });
            Assert::IsTrue  (withoutRx.pdl1.empty(), L"a device without Rx leaves PDL1 unassigned");
            Assert::IsTrue  (withoutRx.pb1.empty(),  L"and binds only buttons it reports");
        }


        TEST_METHOD (Empty_TargetsRestAtCenterAndReleased)
        {
            ControllerSample      sample = MakeSample();
            MappingEvaluator      evaluator;
            GamePortContribution  result = evaluator.Evaluate (sample, ControlMapping(), kNoDeadzone);

            Assert::AreEqual (kCenter, result.paddle.value()[0], L"no binding is center");
            Assert::AreEqual (kCenter, result.paddle.value()[1], L"on both axes");
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

            Assert::AreEqual (static_cast<Byte> (0), result.paddle.value()[0],
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
            Assert::AreEqual (static_cast<Byte> (255), result.paddle.value()[1], L"down drives the far end");

            sample.hats[0] = ControllerSample::kHatUp;
            result         = evaluator.Evaluate (sample, mapping, kNoDeadzone);
            Assert::AreEqual (static_cast<Byte> (0), result.paddle.value()[1], L"up drives the near end");

            sample.hats[0] = ControllerSample::kHatUp | ControllerSample::kHatDown;
            result         = evaluator.Evaluate (sample, mapping, kNoDeadzone);
            Assert::AreEqual (kCenter, result.paddle.value()[1], L"both at once is no direction, as with opposing arrow keys");
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

            Assert::AreEqual (static_cast<Byte> (255), result.paddle.value()[1], L"inverted turns full up into full down");
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
            Assert::AreEqual ((int) 129, (int) evaluator.Evaluate (sample, mapping, kNoDeadzone, 0.04f).paddle.value()[0]);
            Assert::AreEqual ((int) 131, (int) evaluator.Evaluate (sample, mapping, kNoDeadzone, 0.04f).paddle.value()[0]);
            Assert::IsTrue   (evaluator.IsRateMoving(), L"a deflected rate binding is moving");
        }


        TEST_METHOD (Rate_HoldsItsPositionWhenTheStickIsLetGo)
        {
            MappingEvaluator  evaluator;
            ControlMapping    mapping = MakeRateMapping (1000.0f);
            ControllerSample  pushed  = MakeSample();
            Byte              moved   = 0;

            pushed.axes[0] = 1.0f;
            moved = evaluator.Evaluate (pushed, mapping, kNoDeadzone, 0.05f).paddle.value()[0];

            Assert::AreEqual ((int) moved, (int) evaluator.Evaluate (MakeSample(), mapping, kNoDeadzone, 0.05f).paddle.value()[0],
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

            Assert::AreEqual ((int) 255, (int) evaluator.Evaluate (sample, mapping, kNoDeadzone, 0.05f).paddle.value()[0]);

            sample.axes[0] = -1.0f;

            for (i = 0; i < 20; i++)
            {
                evaluator.Evaluate (sample, mapping, kNoDeadzone, 0.05f);
            }

            Assert::AreEqual ((int) 0, (int) evaluator.Evaluate (sample, mapping, kNoDeadzone, 0.05f).paddle.value()[0]);
        }


        TEST_METHOD (Rate_ALongGapMovesNoFurtherThanOneStep)
        {
            MappingEvaluator  capped;
            MappingEvaluator  stepped;
            ControlMapping    mapping = MakeRateMapping (256.0f);
            ControllerSample  sample  = MakeSample();

            sample.axes[0] = 1.0f;

            Assert::AreEqual ((int) stepped.Evaluate (sample, mapping, kNoDeadzone, MappingEvaluator::kMaxRateStep).paddle.value()[0],
                              (int) capped.Evaluate  (sample, mapping, kNoDeadzone, 10.0f).paddle.value()[0],
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

            Assert::AreEqual ((int) kCenter, (int) evaluator.Evaluate (MakeSample(), mapping, kNoDeadzone, 0.05f).paddle.value()[0]);
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
    };
}
