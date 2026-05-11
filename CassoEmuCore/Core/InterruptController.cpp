#include "Pch.h"

#include "Ehm.h"
#include "InterruptController.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InterruptController::InterruptController
//
////////////////////////////////////////////////////////////////////////////////

InterruptController::InterruptController (ICpu * cpu)
    : m_cpu (cpu)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegisterSource
//
//  Allocates the next free source token. Fails if all 32 source slots are
//  in use. The returned token is the caller's identifier for subsequent
//  Assert / Clear calls.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT InterruptController::RegisterSource (IrqSourceId & outId)
{
    HRESULT     hr = S_OK;



    outId = 0;

    if (m_nextSource >= kMaxSources)
    {
        hr = E_OUTOFMEMORY;
        goto Error;
    }

    outId = m_nextSource;
    ++m_nextSource;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Assert
//
//  Sets the source's bit and updates the CPU line. Unregistered or
//  out-of-range tokens are silently ignored — Assert/Clear are
//  best-effort surfaces (level-sensitive); RegisterSource is the only
//  path that returns failure.
//
////////////////////////////////////////////////////////////////////////////////

void InterruptController::Assert (IrqSourceId source)
{
    uint32_t    mask = 0;



    if (source >= m_nextSource)
    {
        return;
    }

    mask = static_cast<uint32_t> (1u) << source;

    m_aggregate |= mask;

    UpdateLine ();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Clear
//
////////////////////////////////////////////////////////////////////////////////

void InterruptController::Clear (IrqSourceId source)
{
    uint32_t    mask = 0;



    if (source >= m_nextSource)
    {
        return;
    }

    mask = static_cast<uint32_t> (1u) << source;

    m_aggregate &= ~mask;

    UpdateLine ();
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateLine
//
//  Drives the wired CPU's maskable interrupt line from the current
//  aggregate. Level-sensitive — aggregate=true keeps the line
//  asserted continuously.
//
////////////////////////////////////////////////////////////////////////////////

void InterruptController::UpdateLine ()
{
    if (m_cpu == nullptr)
    {
        return;
    }

    m_cpu->SetInterruptLine (CpuInterruptKind::kMaskable, m_aggregate != 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoftReset
//
//  Phase 4 / FR-034: clear all per-source assertions and de-assert the
//  wired CPU's maskable line. Source-ID allocations are preserved so
//  peripherals keep their tokens across a reset (audit §10).
//
////////////////////////////////////////////////////////////////////////////////

void InterruptController::SoftReset ()
{
    m_aggregate = 0;

    UpdateLine ();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PowerCycle
//
//  Phase 4 / FR-035: same effect as SoftReset — there is no DRAM-shaped
//  state on the controller. Provided as a separate entry point so the
//  EmulatorShell power path can be uniform across components.
//
////////////////////////////////////////////////////////////////////////////////

void InterruptController::PowerCycle ()
{
    SoftReset ();
}
