#include "Pch.h"

#include "Machines/Apple2/Apple2e/Apple2eKeyboard.h"
#include "KeystrokeInjector.h"
#include "MachineIdle.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WaitForStrobeClear
//
//  Pumps CPU cycles in small batches until the keyboard strobe is
//  consumed by the ROM polling loop, or the budget is exhausted.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeystrokeInjector::WaitForStrobeClear (MachineHost & host, uint64_t cycleBudget)
{
    constexpr int  kPumpBatchSize = 64;



    HRESULT    hr      = S_OK;
    uint64_t   target  = 0;
    int        i       = 0;
    bool       cleared = false;



    target = host.GetCpu()->GetTotalCycles() + cycleBudget;

    while (!cleared && host.GetCpu()->GetTotalCycles() < target)
    {
        cleared = host.GetRefs().iieKeyboard->IsStrobeClear();

        if (!cleared)
        {
            for (i = 0; i < kPumpBatchSize; i++)
            {
                host.GetCpu()->StepOne();
                host.GetCpu()->AddCycles (host.GetCpu()->GetLastInstructionCycles());
            }
        }
    }

    // Re-checking after the loop covers the budget-exhausted exit, where the
    // last batch may have cleared the strobe on its final instruction.
    cleared = cleared || host.GetRefs().iieKeyboard->IsStrobeClear();
    CBR (cleared);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeystrokeInjector::InjectKey
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeystrokeInjector::InjectKey (
    MachineHost  &  host,
    Byte             ch,
    uint64_t         cycleBudget)
{
    HRESULT  hr    = S_OK;
    bool     has2e = (host.GetCpu() != nullptr && host.GetMmu() != nullptr);



    CBR (has2e);

    // Two waits, not one: the first makes sure the PREVIOUS key was consumed
    // before overwriting the latch, the second that this one was.
    hr = WaitForStrobeClear (host, cycleBudget);
    CHR (hr);

    host.GetRefs().iieKeyboard->PressKey (ch);

    hr = WaitForStrobeClear (host, cycleBudget);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeystrokeInjector::InjectString
//
////////////////////////////////////////////////////////////////////////////////

size_t KeystrokeInjector::InjectString (
    MachineHost       &  host,
    const std::string  &  text,
    uint64_t              keyCycles)
{
    HRESULT  hrKey    = S_OK;
    size_t   consumed = 0;
    bool     ok       = true;



    // Stops at the first key the machine would not take, and reports how many
    // did land -- callers compare against text.size() to detect a short write.
    for (char ch : text)
    {
        if (ok)
        {
            hrKey = InjectKey (host, static_cast<Byte> (ch), keyCycles);
            ok    = SUCCEEDED (hrKey);
        }

        if (ok)
        {
            consumed++;
        }
    }

    return consumed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeystrokeInjector::InjectLine
//
////////////////////////////////////////////////////////////////////////////////

size_t KeystrokeInjector::InjectLine (
    MachineHost       &  host,
    const std::string  &  text,
    uint64_t              settleCycles)
{
    HRESULT  hrReturn = S_OK;
    size_t   consumed = InjectString (host, text, kPerKeyCycleBudget);



    // The RETURN only goes in if the whole line did; a short line leaves the
    // count short and never settles, so the caller sees the failure.
    if (consumed == text.size())
    {
        hrReturn = InjectKey (host, kAppleReturn, kPerKeyCycleBudget);

        if (SUCCEEDED (hrReturn))
        {
            consumed++;

            // Settle until the machine is actually done, with settleCycles
            // as the ceiling rather than the target. A caller whose ceiling
            // is shorter than the quiet window never reaches idle and so
            // spends the whole budget, exactly as this used to.
            MachineIdle::RunUntilIdle (host, settleCycles);
        }
    }

    return consumed;
}
