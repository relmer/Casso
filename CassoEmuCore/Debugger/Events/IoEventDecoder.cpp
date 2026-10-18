#include "Pch.h"

#include "Debugger/Events/IoEventDecoder.h"
#include "Machines/Apple2/Apple2e/Apple2eMmu.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "Machines/Apple2/Common/AppleSoftSwitchBank.h"
#include "Machines/Apple2/Common/MockingboardCard.h"
#include "Machines/Apple2/Common/VideoTiming.h"
#include "Shell/MachineHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IoEventDecoder::Seed
//
////////////////////////////////////////////////////////////////////////////////

void IoEventDecoder::Seed (MachineHost & machine)
{
    MockingboardCard  * card = machine.GetRefs().mockingboard;
    int                 chip = 0;



    m_cycles    = machine.GetCpu()->GetCycleCounterPtr();
    m_videoBits = GetVideoModeBits (machine);
    m_boardPage = (card != nullptr) ? (card->GetStart() >> kPageShift) : kNoBoard;

    for (chip = 0; chip < kChipCount; chip++)
    {
        m_lastControl[chip] = (card != nullptr) ? card->GetLastControl (chip) : 0;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IoEventDecoder::GetFilter
//
//  The ranges IoEventLog::IsVideoSwitch and IsSpeaker take, and the card's
//  page for writes.
//
////////////////////////////////////////////////////////////////////////////////

void IoEventDecoder::GetFilter (IoAccessFilter & outFilter) const
{
    constexpr Word  kPageLast = 0xFF;
    Word            board     = 0;



    outFilter = IoAccessFilter();

    outFilter.AddReads  (IoEventLog::kSpeakerFirst, IoEventLog::kSpeakerLast);
    outFilter.AddReads  (IoEventLog::kDisplayFirst, IoEventLog::kDisplayLast);
    outFilter.AddWrites (IoEventLog::kIieFirst,     IoEventLog::kIieLast);
    outFilter.AddWrites (IoEventLog::kSpeakerFirst, IoEventLog::kSpeakerLast);
    outFilter.AddWrites (IoEventLog::kDisplayFirst, IoEventLog::kDisplayLast);

    if (m_boardPage != kNoBoard)
    {
        board = (Word) (m_boardPage << kPageShift);
        outFilter.AddWrites (board, (Word) (board + kPageLast));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IoEventDecoder::DecodeOther
//
//  An access that is not the speaker's. A video switch is judged by the
//  mode after the access, so an access that changes nothing makes no entry.
//  The cycle and the beam are the machine's at the access.
//
////////////////////////////////////////////////////////////////////////////////

size_t IoEventDecoder::DecodeOther (
    MachineHost  & machine,
    Word           address,
    Byte           value,
    bool           isWrite,
    Events       & outEvents)
{
    const VideoTiming  * timing = nullptr;
    uint64_t             cycle  = 0;
    uint16_t             after  = 0;
    IoEvent              entry;



    if (IoEventLog::IsVideoSwitch (address, isWrite))
    {
        after = GetVideoModeBits (machine);

        if (after == m_videoBits)
        {
            return 0;
        }

        timing = machine.GetVideoTiming();
        cycle  = *m_cycles;

        entry.cycle       = cycle;
        entry.kind        = IoEventKind::VideoMode;
        entry.address     = address;
        entry.turnedOn    = (uint16_t) (after & ~m_videoBits);
        entry.turnedOff   = (uint16_t) (m_videoBits & ~after);
        entry.scanline    = (timing != nullptr) ? timing->GetCurrentScanline() : 0;
        entry.cycleInLine = (timing != nullptr) ? timing->GetHorizontalPos()   : 0;

        entry.cycleInFrame = entry.scanline * VideoTiming::kCyclesPerScanline + entry.cycleInLine;

        m_videoBits  = after;
        outEvents[0] = entry;
        return 1;
    }

    if (isWrite && m_boardPage != kNoBoard && (address >> kPageShift) == m_boardPage)
    {
        return DecodeBoard (machine, address, value, *m_cycles, outEvents);
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IoEventDecoder::DecodeBoard
//
//  A write to the card's page: bit 7 of the address picks the chip and the
//  low four bits the 6522 register. A write to port B whose control lines
//  change to write makes an AY register write of port A's byte to the
//  register the AY has latched, as the card does; reset low clears the
//  decode.
//
////////////////////////////////////////////////////////////////////////////////

size_t IoEventDecoder::DecodeBoard (
    MachineHost  & machine,
    Word           address,
    Byte           value,
    uint64_t       cycle,
    Events       & outEvents)
{
    MockingboardCard  * card    = machine.GetRefs().mockingboard;
    int                 chip    = ((address & kChipSelect) != 0) ? 1 : 0;
    Byte                reg     = (Byte) (address & kRegMask);
    Byte                portB   = 0;
    Byte                control = 0;
    IoEvent             entry;



    entry.cycle   = cycle;
    entry.kind    = IoEventKind::ViaWrite;
    entry.address = address;
    entry.chip    = (Byte) chip;
    entry.reg     = reg;
    entry.value   = value;

    outEvents[0] = entry;

    if (reg != kRegOrb || card == nullptr)
    {
        return 1;
    }

    portB   = card->GetVia (chip).GetPortB();
    control = (Byte) (portB & kAyControl);

    if ((portB & kAyResetLow) == 0)
    {
        m_lastControl[chip] = 0;
        return 1;
    }

    if (control == m_lastControl[chip])
    {
        return 1;
    }

    m_lastControl[chip] = control;

    if (control != kAyWrite)
    {
        return 1;
    }

    entry         = IoEvent();
    entry.cycle   = cycle;
    entry.kind    = IoEventKind::AyWrite;
    entry.chip    = (Byte) chip;
    entry.reg     = card->GetPsg (chip).GetLatchedAddress();
    entry.value   = card->GetVia (chip).GetPortA();

    outEvents[1] = entry;
    return 2;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IoEventDecoder::GetVideoModeBits
//
//  The display switches that are on, each where the machine has it, as
//  IoEventLog's mode bits.
//
////////////////////////////////////////////////////////////////////////////////

uint16_t IoEventDecoder::GetVideoModeBits (MachineHost & machine)
{
    const MachineRefs            & refs = machine.GetRefs();
    const Apple2eMmu             * mmu  = machine.GetMmu();
    const AppleSoftSwitchBank    * bank = refs.softSwitches;
    const Apple2eSoftSwitchBank  * iie  = refs.iieSoftSwitches;
    uint16_t                       bits = 0;



    if (bank != nullptr)
    {
        bits |= !bank->IsGraphicsMode() ? IoEventLog::kText  : 0;
        bits |= bank->IsMixedMode()     ? IoEventLog::kMixed : 0;
        bits |= bank->IsPage2()         ? IoEventLog::kPage2 : 0;
        bits |= bank->IsHiresMode()     ? IoEventLog::kHires : 0;
    }

    if (iie != nullptr)
    {
        bits |= iie->Is80ColMode()   ? IoEventLog::k80Col      : 0;
        bits |= iie->IsDoubleHiRes() ? IoEventLog::kDhires     : 0;
        bits |= iie->IsAltCharSet()  ? IoEventLog::kAltCharset : 0;
    }

    if (mmu != nullptr)
    {
        bits |= mmu->Get80Store() ? IoEventLog::k80Store : 0;
    }

    return bits;
}
