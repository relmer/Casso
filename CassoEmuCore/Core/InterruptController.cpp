#include "Pch.h"

#include "InterruptController.h"
#include "StateReader.h"
#include "StateWriter.h"
#include "ICpu.h"





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

    CBREx (m_nextSource < kMaxSources, E_OUTOFMEMORY);

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

    UpdateLine();
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

    UpdateLine();
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

void InterruptController::UpdateLine()
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

void InterruptController::SoftReset()
{
    m_aggregate = 0;

    UpdateLine();
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

void InterruptController::PowerCycle()
{
    SoftReset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResetSources
//
//  Machine-teardown path: reclaim every source token and clear the
//  aggregate so a rebuilt machine re-registers its IRQ sources from a
//  fresh pool.
//
////////////////////////////////////////////////////////////////////////////////

void InterruptController::ResetSources()
{
    m_aggregate  = 0;
    m_nextSource = 0;

    UpdateLine();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SaveState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT InterruptController::SaveState (StateWriter & writer) const
{
    writer.BeginSection (kStateTag, kStateVersion);

    writer.WriteUInt32 (m_nextSource);
    writer.WriteUInt32 (m_aggregate);

    return writer.EndSection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadState
//
//  Fails with ERROR_INVALID_DATA when the saved machine allocated a different
//  number of sources, or a saved bit holds a source this machine never
//  allocated: the bits would then belong to other devices.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT InterruptController::LoadState (StateReader & reader)
{
    HRESULT   hr          = S_OK;
    uint16_t  version     = 0;
    uint32_t  sourceCount = 0;
    uint32_t  aggregate   = 0;
    uint64_t  validMask   = 0;



    hr = reader.BeginSection (kStateTag, kStateVersion, version);
    CHR (hr);

    reader.ReadUInt32 (sourceCount);
    reader.ReadUInt32 (aggregate);

    hr = reader.EndSection();
    CHR (hr);

    validMask = (static_cast<uint64_t> (1) << m_nextSource) - 1;

    CBREx (sourceCount == m_nextSource,   HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx ((aggregate & ~validMask) == 0, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    m_aggregate = aggregate;

Error:
    return hr;
}
