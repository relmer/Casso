#include "Pch.h"

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
        int         IrqUpdateCount() const { return m_irqUpdateCount; }

    private:
        bool    m_irqAsserted    = false;
        bool    m_nmiAsserted    = false;
        int     m_irqUpdateCount = 0;
    };





    ////////////////////////////////////////////////////////////////////////////
    //
    //  InterruptControllerTests
    //
    //  The shared IRQ line: several sources asserting and releasing it
    //  independently.
    //
    //  IRQ is WIRED-OR, and that is the whole subject. The line stays asserted
    //  while ANY source holds it, so a source releasing must not clear the line
    //  for the others -- the classic bug being an interrupt handler that
    //  services one device and inadvertently dismisses another's pending
    //  request.
    //
    //  Source tokens are covered because they are reclaimed on a machine
    //  switch: a stale token asserting after its device is gone would leave the
    //  CPU permanently interrupted.
    //
    //  Assert and release are tested in several orders, since the count is what
    //  makes the wired-OR work and an order-dependent implementation passes the
    //  simple sequence.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (InterruptControllerTests)
    {
    public:
        TEST_METHOD (SingleAssertReachesCpu)
        {
            RecordingCpu            cpu;
            HRESULT                 hr = S_OK;
            InterruptController     ic (&cpu);
            IrqSourceId             id = 0;



            hr = ic.RegisterSource (id);
            Assert::AreEqual (S_OK, hr);

            Assert::IsFalse (cpu.IrqAsserted(), L"Pre-assert: line clear");

            ic.Assert (id);
            Assert::IsTrue  (cpu.IrqAsserted(), L"Assert should drive line");

            ic.Clear (id);
            Assert::IsFalse (cpu.IrqAsserted(), L"Clear should drop line");
        }


        TEST_METHOD (MultipleAssertersOredTogether)
        {
            RecordingCpu            cpu;
            HRESULT                 hr = S_OK;
            InterruptController     ic (&cpu);
            IrqSourceId             a  = 0;
            IrqSourceId             b  = 0;
            IrqSourceId             c  = 0;



            hr = ic.RegisterSource (a); Assert::AreEqual (S_OK, hr);
            hr = ic.RegisterSource (b); Assert::AreEqual (S_OK, hr);
            hr = ic.RegisterSource (c); Assert::AreEqual (S_OK, hr);

            ic.Assert (a);
            ic.Assert (b);
            ic.Assert (c);

            Assert::IsTrue (cpu.IrqAsserted());
            Assert::IsTrue (ic.IsAnyAsserted());
        }


        TEST_METHOD (ClearOnlyDeassertsWhenAllSourcesClear)
        {
            RecordingCpu            cpu;
            HRESULT                 hr = S_OK;
            InterruptController     ic (&cpu);
            IrqSourceId             a  = 0;
            IrqSourceId             b  = 0;



            hr = ic.RegisterSource (a); Assert::AreEqual (S_OK, hr);
            hr = ic.RegisterSource (b); Assert::AreEqual (S_OK, hr);

            ic.Assert (a);
            ic.Assert (b);
            Assert::IsTrue (cpu.IrqAsserted());

            ic.Clear (a);
            Assert::IsTrue (cpu.IrqAsserted(),
                            L"Line must remain asserted while another source is asserting");

            ic.Clear (b);
            Assert::IsFalse (cpu.IrqAsserted(),
                             L"Line drops only when all sources are clear");
        }


        TEST_METHOD (UnregisteredSourceRejected)
        {
            RecordingCpu            cpu;
            HRESULT                 hr          = S_OK;
            int                     i           = 0;
            InterruptController     ic (&cpu);
            IrqSourceId             id          = 0;
            IrqSourceId             allocated   = 0;



            // Saturate the 32-source pool.
            for (i = 0; i < InterruptController::kMaxSources; ++i)
            {
                hr = ic.RegisterSource (allocated);
                Assert::AreEqual (S_OK, hr);
            }

            // 33rd registration must fail.
            hr = ic.RegisterSource (id);
            AssertFailed (hr,
                            L"Registration past kMaxSources must fail");

            // Asserting an out-of-range / never-registered token is a no-op.
            ic.Assert (200);
            Assert::IsFalse (cpu.IrqAsserted(),
                             L"Unregistered source must not drive the line");
        }


        TEST_METHOD (WorksWithMockIrqAsserter)
        {
            RecordingCpu            cpu;
            HRESULT                 hr = S_OK;
            InterruptController     ic (&cpu);
            MockIrqAsserter         asserter (&ic);



            hr = asserter.Bind();
            Assert::AreEqual (S_OK, hr);
            Assert::IsTrue (asserter.IsBound());

            asserter.Assert();
            Assert::IsTrue (cpu.IrqAsserted(),
                            L"MockIrqAsserter::Assert must drive the line");

            asserter.Clear();
            Assert::IsFalse (cpu.IrqAsserted(),
                             L"MockIrqAsserter::Clear must drop the line");
        }
    };
}
