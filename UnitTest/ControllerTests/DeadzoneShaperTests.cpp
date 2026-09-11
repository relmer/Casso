#include "Pch.h"

#include "Controllers/DeadzoneShaper.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DeadzoneShaperTests
//
//  A deadzone that only silenced the rest area would cost travel at both
//  ends: a stick pushed fully over would stop short of the paddle's limit,
//  and a game that reads the full range would never see it. So the rule under
//  test is that the rest area reads center AND the remaining travel still
//  reaches both ends.
//
//  The round rest area matters on diagonals. Shaped separately, a stick
//  pushed diagonally at a shallow angle leaves one axis inside its deadzone,
//  which reads as a pure cardinal direction: the classic "my joystick will
//  not go diagonally" complaint.
//
////////////////////////////////////////////////////////////////////////////////

namespace ControllerTests
{
    TEST_CLASS (DeadzoneShaperTests)
    {
    public:

        static constexpr float  kTolerance = 0.0001f;
        static constexpr float  kDeadzone  = 0.25f;


        TEST_METHOD (Axis_RestAreaReadsExactlyCenter)
        {
            Assert::AreEqual (0.0f, DeadzoneShaper::ShapeAxis (0.0f,   kDeadzone), 0.0f, L"center is center");
            Assert::AreEqual (0.0f, DeadzoneShaper::ShapeAxis (0.2f,   kDeadzone), 0.0f, L"inside the deadzone is center");
            Assert::AreEqual (0.0f, DeadzoneShaper::ShapeAxis (-0.25f, kDeadzone), 0.0f, L"the deadzone edge is still center");
        }


        TEST_METHOD (Axis_FullDeflectionStillReachesTheEnds)
        {
            Assert::AreEqual ( 1.0f, DeadzoneShaper::ShapeAxis ( 1.0f, kDeadzone), kTolerance, L"a deadzone must not cost travel");
            Assert::AreEqual (-1.0f, DeadzoneShaper::ShapeAxis (-1.0f, kDeadzone), kTolerance, L"and not at the other end either");
        }


        TEST_METHOD (Axis_RisesMonotonicallyOutsideTheDeadzone)
        {
            float  previous = 0.0f;

            for (float value = 0.26f; value <= 1.0f; value += 0.05f)
            {
                float  shaped = DeadzoneShaper::ShapeAxis (value, kDeadzone);

                Assert::IsTrue (shaped > previous, L"each step further out must read further out");
                previous = shaped;
            }
        }


        TEST_METHOD (Stick_ShallowDiagonalKeepsBothAxes)
        {
            constexpr float  kPushX = 0.9f;
            constexpr float  kPushY = 0.2f;

            float  shapedX = 0.0f;
            float  shapedY = 0.0f;

            DeadzoneShaper::ShapeStick (kPushX, kPushY, kDeadzone, shapedX, shapedY);

            Assert::IsTrue (shapedX > 0.0f, L"the pushed axis reads");
            Assert::IsTrue (shapedY > 0.0f,
                L"the smaller axis must survive too: shaped on its own it would fall inside the deadzone and the push would read as pure right");
        }


        TEST_METHOD (Stick_RestAreaIsRoundNotSquare)
        {
            constexpr float  kInside = 0.17f;   // 0.24 away from center, inside a 0.25 circle

            float  shapedX = 0.0f;
            float  shapedY = 0.0f;

            DeadzoneShaper::ShapeStick (kInside, kInside, kDeadzone, shapedX, shapedY);

            Assert::AreEqual (0.0f, shapedX, 0.0f, L"a diagonal nudge shorter than the deadzone is still rest");
            Assert::AreEqual (0.0f, shapedY, 0.0f, L"on both axes");
        }


        TEST_METHOD (Stick_DiagonalRimIsFullDeflection)
        {
            constexpr float  kRim = 0.7071f;   // a stick pushed to its rim, diagonally

            float  shapedX   = 0.0f;
            float  shapedY   = 0.0f;
            float  magnitude = 0.0f;

            DeadzoneShaper::ShapeStick (kRim, kRim, kDeadzone, shapedX, shapedY);
            magnitude = std::sqrt (shapedX * shapedX + shapedY * shapedY);

            // The rim is as far as the stick goes, so a diagonal push there is
            // full deflection and must not be cut short by the deadzone. It is
            // NOT 1.0 per axis: on a round envelope each axis reads about
            // 0.707 at the diagonal, and demanding more would mean a square
            // one, which is what the round rest area exists to avoid.
            Assert::AreEqual (1.0f,  magnitude, 0.01f, L"a diagonal push to the rim is full deflection");
            Assert::AreEqual (kRim,  shapedX,   0.01f, L"split evenly across X");
            Assert::AreEqual (kRim,  shapedY,   0.01f, L"and Y");
        }


        TEST_METHOD (Paddle_EndsAndCenter)
        {
            Assert::AreEqual (static_cast<Byte> (0),   DeadzoneShaper::ToPaddle (-1.0f), L"full negative is 0");
            Assert::AreEqual (static_cast<Byte> (127), DeadzoneShaper::ToPaddle (0.0f),  L"center is 127, where every other source rests");
            Assert::AreEqual (static_cast<Byte> (255), DeadzoneShaper::ToPaddle (1.0f),  L"full positive is 255");
        }


        TEST_METHOD (Paddle_HalfDeflectionIsNearHalfway)
        {
            Byte  half = DeadzoneShaper::ToPaddle (0.5f);

            Assert::IsTrue (half > 185 && half < 200, L"half a push reads about halfway between center and the end");
        }


        TEST_METHOD (Defaults_XboxUsesTheValueXInputPublishes)
        {
            Assert::AreEqual (7849.0f / 32767.0f, DeadzoneShaper::GetDefaultDeadzone (ControllerKind::XInput), kTolerance);
            Assert::AreEqual (0.12f,              DeadzoneShaper::GetDefaultDeadzone (ControllerKind::DirectInput), kTolerance);
        }
    };
}
