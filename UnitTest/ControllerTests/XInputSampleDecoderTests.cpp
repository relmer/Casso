#include "Pch.h"

#include "Controllers/XInputSampleDecoder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  XInputSampleDecoderTests
//
//  Every Xbox-class controller shares one layout, so the bit table is the
//  contract a saved mapping depends on: "button 0" must mean A on every Xbox
//  controller on every launch. Thumb Y is flipped so up reads negative, and
//  -32768 is the value that would overflow a naive integer negation.
//
////////////////////////////////////////////////////////////////////////////////

namespace ControllerTests
{
    TEST_CLASS (XInputSampleDecoderTests)
    {
    public:

        static constexpr float  kTolerance = 0.0001f;


        static bool IsOnlyButtonPressed (const ControllerSample & sample, int index)
        {
            return sample.buttons.count() == 1 && sample.buttons.test (static_cast<size_t> (index));
        }


        TEST_METHOD (Buttons_EachBitMapsToItsIndex)
        {
            struct Case
            {
                WORD              mask;
                int               index;
                const wchar_t   * name;
            };

            const Case  cases[] =
            {
                { 0x1000, 0, L"A"           },
                { 0x2000, 1, L"B"           },
                { 0x4000, 2, L"X"           },
                { 0x8000, 3, L"Y"           },
                { 0x0100, 4, L"LB"          },
                { 0x0200, 5, L"RB"          },
                { 0x0020, 6, L"Back"        },
                { 0x0010, 7, L"Start"       },
                { 0x0040, 8, L"left stick"  },
                { 0x0080, 9, L"right stick" },
            };

            for (const Case & testCase : cases)
            {
                XInputGamepadState  state;
                ControllerSample    sample;

                state.buttons = testCase.mask;
                sample        = XInputSampleDecoder::Decode (state);

                Assert::IsTrue (IsOnlyButtonPressed (sample, testCase.index), testCase.name);
                Assert::AreEqual (static_cast<Byte> (0), sample.hats[0], testCase.name);
            }
        }


        TEST_METHOD (Dpad_CardinalsAndDiagonals)
        {
            XInputGamepadState  state;
            ControllerSample    sample;

            state.buttons = 0x0001 | 0x0008;
            sample        = XInputSampleDecoder::Decode (state);

            Assert::AreEqual (static_cast<Byte> (ControllerSample::kHatUp | ControllerSample::kHatRight), sample.hats[0], L"up and right together");
            Assert::IsTrue   (sample.buttons.none(), L"the D-pad is not a button");

            state.buttons = 0x0002 | 0x0004;
            sample        = XInputSampleDecoder::Decode (state);

            Assert::AreEqual (static_cast<Byte> (ControllerSample::kHatDown | ControllerSample::kHatLeft), sample.hats[0], L"down and left together");
        }


        TEST_METHOD (Thumbs_ExtremesAndYOrientation)
        {
            XInputGamepadState  state;
            ControllerSample    sample;

            state.thumbLX = -32768;
            state.thumbLY = 32767;
            state.thumbRX = 32767;
            state.thumbRY = -32768;

            sample = XInputSampleDecoder::Decode (state);

            Assert::AreEqual (-1.0f, sample.axes[XInputSampleDecoder::kLeftStickX],  kTolerance, L"left stick full left");
            Assert::AreEqual (-1.0f, sample.axes[XInputSampleDecoder::kLeftStickY],  kTolerance, L"left stick full up reads negative");
            Assert::AreEqual ( 1.0f, sample.axes[XInputSampleDecoder::kRightStickX], kTolerance, L"right stick full right");
            Assert::AreEqual ( 1.0f, sample.axes[XInputSampleDecoder::kRightStickY], kTolerance, L"right stick full down reads positive, without overflow at -32768");
        }


        TEST_METHOD (Triggers_ScaleToUnitRange)
        {
            XInputGamepadState  state;
            ControllerSample    sample;

            state.leftTrigger  = 255;
            state.rightTrigger = 0;

            sample = XInputSampleDecoder::Decode (state);

            Assert::AreEqual (1.0f, sample.triggers[0], kTolerance, L"a full left trigger reads 1");
            Assert::AreEqual (0.0f, sample.triggers[1], 0.0f,       L"a released right trigger reads 0");
        }


        TEST_METHOD (ListControls_HasTheFixedXboxLayout)
        {
            std::vector<ControlId>  controls = XInputSampleDecoder::ListControls();

            Assert::AreEqual (static_cast<size_t> (4 + 2 + 10 + 4), controls.size(), L"four stick axes, two triggers, ten buttons, four D-pad directions");
            Assert::IsTrue   (std::find (controls.begin(), controls.end(), ControlId { ControlKind::Trigger, 1 }) != controls.end(), L"RT is listed");
            Assert::IsTrue   (std::find (controls.begin(), controls.end(), ControlId { ControlKind::Axis, 2 })    == controls.end(), L"there is no Z axis on an Xbox controller");
        }
    };
}
