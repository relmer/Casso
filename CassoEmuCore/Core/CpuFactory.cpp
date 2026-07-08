#include "Pch.h"

#include "CpuFactory.h"
#include "MemoryBusCpu.h"

// Recognized CPU strategy identifiers.
static constexpr const char *    s_kpszCpu6502  = "6502";
static constexpr const char *    s_kpszCpu65C02 = "65C02";




////////////////////////////////////////////////////////////////////////////////
//
//  Create
//
//  The 65C02 strategy is recognized as a valid type but is not buildable until
//  the CMOS core lands; it returns E_NOTIMPL rather than silently falling back
//  to the NMOS part (the very defect this seam exists to fix).
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CpuFactory::Create (const string & cpuType, MemoryBus & bus, unique_ptr<ICpu> & outCpu)
{
    HRESULT   hr = S_OK;

    outCpu.reset ();

    if (cpuType == s_kpszCpu6502)
    {
        outCpu = make_unique<MemoryBusCpu> (bus);
    }
    else if (cpuType == s_kpszCpu65C02)
    {
        hr = E_NOTIMPL;
    }
    else
    {
        hr = E_INVALIDARG;
    }

    return hr;
}
