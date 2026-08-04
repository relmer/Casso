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

bool KeystrokeInjector::WaitForStrobeClear (EmulatorCore & core, uint64_t cycleBudget)
{
    constexpr int  kPumpBatchSize = 64;



    uint64_t   target  = 0;
    int        i       = 0;
    bool       cleared = false;



    target = core.cpu->GetTotalCycles() + cycleBudget;

    // Re-checking after the loop covers the budget-exhausted exit, where the
    // last batch may have cleared the strobe on its final instruction.
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

    return cleared || core.keyboard->IsStrobeClear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeystrokeInjector::InjectKey
//
////////////////////////////////////////////////////////////////////////////////

bool KeystrokeInjector::InjectKey (
    EmulatorCore  &  core,
    Byte             ch,
    uint64_t         cycleBudget)
{
    // Two waits, not one: the first makes sure the PREVIOUS key was consumed
    // before overwriting the latch, the second that this one was.
    bool  injected = core.HasApple2e() && WaitForStrobeClear (core, cycleBudget);

    if (injected)
    {
        core.keyboard->KeyPressRaw (ch);
        injected = WaitForStrobeClear (core, cycleBudget);
    }

    return injected;
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
    size_t   consumed = 0;
    bool     ok       = true;

    // Stops at the first key the machine would not take, and reports how many
    // did land -- callers compare against text.size() to detect a short write.
    for (char ch : text)
    {
        if (ok)
        {
            ok = InjectKey (core, static_cast<Byte> (ch), keyCycles);
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
    size_t   consumed = InjectString (core, text, kPerKeyCycleBudget);

    // The RETURN only goes in if the whole line did; a short line leaves the
    // count short and never settles, so the caller sees the failure.
    if (consumed == text.size() && InjectKey (core, kAppleReturn, kPerKeyCycleBudget))
    {
        consumed++;
        core.RunCycles (settleCycles);
    }

    return consumed;
}
