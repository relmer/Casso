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



    uint64_t   target;
    int        i;



    target = core.cpu->GetTotalCycles() + cycleBudget;

    while (core.cpu->GetTotalCycles() < target)
    {
        if (core.keyboard->IsStrobeClear())
        {
            return true;
        }

        for (i = 0; i < kPumpBatchSize; i++)
        {
            core.cpu->StepOne();
            core.cpu->AddCycles (core.cpu->GetLastInstructionCycles());
        }
    }

    return core.keyboard->IsStrobeClear();
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
    if (!core.HasApple2e())
    {
        return false;
    }

    if (!WaitForStrobeClear (core, cycleBudget))
    {
        return false;
    }

    core.keyboard->KeyPressRaw (ch);

    return WaitForStrobeClear (core, cycleBudget);
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

    for (char ch : text)
    {
        if (!InjectKey (core, static_cast<Byte> (ch), keyCycles))
        {
            return consumed;
        }

        consumed++;
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
    size_t   consumed;

    consumed = InjectString (core, text, kPerKeyCycleBudget);

    if (consumed != text.size())
    {
        return consumed;
    }

    if (!InjectKey (core, kAppleReturn, kPerKeyCycleBudget))
    {
        return consumed;
    }

    consumed++;
    core.RunCycles (settleCycles);

    return consumed;
}
