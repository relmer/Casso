#include "Pch.h"

#include "Devices/Printer/PrinterStatusModel.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// User-facing printer status for the chrome indicator (FR-019). Clock-injected
// so the Receiving latch and the Error > Receiving > Pending > Idle priority are
// deterministic.
namespace PrinterStatusModelTests
{
    static PrinterStatusModel::Config Win (double ms)
    {
        PrinterStatusModel::Config   c;
        c.receivingWindowMs = ms;
        return c;
    }


    TEST_CLASS (PrinterStatusModelTests)
    {
    public:

        TEST_METHOD (IdleWhenNothingHappening)
        {
            PrinterStatusModel   m;

            m.Update (0, 0.0, false, false);
            Assert::IsTrue (m.Status() == PrinterStatus::Idle);
        }


        TEST_METHOD (PendingWhenStripHasContentNoActivity)
        {
            PrinterStatusModel   m;

            m.Update (0, 0.0, true, false);
            Assert::IsTrue (m.Status() == PrinterStatus::Pending);
        }


        TEST_METHOD (ReceivingWhenActivityAdvances)
        {
            PrinterStatusModel   m (Win (400.0));

            m.Update (0,   0.0,   false, false);   // prime baseline
            m.Update (128, 10.0,  true,  false);   // counter advanced -> receiving
            Assert::IsTrue (m.Status() == PrinterStatus::Receiving);
        }


        TEST_METHOD (ReceivingLatchesThenFallsToPending)
        {
            PrinterStatusModel   m (Win (400.0));

            m.Update (0,   0.0,   false, false);
            m.Update (128, 10.0,  true,  false);
            Assert::IsTrue (m.Status() == PrinterStatus::Receiving);

            // No further activity: still receiving inside the window...
            m.Update (128, 300.0, true, false);
            Assert::IsTrue (m.Status() == PrinterStatus::Receiving);

            // ...then falls back to Pending once the window elapses.
            m.Update (128, 500.0, true, false);
            Assert::IsTrue (m.Status() == PrinterStatus::Pending);
        }


        TEST_METHOD (ContinuedActivityKeepsReceiving)
        {
            PrinterStatusModel   m (Win (400.0));

            m.Update (0,   0.0,   false, false);
            m.Update (10,  10.0,  true,  false);
            m.Update (20,  300.0, true,  false);   // refreshes the window
            m.Update (30,  600.0, true,  false);   // still within 400ms of 300
            Assert::IsTrue (m.Status() == PrinterStatus::Receiving);
        }


        TEST_METHOD (FirstSampleWithAdvancedCounterIsNotReceiving)
        {
            PrinterStatusModel   m (Win (400.0));

            // Worker already accumulated activity before the first sample: the
            // baseline must not read as a burst.
            m.Update (5000, 0.0, true, false);
            Assert::IsTrue (m.Status() == PrinterStatus::Pending);
        }


        TEST_METHOD (ErrorOutranksEverything)
        {
            PrinterStatusModel   m (Win (400.0));

            m.Update (0,  0.0,  false, false);
            m.Update (99, 10.0, true,  true);   // receiving + content, but error wins
            Assert::IsTrue (m.Status() == PrinterStatus::Error);
        }


        TEST_METHOD (ResetForgetsHistory)
        {
            PrinterStatusModel   m (Win (400.0));

            m.Update (0,   0.0,  false, false);
            m.Update (128, 10.0, true,  false);
            Assert::IsTrue (m.Status() == PrinterStatus::Receiving);

            m.Reset();
            Assert::IsTrue (m.Status() == PrinterStatus::Idle);

            // After reset an advanced counter re-primes without spurious activity.
            m.Update (256, 20.0, true, false);
            Assert::IsTrue (m.Status() == PrinterStatus::Pending);
        }
    };
}
