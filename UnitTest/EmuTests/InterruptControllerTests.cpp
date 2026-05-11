#include "Pch.h"

#include "CppUnitTest.h"
#include "Core/InterruptController.h"
#include "ICpu.h"
#include "MockIrqAsserter.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  RecordingCpu
//
//  Minimal ICpu test double that records the most recent line state per
//  interrupt kind. Other ICpu methods are no-ops — InterruptController
//  only ever calls SetInterruptLine.
//
////////////////////////////////////////////////////////////////////////////////

namespace Apple2eFidelityIc
{
    class RecordingCpu : public ICpu
    {
    public:
        HRESULT     Reset            () override                                 { return S_OK; }
        HRESULT     Step             (uint32_t & outCycles) override             { outCycles = 0; return S_OK; }
        uint64_t    GetCycleCount    () const override                           { return 0; }

        void        SetInterruptLine (CpuInterruptKind kind, bool asserted) override
        {
            if (kind == CpuInterruptKind::kMaskable)
            {
                m_irqAsserted = asserted;
                ++m_irqUpdateCount;
            }
            else
            {
                m_nmiAsserted = asserted;
            }
        }

        bool        IrqAsserted    () const { return m_irqAsserted; }
        bool        NmiAsserted    () const { return m_nmiAsserted; }
        int         IrqUpdateCount () const { return m_irqUpdateCount; }

    private:
        bool    m_irqAsserted    = false;
        bool    m_nmiAsserted    = false;
        int     m_irqUpdateCount = 0;
    };





    ////////////////////////////////////////////////////////////////////////////
    //
    //  InterruptControllerTests
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (InterruptControllerTests)
    {
    public:
        TEST_METHOD (SingleAssertReachesCpu)
        {
            RecordingCpu            cpu;
            InterruptController     ic (&cpu);
            IrqSourceId             id = 0;
            HRESULT                 hr = S_OK;



            hr = ic.RegisterSource (id);
            Assert::AreEqual (S_OK, hr);

            Assert::IsFalse (cpu.IrqAsserted (), L"Pre-assert: line clear");

            ic.Assert (id);
            Assert::IsTrue  (cpu.IrqAsserted (), L"Assert should drive line");

            ic.Clear (id);
            Assert::IsFalse (cpu.IrqAsserted (), L"Clear should drop line");
        }


        TEST_METHOD (MultipleAssertersOredTogether)
        {
            RecordingCpu            cpu;
            InterruptController     ic (&cpu);
            IrqSourceId             a  = 0;
            IrqSourceId             b  = 0;
            IrqSourceId             c  = 0;
            HRESULT                 hr = S_OK;



            hr = ic.RegisterSource (a); Assert::AreEqual (S_OK, hr);
            hr = ic.RegisterSource (b); Assert::AreEqual (S_OK, hr);
            hr = ic.RegisterSource (c); Assert::AreEqual (S_OK, hr);

            ic.Assert (a);
            ic.Assert (b);
            ic.Assert (c);

            Assert::IsTrue (cpu.IrqAsserted ());
            Assert::IsTrue (ic.IsAnyAsserted ());
        }


        TEST_METHOD (ClearOnlyDeassertsWhenAllSourcesClear)
        {
            RecordingCpu            cpu;
            InterruptController     ic (&cpu);
            IrqSourceId             a  = 0;
            IrqSourceId             b  = 0;
            HRESULT                 hr = S_OK;



            hr = ic.RegisterSource (a); Assert::AreEqual (S_OK, hr);
            hr = ic.RegisterSource (b); Assert::AreEqual (S_OK, hr);

            ic.Assert (a);
            ic.Assert (b);
            Assert::IsTrue (cpu.IrqAsserted ());

            ic.Clear (a);
            Assert::IsTrue (cpu.IrqAsserted (),
                            L"Line must remain asserted while another source is asserting");

            ic.Clear (b);
            Assert::IsFalse (cpu.IrqAsserted (),
                             L"Line drops only when all sources are clear");
        }


        TEST_METHOD (UnregisteredSourceRejected)
        {
            RecordingCpu            cpu;
            InterruptController     ic (&cpu);
            IrqSourceId             id          = 0;
            IrqSourceId             allocated   = 0;
            HRESULT                 hr          = S_OK;
            int                     i           = 0;



            // Saturate the 32-source pool.
            for (i = 0; i < InterruptController::kMaxSources; ++i)
            {
                hr = ic.RegisterSource (allocated);
                Assert::AreEqual (S_OK, hr);
            }

            // 33rd registration must fail.
            hr = ic.RegisterSource (id);
            Assert::IsTrue (FAILED (hr),
                            L"Registration past kMaxSources must fail");

            // Asserting an out-of-range / never-registered token is a no-op.
            ic.Assert (200);
            Assert::IsFalse (cpu.IrqAsserted (),
                             L"Unregistered source must not drive the line");
        }


        TEST_METHOD (WorksWithMockIrqAsserter)
        {
            RecordingCpu            cpu;
            InterruptController     ic (&cpu);
            MockIrqAsserter         asserter (&ic);
            HRESULT                 hr = S_OK;



            hr = asserter.Bind ();
            Assert::AreEqual (S_OK, hr);
            Assert::IsTrue (asserter.IsBound ());

            asserter.Assert ();
            Assert::IsTrue (cpu.IrqAsserted (),
                            L"MockIrqAsserter::Assert must drive the line");

            asserter.Clear ();
            Assert::IsFalse (cpu.IrqAsserted (),
                             L"MockIrqAsserter::Clear must drop the line");
        }
    };
}
