#include "Pch.h"

#include "Controllers/DirectInputSampleDecoder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DirectInputSampleDecoderTests
//
//  The decoder is where a DirectInput device's raw state becomes the sample
//  every later rule reads, so a wrong edge here is wrong everywhere. The hat
//  is the delicate part: centered is a low word of 0xFFFF, not zero, and zero
//  is north. A decoder that treated zero as centered would lose "up" on
//  every D-pad, and one that ignored the sector edges would turn a diagonal
//  into a single direction.
//
////////////////////////////////////////////////////////////////////////////////

namespace ControllerTests
{
    TEST_CLASS (DirectInputSampleDecoderTests)
    {
    public:

        static constexpr float  kTolerance = 0.0001f;
        static constexpr DWORD  kCentered  = 0xFFFFFFFF;


        static DirectInputObjectLayout MakeFullLayout()
        {
            DirectInputObjectLayout  layout;

            layout.presentAxes.set();
            layout.hatCount    = 1;
            layout.buttonCount = 12;
            return layout;
        }


        TEST_METHOD (Axis_ExtremesAndCenter)
        {
            Assert::AreEqual (-1.0f, DirectInputSampleDecoder::NormalizeAxis (-32768), kTolerance, L"minimum must reach -1");
            Assert::AreEqual ( 1.0f, DirectInputSampleDecoder::NormalizeAxis (32767),  kTolerance, L"maximum must reach 1");
            Assert::AreEqual ( 0.0f, DirectInputSampleDecoder::NormalizeAxis (0),      0.0f,       L"zero must be exactly center");
        }


        TEST_METHOD (Axis_AbsentSlotsStayAtRest)
        {
            DirectInputJoystickState  state;
            DirectInputObjectLayout   layout;
            ControllerSample          sample;

            state.x  = 32767;
            state.z  = -32768;
            layout.presentAxes.set (0);

            sample = DirectInputSampleDecoder::Decode (state, layout);

            Assert::AreEqual (1.0f, sample.axes[0], kTolerance, L"a present axis must be read");
            Assert::AreEqual (0.0f, sample.axes[2], 0.0f,       L"an axis the device does not report must stay at rest");
            Assert::IsTrue   (sample.connected,                  L"a decoded sample is connected");
        }


        TEST_METHOD (Pov_CenteredSetsNoDirection)
        {
            Assert::AreEqual (static_cast<Byte> (0), DirectInputSampleDecoder::DecodePov (kCentered), L"0xFFFFFFFF is centered");
            Assert::AreEqual (static_cast<Byte> (0), DirectInputSampleDecoder::DecodePov (0x0000FFFF), L"only the low word decides centered");
        }


        TEST_METHOD (Pov_CardinalsSetOneDirection)
        {
            Assert::AreEqual (ControllerSample::kHatUp,    DirectInputSampleDecoder::DecodePov (0),     L"0 is north, not centered");
            Assert::AreEqual (ControllerSample::kHatRight, DirectInputSampleDecoder::DecodePov (9000),  L"9000 is east");
            Assert::AreEqual (ControllerSample::kHatDown,  DirectInputSampleDecoder::DecodePov (18000), L"18000 is south");
            Assert::AreEqual (ControllerSample::kHatLeft,  DirectInputSampleDecoder::DecodePov (27000), L"27000 is west");
        }


        TEST_METHOD (Pov_DiagonalsSetTwoDirections)
        {
            Assert::AreEqual (static_cast<Byte> (ControllerSample::kHatUp   | ControllerSample::kHatRight), DirectInputSampleDecoder::DecodePov (4500),  L"northeast");
            Assert::AreEqual (static_cast<Byte> (ControllerSample::kHatDown | ControllerSample::kHatRight), DirectInputSampleDecoder::DecodePov (13500), L"southeast");
            Assert::AreEqual (static_cast<Byte> (ControllerSample::kHatDown | ControllerSample::kHatLeft),  DirectInputSampleDecoder::DecodePov (22500), L"southwest");
            Assert::AreEqual (static_cast<Byte> (ControllerSample::kHatUp   | ControllerSample::kHatLeft),  DirectInputSampleDecoder::DecodePov (31500), L"northwest");
        }


        TEST_METHOD (Pov_SectorEdgesAndWrap)
        {
            Assert::AreEqual (ControllerSample::kHatUp, DirectInputSampleDecoder::DecodePov (2249),  L"just short of the northeast sector is still north");
            Assert::AreEqual (static_cast<Byte> (ControllerSample::kHatUp | ControllerSample::kHatRight),
                              DirectInputSampleDecoder::DecodePov (2250), L"the sector edge belongs to the diagonal");
            Assert::AreEqual (ControllerSample::kHatUp, DirectInputSampleDecoder::DecodePov (35999), L"just short of 360 wraps back to north");
        }


        TEST_METHOD (Buttons_HighBitMeansPressed)
        {
            DirectInputJoystickState  state;
            ControllerSample          sample;

            state.buttons[0]  = 0x80;
            state.buttons[3]  = 0x7F;
            state.buttons[11] = 0xFF;

            sample = DirectInputSampleDecoder::Decode (state, MakeFullLayout());

            Assert::IsTrue  (sample.buttons.test (0),  L"0x80 is pressed");
            Assert::IsFalse (sample.buttons.test (3),  L"only the high bit counts");
            Assert::IsTrue  (sample.buttons.test (11), L"the last reported button is read");
        }


        TEST_METHOD (Buttons_BeyondTheReportedCountAreIgnored)
        {
            DirectInputJoystickState  state;
            DirectInputObjectLayout   layout;
            ControllerSample          sample;

            state.buttons[2] = 0x80;
            layout.buttonCount = 2;

            sample = DirectInputSampleDecoder::Decode (state, layout);

            Assert::IsFalse (sample.buttons.test (2), L"a button the device does not report must not read pressed");
        }


        TEST_METHOD (ListControls_CoversEveryReportedControl)
        {
            DirectInputObjectLayout  layout;
            std::vector<ControlId>   controls;

            layout.presentAxes.set (0);
            layout.presentAxes.set (1);
            layout.buttonCount = 3;
            layout.hatCount    = 1;

            controls = DirectInputSampleDecoder::ListControls (layout);

            Assert::AreEqual (static_cast<size_t> (2 + 3 + 4), controls.size(), L"two axes, three buttons and four hat directions");
            Assert::IsTrue   (controls.front() == ControlId { ControlKind::Axis, 0 }, L"axes come first");
            Assert::IsTrue   (controls.back()  == ControlId { ControlKind::DpadRight, 0 }, L"hat directions come last");
        }
    };
}
