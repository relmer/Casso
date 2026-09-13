#include "Pch.h"

#include "Controllers/ControlCapture.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ControlCaptureTests
//
//  Press-to-assign. THE RULE THAT MATTERS MOST IS THAT A CONTROL ALREADY
//  ACTIVE WHEN WAITING BEGINS IS IGNORED: a stick resting off center or the
//  button just clicked would otherwise be assigned before the user did
//  anything.
//
////////////////////////////////////////////////////////////////////////////////

namespace ControllerTests
{
    TEST_CLASS (ControlCaptureTests)
    {
    public:

        static ControllerSample Rest()
        {
            ControllerSample  sample;

            sample.connected = true;
            return sample;
        }


        static std::vector<ControlId> Controls()
        {
            return
            {
                { ControlKind::Axis,    0 }, { ControlKind::Axis,   1 },
                { ControlKind::Trigger, 0 },
                { ControlKind::Button,  0 }, { ControlKind::Button, 1 },
                { ControlKind::DpadUp,  0 },
            };
        }


        TEST_METHOD (TheFirstControlActivatedIsAssigned)
        {
            ControlCapture                  capture;
            ControllerSample                pressed = Rest();
            std::optional<CapturedControl>  result;

            capture.Begin (Rest(), Controls());
            pressed.buttons.set (1);
            result = capture.Feed (pressed);

            Assert::IsTrue  (result.has_value(), L"a press while waiting is assigned");
            Assert::IsTrue  (result->control == ControlId { ControlKind::Button, 1 });
            Assert::IsFalse (capture.IsActive(), L"and waiting ends");
        }


        TEST_METHOD (AnAxisReportsWhichWayItWasPushed)
        {
            ControlCapture                  capture;
            ControllerSample                pushed = Rest();
            std::optional<CapturedControl>  result;

            capture.Begin (Rest(), Controls());
            pushed.axes[1] = -0.8f;
            result = capture.Feed (pushed);

            Assert::IsTrue (result->control == ControlId { ControlKind::Axis, 1 });
            Assert::IsTrue (result->negativeDirection, L"pushed toward negative, so a button binding counts that side");
        }


        TEST_METHOD (SmallMovementIsNotActivation)
        {
            ControlCapture    capture;
            ControllerSample  nudged = Rest();

            capture.Begin (Rest(), Controls());
            nudged.axes[0]     = 0.3f;
            nudged.triggers[0] = 0.2f;

            Assert::IsFalse (capture.Feed (nudged).has_value(), L"rest jitter and a light touch assign nothing");
            Assert::IsTrue  (capture.IsActive());
        }


        TEST_METHOD (AControlHeldWhenWaitingBeganIsIgnoredUntilLetGo)
        {
            ControlCapture    capture;
            ControllerSample  held = Rest();

            held.buttons.set (0);
            held.axes[0] = 0.9f;

            capture.Begin (held, Controls());

            Assert::IsFalse (capture.Feed (held).has_value(),
                L"the button clicked to start waiting, and a stick already pushed, are not the user's answer");

            capture.Feed (Rest());

            Assert::IsTrue (capture.Feed (held).has_value(), L"once let go, the same control counts");
        }


        TEST_METHOD (ATriggerPartlyPulledAtTheStartMustBeReleasedFirst)
        {
            ControlCapture    capture;
            ControllerSample  pulled = Rest();

            pulled.triggers[0] = 0.4f;
            capture.Begin (pulled, Controls());

            pulled.triggers[0] = 0.9f;
            Assert::IsFalse (capture.Feed (pulled).has_value(), L"pulling further is not a new activation");

            pulled.triggers[0] = 0.0f;
            capture.Feed (pulled);
            pulled.triggers[0] = 0.9f;
            Assert::IsTrue (capture.Feed (pulled).has_value());
        }


        TEST_METHOD (ADpadDirectionIsAssignable)
        {
            ControlCapture    capture;
            ControllerSample  up = Rest();

            capture.Begin (Rest(), Controls());
            up.hats[0] = ControllerSample::kHatUp;

            Assert::IsTrue (capture.Feed (up)->control == ControlId { ControlKind::DpadUp, 0 });
        }


        TEST_METHOD (CancelAssignsNothing)
        {
            ControlCapture    capture;
            ControllerSample  pressed = Rest();

            capture.Begin (Rest(), Controls());
            capture.Cancel();
            pressed.buttons.set (0);

            Assert::IsFalse (capture.IsActive());
            Assert::IsFalse (capture.Feed (pressed).has_value(), L"a press after cancel assigns nothing");
        }
    };
}
