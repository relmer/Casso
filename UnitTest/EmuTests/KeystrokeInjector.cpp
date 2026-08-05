#include "Pch.h"
#include "KeystrokeInjector.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WaitForStrobeClear
//
//  Pumps CPU cycles in small batches until the keyboard strobe is
//  consumed by the ROM polling loop, or the budget is exhausted.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeystrokeInjector::WaitForStrobeClear (EmulatorCore & core, uint64_t cycleBudget)
{
    constexpr int  kPumpBatchSize = 64;



    HRESULT    hr      = S_OK;
    uint64_t   target  = 0;
    int        i       = 0;
    bool       cleared = false;



    target = core.cpu->GetTotalCycles() + cycleBudget;

    while (!cleared && core.cpu->GetTotalCycles() < target)
    {
        cleared = core.keyboard->IsStrobeClear();

        if (!cleared)
        {
            for (i = 0; i < kPumpBatchSize; i++)
            {
                core.cpu->StepOne();
                core.cpu->AddCycles (core.cpu->GetLastInstructionCycles());
            }
        }
    }

    // Re-checking after the loop covers the budget-exhausted exit, where the
    // last batch may have cleared the strobe on its final instruction.
    cleared = cleared || core.keyboard->IsStrobeClear();
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
    EmulatorCore  &  core,
    Byte             ch,
    uint64_t         cycleBudget)
{
    HRESULT  hr    = S_OK;
    bool     has2e = core.HasApple2e();



    CBR (has2e);

    // Two waits, not one: the first makes sure the PREVIOUS key was consumed
    // before overwriting the latch, the second that this one was.
    hr = WaitForStrobeClear (core, cycleBudget);
    CHR (hr);

    core.keyboard->KeyPressRaw (ch);

    hr = WaitForStrobeClear (core, cycleBudget);
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
    EmulatorCore       &  core,
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
            hrKey = InjectKey (core, static_cast<Byte> (ch), keyCycles);
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
    EmulatorCore       &  core,
    const std::string  &  text,
    uint64_t              settleCycles)
{
    HRESULT  hrReturn = S_OK;
    size_t   consumed = InjectString (core, text, kPerKeyCycleBudget);

    // The RETURN only goes in if the whole line did; a short line leaves the
    // count short and never settles, so the caller sees the failure.
    if (consumed == text.size())
    {
        hrReturn = InjectKey (core, kAppleReturn, kPerKeyCycleBudget);

        if (SUCCEEDED (hrReturn))
        {
            consumed++;
            core.RunCycles (settleCycles);
        }
    }

    return consumed;
}
