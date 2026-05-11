#pragma once

#include "Pch.h"
#include "ICpu.h"
#include "IInterruptController.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InterruptController
//
//  Concrete IInterruptController. Aggregates up to 32 per-source IRQ
//  assertions into a single bitmask. Whenever the aggregate transitions
//  between zero and non-zero, the change is forwarded to the wired
//  ICpu via SetInterruptLine(kMaskable, ...). NMI sources, if added
//  later, follow the same pattern with kNonMaskable.
//
//  Source IDs are allocated dynamically via RegisterSource, returning a
//  token (0..31). Up to 32 sources are supported by the bitmask.
//
////////////////////////////////////////////////////////////////////////////////

class InterruptController : public IInterruptController
{
public:
    static constexpr IrqSourceId    kMaxSources = 32;

    explicit                        InterruptController (ICpu * cpu = nullptr);
                                    ~InterruptController () override = default;

    // IInterruptController
    HRESULT                         RegisterSource (IrqSourceId & outId) override;
    void                            Assert         (IrqSourceId source) override;
    void                            Clear          (IrqSourceId source) override;
    bool                            IsAnyAsserted  () const override { return m_aggregate != 0; }

    // Setter for late-binding the CPU after construction.
    void                            SetCpu (ICpu * cpu) { m_cpu = cpu; }

    // Phase 4 split-reset. Both clear the aggregate and de-assert the
    // CPU's IRQ line; PowerCycle additionally accepts the shared Prng for
    // signature symmetry with MemoryDevice::PowerCycle. Sources keep
    // their allocated IrqSourceIds — re-enumerating them would invalidate
    // tokens already held by peripheral devices.
    void                            SoftReset  ();
    void                            PowerCycle ();

private:
    void                            UpdateLine ();

    ICpu      *                     m_cpu        = nullptr;
    uint32_t                        m_aggregate  = 0;
    IrqSourceId                     m_nextSource = 0;
};
